/******************************************************************************
 * The MIT License (MIT)
 * Copyright (c) 2026 ReconDuck contributors
 * See LICENSE.md for permission, warranty and redistribution terms.
 ******************************************************************************/

#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include "detours.h"

namespace ExportDetour
{
// The caller serialises registry operations. Target execution is coordinated
// by enrolling the process threads in each Detours transaction.
class Registry
{
  struct Binding
  {
    void **original;
    Binding *next;
  };
  struct Entry
  {
    void *target;
    void *hook;
    HMODULE module;
    Binding *bindings;
    Entry *next;
  };
  struct ThreadHandle
  {
    HANDLE handle;
    ThreadHandle *next;
  };
  Entry *entries = NULL;

  static bool StartsWith(const char *name, const char *prefix)
  {
    while(*prefix)
    {
      char c = *name++;
      if(c >= 'A' && c <= 'Z')
        c += 'a' - 'A';
      if(c != *prefix++)
        return false;
    }
    return true;
  }

  static bool Supported(const char *library)
  {
    return library && lstrcmpiA(library, "kernel32.dll") &&
           lstrcmpiA(library, "kernelbase.dll") && lstrcmpiA(library, "nvapi.dll") &&
           lstrcmpiA(library, "nvapi64.dll") &&
           !StartsWith(library, "api-ms-win-core-libraryloader");
  }

  static void CloseThreads(ThreadHandle *threads)
  {
    while(threads)
    {
      ThreadHandle *next = threads->next;
      CloseHandle(threads->handle);
      HeapFree(GetProcessHeap(), 0, threads);
      threads = next;
    }
  }

  static DWORD OpenThreads(ThreadHandle *&threads)
  {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if(snapshot == INVALID_HANDLE_VALUE)
      return GetLastError();
    THREADENTRY32 thread = {};
    thread.dwSize = sizeof(thread);
    DWORD result = NO_ERROR;
    if(!Thread32First(snapshot, &thread))
      result = GetLastError();
    else
    {
      do
      {
        if(thread.th32OwnerProcessID != GetCurrentProcessId() ||
           thread.th32ThreadID == GetCurrentThreadId())
          continue;
        HANDLE handle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT |
                                       THREAD_QUERY_INFORMATION,
                                   FALSE, thread.th32ThreadID);
        if(!handle)
        {
          // A thread may have exited between the snapshot and OpenThread.
          if(GetLastError() == ERROR_INVALID_PARAMETER)
            continue;
          result = GetLastError();
          break;
        }
        ThreadHandle *node = (ThreadHandle *)HeapAlloc(GetProcessHeap(), 0, sizeof(ThreadHandle));
        if(!node)
        {
          CloseHandle(handle);
          result = ERROR_NOT_ENOUGH_MEMORY;
          break;
        }
        node->handle = handle;
        node->next = threads;
        threads = node;
      } while(Thread32Next(snapshot, &thread));
    }
    CloseHandle(snapshot);
    return result;
  }

  static DWORD Change(void **original, void *hook, bool attach, Binding *aliases = NULL,
                      void *target = NULL)
  {
    ThreadHandle *threads = NULL;
    DWORD result = OpenThreads(threads);
    if(result != NO_ERROR)
    {
      CloseThreads(threads);
      return result;
    }
    result = DetourTransactionBegin();
    if(result == NO_ERROR)
    {
      result = attach ? DetourAttach((PVOID *)original, hook) : DetourDetach((PVOID *)original, hook);
      if(result == NO_ERROR)
        result = DetourUpdateThread(GetCurrentThread());
      for(ThreadHandle *thread = threads; result == NO_ERROR && thread; thread = thread->next)
      {
        DWORD exitCode;
        if(GetExitCodeThread(thread->handle, &exitCode) && exitCode != STILL_ACTIVE)
          continue;
        result = DetourUpdateThread(thread->handle);
      }
      if(result == NO_ERROR)
      {
        // All enrolled threads are still suspended. Restore aliases before
        // Detours resumes them and releases the removed trampoline.
        void *trampoline = *original;
        for(Binding *binding = aliases; binding; binding = binding->next)
          *binding->original = target;
        result = DetourTransactionCommit();
        if(result != NO_ERROR)
          for(Binding *binding = aliases; binding; binding = binding->next)
            *binding->original = trampoline;
      }
      else
        DetourTransactionAbort();
    }
    else if(result != ERROR_INVALID_OPERATION)
    {
      // Begin can fail after claiming transaction ownership (e.g. protection).
      // An existing owner's ERROR_INVALID_OPERATION must never be aborted here.
      DetourTransactionAbort();
    }
    CloseThreads(threads);
    return result;
  }

public:
  Registry() = default;
  Registry(const Registry &) = delete;
  Registry &operator=(const Registry &) = delete;

  bool IsAttached(void **original) const
  {
    for(Entry *entry = entries; entry; entry = entry->next)
      for(Binding *binding = entry->bindings; binding; binding = binding->next)
        if(binding->original == original)
          return true;
    return false;
  }

  DWORD Attach(const char *library, HMODULE module, const char *function, void **original, void *hook)
  {
    if(!Supported(library))
      return ERROR_NOT_SUPPORTED;
    if(!module || !function || !original || !hook)
      return ERROR_INVALID_PARAMETER;
    void *target = (void *)GetProcAddress(module, function);
    if(!target || target == hook)
      return ERROR_PROC_NOT_FOUND;
    for(Entry *entry = entries; entry; entry = entry->next)
    {
      for(Binding *binding = entry->bindings; binding; binding = binding->next)
        if(binding->original == original)
          return entry->target == target && entry->hook == hook ? NO_ERROR : ERROR_ALREADY_EXISTS;
      if(entry->target == target)
      {
        if(*original && *original != target && *original != *entry->bindings->original)
          return ERROR_ALREADY_EXISTS;
        Binding *binding = (Binding *)HeapAlloc(GetProcessHeap(), 0, sizeof(Binding));
        if(!binding)
          return ERROR_NOT_ENOUGH_MEMORY;
        binding->original = original;
        binding->next = entry->bindings->next;
        entry->bindings->next = binding;
        *original = *entry->bindings->original;
        // A different IAT callback can still forward to the native trampoline.
        // Track its pointer without replacing the already installed export hook.
        return entry->hook == hook ? NO_ERROR : ERROR_ALREADY_EXISTS;
      }
    }
    if(*original && *original != target)
      return ERROR_ALREADY_EXISTS;
    Entry *entry = (Entry *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Entry));
    Binding *binding = (Binding *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Binding));
    if(!entry || !binding)
    {
      if(entry)
        HeapFree(GetProcessHeap(), 0, entry);
      if(binding)
        HeapFree(GetProcessHeap(), 0, binding);
      return ERROR_NOT_ENOUGH_MEMORY;
    }
    DWORD result = NO_ERROR;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)target, &entry->module))
      result = GetLastError();
    if(result == NO_ERROR)
    {
      *original = target;
      result = Change(original, hook, true);
    }
    if(result != NO_ERROR)
    {
      if(entry->module)
        FreeLibrary(entry->module);
      HeapFree(GetProcessHeap(), 0, binding);
      HeapFree(GetProcessHeap(), 0, entry);
      return result;
    }
    entry->target = target;
    entry->hook = hook;
    entry->bindings = binding;
    binding->original = original;
    entry->next = entries;
    entries = entry;
    return NO_ERROR;
  }

  DWORD RemoveAll()
  {
    DWORD result = NO_ERROR;
    Entry **position = &entries;
    while(*position)
    {
      Entry *entry = *position;
      DWORD error = Change(entry->bindings->original, entry->hook, false,
                           entry->bindings->next, entry->target);
      if(error != NO_ERROR)
      {
        result = error;
        position = &entry->next;
        continue;
      }
      *position = entry->next;
      Binding *binding = entry->bindings;
      while(binding)
      {
        Binding *next = binding->next;
        *binding->original = entry->target;
        HeapFree(GetProcessHeap(), 0, binding);
        binding = next;
      }
      FreeLibrary(entry->module);
      HeapFree(GetProcessHeap(), 0, entry);
    }
    return result;
  }
};
}
