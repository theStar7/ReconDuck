"""Build/run an actual WARP capture and verify replay pixels using the rebuilt DLL."""
import os
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
from d3d11_tests import HERE, ROOT, SDK, VERSION

MSVC = Path("D:/Code/VS2026/VC/Tools/MSVC/14.44.35207")
arch = sys.argv[1] if len(sys.argv) > 1 else "x64"
platform = "x64" if arch == "x64" else "Win32"
binary = ROOT / platform / "Development"
out = ROOT.parent / "build-temp" / ("warp-smoke-" + arch)
out.mkdir(exist_ok=True)
log = out / "smoke.log"
log.write_text("", encoding="utf-8")
env = {key.upper(): value for key, value in os.environ.items()}
env["PATH"] = str(binary) + ";" + env.get("PATH", "")


def run(args, timeout=60):
    result = subprocess.run([str(x) for x in args], env=env, cwd=binary,
                            capture_output=True, timeout=timeout)
    stdout = result.stdout.decode("utf-8", errors="replace")
    stderr = result.stderr.decode("utf-8", errors="replace")
    with log.open("a", encoding="utf-8") as stream:
        stream.write(stdout + stderr)
    print(stdout, end="", flush=True)
    print(stderr, end="", flush=True)
    if result.returncode:
        raise SystemExit(result.returncode)
    return stdout


for name in ["capture", "replay"]:
    args = [MSVC / "bin/Hostx64" / arch / "cl.exe", "/nologo", "/W4", "/WX", "/wd4100", "/EHsc", "/MD",
            "/DWIN32", "/DRECONDUCK_PLATFORM_WIN32", "/I" + str(MSVC / "include"),
            "/I" + str(ROOT / "reconduck"),
            *["/I" + str(SDK / "Include" / VERSION / x) for x in ["um", "shared", "ucrt"]],
            HERE / ("d3d11_warp_" + name + ".cpp"), "/Fo" + str(out / (name + ".obj")),
            "/Fe" + str(out / (name + ".exe")), "/link",
            "/LIBPATH:" + str(MSVC / "lib" / arch),
            "/LIBPATH:" + str(SDK / "Lib" / VERSION / "um" / arch),
            "/LIBPATH:" + str(SDK / "Lib" / VERSION / "ucrt" / arch), "user32.lib"]
    if name == "replay":
        args += [binary / "reconduck.lib"]
    run(args)
capture = run([out / "capture.exe", binary / "reconduck.dll", out / "warp"], timeout=90)
match = re.search(r"^CAPTURE=(.+)$", capture, re.MULTILINE)
if not match:
    raise SystemExit("Capture process did not report a capture file")
run([out / "replay.exe", match.group(1).strip()], timeout=90)
