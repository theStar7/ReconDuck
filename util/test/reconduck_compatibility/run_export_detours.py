from pathlib import Path
import os
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUT = ROOT.parent / 'build-temp/export-detours-validation'
SDK = Path('C:/Program Files (x86)/Windows Kits/10')
INCLUDE = SDK / 'Include/10.0.26100.0'
LIB = SDK / 'Lib/10.0.26100.0'
MSVC = Path('D:/Code/VS2026/VC/Tools/MSVC/14.44.35207')
LLVM = Path('D:/Tool/LLVM/bin')

def run(arguments, **kwargs):
    try:
        result = subprocess.run([str(item) for item in arguments], capture_output=True, text=True, **kwargs)
    except subprocess.TimeoutExpired as error:
        print(error.stdout)
        print(error.stderr)
        raise
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    if result.stdout:
        print(result.stdout.strip())

for arch, triple in [('x64', 'x86_64-pc-windows-msvc'), ('x86', 'i686-pc-windows-msvc')]:
    out = OUT / arch
    out.mkdir(parents=True, exist_ok=True)
    includes = ['/I' + str(INCLUDE / part) for part in ('um', 'shared', 'ucrt')]
    common = [LLVM / 'clang-cl.exe', '--target=' + triple, '/nologo', '/c', '/GS-', '/O1',
              '/std:c++14', '/I' + str(ROOT / 'reconduck'),
              '/I' + str(ROOT / 'reconduck/3rdparty/detours'), *includes]
    sources = [HERE / 'export_fixture.cpp', HERE / 'export_detours.cpp', HERE / 'export_runtime.cpp']
    detours = ROOT / 'reconduck/3rdparty/detours'
    if (ROOT / 'reconduck/os/win32/win32_export_detour.h').exists():
        sources += [detours / (name + '.cpp') for name in ('detours', 'disasm', 'modules', 'image')]
    for source in sources:
        run([*common, source, '/Fo' + str(out / (source.stem + '.obj'))])
    link = [LLVM / 'lld-link.exe', '/nodefaultlib', '/machine:' + arch,
            '/libpath:' + str(LIB / 'um' / arch), '/libpath:' + str(LIB / 'ucrt' / arch),
            '/libpath:' + str(MSVC / 'lib' / arch),
            'kernel32.lib', 'ntdll.lib', 'ucrt.lib', 'vcruntime.lib']
    defs = out / 'fixture.def'
    defs.write_text('EXPORTS\nCachedTarget' + ('=_CachedTarget@4' if arch == 'x86' else '') + '\n')
    run([*link, out / 'export_fixture.obj', '/dll', '/entry:DllMain', '/def:' + str(defs),
         '/out:' + str(out / 'fixture.dll')])
    run([*link, *[out / (source.stem + '.obj') for source in sources[1:]],
         '/entry:entry', '/subsystem:console', '/out:' + str(out / 'harness.exe')])
    print(arch, flush=True)
    run([out / 'harness.exe'], env=os.environ | {'DETOUR_FIXTURE': str(out / 'fixture.dll')},
        timeout=20)
