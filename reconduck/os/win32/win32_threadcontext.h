/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
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

#pragma once

#include <string.h>
#include <windows.h>

// Only for the primary thread of a process created with CREATE_SUSPENDED. The caller owns
// that process and must terminate it on failure: a timed-out call may still be executing.
static bool CallOnSuspendedThread(HANDLE process, DWORD threadId, void *function, void *data,
                                  SIZE_T dataLen, ULONG_PTR *callResult)
{
  struct CallState
  {
    ULONG_PTR result;
    DWORD done;
  };

  if(!function || !data || !dataLen || dataLen > SIZE_T(-1) - sizeof(CallState))
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return false;
  }

  HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME |
                                 THREAD_QUERY_INFORMATION | SYNCHRONIZE,
                             FALSE, threadId);
  if(!thread)
    return false;

  bool success = false, contextChanged = false, ran = false, safeToFree = true;
  DWORD error = ERROR_SUCCESS, suspensionCount = 0;
  void *contextBuffer = NULL, *remoteData = NULL, *remoteCode = NULL;
  PCONTEXT saved = NULL;

  do
  {
    if(GetProcessIdOfThread(thread) != GetProcessId(process))
    {
      SetLastError(ERROR_INVALID_PARAMETER);
      break;
    }

    DWORD previous = SuspendThread(thread);
    if(previous == DWORD(-1))
      break;
    if(previous != 1)
    {
      ResumeThread(thread);
      SetLastError(ERROR_INVALID_STATE);
      break;
    }
    suspensionCount = 2;

    DWORD flags = CONTEXT_ALL, contextSize = 0;
    DWORD64 features = GetEnabledXStateFeatures();
    if(features)
      flags |= CONTEXT_XSTATE;
    InitializeContext(NULL, flags, &saved, &contextSize);
    if(GetLastError() != ERROR_INSUFFICIENT_BUFFER)
      break;
    contextBuffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, contextSize);
    if(!contextBuffer)
    {
      SetLastError(ERROR_NOT_ENOUGH_MEMORY);
      break;
    }
    if(!InitializeContext(contextBuffer, flags, &saved, &contextSize) ||
       (features && !SetXStateFeaturesMask(saved, features)) || !GetThreadContext(thread, saved))
      break;
    if(features)
    {
      DWORD64 present = 0;
      if(!GetXStateFeaturesMask(saved, &present))
        break;
      // GetThreadContext may omit components in INIT state. Restore those explicitly too;
      // otherwise a callback could leave its extended registers in the original thread.
      bool valid = true;
      for(DWORD feature = XSTATE_AVX; feature < 64; feature++)
      {
        DWORD64 bit = DWORD64(1) << feature;
        if((features & bit) && !(present & bit))
        {
          DWORD length = 0;
          void *state = LocateXStateFeature(saved, feature, &length);
          if(!state)
          {
            valid = false;
            SetLastError(ERROR_NOT_SUPPORTED);
            break;
          }
          memset(state, 0, length);
        }
      }
      if(!valid || !SetXStateFeaturesMask(saved, features))
        break;
    }

    remoteData = VirtualAllocEx(process, NULL, sizeof(CallState) + dataLen,
                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if(!remoteData)
      break;
    CallState state = {};
    SIZE_T transferred = 0;
    void *argument = (BYTE *)remoteData + sizeof(CallState);
    if(!WriteProcessMemory(process, remoteData, &state, sizeof(state), &transferred) ||
       transferred != sizeof(state) ||
       !WriteProcessMemory(process, argument, data, dataLen, &transferred) || transferred != dataLen)
      break;

#if defined(_WIN64)
    BYTE code[] = {
        0x48, 0x83, 0xec, 0x28,    // sub rsp, 40 (shadow space + alignment)
        0x48, 0xb9, 0,    0,    0,    0,    0,    0, 0, 0,    // mov rcx, argument
        0x48, 0xb8, 0,    0,    0,    0,    0,    0, 0, 0,    // mov rax, function
        0xff, 0xd0,                                           // call rax
        0x48, 0xba, 0,    0,    0,    0,    0,    0, 0, 0,    // mov rdx, state
        0x48, 0x89, 0x02,                                     // mov [rdx], rax
        0xc7, 0x42, 0x08, 0x01, 0x00, 0x00, 0x00,             // mov [rdx + 8], 1
        0xeb, 0xfe,                                           // wait for context restoration
    };
    memcpy(code + 6, &argument, sizeof(argument));
    memcpy(code + 16, &function, sizeof(function));
    memcpy(code + 28, &remoteData, sizeof(remoteData));
#else
    BYTE code[] = {
        0x68, 0,    0,    0,    0,                   // push argument
        0xb8, 0,    0,    0,    0,                   // mov eax, function
        0xff, 0xd0,                                  // call eax (cdecl or stdcall)
        0xba, 0,    0,    0,    0,                   // mov edx, state
        0x89, 0x02,                                  // mov [edx], eax
        0xc7, 0x42, 0x04, 0x01, 0x00, 0x00, 0x00,    // mov [edx + 4], 1
        0xeb, 0xfe,                                  // wait for context restoration
    };
    memcpy(code + 1, &argument, sizeof(argument));
    memcpy(code + 6, &function, sizeof(function));
    memcpy(code + 13, &remoteData, sizeof(remoteData));
#endif
    remoteCode =
        VirtualAllocEx(process, NULL, sizeof(code), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if(!remoteCode)
      break;
    DWORD oldProtection;
    if(!WriteProcessMemory(process, remoteCode, code, sizeof(code), &transferred) ||
       transferred != sizeof(code) ||
       !VirtualProtectEx(process, remoteCode, sizeof(code), PAGE_EXECUTE_READ, &oldProtection) ||
       !FlushInstructionCache(process, remoteCode, sizeof(code)))
      break;

    CONTEXT redirect = *saved;
    redirect.ContextFlags = CONTEXT_CONTROL;
#if defined(_WIN64)
    redirect.Rip = (DWORD64)remoteCode;
    redirect.Rsp = (saved->Rsp & ~DWORD64(15)) - 8;
#else
    redirect.Eip = (DWORD)remoteCode;
#endif
    if(!SetThreadContext(thread, &redirect))
      break;
    contextChanged = true;
    while(suspensionCount)
    {
      if(ResumeThread(thread) == DWORD(-1))
        break;
      suspensionCount--;
    }
    if(suspensionCount)
      break;
    ran = true;
    safeToFree = false;

    ULONGLONG start = GetTickCount64();
    while(!state.done)
    {
      if(!ReadProcessMemory(process, remoteData, &state, sizeof(state), &transferred) ||
         transferred != sizeof(state))
        break;
      if(state.done)
        break;
      if(WaitForSingleObject(thread, 0) != WAIT_TIMEOUT)
      {
        SetLastError(ERROR_PROCESS_ABORTED);
        break;
      }
      if(GetTickCount64() - start >= 30000)
      {
        SetLastError(ERROR_TIMEOUT);
        break;
      }
      Sleep(1);
    }
    if(!state.done)
      break;
    if(SuspendThread(thread) == DWORD(-1))
      break;
    suspensionCount = 1;
    if(!SetThreadContext(thread, saved))
      break;
    contextChanged = false;
    safeToFree = true;
    // Read the return value again after suspension so result and done cannot be sampled
    // across the trampoline's two separate stores.
    if(!ReadProcessMemory(process, remoteData, &state, sizeof(state), &transferred) ||
       transferred != sizeof(state) ||
       !ReadProcessMemory(process, argument, data, dataLen, &transferred) || transferred != dataLen)
      break;
    if(callResult)
      *callResult = state.result;
    success = true;
  } while(false);

  error = success ? ERROR_SUCCESS : GetLastError();
  if(!ran && contextChanged)
    safeToFree = SetThreadContext(thread, saved) != FALSE;
  // Undo our extra suspension if execution never started. Successful calls leave the
  // original single suspension in place for the launch owner to resume.
  if(!ran && suspensionCount == 2)
    ResumeThread(thread);
  if(safeToFree)
  {
    if(remoteCode)
      VirtualFreeEx(process, remoteCode, 0, MEM_RELEASE);
    if(remoteData)
      VirtualFreeEx(process, remoteData, 0, MEM_RELEASE);
  }
  if(contextBuffer)
    HeapFree(GetProcessHeap(), 0, contextBuffer);
  CloseHandle(thread);
  SetLastError(error);
  return success;
}
