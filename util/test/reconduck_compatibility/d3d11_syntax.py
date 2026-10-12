"""Check complete changed source files with MSVC; this does not link the DLL."""
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from d3d11_tests import ROOT, SDK, VERSION

MSVC = Path("D:/Code/VS2026/VC/Tools/MSVC/14.44.35207")
SOURCES = ["driver/d3d11/d3d11_hooks.cpp", "driver/d3d11/d3d11_device_wrap.cpp",
           "driver/dxgi/dxgi_wrapped.cpp", "driver/ihv/nv/nvapi_hooks.cpp"]


for arch in ["x64", "x86"]:
    for source in SOURCES:
        args = [MSVC / "bin/Hostx64" / arch / "cl.exe", "/nologo", "/Zs", "/W4", "/WX", "/wd4100",
                "/DWIN32", "/DRECONDUCK_EXPORTS", "/DRECONDUCK_PLATFORM_WIN32",
                "/DRDOC_BASE_NAME=reconduck", "/DNDEBUG", "/D_WINDOWS", "/D_USRDLL",
                "/I" + str(MSVC / "include"), "/I" + str(ROOT / "reconduck"),
                "/I" + str(ROOT / "reconduck/3rdparty"),
                "/I" + str(ROOT / "reconduck/driver/dx/official"),
                *["/I" + str(SDK / "Include" / VERSION / x) for x in ["um", "shared", "ucrt"]]]
        if arch == "x64":
            args.append("/DWIN64")
        if not source.startswith("driver/ihv/"):
            args.append("/FIprecompiled.h")
        print(arch, source, flush=True)
        result = subprocess.run([str(x) for x in args] + [str(ROOT / "reconduck" / source)],
                                capture_output=True)
        for output in [result.stdout, result.stderr]:
            if output:
                print(output.decode("utf-8", errors="replace"), end="")
        if result.returncode:
            raise SystemExit(result.returncode)
