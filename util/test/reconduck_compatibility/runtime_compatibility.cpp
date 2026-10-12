// Copyright (c) 2026 ReconDuck contributors
// Licensed under the MIT License, see LICENSE.md.
#include <windows.h>
#include <stdio.h>
#ifndef RUNTIME_LOAD_PROBE
#include <mutex>
#endif

static LONG WINAPI ReportCrash(EXCEPTION_POINTERS *exception)
{
  HMODULE runtime = GetModuleHandleW(L"msvcp140.dll");
  fprintf(stderr, "CRASH code=0x%08lx msvcp140_offset=0x%llx\n",
          exception->ExceptionRecord->ExceptionCode,
          (unsigned long long)exception->ExceptionRecord->ExceptionAddress -
              (unsigned long long)runtime);
  fflush(stderr);
  return EXCEPTION_EXECUTE_HANDLER;
}

int wmain(int argc, wchar_t **argv)
{
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  SetUnhandledExceptionFilter(ReportCrash);
#ifdef RUNTIME_LOAD_PROBE
  if(argc != 2 || !LoadLibraryW(L"msvcp140.dll"))
    return 2;
  if(!LoadLibraryW(argv[1]))
  {
    fprintf(stderr, "LOAD FAILED error=%lu\n", GetLastError());
    return 3;
  }
  puts("PASS ReconDuck DLL loaded alongside older MSVCP140");
#else
  (void)argc;
  (void)argv;
  std::mutex mutex;
  mutex.lock();
  mutex.unlock();
  puts("PASS mutex construction/lock with older MSVCP140");
#endif
  return 0;
}
