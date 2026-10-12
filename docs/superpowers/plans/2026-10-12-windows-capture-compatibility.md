# Windows Capture Compatibility Implementation Plan

> Execution: subagent-driven-development with separate file ownership; review and integration in the current checkout. The user approved the design and requested continuation.

**Goal:** Add export Detours, D3D11 compatibility fixes and manual-launch capture to ReconDuck while retaining its startup-thread injector.

**Architecture:** Keep existing IAT/GetProcAddress hooks and add tracked export trampolines. Adapt the RenderDuck changes onto the current 1.47 sources rather than replacing whole files. Keep Global Hook session cleanup independent of GUI waiting.

**Tech Stack:** C++, Win32, Microsoft Detours 4.0.1, Qt 5, LLVM and Windows SDK native regression harnesses.

## Task 1: Export interception and build integration

- [x] Create a native cached-function regression harness in `util/test/reconduck_compatibility/` and run the old behavior: a cached export bypasses interception.
- [x] Vendor the referenced Microsoft Detours source and its MIT license under `reconduck/3rdparty/detours/`; record the source revision.
- [x] Add the sources and include path to `reconduck/reconduck.vcxproj` and its filters, only for the Windows core target.
- [x] Update `reconduck/os/win32/win32_hook.cpp`: preserve trampoline pointers, avoid duplicate/conflicting export attachments, skip loaders/NVAPI, leave NvCamera imports native and bound import descriptor scans. Make detach failures retain tracked state.
- [x] Run native x86/x64 cached address, trampoline, repeated installation, failure and detach checks; inspect every original-pointer write site.

The forward call must remain a trampoline after repeat enumeration:

```cpp
// The hook's onward pointer is owned by Detours after successful attachment.
// Repeated export resolution must not replace it with the patched entry.
if(!IsExportDetoured(hook.orig))
  *hook.orig = GetProcAddress(module, hook.function.c_str());
```

## Task 2: D3D11 and driver compatibility

- [x] Establish failing creation-output regression cases before changing `reconduck/driver/d3d11/d3d11_hooks.cpp`.
- [x] Adapt the registry-layer wrapping fix and repair optional device/context outputs without changing failure HRESULTs or leaking references.
- [x] Adapt unwrapped-device DXGI forwarding and NVAPI null checks in `reconduck/driver/dxgi/dxgi_wrapped.cpp` and `reconduck/driver/ihv/nv/nvapi_hooks.cpp`.
- [x] Add bounded creation/presentation failure diagnostics using `reconduck/driver/d3d11/d3d11_device_diagnostics.h`; keep API results unchanged.
- [x] Run creation/output cases and validate changed source syntax where the local SDK permits. Document that full WARP capture/replay still requires the complete application build.

```cpp
// This flag controls registry-driven layers, not whether a successful
// explicitly intercepted device is wrapped by the capture tool.
if(SUCCEEDED(ret) && ppDevice && *ppDevice)
{
  // Keep a single wrapper and return requested immediate-context output.
}
```

## Task 3: Manual launch UI and process discovery

- [x] Add process-selection regression cases covering duplicate names, different full paths, existing PIDs, unrelated helpers and bounded ancestry.
- [x] Extend `qreconduck/Code/qprocessinfo.{h,cpp}` with parent PID and executable path. Preserve existing enumeration callers.
- [x] Adapt `qreconduck/Windows/Dialogs/CaptureDialog.{h,cpp,ui}`: local waiting button, existing-target snapshot, exact target detection, presenting child recognition, bounded retries, cancellable background polling and thread-safe status delivery.
- [x] Ensure only newly started exact targets are fallback injection candidates; broad same-directory matching alone is insufficient. Keep current UI capture connection callbacks and queued-frame behavior.
- [x] Stop only the Global Hook session owned by waiting; retain shared state until the worker finishes, reject application exit while waiting, and restore controls on every exit path. Cancellation must not block the UI on socket/injection waits.
- [x] Validate UI XML, signal/slot declarations and process-policy tests. Compile changed UI sources if installed Qt/toolchain permits.

```cpp
// A matching executable name from a different known path is not the target.
bool selected = processPath == selectedExePath;
// A presenting child may be connected after its ancestry is established;
// a helper is never selected merely because it shares the installation folder.
```

## Task 4: Global Hook transaction and integration

- [x] Add failure-recovery/path tests that do not change the machine's registry.
- [x] Adapt `reconduck/os/win32/win32_global_hook.h` and only the Global Hook methods in `reconduck/os/win32/win32_process.cpp`. Leave the startup-thread loader and cross-bitness delegation intact.
- [x] Validate native/WOW32 components, preserve previous AppInit values, publish a session only after complete startup, and roll back partial starts.
- [x] Add the new header to Windows project/filter references and retain errors from cleanup failures.

```cpp
GlobalHook::Session *candidate = new GlobalHook::Session;
if(!candidate->Start(components, arguments))
{
  // Return its diagnostic after rollback; do not publish an active session.
}
```

## Task 5: Review and final checks

- [x] Review scope against the approved design, then review lifecycle/race/refcount/error behavior independently.
- [x] Run `python verify_reconduck.py`, `python threadcontext-validation/run_tests.py`, Windows resource checks, and the new native compatibility tests.
- [x] Run project/source reference checks and inspect the final diff for old names, unrelated file changes and build integration gaps.
- [x] Record full-build and actual capture/replay results using the discovered MSVC installation. Keep changes reviewable in the current workspace; do not push.
