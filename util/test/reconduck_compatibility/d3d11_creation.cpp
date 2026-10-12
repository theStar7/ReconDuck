// The runner inserts the real Create_Internal method at METHOD below. The small
// COM model isolates the driver boundary while retaining observable references.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
extern "C" int memcmp(const void *a, const void *b, size_t count)
{
  const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
  for(size_t i = 0; i < count; ++i)
    if(x[i] != y[i]) return x[i] - y[i];
  return 0;
}
void *operator new(size_t size) { return malloc(size); }
void operator delete(void *ptr) noexcept { free(ptr); }
void operator delete(void *ptr, size_t) noexcept { free(ptr); }
#define RDCDEBUG(...) ((void)0)
#define RDCLOG(...) ((void)0)
#define RDCERR(...) ((void)0)
#define RDCASSERT(...) ((void)0)
#define SAFE_RELEASE(p) do { if(p) { (p)->Release(); (p) = NULL; } } while(0)
enum D3D_DRIVER_TYPE { D3D_DRIVER_TYPE_HARDWARE = 1 };
typedef UINT D3D_FEATURE_LEVEL;
const UINT D3D11_CREATE_DEVICE_DEBUG = 2;
const UINT D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY = 0x80;
struct IDXGIAdapter {};
struct DXGI_SWAP_CHAIN_DESC { BOOL Windowed; HWND OutputWindow; };
struct D3D11InitParams { D3D_DRIVER_TYPE DriverType; UINT Flags, SDKVersion, NumFeatureLevels; D3D_FEATURE_LEVEL FeatureLevels[16]; };
struct Options { bool apiValidation = false, allowFullscreen = true, allowVSync = true; };
struct ReconDuck {
  static ReconDuck &Inst() { static ReconDuck value; return value; }
  bool IsReplayApp() { return false; }
  Options &GetCaptureOptions() { static Options value; return value; }
};
static int alive = 0, wrappers = 0;
struct ID3D11DeviceContext;
struct ID3D11Device {
  int refs = 1;
  bool wrapped = false;
  ID3D11Device *real = NULL;
  ID3D11Device() { ++alive; }
  virtual ~ID3D11Device() {}
  void AddRef() { ++refs; }
  void Release() { if(--refs == 0) { if(real) real->Release(); --alive; delete this; } }
  void GetImmediateContext(ID3D11DeviceContext **context);
};
struct ID3D11DeviceContext {
  ID3D11Device *device;
  bool wrapped;
  ID3D11DeviceContext(ID3D11Device *d) : device(d), wrapped(d->wrapped) { d->AddRef(); ++alive; }
  void Release() { device->Release(); --alive; delete this; }
};
void ID3D11Device::GetImmediateContext(ID3D11DeviceContext **context)
{ if(context) *context = new ID3D11DeviceContext(this); }
struct WrappedID3D11Device : ID3D11Device {
  WrappedID3D11Device(ID3D11Device *d, D3D11InitParams) { wrapped = true; real = d; ++wrappers; }
  static bool IsAlloc(ID3D11Device *d) { return d && d->wrapped; }
};
struct IDXGISwapChain {
  int refs = 1;
  bool wrapped = false;
  IDXGISwapChain *real = NULL;
  ID3D11Device *device;
  IDXGISwapChain(ID3D11Device *d) : device(d) { d->AddRef(); ++alive; }
  virtual ~IDXGISwapChain() {}
  void Release() { if(--refs == 0) { if(real) real->Release(); device->Release(); --alive; delete this; } }
};
struct WrappedIDXGISwapChain4 : IDXGISwapChain {
  WrappedIDXGISwapChain4(IDXGISwapChain *s, HWND, ID3D11Device *d) : IDXGISwapChain(d) { real = s; wrapped = true; }
};
typedef HRESULT (*RealD3D11CreateFunction)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT,
 const D3D_FEATURE_LEVEL *, UINT, UINT, const DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **,
 ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);
static int failureDiagnostics = 0;
static ID3D11Device *failureDiagnosticDevice = NULL;
void LogD3D11Failure(ID3D11Device *device, const char *, HRESULT)
{ ++failureDiagnostics; failureDiagnosticDevice = device; }
struct Harness {
  bool recurse = false;
  bool CheckRecurse() { if(recurse) return true; recurse = true; return false; }
  void EndRecurse() { recurse = false; }
  // METHOD
};
static int mode = 0;
static UINT flagsSeen;
static HRESULT NativeCreate(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT flags,
 const D3D_FEATURE_LEVEL *, UINT, UINT, const DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **swap,
 ID3D11Device **device, D3D_FEATURE_LEVEL *level, ID3D11DeviceContext **context)
{
  flagsSeen = flags;
  // A failed driver call need not provide a usable object in an output slot.
  if(mode == 1) return E_INVALIDARG;
  if(device) *device = NULL;
  if(context) *context = NULL;
  if(swap) *swap = NULL;
  if(level) *level = 0xb000;
  if(!device && !context && !swap) return S_FALSE;
  ID3D11Device *d = new ID3D11Device;
  if(mode == 2) d = new WrappedID3D11Device(d, {});
  if(context) d->GetImmediateContext(context);
  if(swap) *swap = new IDXGISwapChain(d);
  if(device) *device = d; else d->Release();
  return mode == 3 ? S_FALSE : S_OK;
}
static void Check(bool ok, const char *message) { if(!ok) { printf("FAIL: %s\n", message); ExitProcess(1); } }
extern "C" void entry()
{
  char scenario[64] = {};
  GetEnvironmentVariableA("D3D11_CASE", scenario, sizeof(scenario));
  const bool contextOnly = !strcmp(scenario, "context-only");
  const bool swapOnly = !strcmp(scenario, "swap-only");
  const bool validate = !strcmp(scenario, "validation");
  const bool noContext = !strcmp(scenario, "device-only");
  mode = !strcmp(scenario, "failure") ? 1 : !strcmp(scenario, "already-wrapped") ? 2 : !strcmp(scenario, "nonzero-success") ? 3 : 0;
  ID3D11Device *device = NULL;
  if(mode == 1) device = (ID3D11Device *)1;
  ID3D11DeviceContext *context = NULL;
  IDXGISwapChain *swap = NULL;
  D3D_FEATURE_LEVEL level = 0;
  Harness harness;
  DXGI_SWAP_CHAIN_DESC desc = {TRUE, NULL};
  const UINT flags = D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY;
  HRESULT hr = harness.Create_Internal(NativeCreate, NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags,
      NULL, 0, 7, swapOnly ? &desc : NULL, swapOnly ? &swap : NULL,
      contextOnly || swapOnly || validate ? NULL : &device, &level,
      validate || swapOnly || noContext ? NULL : &context);
  Check(flagsSeen == flags, "registry-layer creation flag must reach driver unchanged");
  Check(!harness.recurse, "recursion guard must clear");
  if(mode == 1) {
    Check(hr == E_INVALIDARG, "creation failure HRESULT changed");
    Check(device == (ID3D11Device *)1 && !context && !swap, "failed outputs must retain driver behavior");
    Check(failureDiagnostics == 1, "creation failure must call bounded diagnostics");
    Check(!failureDiagnosticDevice, "failed creation must not query a stale device output");
    device = NULL;
  } else if(validate) {
    Check(hr == S_FALSE, "validation-only HRESULT must remain S_FALSE");
    Check(wrappers == 0, "validation must not construct a device wrapper");
  } else {
    Check(hr == (mode == 3 ? S_FALSE : S_OK), "successful HRESULT changed");
    if(!contextOnly && !swapOnly) Check(device && device->wrapped, "registry-layer flag must not suppress wrapping");
    if(!noContext && !swapOnly) Check(context && context->wrapped, "requested immediate context must be wrapped");
    if(swapOnly) Check(swap && swap->wrapped, "swapchain-only call must retain wrapped device");
    Check(wrappers == 1, "device must be wrapped once");
  }
  SAFE_RELEASE(context);
  SAFE_RELEASE(swap);
  SAFE_RELEASE(device);
  Check(alive == 0, "creation must balance device, context and swapchain references");
  printf("PASS: %s\n", scenario);
  ExitProcess(0);
}
