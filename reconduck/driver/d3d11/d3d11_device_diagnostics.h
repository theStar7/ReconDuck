#pragma once

#include "common/common.h"
#include "driver/dx/official/d3d11.h"
#include "os/os_specific.h"

// Share a bounded budget across creation and presentation, including concurrent callers.
inline bool ReserveD3D11FailureDiagnostic()
{
  static int32_t count = 0;
  int32_t current = Atomic::CmpExch32(&count, 0, 0);
  while(current < 8)
  {
    int32_t observed = Atomic::CmpExch32(&count, current, current + 1);
    if(observed == current)
      return true;
    current = observed;
  }
  return false;
}

// Read-only diagnostics: preserve the API result and queued validation messages.
inline void LogD3D11Failure(ID3D11Device *device, const char *operation, HRESULT result)
{
  if(SUCCEEDED(result) || !ReserveD3D11FailureDiagnostic())
    return;

  if(device)
  {
    const HRESULT reason = device->GetDeviceRemovedReason();
    RDCERR("D3D11 diagnostic: %s returned 0x%08x; GetDeviceRemovedReason=0x%08x; device=%p",
           operation, (uint32_t)result, (uint32_t)reason, device);
  }
  else
  {
    RDCERR("D3D11/DXGI diagnostic: %s returned 0x%08x (D3D11 device unavailable)", operation,
           (uint32_t)result);
  }
}
