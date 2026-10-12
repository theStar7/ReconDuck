// Production DXGI and NVAPI methods are inserted by d3d11_tests.py.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define IUnknown TestUnknown
#define RDCERR(...) ((void)0)
#define RDCWARN(...) ((void)0)
typedef int NvAPI_Status;
typedef UINT NvU32;
const int NVAPI_INVALID_POINTER = -14;
struct __declspec(uuid("10000000-0000-0000-0000-000000000001")) ID3D12Device;
struct __declspec(uuid("10000000-0000-0000-0000-000000000002")) INVAPID3DDevice;
struct IUnknown {
  INVAPID3DDevice *backdoor = NULL;
  int queries = 0;
  HRESULT QueryInterface(REFIID, void **out) { ++queries; *out = backdoor; return backdoor ? S_OK : E_NOINTERFACE; }
};
struct ID3D12Device : IUnknown { void Release() {} };
struct INVAPID3DDevice { IUnknown *real; IUnknown *GetReal() { return real; } };
struct ID3DDevice { IUnknown *real; IUnknown *GetRealIUnknown() { return real; } };
static IUnknown *known;
static ID3DDevice *wrapper;
static ID3DDevice *GetD3DDevice(IUnknown *dev) { return dev == known ? wrapper : NULL; }
struct DXGI_MODE_DESC {};
struct DXGI_MODE_DESC1 {};
struct NativeOutput {
  IUnknown *seen = NULL;
  HRESULT FindClosestMatchingMode(const DXGI_MODE_DESC *, DXGI_MODE_DESC *, IUnknown *dev) { seen = dev; return E_ACCESSDENIED; }
  HRESULT FindClosestMatchingMode1(const DXGI_MODE_DESC1 *, DXGI_MODE_DESC1 *, IUnknown *dev) { seen = dev; return E_ACCESSDENIED; }
};
struct WrappedIDXGIOutput6 {
  NativeOutput *m_pReal, *m_pReal1;
  HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(const DXGI_MODE_DESC *, DXGI_MODE_DESC *, IUnknown *);
  HRESULT STDMETHODCALLTYPE FindClosestMatchingMode1(const DXGI_MODE_DESC1 *, DXGI_MODE_DESC1 *, IUnknown *);
};
// DXGI_METHODS
static int nativeCalls = 0;
static NvAPI_Status NativeSupported(IUnknown *, NvU32, bool *supported) { ++nativeCalls; if(supported) *supported = true; return 123; }
struct Hook { typedef NvAPI_Status (*Func)(IUnknown *, NvU32, bool *); Func operator()() { return NativeSupported; } };
struct NVHook {
  Hook NvAPI_D3D11_IsNvShaderExtnOpCodeSupported, NvAPI_D3D12_IsNvShaderExtnOpCodeSupported;
  static NVHook nvhooks;
  // NVAPI_METHODS
};
NVHook NVHook::nvhooks;
static void Check(bool ok, const char *message) { if(!ok) { printf("FAIL: %s\n", message); ExitProcess(1); } }
extern "C" void entry()
{
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  char scenario[64] = {};
  GetEnvironmentVariableA("D3D11_CASE", scenario, sizeof(scenario));
  IUnknown dev, real;
  INVAPID3DDevice backdoor = {&real};
  dev.backdoor = &backdoor;
  if(scenario[0] == 'd') {
    NativeOutput native;
    WrappedIDXGIOutput6 output = {&native, &native};
    ID3DDevice wrapped = {&real};
    const bool wrappedCase = scenario[5] == 'w';
    const bool nullCase = scenario[5] == 'n';
    if(wrappedCase) { known = &dev; wrapper = &wrapped; }
    IUnknown *passed = nullCase ? NULL : &dev;
    Check(output.FindClosestMatchingMode(NULL, NULL, passed) == E_ACCESSDENIED, "DXGI must preserve native result for concerned device");
    Check(native.seen == (wrappedCase ? &real : passed), "DXGI must unwrap only known wrapped devices");
    Check(output.FindClosestMatchingMode1(NULL, NULL, passed) == E_ACCESSDENIED, "DXGI1 must preserve native result for concerned device");
    Check(native.seen == (wrappedCase ? &real : passed), "DXGI1 must unwrap only known wrapped devices");
  } else {
    bool supported = true;
    const bool d3d12 = scenario[1] == '2';
    const bool nullDevice = scenario[3] == 'd';
    auto fn = d3d12 ? NVHook::NvAPI_D3D12_IsNvShaderExtnOpCodeSupported_hook : NVHook::NvAPI_D3D11_IsNvShaderExtnOpCodeSupported_hook;
    Check(fn(nullDevice ? NULL : &dev, 7, nullDevice ? &supported : NULL) == NVAPI_INVALID_POINTER, "NVAPI null input must return invalid pointer");
    if(nullDevice) Check(!supported, "NVAPI null-device query must clear support output");
    Check(dev.queries == 0 && nativeCalls == 0, "NVAPI invalid pointers must not reach COM or driver");
  }
  printf("PASS: %s\n", scenario);
  ExitProcess(0);
}
