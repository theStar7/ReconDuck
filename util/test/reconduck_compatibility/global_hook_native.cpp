// Tests compile the real transaction header with real STL/SDK and isolated registry/process APIs.
#include <windows.h>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

struct RegistryValue
{
  bool exists = true;
  DWORD type = REG_SZ;
  std::vector<BYTE> bytes;
};
static RegistryValue values[2][2], originals[2][2];
static bool keys[2] = {};
static int writes = 0, reads = 0, opens = 0, launches = 0;
static int failWrite = 0, failLaunch = 0, failOpen = 0, failRead = 0;
static bool failRestore = false, restoring = false, failBackup = false, failPipe = false;
static bool failInheritance = false, exitedHelper = false, missingMapping = false;
static bool hangHelper = false, failTerminate = false, foreignMapping = false;
static bool failRestoreAfterLaunch = false;
static bool failClosePipe = false;
static int failPipeAt = 0, pipeCalls = 0;
static HANDLE latestPipe = NULL;
struct Helper
{
  HANDLE pipe, process;
};
static std::vector<Helper> helpers;
static std::set<HANDLE> owned;
static std::wstring testDirectory;

static void Check(bool ok, const char *message)
{
  if(!ok)
  {
    printf("FAIL: %s (error %lu)\n", message, GetLastError());
    fflush(stdout);
    ExitProcess(1);
  }
}
static int Index(HKEY key)
{
  return key == (HKEY)1 ? 0 : 1;
}
static int ValueIndex(LPCWSTR name)
{
  return wcscmp(name, L"AppInit_DLLs") == 0 ? 0 : 1;
}
static LSTATUS WINAPI FakeOpenKey(HKEY, LPCWSTR path, DWORD, REGSAM view, PHKEY key)
{
  Check(wcscmp(path, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows") == 0,
        "explicit registry view rather than Wow6432Node path");
  if(++opens == failOpen)
    return ERROR_ACCESS_DENIED;
  int i = (view & KEY_WOW64_64KEY) ? 0 : 1;
  keys[i] = true;
  *key = (HKEY)(ULONG_PTR)(i + 1);
  return ERROR_SUCCESS;
}
static LSTATUS WINAPI FakeCloseKey(HKEY key)
{
  Check(keys[Index(key)], "registry key closed exactly once");
  keys[Index(key)] = false;
  return ERROR_SUCCESS;
}
static LSTATUS WINAPI FakeQuery(HKEY key, LPCWSTR name, LPDWORD, LPDWORD type, LPBYTE data,
                                LPDWORD size)
{
  if(++reads == failRead)
    return ERROR_ACCESS_DENIED;
  RegistryValue &v = values[Index(key)][ValueIndex(name)];
  if(!v.exists)
    return ERROR_FILE_NOT_FOUND;
  *type = v.type;
  DWORD required = DWORD(v.bytes.size());
  if(data && *size < required)
  {
    *size = required;
    return ERROR_MORE_DATA;
  }
  if(data && required)
    memcpy(data, v.bytes.data(), required);
  *size = required;
  return ERROR_SUCCESS;
}
static LSTATUS WINAPI FakeSet(HKEY key, LPCWSTR name, DWORD, DWORD type, const BYTE *data, DWORD size)
{
  ++writes;
  if((!restoring && writes == failWrite) || (restoring && failRestore && Index(key) == 0))
    return ERROR_ACCESS_DENIED;
  RegistryValue &v = values[Index(key)][ValueIndex(name)];
  v.exists = true;
  v.type = type;
  v.bytes.assign(data, data + size);
  return ERROR_SUCCESS;
}
static LSTATUS WINAPI FakeDelete(HKEY key, LPCWSTR name)
{
  if(restoring && failRestore && Index(key) == 0)
    return ERROR_ACCESS_DENIED;
  RegistryValue &v = values[Index(key)][ValueIndex(name)];
  if(!v.exists)
    return ERROR_FILE_NOT_FOUND;
  v.exists = false;
  v.bytes.clear();
  return ERROR_SUCCESS;
}
static BOOL WINAPI FakeCloseHandle(HANDLE handle)
{
  if(failClosePipe && handle == latestPipe)
  {
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
  }
  for(const Helper &helper : helpers)
    if(helper.pipe == handle && !hangHelper)
      SetEvent(helper.process);
  owned.erase(handle);
  return CloseHandle(handle);
}
static HANDLE WINAPI FakeMutex(LPSECURITY_ATTRIBUTES security, BOOL initial, LPCWSTR)
{
  std::wstring name =
      L"Local\\ReconDuckGlobalHookNativeTest" + std::to_wstring(GetCurrentProcessId());
  HANDLE handle = CreateMutexW(security, initial, name.c_str());
  DWORD error = GetLastError();
  if(handle)
    owned.insert(handle);
  SetLastError(error);
  return handle;
}
static BOOL WINAPI FakePipe(PHANDLE read, PHANDLE write, LPSECURITY_ATTRIBUTES security, DWORD size)
{
  if(failPipe || ++pipeCalls == failPipeAt)
  {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return FALSE;
  }
  BOOL ok = CreatePipe(read, write, security, size);
  if(ok)
  {
    latestPipe = *write;
    owned.insert(*read);
    owned.insert(*write);
  }
  return ok;
}
static BOOL WINAPI FakeInheritance(HANDLE handle, DWORD mask, DWORD flags)
{
  if(failInheritance)
  {
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
  }
  return SetHandleInformation(handle, mask, flags);
}
static BOOL WINAPI FakeProcess(LPCWSTR application, LPWSTR, LPSECURITY_ATTRIBUTES,
                               LPSECURITY_ATTRIBUTES, BOOL inherit, DWORD flags, LPVOID, LPCWSTR,
                               LPSTARTUPINFOW startup, LPPROCESS_INFORMATION process)
{
  Check(inherit && (flags & EXTENDED_STARTUPINFO_PRESENT) && application,
        "restricted helper handle inheritance/application path");
  Check(startup->hStdInput && startup->hStdOutput == startup->hStdError, "helper standard handles");
  if(++launches == failLaunch)
  {
    if(failRestoreAfterLaunch)
      restoring = failRestore = true;
    SetLastError(ERROR_BAD_EXE_FORMAT);
    return FALSE;
  }
  process->hProcess = CreateEventW(NULL, TRUE, exitedHelper, NULL);
  process->hThread = CreateEventW(NULL, TRUE, TRUE, NULL);
  owned.insert(process->hProcess);
  owned.insert(process->hThread);
  helpers.push_back({latestPipe, process->hProcess});
  return TRUE;
}
static BOOL WINAPI FakeTerminate(HANDLE process, UINT)
{
  if(failTerminate)
  {
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
  }
  return SetEvent(process);
}
static BOOL WINAPI FakeExitCode(HANDLE, LPDWORD code)
{
  *code = 7;
  return TRUE;
}
static DWORD WINAPI FakeWait(HANDLE handle, DWORD milliseconds)
{
  for(const Helper &helper : helpers)
    if(helper.process == handle)
      return WaitForSingleObject(handle, 0);
  return WaitForSingleObject(handle, milliseconds);
}
static void WINAPI FakeSleep(DWORD)
{
}
static HANDLE WINAPI FakeMapping(DWORD, BOOL, LPCSTR name)
{
  size_t i = strcmp(name, "ReconDuckGlobalHookData64") == 0 ? 0 : 1;
  if(foreignMapping || (!missingMapping && helpers.size() > i))
  {
    HANDLE mapping = CreateEventW(NULL, TRUE, TRUE, NULL);
    owned.insert(mapping);
    return mapping;
  }
  SetLastError(ERROR_FILE_NOT_FOUND);
  return NULL;
}
static DWORD WINAPI FakeTempPath(DWORD size, LPWSTR output)
{
  if(failBackup)
  {
    SetLastError(ERROR_ACCESS_DENIED);
    return 0;
  }
  Check(size > testDirectory.size(), "backup output length");
  wcscpy_s(output, size, testDirectory.c_str());
  return DWORD(testDirectory.size());
}
// No registry API in the production header reaches advapi32 or the machine registry.
#define RegOpenKeyExW FakeOpenKey
#define RegCloseKey FakeCloseKey
#define RegQueryValueExW FakeQuery
#define RegSetValueExW FakeSet
#define RegDeleteValueW FakeDelete
#define CloseHandle FakeCloseHandle
#define CreateMutexW FakeMutex
#define CreatePipe FakePipe
#define SetHandleInformation FakeInheritance
#define CreateProcessW FakeProcess
#define TerminateProcess FakeTerminate
#define GetExitCodeProcess FakeExitCode
#define OpenFileMappingA FakeMapping
#define GetTempPathW FakeTempPath
#define WaitForSingleObject FakeWait
#define Sleep FakeSleep
#include "../../../reconduck/os/win32/win32_global_hook.h"
#undef CloseHandle
#include "global_hook_state.inc"
// Compiled directly from the actual public Process query, without duplicating its logic.
#include "global_hook_activity.inc"

static void Reset()
{
  Check(owned.empty() && !keys[0] && !keys[1], "previous session releases all owned resources");
  helpers.clear();
  writes = reads = opens = launches = 0;
  failPipeAt = pipeCalls = 0;
  failWrite = failLaunch = failOpen = failRead = 0;
  failRestore = restoring = failBackup = failPipe = failInheritance = false;
  exitedHelper = missingMapping = hangHelper = failTerminate = foreignMapping = false;
  failRestoreAfterLaunch = false;
  failClosePipe = false;
  for(int i = 0; i < 2; ++i)
  {
    const wchar_t *dlls = i == 0 ? L"D:\\Existing\\Native.dll,Another.dll" : L"E:\\Other\\Wow32.dll";
    values[i][0].exists = true;
    values[i][0].type = REG_SZ;
    values[i][0].bytes.assign((const BYTE *)dlls, (const BYTE *)(dlls + wcslen(dlls) + 1));
    DWORD enabled = i == 0 ? 0x12345678 : 0;
    values[i][1].exists = true;
    values[i][1].type = REG_DWORD;
    values[i][1].bytes.assign((const BYTE *)&enabled, (const BYTE *)&enabled + sizeof(enabled));
    originals[i][0] = values[i][0];
    originals[i][1] = values[i][1];
  }
}
static void Restored()
{
  for(int i = 0; i < 2; ++i)
    for(int j = 0; j < 2; ++j)
      Check(values[i][j].exists == originals[i][j].exists &&
                (!values[i][j].exists || (values[i][j].type == originals[i][j].type &&
                                          values[i][j].bytes == originals[i][j].bytes)),
            "original registry existence/type/bytes restored exactly");
  Check(owned.empty() && !keys[0] && !keys[1], "cleanup releases all handles and keys");
}
static std::vector<GlobalHook::Component> Components()
{
  return {{testDirectory + L"shim64.dll", testDirectory + L"command64.exe",
           testDirectory + L"library64.dll", KEY_WOW64_64KEY, L"64-bit"},
          {testDirectory + L"shim32.dll", testDirectory + L"command32.exe",
           testDirectory + L"library32.dll", KEY_WOW64_32KEY, L"32-bit"}};
}
static HANDLE operationEntered = NULL, releaseOperation = NULL;
static HANDLE secondAttempting = NULL, secondEntered = NULL;
static DWORD WINAPI FirstOperation(void *)
{
  GlobalHookOperation operation;
  SetEvent(operationEntered);
  WaitForSingleObject(releaseOperation, INFINITE);
  return 0;
}
static DWORD WINAPI SecondOperation(void *)
{
  SetEvent(secondAttempting);
  GlobalHookOperation operation;
  SetEvent(secondEntered);
  return 0;
}
static void CheckPublication()
{
  operationEntered = CreateEventW(NULL, TRUE, FALSE, NULL);
  releaseOperation = CreateEventW(NULL, TRUE, FALSE, NULL);
  secondAttempting = CreateEventW(NULL, TRUE, FALSE, NULL);
  secondEntered = CreateEventW(NULL, TRUE, FALSE, NULL);
  HANDLE first = CreateThread(NULL, 0, FirstOperation, NULL, 0, NULL);
  Check(WaitForSingleObject(operationEntered, 1000) == WAIT_OBJECT_0, "first operation started");
  Check(IsGlobalHookActive(), "startup visible before successful session publication");
  HANDLE second = CreateThread(NULL, 0, SecondOperation, NULL, 0, NULL);
  Check(WaitForSingleObject(secondAttempting, 1000) == WAIT_OBJECT_0, "second operation attempted");
  Check(WaitForSingleObject(secondEntered, 20) == WAIT_TIMEOUT, "Start/Stop mutations serialize");
  for(int i = 0; i < 10000; ++i)
    Check(IsGlobalHookActive(), "activity query stays nonblocking while operation lock is held");
  SetEvent(releaseOperation);
  Check(WaitForSingleObject(first, 1000) == WAIT_OBJECT_0, "first operation completed");
  Check(WaitForSingleObject(second, 1000) == WAIT_OBJECT_0, "second operation completed");
  Check(!IsGlobalHookActive(), "failed/unowned transaction clears atomic activity");
  for(HANDLE handle :
      {first, second, operationEntered, releaseOperation, secondAttempting, secondEntered})
    CloseHandle(handle);
}
int main()
{
  InterlockedExchange(&globalHookActivity, 1);
  Check(IsGlobalHookActive(), "public query reports startup before session pointer publication");
  InterlockedExchange(&globalHookActivity, 0);
  CheckPublication();
  wchar_t module[MAX_PATH];
  Check(GetModuleFileNameW(NULL, module, MAX_PATH) != 0, "test executable path");
  testDirectory = module;
  testDirectory.erase(testDirectory.find_last_of(L'\\') + 1);
  for(const GlobalHook::Component &component : Components())
    for(const std::wstring &path : {component.shim, component.command, component.library})
    {
      HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
      Check(file != INVALID_HANDLE_VALUE, "create isolated component fixtures");
      CloseHandle(file);
    }
  std::wstring reason;
  for(const wchar_t *path : {L"", L"relative.dll", L"\\\\server\\shim.dll", L"\\\\?\\D:\\shim.dll",
                             L"D:\\Space Folder\\shim.dll", L"D:\\a,b.dll", L"D:\\a\".dll",
                             L"D:\\a*.dll", L"D:\\a:b.dll", L"D:\\\u4e2d.dll"})
    Check(!GlobalHook::CheckPathSyntax(path, reason), "reject unsupported component path");
  Check(GlobalHook::CheckPathSyntax(L"D:\\Valid\\shim.dll", reason), "accept local ASCII path");
  Check(!GlobalHook::CheckPathSyntax(L"D:\\" + std::wstring(257, L'x'), reason), "reject MAX_PATH");
  Reset();
  {
    GlobalHook::Session session;
    auto components = Components();
    components[1].library += L".missing";
    Check(!session.Start(components, L"globalhook"), "missing alternate component rejected");
    Check(writes == 0 && opens == 0 && launches == 0, "validate all paths before side effects");
    Restored();
  }
  Reset();
  {
    GlobalHook::Session session;
    Check(session.Start(Components(), L"globalhook"), "two-view startup");
    for(int i = 0; i < 2; ++i)
    {
      std::wstring dllList((const wchar_t *)values[i][0].bytes.data());
      Check(dllList.find((const wchar_t *)originals[i][0].bytes.data()) == 0,
            "preserve existing AppInit entries while active");
      Check(dllList.find(Components()[i].shim) != std::wstring::npos, "append owned shim");
    }
    Check(session.Cleanup(), "successful cleanup");
    Check(session.Cleanup(), "repeated cleanup");
    Restored();
  }
  for(int stage = 1; stage <= 4; ++stage)
  {
    Reset();
    failWrite = stage;
    GlobalHook::Session session;
    Check(!session.Start(Components(), L"globalhook"), "registry write failure rolls back");
    Check(launches == 0, "no helper before registry transaction complete");
    Restored();
  }
  for(int stage = 1; stage <= 2; ++stage)
  {
    Reset();
    failOpen = stage;
    GlobalHook::Session session;
    Check(!session.Start(Components(), L"globalhook"), "registry view open failure");
    Check(writes == 0, "snapshot every view before writing");
    Restored();
  }
  for(int stage = 1; stage <= 2; ++stage)
  {
    Reset();
    failPipeAt = stage;
    GlobalHook::Session session;
    Check(!session.Start(Components(), L"globalhook"), "partial pipe start failure");
    Restored();
  }
  for(int stage = 1; stage <= 8; ++stage)
  {
    Reset();
    failRead = stage;
    GlobalHook::Session session;
    Check(!session.Start(Components(), L"globalhook"), "registry snapshot read failure");
    Check(writes == 0, "read failure leaves both registry views untouched");
    Restored();
  }
  for(int stage = 1; stage <= 2; ++stage)
  {
    Reset();
    failLaunch = stage;
    GlobalHook::Session session;
    Check(!session.Start(Components(), L"globalhook"), "partial process start failure");
    Restored();
  }
  for(int stage = 0; stage < 6; ++stage)
  {
    Reset();
    failBackup = stage == 0;
    failPipe = stage == 1;
    failInheritance = stage == 2;
    exitedHelper = stage == 3;
    foreignMapping = stage == 4;
    missingMapping = stage == 5;
    GlobalHook::Session session;
    Check(!session.Start(Components(), L"globalhook"),
          "backup/pipe/inheritance/readiness/foreign failure");
    if(stage == 0 || stage == 4)
      Check(writes == 0, "backup/existing hook fails before writes");
    Restored();
  }
  Reset();
  {
    failLaunch = 2;
    failRestoreAfterLaunch = true;
    GlobalHook::Session session;
    Check(!session.Start(Components(), L"globalhook"), "partial startup plus rollback failure");
    Check(keys[0] && !keys[1] && session.targets[0].dlls.modified,
          "failed startup retains snapshots for retry");
    Check(session.error.find(L"launch") != std::wstring::npos &&
              session.error.find(L"restore") != std::wstring::npos,
          "original startup failure and recovery error retained together");
    failRestore = false;
    Check(session.Cleanup(), "retry cleanup after failed startup");
    Restored();
  }
  Reset();
  {
    GlobalHook::Session session;
    auto components = Components();
    components[0].shim = testDirectory;
    Check(!session.Start(components, L"globalhook"), "directory component rejected");
    Check(opens == 0 && writes == 0, "directory path fails before registry access");
    Restored();
  }
  for(int stage = 0; stage < 4; ++stage)
  {
    Reset();
    if(stage == 0)
      values[1][0].type = REG_BINARY;
    if(stage == 1)
      values[1][0].bytes.pop_back();
    if(stage == 2)
      values[1][0].bytes[values[1][0].bytes.size() - 2] = 'x';
    if(stage == 3)
      values[1][0].bytes[2] = values[1][0].bytes[3] = 0;
    originals[1][0] = values[1][0];
    GlobalHook::Session session;
    Check(!session.Start(Components(), L"globalhook"),
          "malformed AppInit values fail with rollback");
    Restored();
  }
  Reset();
  {
    for(int i = 0; i < 2; ++i)
      for(int j = 0; j < 2; ++j)
        values[i][j].exists = originals[i][j].exists = false;
    GlobalHook::Session session;
    Check(session.Start(Components(), L"globalhook"), "absent original values supported");
    Check(session.Cleanup(), "absent values restored by deletion");
    Restored();
  }
  Reset();
  {
    GlobalHook::Session session;
    Check(session.Start(Components(), L"globalhook"), "retry cleanup startup");
    restoring = failRestore = true;
    Check(!session.Cleanup(), "restoration failure reported");
    {
      GlobalHookOperation operation;
      pendingGlobalHook = &session;
    }
    Check(IsGlobalHookActive(), "public query reports unresolved restoration for Stop retry");
    Check(keys[0] && !keys[1], "failed snapshot retained, independent view restored");
    Check(session.targets[0].dlls.modified && session.targets[0].enabled.modified,
          "owned state retained until restored");
    GlobalHook::Session contender;
    Check(!contender.Start(Components(), L"globalhook"),
          "failed restore retains transaction lease");
    failRestore = false;
    Check(session.Cleanup(), "later cleanup retries restoration");
    {
      GlobalHookOperation operation;
      pendingGlobalHook = NULL;
    }
    Check(!IsGlobalHookActive(), "public query clears after recovery completes");
    Restored();
  }
  Reset();
  {
    GlobalHook::Session session;
    Check(session.Start(Components(), L"globalhook"), "pipe close recovery startup");
    failClosePipe = true;
    Check(!session.Cleanup(), "pipe close failure reported");
    Check(session.targets[1].pipe == latestPipe, "failed pipe handle retained for retry");
    failClosePipe = false;
    Check(session.Cleanup(), "retry pipe close recovery");
    Restored();
  }
  Reset();
  {
    GlobalHook::Session session;
    Check(session.Start(Components(), L"globalhook"), "helper termination startup");
    hangHelper = true;
    Check(session.Cleanup(), "owned unresponsive helpers terminated after EOF timeout");
    Restored();
  }
  Reset();
  {
    GlobalHook::Session session;
    Check(session.Start(Components(), L"globalhook"), "helper retry startup");
    hangHelper = failTerminate = true;
    Check(!session.Cleanup(), "helper termination failure reported");
    Check(session.targets[0].process != NULL, "failed helper handle retained");
    failTerminate = false;
    Check(session.Cleanup(), "later cleanup retries helper termination");
    Restored();
  }
  printf(
      "PASS: Global Hook paths, both registry views, preserved values, rollback, readiness, "
      "retryable recovery\n");
  return 0;
}
