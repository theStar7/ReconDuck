// Extracted presentation and input-layout driver calls plus the real diagnostics header.
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
static LONG logs = 0, reasonQueries = 0;
static int deviceRefs = 0;
namespace Atomic {
int32_t CmpExch32(int32_t *dest, int32_t oldVal, int32_t newVal)
{ return InterlockedCompareExchange((volatile LONG *)dest, newVal, oldVal); }
}
static void RecordLog(const char *, ...) { InterlockedIncrement(&logs); }
#define RDCERR(...) RecordLog(__VA_ARGS__)
#define SAFE_RELEASE(p) do { if(p) { (p)->Release(); (p) = NULL; } } while(0)
#define SERIALISE_TIME_CALL(call) call
struct D3D11_INPUT_ELEMENT_DESC {};
struct ID3D11InputLayout {};
struct __declspec(uuid("10000000-0000-0000-0000-000000000003")) ID3D11Device {
  HRESULT result = E_FAIL;
  void Release() { --deviceRefs; }
  HRESULT GetDeviceRemovedReason() { InterlockedIncrement(&reasonQueries); return DXGI_ERROR_DEVICE_REMOVED; }
  HRESULT CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC *, UINT, const void *, SIZE_T, ID3D11InputLayout **out)
  { if(out) *out = NULL; return result; }
};
// DIAGNOSTICS_HEADER
struct DXGI_PRESENT_PARAMETERS {};
const UINT DXGI_PRESENT_TEST = 1;
struct IDXGISwapChain {
  ID3D11Device *device;
  HRESULT result;
  HRESULT GetDevice(REFIID, void **out) { *out = device; if(device) ++deviceRefs; return device ? S_OK : E_NOINTERFACE; }
  HRESULT Present(UINT, UINT) { return result; }
  HRESULT Present1(UINT, UINT, const DXGI_PRESENT_PARAMETERS *) { return result; }
};
// SWAPCHAIN_DIAGNOSTICS
struct Options { bool allowVSync = true; };
struct ReconDuck {
  static ReconDuck &Inst() { static ReconDuck value; return value; }
  Options &GetCaptureOptions() { static Options value; return value; }
};
struct DeviceCallbacks { void Present(void *, UINT, UINT) {} };
struct WrappedIDXGISwapChain4 {
  IDXGISwapChain *m_pReal, *m_pReal1;
  DeviceCallbacks *m_pDevice;
  void TickLastPresentedBuffer() {}
  HRESULT Present(UINT, UINT);
  HRESULT Present1(UINT, UINT, const DXGI_PRESENT_PARAMETERS *);
};
// PRESENT_METHODS
struct WrappedID3D11Device {
  ID3D11Device *m_pDevice;
  HRESULT CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC *, UINT, const void *, SIZE_T, ID3D11InputLayout **);
};
// INPUT_LAYOUT_PREFIX
static void Check(bool ok, const char *message) { if(!ok) { printf("FAIL: %s\n", message); ExitProcess(1); } }
static DWORD WINAPI FailConcurrently(void *device)
{
  for(int i = 0; i < 100; ++i)
    LogD3D11Failure((ID3D11Device *)device, "concurrent", E_FAIL);
  return 0;
}
extern "C" void entry()
{
  char scenario[64] = {};
  GetEnvironmentVariableA("D3D11_CASE", scenario, sizeof(scenario));
  ID3D11Device device;
  if(scenario[0] == 's') {
    LogD3D11Failure(&device, "success", S_FALSE);
    Check(logs == 0 && reasonQueries == 0, "success must not consume diagnostics or query removal reason");
  } else if(scenario[0] == 't') {
    HANDLE threads[16];
    for(int i = 0; i < 16; ++i) {
      threads[i] = CreateThread(NULL, 0, FailConcurrently, &device, 0, NULL);
      Check(threads[i] != NULL, "test thread creation failed");
    }
    Check(WaitForMultipleObjects(16, threads, TRUE, 5000) == WAIT_OBJECT_0, "test threads did not finish");
    for(int i = 0; i < 16; ++i) CloseHandle(threads[i]);
    Check(logs == 8 && reasonQueries == 8, "concurrent failures must reserve each diagnostic exactly once");
  } else if(scenario[0] == 'c') {
    LogD3D11Failure(NULL, "CreateDevice", E_FAIL);
    Check(logs == 1, "creation failure without a device must be diagnosed");
  } else if(scenario[0] == 'i') {
    WrappedID3D11Device wrapper = {&device};
    ID3D11InputLayout *layout = NULL;
    Check(wrapper.CreateInputLayout(NULL, 0, NULL, 0, NULL) == E_FAIL, "validation failure HRESULT changed");
    Check(wrapper.CreateInputLayout(NULL, 0, NULL, 0, &layout) == E_FAIL, "creation failure HRESULT changed");
    Check(logs == 2 && reasonQueries == 2, "input-layout failures must include device removal reason");
  } else {
    const bool unavailable = scenario[1] == 'n';
    IDXGISwapChain native = {unavailable ? NULL : &device, E_FAIL};
    DeviceCallbacks callbacks;
    WrappedIDXGISwapChain4 wrapper = {&native, &native, &callbacks};
    for(int i = 0; i < 100; ++i) {
      Check(wrapper.Present(1, 0) == E_FAIL, "Present failure HRESULT changed");
      Check(wrapper.Present1(1, 0, NULL) == E_FAIL, "Present1 failure HRESULT changed");
    }
    Check(logs > 0 && logs <= 8, "repeated presentation failures must have a bounded diagnostic budget");
    Check(unavailable || reasonQueries == logs, "presentation diagnostics must query device removal reason");
    Check(deviceRefs == 0, "presentation diagnostics must release queried device references");
    int before = logs;
    LogD3D11Failure(&device, "after-budget", E_FAIL);
    Check(logs == before, "creation and presentation must share the diagnostic budget");
  }
  printf("PASS: %s\n", scenario);
  ExitProcess(0);
}
