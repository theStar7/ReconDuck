#include <windows.h>

#if __has_include("os/win32/win32_export_detour.h")
#include "os/win32/win32_export_detour.h"
#else
// Baseline IAT/GetProcAddress behavior cannot intercept an already cached export.
namespace ExportDetour
{
struct Registry
{
  DWORD Attach(const char *, HMODULE module, const char *name, void **original, void *)
  {
    *original = (void *)GetProcAddress(module, name);
    return NO_ERROR;
  }
  bool IsAttached(void **) { return false; }
  DWORD RemoveAll() { return NO_ERROR; }
};
}
#endif

typedef int(WINAPI *Target)(int);
static Target onward;
static volatile LONG hookCalls;
static Target alias;
static volatile LONG stopWorker;
static volatile LONG workerErrors;
static Target cachedForWorker;
static HANDLE heapLocked;
static HANDLE releaseHeap;

static DWORD WINAPI HoldProcessHeap(void *)
{
  HeapLock(GetProcessHeap());
  SetEvent(heapLocked);
  WaitForSingleObject(releaseHeap, INFINITE);
  HeapUnlock(GetProcessHeap());
  return 0;
}

static DWORD WINAPI CallWhilePatching(void *)
{
  while(!InterlockedCompareExchange(&stopWorker, 0, 0))
  {
    int result = cachedForWorker(5);
    if(result != 22 && result != 122)
      InterlockedIncrement(&workerErrors);
    result = alias(5);
    if(result != 22 && result != 122)
      InterlockedIncrement(&workerErrors);
  }
  return 0;
}

static int WINAPI Intercept(int value)
{
  InterlockedIncrement(&hookCalls);
  return onward(value) + 100;
}

static int WINAPI ConflictingIntercept(int value)
{
  return value - 10;
}

static void Check(bool condition, const char *message)
{
  if(!condition)
  {
    DWORD written;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), message, lstrlenA(message), &written, NULL);
    ExitProcess(1);
  }
}

void entry()
{
  wchar_t filename[MAX_PATH];
  Check(GetEnvironmentVariableW(L"DETOUR_FIXTURE", filename, MAX_PATH) != 0, "FAIL fixture path\n");
  HMODULE module = LoadLibraryW(filename);
  Check(module != NULL, "FAIL fixture load\n");
  Target cached = (Target)GetProcAddress(module, "CachedTarget");
  Check(cached && cached(5) == 22, "FAIL baseline export\n");

  ExportDetour::Registry registry;
  Check(registry.Attach("d3d11.dll", module, "CachedTarget", (void **)&onward, (void *)Intercept) ==
            NO_ERROR,
        "FAIL attach\n");
  Check(cached(5) == 122 && hookCalls == 1, "FAIL cached export bypassed interception\n");
  Check(onward(5) == 22, "FAIL onward trampoline\n");
  Check(registry.IsAttached((void **)&onward), "FAIL tracked trampoline\n");
  Check(registry.Attach("d3d11.dll", module, "CachedTarget", (void **)&onward, (void *)Intercept) ==
            NO_ERROR,
        "FAIL repeated attach\n");
  Check(cached(5) == 122 && onward(5) == 22, "FAIL repeated attach recursion\n");

  Check(registry.Attach("dxgi.dll", module, "CachedTarget", (void **)&alias, (void *)Intercept) ==
            NO_ERROR && alias(5) == 22,
        "FAIL shared export trampoline\n");
  Target conflict = NULL;
  Check(registry.Attach("d3d12.dll", module, "CachedTarget", (void **)&conflict,
                        (void *)ConflictingIntercept) == ERROR_ALREADY_EXISTS,
        "FAIL conflicting hook accepted\n");
  Check(cached(5) == 122, "FAIL conflict damaged existing hook\n");
  Check(conflict && conflict(5) == 22 && registry.IsAttached((void **)&conflict),
        "FAIL conflicting IAT callback cannot forward natively\n");

  const char *excluded[] = {"kernel32.dll", "KERNELBASE.DLL", "nvapi.dll", "NVAPI64.DLL",
                            "api-ms-win-core-libraryloader-l1-2-0.dll"};
  for(const char *library : excluded)
    Check(registry.Attach(library, module, "CachedTarget", (void **)&conflict, (void *)Intercept) ==
              ERROR_NOT_SUPPORTED,
          "FAIL unsafe library detoured\n");

  Check(registry.RemoveAll() == NO_ERROR, "FAIL remove hooks\n");
  Check(cached(5) == 22 && onward(5) == 22 && alias(5) == 22 && conflict(5) == 22,
        "FAIL entry or alias not restored\n");
  Check(!registry.IsAttached((void **)&onward), "FAIL detached state retained\n");
  Check(registry.RemoveAll() == NO_ERROR, "FAIL repeated cleanup\n");

  heapLocked = CreateEventW(NULL, TRUE, FALSE, NULL);
  releaseHeap = CreateEventW(NULL, TRUE, FALSE, NULL);
  Check(heapLocked && releaseHeap, "FAIL heap test events\n");
  Check(DetourTransactionBegin() == NO_ERROR &&
            DetourAttach((PVOID *)&onward, (PVOID)Intercept) == NO_ERROR,
        "FAIL heap test transaction\n");
  HANDLE heapHolder = CreateThread(NULL, 0, HoldProcessHeap, NULL, 0, NULL);
  Check(heapHolder && WaitForSingleObject(heapLocked, 5000) == WAIT_OBJECT_0,
        "FAIL heap holder startup\n");
  // The suspended thread owns the process heap until the transaction resumes it.
  Check(DetourUpdateThread(heapHolder) == NO_ERROR && DetourTransactionCommit() == NO_ERROR,
        "FAIL transaction while process heap is held\n");
  SetEvent(releaseHeap);
  Check(WaitForSingleObject(heapHolder, 5000) == WAIT_OBJECT_0, "FAIL heap holder exit\n");
  CloseHandle(heapHolder);
  CloseHandle(heapLocked);
  CloseHandle(releaseHeap);
  Check(cached(5) == 122 && onward(5) == 22, "FAIL heap test interception\n");
  Check(DetourTransactionBegin() == NO_ERROR &&
            DetourDetach((PVOID *)&onward, (PVOID)Intercept) == NO_ERROR &&
            DetourTransactionCommit() == NO_ERROR,
        "FAIL heap test detach\n");

  cachedForWorker = cached;
  HANDLE worker = CreateThread(NULL, 0, CallWhilePatching, NULL, 0, NULL);
  Check(worker != NULL, "FAIL worker startup\n");
  DWORD progress;
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), "Concurrent installation starting\n", 33, &progress, NULL);
  for(int iteration = 0; iteration < 100; ++iteration)
  {
    Check(registry.Attach("dxgi.dll", module, "CachedTarget", (void **)&onward, (void *)Intercept) ==
              NO_ERROR,
          "FAIL concurrent attach\n");
    Check(registry.Attach("dxgi.dll", module, "CachedTarget", (void **)&alias, (void *)Intercept) ==
              NO_ERROR,
          "FAIL concurrent alias\n");
    if(iteration % 10 == 0)
      WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), "Cycle batch passed\n", 19, &progress, NULL);
  }
  InterlockedExchange(&stopWorker, 1);
  Check(WaitForSingleObject(worker, 5000) == WAIT_OBJECT_0, "FAIL worker did not exit\n");
  Check(workerErrors == 0, "FAIL worker observed an invalid result\n");
  CloseHandle(worker);
  // ReconDuck's public RemoveHooks contract requires no API work in flight.
  // Detours cannot rewrite a function pointer already held in a hook frame.
  Check(registry.RemoveAll() == NO_ERROR, "FAIL quiescent detach\n");
  Check(cached(5) == 22 && onward(5) == 22 && alias(5) == 22,
        "FAIL quiescent restoration\n");
  for(int iteration = 0; iteration < 100; ++iteration)
  {
    Check(registry.Attach("dxgi.dll", module, "CachedTarget", (void **)&onward, (void *)Intercept) ==
              NO_ERROR,
          "FAIL reinstall\n");
    Check(cached(5) == 122 && onward(5) == 22, "FAIL reinstalled trampoline\n");
    Check(registry.RemoveAll() == NO_ERROR, "FAIL repeated quiescent detach\n");
  }
  FreeLibrary(module);
  DWORD written;
  const char message[] = "PASS cached export, trampoline, repeat/alias/conflict, exclusions, concurrent installation and 100 quiescent patch cycles\n";
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), message, sizeof(message) - 1, &written, NULL);
  ExitProcess(0);
}
