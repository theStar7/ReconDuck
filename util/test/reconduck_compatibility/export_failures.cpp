#include <windows.h>
#include "detours.h"

static LONG WINAPI MockBegin();
static LONG WINAPI MockAbort();
static LONG WINAPI MockCommit();
static LONG WINAPI MockAttach(PVOID *, PVOID);
static LONG WINAPI MockDetach(PVOID *, PVOID);
static LONG WINAPI MockUpdate(HANDLE);
static BOOL WINAPI MockPin(DWORD, LPCSTR, HMODULE *);
static BOOL WINAPI MockRelease(HMODULE);

// Compile the real registry with a failure-injecting transaction backend.
// PE export resolution, module reference counts and allocation stay native.
#define DetourTransactionBegin MockBegin
#define DetourTransactionAbort MockAbort
#define DetourTransactionCommit MockCommit
#define DetourAttach MockAttach
#define DetourDetach MockDetach
#define DetourUpdateThread MockUpdate
#define GetModuleHandleExA MockPin
#define FreeLibrary MockRelease
#include "os/win32/win32_export_detour.h"
#undef DetourTransactionBegin
#undef DetourTransactionAbort
#undef DetourTransactionCommit
#undef DetourAttach
#undef DetourDetach
#undef DetourUpdateThread
#undef GetModuleHandleExA
#undef FreeLibrary

typedef int(WINAPI *Target)(int);
static Target nativeTarget;
static LONG beginResult, attachResult, detachResult, updateResult, commitResult;
static bool pinFailure, transactionOwned, pendingRemove;
static PVOID *pendingOriginal;
static int abortCalls, pinCalls, releaseCalls;
static const char *testName;

static int WINAPI Trampoline(int value) { return nativeTarget(value); }
static int WINAPI Hook(int value) { return value + 100; }

static void Write(const char *message)
{
  DWORD written;
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), message, lstrlenA(message), &written, NULL);
}

static void Check(bool condition, const char *message)
{
  if(!condition)
  {
    Write("FAIL ");
    Write(testName);
    Write(": ");
    Write(message);
    Write("\n");
    ExitProcess(1);
  }
}

static LONG WINAPI MockBegin()
{
  if(transactionOwned)
    return ERROR_INVALID_OPERATION;
  if(beginResult != ERROR_INVALID_OPERATION)
    transactionOwned = true;
  return beginResult;
}

static LONG WINAPI MockAbort()
{
  ++abortCalls;
  transactionOwned = false;
  pendingOriginal = NULL;
  return NO_ERROR;
}

static LONG WINAPI MockAttach(PVOID *original, PVOID)
{
  if(attachResult == NO_ERROR)
  {
    pendingOriginal = original;
    pendingRemove = false;
  }
  return attachResult;
}

static LONG WINAPI MockDetach(PVOID *original, PVOID)
{
  if(detachResult == NO_ERROR)
  {
    pendingOriginal = original;
    pendingRemove = true;
  }
  return detachResult;
}

static LONG WINAPI MockUpdate(HANDLE) { return updateResult; }

static LONG WINAPI MockCommit()
{
  if(commitResult == NO_ERROR && pendingOriginal)
    *pendingOriginal = pendingRemove ? (void *)nativeTarget : (void *)Trampoline;
  pendingOriginal = NULL;
  transactionOwned = false;
  return commitResult;
}

static BOOL WINAPI MockPin(DWORD flags, LPCSTR address, HMODULE *module)
{
  if(pinFailure)
  {
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
  }
  BOOL result = GetModuleHandleExA(flags, address, module);
  if(result)
    ++pinCalls;
  return result;
}

static BOOL WINAPI MockRelease(HMODULE module)
{
  BOOL result = FreeLibrary(module);
  if(result)
    ++releaseCalls;
  return result;
}

static void Reset()
{
  beginResult = attachResult = detachResult = updateResult = commitResult = NO_ERROR;
  pinFailure = transactionOwned = pendingRemove = false;
  pendingOriginal = NULL;
  abortCalls = pinCalls = releaseCalls = 0;
}

void entry()
{
  testName = "fixture";
  wchar_t filename[MAX_PATH];
  Check(GetEnvironmentVariableW(L"DETOUR_FIXTURE", filename, MAX_PATH) != 0, "path missing");
  HMODULE fixture = LoadLibraryW(filename);
  Check(fixture != NULL, "load failed");
  nativeTarget = (Target)GetProcAddress(fixture, "CachedTarget");
  Check(nativeTarget && nativeTarget(5) == 22, "native export failed");

  const char *installCases[] = {"existing owner", "owned begin failure", "attach failure",
                                "thread update failure", "commit failure", "module pin failure"};
  for(int failure = 0; failure < 6; ++failure)
  {
    Reset();
    testName = installCases[failure];
    if(failure == 0) { beginResult = ERROR_INVALID_OPERATION; transactionOwned = true; }
    if(failure == 1) beginResult = ERROR_NOACCESS;
    if(failure == 2) attachResult = ERROR_INVALID_BLOCK;
    if(failure == 3) updateResult = ERROR_ACCESS_DENIED;
    if(failure == 4) commitResult = ERROR_INVALID_DATA;
    if(failure == 5) pinFailure = true;

    ExportDetour::Registry registry;
    Target original = nativeTarget;
    DWORD expected = failure == 0 ? ERROR_INVALID_OPERATION : failure == 1 ? ERROR_NOACCESS :
                     failure == 2 ? ERROR_INVALID_BLOCK : failure == 4 ? ERROR_INVALID_DATA :
                     ERROR_ACCESS_DENIED;
    Check(registry.Attach("d3d11.dll", fixture, "CachedTarget", (void **)&original,
                          (void *)Hook) == expected, "wrong failure result");
    Check(!registry.IsAttached((void **)&original), "failed install retained tracking");
    Check(original == nativeTarget && original(5) == 22, "native fallback damaged");
    Check(abortCalls == (failure >= 1 && failure <= 3 ? 1 : 0), "wrong abort ownership");
    Check(transactionOwned == (failure == 0), "transaction owner changed incorrectly");
    Check(pinCalls == releaseCalls, "failed install leaked module reference");

    // Retry proves an owned begin failure did not latch transaction state.
    beginResult = attachResult = updateResult = commitResult = NO_ERROR;
    pinFailure = transactionOwned = false;
    Check(registry.Attach("d3d11.dll", fixture, "CachedTarget", (void **)&original,
                          (void *)Hook) == NO_ERROR, "retry failed");
    Check(registry.IsAttached((void **)&original) && original(5) == 22, "retry trampoline failed");
    Check(registry.RemoveAll() == NO_ERROR, "retry cleanup failed");
    Check(pinCalls == releaseCalls && original == nativeTarget, "retry reference or pointer leaked");
  }

  const char *removeCases[] = {"detach begin failure", "detach operation failure",
                               "detach thread failure", "detach commit failure"};
  for(int failure = 0; failure < 4; ++failure)
  {
    Reset();
    testName = removeCases[failure];
    ExportDetour::Registry registry;
    Target original = nativeTarget, alias = nativeTarget;
    Check(registry.Attach("d3d11.dll", fixture, "CachedTarget", (void **)&original,
                          (void *)Hook) == NO_ERROR, "setup failed");
    Check(registry.Attach("dxgi.dll", fixture, "CachedTarget", (void **)&alias,
                          (void *)Hook) == NO_ERROR, "alias setup failed");
    if(failure == 0) beginResult = ERROR_NOACCESS;
    if(failure == 1) detachResult = ERROR_INVALID_BLOCK;
    if(failure == 2) updateResult = ERROR_ACCESS_DENIED;
    if(failure == 3) commitResult = ERROR_INVALID_DATA;
    DWORD expected = failure == 0 ? ERROR_NOACCESS : failure == 1 ? ERROR_INVALID_BLOCK :
                     failure == 2 ? ERROR_ACCESS_DENIED : ERROR_INVALID_DATA;
    Check(registry.RemoveAll() == expected, "wrong cleanup failure result");
    Check(registry.IsAttached((void **)&original) && registry.IsAttached((void **)&alias),
          "failed cleanup lost tracking");
    Check(original == Trampoline && alias == Trampoline && original(5) == 22 && alias(5) == 22,
          "failed cleanup damaged original or alias");
    Check(pinCalls == 1 && releaseCalls == 0, "failed cleanup released owned module");
    Check(abortCalls == (failure == 3 ? 0 : 1) && !transactionOwned, "cleanup owner leaked");
    beginResult = detachResult = updateResult = commitResult = NO_ERROR;
    Check(registry.RemoveAll() == NO_ERROR, "cleanup retry failed");
    Check(!registry.IsAttached((void **)&original) && !registry.IsAttached((void **)&alias),
          "cleanup retry retained tracking");
    Check(original == nativeTarget && alias == nativeTarget && pinCalls == releaseCalls,
          "cleanup retry reference or pointer leaked");
  }
  FreeLibrary(fixture);
  Write("PASS 10 export transaction failures, ownership, native fallback, aliases and module refs\n");
  ExitProcess(0);
}
