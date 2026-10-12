#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include "api/app/reconduck_app.h"

static void Check(bool ok, const char *message)
{
  if(!ok) { printf("FAIL: %s\n", message); ExitProcess(1); }
}

int main(int argc, char **argv)
{
  Check(argc == 3, "expected capture DLL and output prefix");
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  HMODULE d3d11 = LoadLibraryA("d3d11.dll");
  Check(d3d11 != NULL, "load D3D11");
  // Cache entry addresses before loading capture hooks, exercising export Detours.
  auto create = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(d3d11, "D3D11CreateDevice");
  auto createSwap = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain");
  HMODULE capture = LoadLibraryA(argv[1]);
  Check(capture != NULL, "load newly built ReconDuck DLL");
  auto getAPI = (pRECONDUCK_GetAPI)GetProcAddress(capture, "RECONDUCK_GetAPI");
  RECONDUCK_API_1_7_0 *api = NULL;
  Check(getAPI && getAPI(eRECONDUCK_API_Version_1_7_0, (void **)&api) == 1, "get capture API");
  api->MaskOverlayBits(0, 0);
  api->SetCaptureFilePathTemplate(argv[2]);

  const UINT flags = D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY;
  ID3D11DeviceContext *context = NULL;
  Check(SUCCEEDED(create(NULL, D3D_DRIVER_TYPE_WARP, NULL, flags, NULL, 0, D3D11_SDK_VERSION,
                         NULL, NULL, &context)) && context, "context-only WARP creation");
  ID3D11Device *device = NULL;
  context->GetDevice(&device);
  api->StartFrameCapture(device, NULL);
  Check(api->IsFrameCapturing() == 1, "context-only creation must expose a capturable wrapped device");
  Check(api->DiscardFrameCapture(device, NULL) == 1, "discard context-only probe");
  device->Release();
  context->Release();

  WNDCLASSA cls = {};
  cls.lpfnWndProc = DefWindowProcA;
  cls.hInstance = GetModuleHandleA(NULL);
  cls.lpszClassName = "ReconDuckWarpSmoke";
  Check(RegisterClassA(&cls) != 0, "register hidden window");
  HWND window = CreateWindowA(cls.lpszClassName, "ReconDuck WARP smoke", WS_OVERLAPPEDWINDOW,
                              0, 0, 64, 64, NULL, NULL, cls.hInstance, NULL);
  Check(window != NULL, "create hidden window");
  DXGI_SWAP_CHAIN_DESC desc = {};
  desc.BufferDesc.Width = desc.BufferDesc.Height = 64;
  desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 1;
  desc.OutputWindow = window;
  desc.Windowed = TRUE;
  IDXGISwapChain *swap = NULL;
  Check(SUCCEEDED(createSwap(NULL, D3D_DRIVER_TYPE_WARP, NULL, flags, NULL, 0,
                             D3D11_SDK_VERSION, &desc, &swap, NULL, NULL, NULL)) && swap,
        "swapchain-only WARP creation with omitted immediate context");
  Check(SUCCEEDED(swap->GetDevice(__uuidof(ID3D11Device), (void **)&device)) && device,
        "get wrapped device through swapchain");
  device->GetImmediateContext(&context);
  ID3D11Texture2D *backbuffer = NULL;
  Check(SUCCEEDED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backbuffer)), "get backbuffer");
  ID3D11RenderTargetView *view = NULL;
  Check(SUCCEEDED(device->CreateRenderTargetView(backbuffer, NULL, &view)), "create render target");
  api->StartFrameCapture(device, window);
  Check(api->IsFrameCapturing() == 1, "swapchain-only device must be capturable");
  const float colour[] = {0.25f, 0.5f, 0.75f, 1.0f};
  context->ClearRenderTargetView(view, colour);
  Check(SUCCEEDED(swap->Present(0, 0)), "present WARP frame");
  Check(api->EndFrameCapture(device, window) == 1, "finish real WARP capture");
  Check(api->GetNumCaptures() == 1, "capture count");
  char path[4096] = {};
  uint32_t length = sizeof(path);
  Check(api->GetCapture(0, path, &length, NULL) == 1, "get capture file");
  printf("CAPTURE=%s\n", path);
  view->Release();
  backbuffer->Release();
  context->Release();
  swap->Release();
  device->Release();
  DestroyWindow(window);
  printf("PASS: cached exports, registry flag, context-only, swapchain-only, WARP capture\n");
  return 0;
}
