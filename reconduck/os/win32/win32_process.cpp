/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

// must be separate so that it's included first and not sorted by clang-format
#include <windows.h>

#include <Psapi.h>
#include <tchar.h>
#include <tlhelp32.h>
#include "common/formatting.h"
#include "core/core.h"
#include "os/os_specific.h"
#include "os/win32/win32_threadcontext.h"
#include "os/win32/win32_global_hook.h"
#include "strings/string_utils.h"

#include <string>

static rdcarray<EnvironmentModification> &GetEnvModifications()
{
  static rdcarray<EnvironmentModification> envCallbacks;
  return envCallbacks;
}

struct InsensitiveComparison
{
  bool operator()(const rdcstr &a, const rdcstr &b) const { return strlower(a) < strlower(b); }
};

typedef std::map<rdcstr, rdcstr, InsensitiveComparison> EnvMap;

static EnvMap EnvStringToEnvMap(const wchar_t *envstring)
{
  EnvMap ret;

  const wchar_t *e = envstring;

  while(*e)
  {
    const wchar_t *equals = wcschr(e, L'=');

    rdcstr name = StringFormat::Wide2UTF8(rdcwstr(e, equals - e));
    rdcstr value = StringFormat::Wide2UTF8(equals + 1);

    ret[name] = value;

    // jump to \0 and past it
    e += wcslen(e) + 1;
  }

  return ret;
}

void Process::RegisterEnvironmentModification(const EnvironmentModification &modif)
{
  GetEnvModifications().push_back(modif);
}

static void ApplyEnvModifications(EnvMap &envValues,
                                  const rdcarray<EnvironmentModification> &modifications,
                                  bool setToSystem)
{
  for(size_t i = 0; i < modifications.size(); i++)
  {
    const EnvironmentModification &m = modifications[i];

    rdcstr value;

    auto it = envValues.find(m.name);
    if(it != envValues.end())
      value = it->second;

    switch(m.mod)
    {
      case EnvMod::Set: value = m.value.c_str(); break;
      case EnvMod::Append:
      {
        if(!value.empty())
        {
          if(m.sep == EnvSep::Platform || m.sep == EnvSep::SemiColon)
            value += ";";
          else if(m.sep == EnvSep::Colon)
            value += ":";
        }
        value += m.value.c_str();
        break;
      }
      case EnvMod::Prepend:
      {
        if(!value.empty())
        {
          rdcstr prep = m.value;
          if(m.sep == EnvSep::Platform || m.sep == EnvSep::SemiColon)
            prep += ";";
          else if(m.sep == EnvSep::Colon)
            prep += ":";
          value = prep + value;
        }
        else
        {
          value = m.value.c_str();
        }
        break;
      }
    }

    envValues[m.name] = value;

    if(setToSystem)
      SetEnvironmentVariableW(StringFormat::UTF82Wide(m.name).c_str(),
                              StringFormat::UTF82Wide(value).c_str());
  }
}

// on windows we apply environment changes here, after process initialisation
// but before any real work (in ReconDuck::Initialise) so that we support
// injecting the dll into processes we didn't launch (ie didn't control the
// starting environment for), or even the application loading the dll itself
// without any interaction with our replay app.
void Process::ApplyEnvironmentModification()
{
  // turn environment string to a UTF-8 map
  LPWCH envStrings = GetEnvironmentStringsW();
  EnvMap envValues = EnvStringToEnvMap(envStrings);
  FreeEnvironmentStringsW(envStrings);
  rdcarray<EnvironmentModification> &modifications = GetEnvModifications();

  ApplyEnvModifications(envValues, modifications, true);

  // these have been applied to the current process
  modifications.clear();
}

rdcstr Process::GetEnvVariable(const rdcstr &name)
{
  DWORD len = GetEnvironmentVariableA(name.c_str(), NULL, 0);
  if(len == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND)
    return rdcstr();

  rdcstr ret;
  ret.resize(len + 1);

  GetEnvironmentVariableA(name.c_str(), ret.data(), len);
  ret.trim();
  return ret;
}

uint64_t Process::GetMemoryUsage()
{
  HANDLE proc = GetCurrentProcess();

  if(proc == NULL)
  {
    RDCERR("Couldn't open process: %d", GetLastError());
    return 0;
  }

  PROCESS_MEMORY_COUNTERS memInfo = {};

  uint64_t ret = 0;

  if(GetProcessMemoryInfo(proc, &memInfo, sizeof(memInfo)))
  {
    ret = memInfo.WorkingSetSize;
  }
  else
  {
    RDCERR("Couldn't get process memory info: %d", GetLastError());
  }

  return ret;
}

// helpers for various shims and dlls etc, not part of the public API
extern "C" __declspec(dllexport) void __cdecl INTERNAL_GetTargetControlIdent(uint32_t *ident)
{
  if(ident)
    *ident = ReconDuck::Inst().GetTargetControlIdent();
}

extern "C" __declspec(dllexport) void __cdecl INTERNAL_SetCaptureOptions(CaptureOptions *opts)
{
  if(opts)
    ReconDuck::Inst().SetCaptureOptions(*opts);
}

extern "C" __declspec(dllexport) void __cdecl INTERNAL_SetCaptureFile(const char *capfile)
{
  if(capfile)
    ReconDuck::Inst().SetCaptureFileTemplate(capfile);
}

extern "C" __declspec(dllexport) void __cdecl INTERNAL_SetDebugLogFile(const char *logfile)
{
  RECONDUCK_SetDebugLogFile(logfile ? logfile : rdcstr());
}

static EnvironmentModification tempEnvMod;

extern "C" __declspec(dllexport) void __cdecl INTERNAL_EnvModName(const char *name)
{
  if(name)
    tempEnvMod.name = name;
}

extern "C" __declspec(dllexport) void __cdecl INTERNAL_EnvModValue(const char *value)
{
  if(value)
    tempEnvMod.value = value;
}

extern "C" __declspec(dllexport) void __cdecl INTERNAL_EnvSep(EnvSep *sep)
{
  if(sep)
    tempEnvMod.sep = *sep;
}

extern "C" __declspec(dllexport) void __cdecl INTERNAL_EnvMod(EnvMod *mod)
{
  if(mod)
  {
    tempEnvMod.mod = *mod;
    Process::RegisterEnvironmentModification(tempEnvMod);
  }
}

extern "C" __declspec(dllexport) void __cdecl INTERNAL_ApplyEnvMods(void *ignored)
{
  Process::ApplyEnvironmentModification();
}

bool InjectDLL(HANDLE hProcess, rdcwstr libName, DWORD startupThreadId)
{
  wchar_t dllPath[MAX_PATH + 1] = {0};
  wcscpy_s(dllPath, libName.c_str());

  static HMODULE kernel32 = GetModuleHandleA("kernel32.dll");

  if(kernel32 == NULL)
  {
    RDCERR("Couldn't get handle for kernel32.dll");
    return false;
  }

  if(startupThreadId)
  {
    ULONG_PTR module = 0;
    bool success = CallOnSuspendedThread(hProcess, startupThreadId,
                                         (void *)GetProcAddress(kernel32, "LoadLibraryW"), dllPath,
                                         sizeof(dllPath), &module);
    if(!success || !module)
    {
      RDCERR("Couldn't load DLL on the startup thread: %u", GetLastError());
      return false;
    }
    return true;
  }

  bool loaded = false;
  void *remoteMem =
      VirtualAllocEx(hProcess, NULL, sizeof(dllPath), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  if(remoteMem)
  {
    BOOL success = WriteProcessMemory(hProcess, remoteMem, (void *)dllPath, sizeof(dllPath), NULL);
    if(success)
    {
      HANDLE hThread = CreateRemoteThread(
          hProcess, NULL, 1024 * 1024U,
          (LPTHREAD_START_ROUTINE)GetProcAddress(kernel32, "LoadLibraryW"), remoteMem, 0, NULL);
      if(hThread)
      {
        loaded = WaitForSingleObject(hThread, INFINITE) == WAIT_OBJECT_0;
        CloseHandle(hThread);
      }
      else
      {
        RDCERR("Couldn't create remote thread for LoadLibraryW: %u", GetLastError());
      }
    }
    else
    {
      RDCERR("Couldn't write remote memory %p with dllPath '%ls': %u", remoteMem, dllPath,
             GetLastError());
    }

    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
  }
  else
  {
    RDCERR("Couldn't allocate remote memory for DLL '%ls': %u", libName.c_str(), GetLastError());
  }
  return loaded;
}

uintptr_t FindRemoteDLL(DWORD pid, rdcstr libName)
{
  HANDLE hModuleSnap = INVALID_HANDLE_VALUE;

  rdcwstr wlibName = StringFormat::UTF82Wide(strlower(libName));

  // up to 10 retries
  for(int i = 0; i < 10; i++)
  {
    hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);

    if(hModuleSnap == INVALID_HANDLE_VALUE)
    {
      DWORD err = GetLastError();

      RDCWARN("CreateToolhelp32Snapshot(%u) -> 0x%08x", pid, err);

      // retry if error is ERROR_BAD_LENGTH
      if(err == ERROR_BAD_LENGTH)
        continue;
    }

    // didn't retry, or succeeded
    break;
  }

  if(hModuleSnap == INVALID_HANDLE_VALUE)
  {
    RDCERR("Couldn't create toolhelp dump of modules in process %u", pid);
    return 0;
  }

  MODULEENTRY32 me32;
  RDCEraseEl(me32);
  me32.dwSize = sizeof(MODULEENTRY32);

  BOOL success = Module32First(hModuleSnap, &me32);

  if(success == FALSE)
  {
    DWORD err = GetLastError();

    RDCERR("Couldn't get first module in process %u: 0x%08x", pid, err);
    CloseHandle(hModuleSnap);
    return 0;
  }

  uintptr_t ret = 0;

  int numModules = 0;

  do
  {
    wchar_t modnameLower[MAX_MODULE_NAME32 + 1];
    RDCEraseEl(modnameLower);
    wcsncpy_s(modnameLower, me32.szModule, MAX_MODULE_NAME32);

    wchar_t *wc = &modnameLower[0];
    while(*wc)
    {
      *wc = towlower(*wc);
      wc++;
    }

    numModules++;

    if(wcsstr(modnameLower, wlibName.c_str()) == modnameLower)
    {
      ret = (uintptr_t)me32.modBaseAddr;
    }
  } while(ret == 0 && Module32Next(hModuleSnap, &me32));

  if(ret == 0)
  {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);

    DWORD exitCode = 0;

    if(h)
      GetExitCodeProcess(h, &exitCode);

    if(h == NULL || exitCode != STILL_ACTIVE)
    {
      RDCERR(
          "Error injecting into remote process with PID %u which is no longer available.\n"
          "Possibly the process has crashed during early startup, or is missing DLLs to run?",
          pid);
    }
    else
    {
      RDCERR("Couldn't find module '%s' among %d modules", libName.c_str(), numModules);
    }

    if(h)
      CloseHandle(h);
  }

  CloseHandle(hModuleSnap);

  return ret;
}

bool InjectFunctionCall(HANDLE hProcess, uintptr_t reconduck_remote, const char *funcName,
                        void *data, const size_t dataLen, DWORD startupThreadId)
{
  if(dataLen == 0)
  {
    RDCERR("Invalid function call injection attempt");
    return false;
  }

  RDCDEBUG("Injecting call to %s", funcName);

  HMODULE reconduck_local = GetModuleHandleA(STRINGIZE(RDOC_BASE_NAME) ".dll");

  uintptr_t func_local = (uintptr_t)GetProcAddress(reconduck_local, funcName);
  if(!func_local)
  {
    RDCERR("Couldn't find injection function %s", funcName);
    return false;
  }

  // we've found SetCaptureOptions in our local instance of the module, now calculate the offset and
  // so get the function
  // in the remote module (which might be loaded at a different base address
  uintptr_t func_remote = func_local + reconduck_remote - (uintptr_t)reconduck_local;

  if(startupThreadId)
  {
    bool success = CallOnSuspendedThread(hProcess, startupThreadId, (void *)func_remote, data,
                                         dataLen, NULL);
    if(!success)
      RDCERR("Couldn't call %s on the startup thread: %u", funcName, GetLastError());
    return success;
  }

  void *remoteMem = VirtualAllocEx(hProcess, NULL, dataLen, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  if(!remoteMem)
    return false;
  SIZE_T numWritten = 0;
  if(!WriteProcessMemory(hProcess, remoteMem, data, dataLen, &numWritten) || numWritten != dataLen)
  {
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    return false;
  }

  HANDLE hThread =
      CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)func_remote, remoteMem, 0, NULL);
  bool success = hThread && WaitForSingleObject(hThread, INFINITE) == WAIT_OBJECT_0 &&
                 ReadProcessMemory(hProcess, remoteMem, data, dataLen, &numWritten) &&
                 numWritten == dataLen;

  if(hThread)
    CloseHandle(hThread);
  VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
  return success;
}

static PROCESS_INFORMATION RunProcess(const rdcstr &app, const rdcstr &workingDir,
                                      const rdcstr &cmdLine,
                                      const rdcarray<EnvironmentModification> &env, bool internal,
                                      HANDLE *phChildStdOutput_Rd, HANDLE *phChildStdError_Rd)
{
  PROCESS_INFORMATION pi;
  STARTUPINFO si;
  SECURITY_ATTRIBUTES pSec;
  SECURITY_ATTRIBUTES tSec;

  RDCEraseEl(pi);
  RDCEraseEl(si);
  RDCEraseEl(pSec);
  RDCEraseEl(tSec);

  si.cb = sizeof(si);

  pSec.nLength = sizeof(pSec);
  tSec.nLength = sizeof(tSec);

  rdcwstr workdir = L"";

  if(!workingDir.empty())
    workdir = StringFormat::UTF82Wide(workingDir);
  else
    workdir = StringFormat::UTF82Wide(get_dirname(app));

  wchar_t *paramsAlloc = NULL;

  rdcwstr wapp = StringFormat::UTF82Wide(app);

  // CreateProcessW can modify the params, need space.
  size_t len = wapp.length() + 10;

  rdcwstr wcmd = L"";

  if(!cmdLine.empty())
  {
    wcmd = StringFormat::UTF82Wide(cmdLine);
    len += wcmd.length();
  }

  paramsAlloc = new wchar_t[len];

  RDCEraseMem(paramsAlloc, len * sizeof(wchar_t));

  wcscpy_s(paramsAlloc, len, L"\"");
  wcscat_s(paramsAlloc, len, wapp.c_str());
  wcscat_s(paramsAlloc, len, L"\"");

  if(!cmdLine.empty())
  {
    wcscat_s(paramsAlloc, len, L" ");
    wcscat_s(paramsAlloc, len, wcmd.c_str());
  }

  bool inheritHandles = false;

  HANDLE hChildStdOutput_Wr = 0, hChildStdError_Wr = 0;
  if(phChildStdOutput_Rd)
  {
    RDCASSERT(phChildStdError_Rd);

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if(!CreatePipe(phChildStdOutput_Rd, &hChildStdOutput_Wr, &sa, 0))
      RDCERR("Could not create pipe to read stdout");
    if(!SetHandleInformation(*phChildStdOutput_Rd, HANDLE_FLAG_INHERIT, 0))
      RDCERR("Could not set pipe handle information");

    if(!CreatePipe(phChildStdError_Rd, &hChildStdError_Wr, &sa, 0))
      RDCERR("Could not create pipe to read stdout");
    if(!SetHandleInformation(*phChildStdError_Rd, HANDLE_FLAG_INHERIT, 0))
      RDCERR("Could not set pipe handle information");

    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = hChildStdOutput_Wr;
    si.hStdError = hChildStdError_Wr;

    // Need to inherit handles in CreateProcess for ReadFile to read stdout
    inheritHandles = true;
  }

  // if it's a utility launch, hide the command prompt window from showing
  if(phChildStdOutput_Rd || internal)
    si.dwFlags |= STARTF_USESHOWWINDOW;

  if(!internal)
    RDCLOG("Running process %s", app.c_str());

  // turn environment string to a UTF-8 map
  std::wstring envString;

  if(!env.empty())
  {
    LPWCH envStrings = GetEnvironmentStringsW();
    EnvMap envValues = EnvStringToEnvMap(envStrings);
    FreeEnvironmentStringsW(envStrings);

    ApplyEnvModifications(envValues, env, false);

    for(auto it = envValues.begin(); it != envValues.end(); ++it)
    {
      envString += StringFormat::UTF82Wide(it->first).c_str();
      envString += L"=";
      envString += StringFormat::UTF82Wide(it->second).c_str();
      envString.push_back(0);
    }
  }

  BOOL retValue = CreateProcessW(
      NULL, paramsAlloc, &pSec, &tSec, inheritHandles, CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
      envString.empty() ? NULL : (void *)envString.data(), workdir.c_str(), &si, &pi);

  DWORD err = GetLastError();

  if(phChildStdOutput_Rd)
  {
    CloseHandle(hChildStdOutput_Wr);
    CloseHandle(hChildStdError_Wr);
  }

  SAFE_DELETE_ARRAY(paramsAlloc);

  if(!retValue)
  {
    if(!internal)
      RDCWARN("Process %s could not be loaded (error %d).", app.c_str(), err);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    RDCEraseEl(pi);
  }

  return pi;
}

static rdcpair<RDResult, uint32_t> InjectIntoProcessInternal(
    uint32_t pid, const rdcarray<EnvironmentModification> &env, const rdcstr &capturefile,
    const CaptureOptions &opts, bool waitForExit, DWORD startupThreadId)
{
  rdcwstr wcapturefile = StringFormat::UTF82Wide(capturefile);

  HANDLE hProcess =
      OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                      PROCESS_VM_WRITE | PROCESS_VM_READ | SYNCHRONIZE,
                  FALSE, pid);

  if(opts.delayForDebugger > 0)
  {
    RDCDEBUG("Waiting for debugger attach to %lu", pid);
    uint32_t timeout = 0;

    BOOL debuggerAttached = FALSE;

    while(!debuggerAttached)
    {
      CheckRemoteDebuggerPresent(hProcess, &debuggerAttached);

      Sleep(10);
      timeout += 10;

      if(timeout > opts.delayForDebugger * 1000)
        break;
    }

    if(debuggerAttached)
      RDCDEBUG("Debugger attach detected after %.2f s", float(timeout) / 1000.0f);
    else
      RDCDEBUG("Timed out waiting for debugger, gave up after %u s", opts.delayForDebugger);
  }

  RDCLOG("Injecting reconduck into process %lu", pid);

  wchar_t reconduckPath[MAX_PATH] = {0};
  GetModuleFileNameW(GetModuleHandleA(STRINGIZE(RDOC_BASE_NAME) ".dll"), &reconduckPath[0],
                                      MAX_PATH - 1);

  wchar_t reconduckPathLower[MAX_PATH] = {0};
  memcpy(reconduckPathLower, reconduckPath, MAX_PATH * sizeof(wchar_t));
  for(size_t i = 0; i < MAX_PATH && reconduckPathLower[i]; i++)
  {
    // lowercase
    if(reconduckPathLower[i] >= 'A' && reconduckPathLower[i] <= 'Z')
      reconduckPathLower[i] = 'a' + char(reconduckPathLower[i] - 'A');

    // normalise paths
    if(reconduckPathLower[i] == '/')
      reconduckPathLower[i] = '\\';
  }

  BOOL isWow64 = FALSE;
  BOOL success = IsWow64Process(hProcess, &isWow64);

  if(!success)
  {
    DWORD err = GetLastError();
    RDResult result;
    SET_ERROR_RESULT(result, ResultCode::IncompatibleProcess,
                     "Couldn't determine bitness of process, err: %08x", err);
    CloseHandle(hProcess);
    return {result, 0};
  }

  bool capalt = false;

#if DISABLED(RDOC_X64)
  BOOL selfWow64 = FALSE;

  HANDLE hSelfProcess = GetCurrentProcess();

  // check to see if we're a WoW64 process
  success = IsWow64Process(hSelfProcess, &selfWow64);

  CloseHandle(hSelfProcess);

  if(!success)
  {
    DWORD err = GetLastError();
    RDResult result;
    SET_ERROR_RESULT(result, ResultCode::IncompatibleProcess,
                     "Couldn't determine bitness of self, err: %08x", err);
    CloseHandle(hProcess);
    return {result, 0};
  }

  // we know we're 32-bit, so if the target process is not wow64
  // and we are, it's 64-bit. If we're both not wow64 then we're
  // running on 32-bit windows, and if we're both wow64 then we're
  // both 32-bit on 64-bit windows.
  //
  // We don't support capturing 64-bit programs from a 32-bit install
  // because it's pointless - a 64-bit install will work for all in
  // that case. But we do want to handle the case of:
  // 64-bit reconduck -> 32-bit program (via 32-bit reconduckcmd)
  //    -> 64-bit program (going back to 64-bit reconduckcmd).
  // so we try to see if we're an x86 invoked reconduckcmd in an
  // otherwise 64-bit install, and 'promote' back to 64-bit.
  if(selfWow64 && !isWow64)
  {
    wchar_t *slash = wcsrchr(reconduckPath, L'\\');

    if(slash && slash > reconduckPath + 4)
    {
      slash -= 4;

      if(slash && !wcsncmp(slash, L"\\x86", 4))
      {
        RDCDEBUG("Promoting back to 64-bit");
        capalt = true;
      }
    }

    // if it looks like we're in the development environment, look for the alternate bitness in the
    // corresponding folder
    if(!capalt)
    {
      const wchar_t *devLocation = wcsstr(reconduckPathLower, L"\\win32\\development\\");
      if(!devLocation)
        devLocation = wcsstr(reconduckPathLower, L"\\win32\\release\\");

      if(devLocation)
      {
        RDCDEBUG("Promoting back to 64-bit");
        capalt = true;
      }
    }

    // if we couldn't promote, then bail out.
    if(!capalt)
    {
      RDCDEBUG("Running from %ls", reconduckPathLower);

      CloseHandle(hProcess);
      RDResult result;
      SET_ERROR_RESULT(result, ResultCode::IncompatibleProcess,
                       "Can't capture 64-bit program with 32-bit build of ReconDuck. Please run a "
                       "64-bit build of ReconDuck");
      return {result, 0};
    }
  }
#else
  // farm off to alternate bitness reconduckcmd.exe

  // if the target process is 'wow64' that means it's 32-bit.
  capalt = (isWow64 == TRUE);
#endif

  if(capalt)
  {
#if ENABLED(RDOC_X64)
    // if it looks like we're in the development environment, look for the alternate bitness in the
    // corresponding folder
    const wchar_t *devLocation = wcsstr(reconduckPathLower, L"\\x64\\development\\");
    if(devLocation)
    {
      size_t idx = devLocation - reconduckPathLower;

      reconduckPath[idx] = 0;

      wcscat_s(reconduckPath, L"\\Win32\\Development\\reconduckcmd.exe");
    }

    if(!devLocation)
    {
      devLocation = wcsstr(reconduckPathLower, L"\\x64\\release\\");

      if(devLocation)
      {
        size_t idx = devLocation - reconduckPathLower;

        reconduckPath[idx] = 0;

        wcscat_s(reconduckPath, L"\\Win32\\Release\\reconduckcmd.exe");
      }
    }

    if(!devLocation)
    {
      // look in a subfolder for x86.

      // remove the filename from the path
      wchar_t *slash = wcsrchr(reconduckPath, L'\\');

      if(slash)
        *slash = 0;

      // append path
      wcscat_s(reconduckPath, L"\\x86\\reconduckcmd.exe");
    }
#else
    // if it looks like we're in the development environment, look for the alternate bitness in the
    // corresponding folder
    const wchar_t *devLocation = wcsstr(reconduckPathLower, L"\\win32\\development\\");
    if(devLocation)
    {
      size_t idx = devLocation - reconduckPathLower;

      reconduckPath[idx] = 0;

      wcscat_s(reconduckPath, L"\\x64\\Development\\reconduckcmd.exe");
    }

    if(!devLocation)
    {
      devLocation = wcsstr(reconduckPathLower, L"\\win32\\release\\");

      if(devLocation)
      {
        size_t idx = devLocation - reconduckPathLower;

        reconduckPath[idx] = 0;

        wcscat_s(reconduckPath, L"\\x64\\Release\\reconduckcmd.exe");
      }
    }

    if(!devLocation)
    {
      // look upwards on 32-bit to find the parent reconduckcmd.
      wchar_t *slash = wcsrchr(reconduckPath, L'\\');

      // remove the filename
      if(slash)
        *slash = 0;

      // remove the \\x86
      slash = wcsrchr(reconduckPath, L'\\');

      if(slash)
        *slash = 0;

      // append path
      wcscat_s(reconduckPath, L"\\reconduckcmd.exe");
    }
#endif

    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    SECURITY_ATTRIBUTES pSec;
    SECURITY_ATTRIBUTES tSec;

    RDCEraseEl(pi);
    RDCEraseEl(si);
    RDCEraseEl(pSec);
    RDCEraseEl(tSec);

    // hide the console window
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    pSec.nLength = sizeof(pSec);
    tSec.nLength = sizeof(tSec);

    // serialise to string with two chars per byte
    rdcstr optstr = opts.EncodeAsString();

    wchar_t *paramsAlloc = new wchar_t[2048];

    rdcstr debugLogfile = RDCGETLOGFILE();
    rdcwstr wdebugLogfile = StringFormat::UTF82Wide(debugLogfile);

    _snwprintf_s(
        paramsAlloc, 2047, 2047,
        L"\"%ls\" capaltbit --pid=%u --thread=%u --capfile=\"%ls\" --debuglog=\"%ls\" --capopts=\"%hs\"",
        reconduckPath, pid, startupThreadId, wcapturefile.c_str(), wdebugLogfile.c_str(), optstr.c_str());

    RDCDEBUG("params %ls", paramsAlloc);

    paramsAlloc[2047] = 0;

    wchar_t *commandLine = paramsAlloc;

    std::wstring cmdWithEnv;

    if(!env.empty())
    {
      cmdWithEnv = paramsAlloc;

      for(const EnvironmentModification &e : env)
      {
        rdcstr name = e.name.trimmed();
        rdcstr value = e.value;

        if(name == "")
          break;

        cmdWithEnv += L" +env-";
        switch(e.mod)
        {
          case EnvMod::Set: cmdWithEnv += L"replace"; break;
          case EnvMod::Append: cmdWithEnv += L"append"; break;
          case EnvMod::Prepend: cmdWithEnv += L"prepend"; break;
        }

        if(e.mod != EnvMod::Set)
        {
          switch(e.sep)
          {
            case EnvSep::Platform: cmdWithEnv += L"-platform"; break;
            case EnvSep::SemiColon: cmdWithEnv += L"-semicolon"; break;
            case EnvSep::Colon: cmdWithEnv += L"-colon"; break;
            case EnvSep::NoSep: break;
          }
        }

        cmdWithEnv += L" ";

        // escape the parameters
        for(size_t it = 0; it < name.size(); it++)
        {
          if(name[it] == '"')
          {
            name.insert(it, '\\');
            it++;
          }
        }

        for(size_t it = 0; it < value.size(); it++)
        {
          if(value[it] == '"')
          {
            value.insert(it, '\\');
            it++;
          }
        }

        if(name.back() == '\\')
          name += "\\";

        if(value.back() == '\\')
          value += "\\";

        cmdWithEnv += L"\"" + std::wstring(StringFormat::UTF82Wide(name).c_str()) + L"\" ";
        cmdWithEnv += L"\"" + std::wstring(StringFormat::UTF82Wide(value).c_str()) + L"\" ";
      }

      commandLine = (wchar_t *)cmdWithEnv.c_str();
    }

    BOOL retValue = CreateProcessW(NULL, commandLine, &pSec, &tSec, false,
                                   CREATE_NEW_CONSOLE | CREATE_SUSPENDED, NULL, NULL, &si, &pi);

    SAFE_DELETE_ARRAY(paramsAlloc);

    if(!retValue)
    {
      RDResult result;
#if RECONDUCK_OFFICIAL_BUILD
      SET_ERROR_RESULT(result, ResultCode::InternalError,
                       "Can't run 32-bit reconduckcmd to capture 32-bit program.");
#else
      SET_ERROR_RESULT(
          result, ResultCode::InternalError,
          "Can't run 32-bit reconduckcmd to capture 32-bit program."
          "If this is a locally built ReconDuck you must build both 32-bit and 64-bit versions.");
#endif
      CloseHandle(hProcess);
      return {result, 0};
    }

    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hThread, INFINITE);
    CloseHandle(pi.hThread);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);

    if(waitForExit)
      WaitForSingleObject(hProcess, INFINITE);

    CloseHandle(hProcess);

    if(exitCode == 0)
    {
      RDResult result;
      SET_ERROR_RESULT(result, ResultCode::UnknownError,
                       "Encountered error while launching target 32-bit program.");
      return {result, 0};
    }

    if(exitCode < ReconDuck_FirstTargetControlPort)
    {
      ResultCode code = (ResultCode)exitCode;

      RDResult result;
      SET_ERROR_RESULT(result, code, "32-bit reconduckcmd returned '%s'", ToStr(code).c_str());
      return {code, 0};
    }

    return {ResultCode::Succeeded, (uint32_t)exitCode};
  }

  bool loaded = InjectDLL(hProcess, reconduckPath, startupThreadId);

  const char *rdoc_dll = STRINGIZE(RDOC_BASE_NAME);

  uintptr_t loc = loaded ? FindRemoteDLL(pid, STRINGIZE(RDOC_BASE_NAME) ".dll") : 0;

  rdcpair<RDResult, uint32_t> result = {ResultCode::Succeeded, 0};

  if(loc == 0)
  {
    const char *largeAddrMsg =
#if ENABLED(RDOC_X64)
        ", and is not built with /LARGEADDRESSAWARE:NO";
#else
        "";
#endif

    SET_ERROR_RESULT(
        result.first, ResultCode::InjectionFailed,
        "Failed to inject %s.dll into process. Check that the process did not crash or exit "
        "early in initialisation, e.g. if the working directory is incorrectly set.\n"
        "Otherwise ensure the program does not have significant "
        "code that will run on global constructors/thread start and crash%s",
        rdoc_dll, largeAddrMsg);
  }
  else
  {
    // safe to cast away the const as we know these functions don't modify the parameters
    bool configured = true;

    if(!capturefile.empty())
      configured =
          InjectFunctionCall(hProcess, loc, "INTERNAL_SetCaptureFile", (void *)capturefile.c_str(),
                             capturefile.size() + 1, startupThreadId);

    rdcstr debugLogfile = RDCGETLOGFILE();

    configured = configured && InjectFunctionCall(hProcess, loc, "INTERNAL_SetDebugLogFile",
                                                  (void *)debugLogfile.c_str(),
                                                  debugLogfile.size() + 1, startupThreadId);

    configured = configured && InjectFunctionCall(hProcess, loc, "INTERNAL_SetCaptureOptions",
                                                  (CaptureOptions *)&opts, sizeof(CaptureOptions),
                                                  startupThreadId);

    configured =
        configured && InjectFunctionCall(hProcess, loc, "INTERNAL_GetTargetControlIdent",
                                         &result.second, sizeof(result.second), startupThreadId);

    if(configured && !env.empty())
    {
      for(const EnvironmentModification &e : env)
      {
        rdcstr name = e.name.trimmed();
        rdcstr value = e.value;
        EnvMod mod = e.mod;
        EnvSep sep = e.sep;

        if(name == "")
          break;

        configured =
            InjectFunctionCall(hProcess, loc, "INTERNAL_EnvModName", (void *)name.c_str(),
                               name.size() + 1, startupThreadId) &&
            InjectFunctionCall(hProcess, loc, "INTERNAL_EnvModValue", (void *)value.c_str(),
                               value.size() + 1, startupThreadId) &&
            InjectFunctionCall(hProcess, loc, "INTERNAL_EnvSep", &sep, sizeof(sep), startupThreadId) &&
            InjectFunctionCall(hProcess, loc, "INTERNAL_EnvMod", &mod, sizeof(mod), startupThreadId);
        if(!configured)
          break;
      }

      // parameter is unused
      void *dummy = NULL;
      configured = configured && InjectFunctionCall(hProcess, loc, "INTERNAL_ApplyEnvMods", &dummy,
                                                    sizeof(dummy), startupThreadId);
    }
    if(!configured)
    {
      SET_ERROR_RESULT(result.first, ResultCode::InjectionFailed,
                       "Failed to initialise capture in the target process.");
      result.second = 0;
    }
  }

  if(waitForExit)
    WaitForSingleObject(hProcess, INFINITE);

  CloseHandle(hProcess);

  return result;
}

rdcpair<RDResult, uint32_t> Process::InjectIntoProcess(uint32_t pid,
                                                       const rdcarray<EnvironmentModification> &env,
                                                       const rdcstr &capturefile,
                                                       const CaptureOptions &opts, bool waitForExit)
{
  return InjectIntoProcessInternal(pid, env, capturefile, opts, waitForExit, 0);
}

// Used by the matching-bitness command helper while the launch owner holds the primary thread.
extern "C" __declspec(dllexport) uint32_t __cdecl INTERNAL_InjectIntoProcessThreadContext(
    uint32_t pid, const rdcarray<EnvironmentModification> &env, const rdcstr &capturefile,
    const CaptureOptions &opts, uint32_t startupThreadId)
{
  if(!startupThreadId)
    return (uint32_t)ResultCode::InvalidParameter;
  rdcpair<RDResult, uint32_t> result =
      InjectIntoProcessInternal(pid, env, capturefile, opts, false, startupThreadId);
  return result.first == ResultCode::Succeeded ? result.second : (uint32_t)result.first.code;
}

uint32_t Process::LaunchProcess(const rdcstr &app, const rdcstr &workingDir, const rdcstr &cmdLine,
                                bool internal, ProcessResult *result)
{
  HANDLE hChildStdOutput_Rd = NULL, hChildStdError_Rd = NULL;

  rdcstr appPath = app;
  size_t len = appPath.length();
  rdcstr ext;
  if(len > 4)
    ext = strlower(appPath.substr(len - 4));
  if(ext != ".exe")
    appPath += ".exe";

  PROCESS_INFORMATION pi =
      RunProcess(appPath, workingDir, cmdLine, {}, internal, result ? &hChildStdOutput_Rd : NULL,
                 result ? &hChildStdError_Rd : NULL);

  if(pi.dwProcessId == 0)
  {
    if(!internal)
      RDCWARN("Couldn't launch process '%s'", appPath.c_str());

    if(hChildStdError_Rd != NULL)
      CloseHandle(hChildStdError_Rd);
    if(hChildStdOutput_Rd != NULL)
      CloseHandle(hChildStdOutput_Rd);

    return 0;
  }

  if(!internal)
    RDCLOG("Launched process '%s' with '%s'", appPath.c_str(), cmdLine.c_str());

  ResumeThread(pi.hThread);

  if(result)
  {
    result->strStdout = "";
    result->strStderror = "";

    char chBuf[4096];
    DWORD dwOutputRead, dwErrorRead;
    BOOL success = FALSE;
    rdcstr s;
    for(;;)
    {
      success = ReadFile(hChildStdOutput_Rd, chBuf, sizeof(chBuf), &dwOutputRead, NULL);
      s = rdcstr(chBuf, dwOutputRead);
      result->strStdout += s;

      if(!success && !dwOutputRead)
        break;
    }

    for(;;)
    {
      success = ReadFile(hChildStdError_Rd, chBuf, sizeof(chBuf), &dwErrorRead, NULL);
      s = rdcstr(chBuf, dwErrorRead);
      result->strStderror += s;

      if(!success && !dwErrorRead)
        break;
    }

    CloseHandle(hChildStdOutput_Rd);
    CloseHandle(hChildStdError_Rd);

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, (LPDWORD)&result->retCode);
  }

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);

  return pi.dwProcessId;
}

uint32_t Process::LaunchScript(const rdcstr &script, const rdcstr &workingDir,
                               const rdcstr &argList, bool internal, ProcessResult *result)
{
  // Change parameters to invoke command interpreter
  rdcstr args = "/C " + script + " " + argList;

  return LaunchProcess("cmd.exe", workingDir, args, internal, result);
}

rdcpair<RDResult, uint32_t> Process::LaunchAndInjectIntoProcess(
    const rdcstr &app, const rdcstr &workingDir, const rdcstr &cmdLine,
    const rdcarray<EnvironmentModification> &env, const rdcstr &capturefile,
    const CaptureOptions &opts, bool waitForExit)
{
  void *func =
      GetProcAddress(GetModuleHandleA(STRINGIZE(RDOC_BASE_NAME) ".dll"), "INTERNAL_SetCaptureFile");

  if(func == NULL)
  {
    const char *rdoc_dll = STRINGIZE(RDOC_BASE_NAME);
    RDResult result;
    SET_ERROR_RESULT(result, ResultCode::InternalError,
                     "Can't find required export function in %s.dll - corrupted/missing file?",
                     rdoc_dll);
    return {result, 0};
  }

  if(get_basename(app) == "explorer.exe" || get_basename(app) == "dllhost.exe")
  {
    RDResult result;
    SET_ERROR_RESULT(
        result, ResultCode::InjectionFailed,
        "For safety reasons ReconDuck does not support capturing executables with a "
        "reserved system filename such as '%s'. Please rename your executable to capture.",
        get_basename(app).c_str());
    return {result, 0};
  }

  PROCESS_INFORMATION pi = RunProcess(app, workingDir, cmdLine, env, false, NULL, NULL);

  if(pi.dwProcessId == 0)
  {
    RDResult result;
    SET_ERROR_RESULT(result, ResultCode::InjectionFailed, "Failed to launch process.");
    return {result, 0};
  }

  rdcpair<RDResult, uint32_t> ret =
      InjectIntoProcessInternal(pi.dwProcessId, {}, capturefile, opts, false, pi.dwThreadId);

  if(ret.second == 0 || ret.first != ResultCode::Succeeded)
  {
    if(ret.first == ResultCode::Succeeded)
      SET_ERROR_RESULT(ret.first, ResultCode::InjectionFailed,
                       "The target process did not initialise capture control.");
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return ret;
  }

  if(ResumeThread(pi.hThread) == DWORD(-1))
  {
    SET_ERROR_RESULT(ret.first, ResultCode::InjectionFailed,
                     "Failed to resume the target process: %u", GetLastError());
    ret.second = 0;
    TerminateProcess(pi.hProcess, 1);
  }
  CloseHandle(pi.hProcess);

  if(waitForExit)
    WaitForSingleObject(pi.hThread, INFINITE);

  CloseHandle(pi.hThread);

  return ret;
}

bool Process::CanGlobalHook()
{
  // all we need is admin rights and it's the caller's responsibility to ensure that.
  return true;
}

// Failed starts and stops retain their recovery state until cleanup succeeds.
static GlobalHook::Session *globalHook = NULL;
static GlobalHook::Session *pendingGlobalHook = NULL;
static SRWLOCK globalHookLock = SRWLOCK_INIT;
static volatile LONG globalHookActivity = 0;

// Start/Stop own the session pointers under this lock. The UI reads only the atomic status,
// including startup and recovery, without blocking on helper readiness or cleanup waits.
struct GlobalHookOperation
{
  GlobalHookOperation()
  {
    AcquireSRWLockExclusive(&globalHookLock);
    InterlockedExchange(&globalHookActivity, 1);
  }
  ~GlobalHookOperation()
  {
    InterlockedExchange(&globalHookActivity, globalHook || pendingGlobalHook ? 1 : 0);
    ReleaseSRWLockExclusive(&globalHookLock);
  }
  GlobalHookOperation(const GlobalHookOperation &) = delete;
  GlobalHookOperation &operator=(const GlobalHookOperation &) = delete;
};

RDResult Process::StartGlobalHook(const rdcstr &pathmatch, const rdcstr &capturefile,
                                  const CaptureOptions &opts)
{
  if(pathmatch.length() < 4 || pathmatch.contains("\"") || capturefile.contains("\""))
  {
    RETURN_ERROR_RESULT(ResultCode::InvalidParameter,
                        "Global hook needs a path match of at least four characters and no quotes");
  }
  GlobalHookOperation operation;
  if(globalHook)
  {
    RETURN_ERROR_RESULT(ResultCode::InjectionFailed, "Global hook is already active");
  }
  if(pendingGlobalHook)
  {
    if(!pendingGlobalHook->Cleanup())
    {
      RETURN_ERROR_RESULT(ResultCode::InjectionFailed, "Global hook recovery is still pending: %s",
                          StringFormat::Wide2UTF8(pendingGlobalHook->error.c_str()).c_str());
    }
    delete pendingGlobalHook;
    pendingGlobalHook = NULL;
  }

  rdcstr reconduckPath;
  FileIO::GetLibraryFilename(reconduckPath);

  reconduckPath = get_dirname(reconduckPath);

  // the native reconduckcmd.exe is always next to the dll. Wow32 will be somewhere else
  rdcstr cmdpathNative = reconduckPath + "\\reconduckcmd.exe";
  rdcstr cmdpathWow32;

  rdcstr shimpathNative = reconduckPath;
  rdcstr shimpathWow32;

#if ENABLED(RDOC_X64)

  // native shim is just reconduckshim64.dll
  shimpathNative = reconduckPath + "\\reconduckshim64.dll";

  // if it looks like we're in the development environment, look for the alternate bitness in the
  // corresponding folder
  int devLocation = reconduckPath.find("\\x64\\Development");
  if(devLocation >= 0)
  {
    reconduckPath.erase(devLocation, ~0U);

    shimpathWow32 = reconduckPath + "\\Win32\\Development\\reconduckshim32.dll";
    cmdpathWow32 = reconduckPath + "\\Win32\\Development\\reconduckcmd.exe";
  }
  else
  {
    devLocation = reconduckPath.find("\\x64\\Release");

    if(devLocation >= 0)
    {
      reconduckPath.erase(devLocation, ~0U);

      shimpathWow32 = reconduckPath + "\\Win32\\Release\\reconduckshim32.dll";
      cmdpathWow32 = reconduckPath + "\\Win32\\Release\\reconduckcmd.exe";
    }
  }

  // if we're not in the dev environment, assume it's under a x86\ subfolder
  if(devLocation < 0)
  {
    shimpathWow32 = reconduckPath + "\\x86\\reconduckshim32.dll";
    cmdpathWow32 = reconduckPath + "\\x86\\reconduckcmd.exe";
  }

#else

  // nothing fancy to do here for 32-bit, just point the shim next to our dll.
  shimpathNative = reconduckPath + "\\reconduckshim32.dll";

#endif

  std::vector<GlobalHook::Component> components;
  GlobalHook::Component native;
  native.shim = StringFormat::UTF82Wide(shimpathNative).c_str();
  native.command = StringFormat::UTF82Wide(cmdpathNative).c_str();
  native.library = StringFormat::UTF82Wide(get_dirname(cmdpathNative) + "\\reconduck.dll").c_str();
#if ENABLED(RDOC_X64)
  native.view = KEY_WOW64_64KEY;
  native.architecture = L"64-bit";
#else
  native.view = KEY_WOW64_32KEY;
  native.architecture = L"32-bit";
#endif
  components.push_back(native);
#if ENABLED(RDOC_X64)
  GlobalHook::Component wow32;
  wow32.shim = StringFormat::UTF82Wide(shimpathWow32).c_str();
  wow32.command = StringFormat::UTF82Wide(cmdpathWow32).c_str();
  wow32.library = StringFormat::UTF82Wide(get_dirname(cmdpathWow32) + "\\reconduck.dll").c_str();
  wow32.view = KEY_WOW64_32KEY;
  wow32.architecture = L"32-bit";
  components.push_back(wow32);
#endif

  rdcstr debugLogfile = RDCGETLOGFILE();
  rdcstr arguments = StringFormat::Fmt(
      "globalhook --match \"%s\" --capfile \"%s\" --debuglog \"%s\" --capopts \"%s\"",
      pathmatch.c_str(), capturefile.c_str(), debugLogfile.c_str(), opts.EncodeAsString().c_str());
  GlobalHook::Session *candidate = new GlobalHook::Session;
  if(!candidate->Start(components, StringFormat::UTF82Wide(arguments).c_str()))
  {
    rdcstr diagnostic = StringFormat::Wide2UTF8(candidate->error.c_str());
    if(candidate->Cleanup())
      delete candidate;
    else
    {
      diagnostic = StringFormat::Wide2UTF8(candidate->error.c_str());
      pendingGlobalHook = candidate;
    }
    RETURN_ERROR_RESULT(ResultCode::InjectionFailed, "%s", diagnostic.c_str());
  }
  // Start has snapshotted both views, written recovery data, and confirmed helper readiness.
  globalHook = candidate;
  return RDResult();
}

bool Process::IsGlobalHookActive()
{
  // Startup, cleanup and pending recovery may still own AppInit writes or a helper.
  return InterlockedCompareExchange(&globalHookActivity, 0, 0) != 0;
}
void Process::StopGlobalHook()
{
  GlobalHookOperation operation;
  GlobalHook::Session *sessions[] = {globalHook, pendingGlobalHook};
  globalHook = pendingGlobalHook = NULL;
  for(GlobalHook::Session *session : sessions)
  {
    if(!session)
      continue;
    if(session->Cleanup())
      delete session;
    else
    {
      // Keep the snapshots, registry keys and lease so Stop/Start can retry recovery.
      pendingGlobalHook = session;
      RDCERR("Global hook cleanup incomplete: %s",
             StringFormat::Wide2UTF8(session->error.c_str()).c_str());
    }
  }
}

bool Process::IsModuleLoaded(const rdcstr &module)
{
  return GetModuleHandleA(module.c_str()) != NULL;
}

void *Process::LoadModule(const rdcstr &module)
{
  HMODULE mod = GetModuleHandleA(module.c_str());
  if(mod != NULL)
    return mod;

  return LoadLibraryA(module.c_str());
}

void *Process::GetFunctionAddress(void *module, const rdcstr &function)
{
  if(module == NULL)
    return NULL;

  return (void *)GetProcAddress((HMODULE)module, function.c_str());
}

uint32_t Process::GetCurrentPID()
{
  return (uint32_t)GetCurrentProcessId();
}

void Process::Shutdown()
{
  // nothing to do
}
