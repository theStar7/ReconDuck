"""Compile the production creation method, not a reimplementation of its policy.

The fake COM boundary verifies wrapping/output/refcount decisions. It does not
substitute for a full DLL build and WARP capture/replay with actual COM devices.
"""
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SDK = Path("C:/Program Files (x86)/Windows Kits/10")
VERSION = "10.0.26100.0"
LLVM = Path("D:/Tool/LLVM/bin")


def method(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def run(args, **kwargs):
    result = subprocess.run([str(x) for x in args], capture_output=True, text=True, **kwargs)
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="")
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {args[0]}")


def main():
    source = (ROOT / "reconduck/driver/d3d11/d3d11_hooks.cpp").read_text()
    create = method(source, "HRESULT Create_Internal(")
    template = (HERE / "d3d11_creation.cpp").read_text()
    failures = []
    with tempfile.TemporaryDirectory(prefix="d3d11-compatibility-") as directory:
        out = Path(directory)
        generated = out / "creation.cpp"
        creation = template.replace("// METHOD", create)
        dxgi = (ROOT / "reconduck/driver/dxgi/dxgi_wrapped.cpp").read_text()
        nvapi = (ROOT / "reconduck/driver/ihv/nv/nvapi_hooks.cpp").read_text()
        driver = (HERE / "d3d11_driver.cpp").read_text().replace("// DXGI_METHODS", "\n".join(
            method(dxgi, "HRESULT STDMETHODCALLTYPE WrappedIDXGIOutput6::" + name + "(")
            for name in ["FindClosestMatchingMode", "FindClosestMatchingMode1"])).replace("// NVAPI_METHODS", "\n".join(
            method(nvapi, "static NvAPI_Status __cdecl NvAPI_D3D" + version + "_IsNvShaderExtnOpCodeSupported_hook(")
            for version in ["11", "12"]))
        header_path = ROOT / "reconduck/driver/d3d11/d3d11_device_diagnostics.h"
        # Before the feature exists, existing production methods execute against
        # a no-op logger. This lets missing diagnostics fail behavior assertions.
        header = ("\n".join(line for line in header_path.read_text().splitlines()
                            if not line.startswith("#include") and not line.startswith("#pragma"))
                  if header_path.exists() else "void LogD3D11Failure(ID3D11Device *, const char *, HRESULT) {}")
        diagnostics = (HERE / "d3d11_diagnostics.cpp").read_text().replace("// DIAGNOSTICS_HEADER", header)
        swaplog = method(dxgi, "static void LogSwapchainFailure(") if "static void LogSwapchainFailure(" in dxgi else ""
        diagnostics = diagnostics.replace("// SWAPCHAIN_DIAGNOSTICS", swaplog).replace("// PRESENT_METHODS", "\n".join(
            method(dxgi, "HRESULT WrappedIDXGISwapChain4::" + name + "(") for name in ["Present", "Present1"]))
        layout = method((ROOT / "reconduck/driver/d3d11/d3d11_device_wrap.cpp").read_text(), "HRESULT WrappedID3D11Device::CreateInputLayout(")
        # Only the actual native-call prefix is needed; successful resource
        # serialization depends on the complete capture DLL and is not modeled.
        layout = layout[:layout.index("  if(SUCCEEDED(ret))")] + "  (void)wrapped;\n  return ret;\n}\n"
        diagnostics = diagnostics.replace("// INPUT_LAYOUT_PREFIX", layout)
        diagnostics = diagnostics.replace("DXGI_ERROR_DEVICE_REMOVED", "((HRESULT)0x887a0005)")
        for arch, triple in [("x64", "x86_64-pc-windows-msvc"), ("x86", "i686-pc-windows-msvc")]:
          for suite, content, cases in [
              ("creation", creation, ["registry-flag", "context-only", "swap-only", "device-only", "already-wrapped", "validation", "failure", "nonzero-success"]),
              ("driver", driver, ["dxgi-unwrapped", "dxgi-wrapped", "dxgi-null", "n1-device-null", "n1-output-null", "n2-device-null", "n2-output-null"]),
              ("diagnostics", diagnostics, ["success", "creation-no-device", "input-layout", "presentation", "pn-unavailable", "threads"])]:
            generated.write_text(content)
            obj, exe = out / (arch + ".obj"), out / (arch + ".exe")
            run([LLVM / "clang-cl.exe", "--target=" + triple, "/nologo", "/c", "/GS-", "/O2", "/std:c++14", "/W4", "/WX", "/GR-", "/EHs-c-",
                 *["/I" + str(SDK / "Include" / VERSION / x) for x in ["um", "shared", "ucrt"]],
                 generated, "/Fo" + str(obj)])
            run([LLVM / "lld-link.exe", "/nodefaultlib", "/machine:" + arch,
                 "/libpath:" + str(SDK / "Lib" / VERSION / "um" / arch),
                 "/libpath:" + str(SDK / "Lib" / VERSION / "ucrt" / arch),
                 "kernel32.lib", "ucrt.lib", obj, "/entry:entry", "/subsystem:console", "/out:" + str(exe)])
            for case in cases:
                print(arch, case, flush=True)
                try:
                    run([exe], env=os.environ | {"D3D11_CASE": case}, timeout=10)
                except RuntimeError:
                    failures.append((arch, case))
    if failures:
        raise SystemExit(f"Failed D3D11 compatibility cases: {failures}")


if __name__ == "__main__":
    main()
