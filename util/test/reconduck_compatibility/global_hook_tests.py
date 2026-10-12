"""Isolated Global Hook regression runner; never writes the machine registry."""
from pathlib import Path
import subprocess
import sys
import re
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
HEADER = ROOT / 'reconduck/os/win32/win32_global_hook.h'

if not HEADER.exists():
    # Inspect the actual pre-transaction AppInit write: its replacement must include
    # the saved DLL list. This is the regression that motivates the native cases.
    source = (ROOT / 'reconduck/os/win32/win32_process.cpp').read_text(encoding='utf-8')
    write = source[source.index('rdcwstr shortpath(shimpathNative.size())'):]
    write = write[:write.index('// if we\'re doing Wow32')]
    assert 'appinitDLLs' in write, 'FAIL: native AppInit write discards pre-existing DLL entries'
    raise AssertionError('FAIL: isolated transaction is not implemented')

SDK = Path('C:/Program Files (x86)/Windows Kits/10')
INC = SDK / 'Include/10.0.26100.0'
LIB = SDK / 'Lib/10.0.26100.0'
LLVM = Path('D:/Tool/LLVM/bin')
MSVC = Path('D:/Code/VS2026/VC/Tools/MSVC/14.44.35207')

def run(args):
    print(' '.join(map(str, args)), flush=True)
    result = subprocess.run(list(map(str, args)), text=True, capture_output=True, timeout=60)
    print(result.stdout, end='')
    print(result.stderr, end='')
    if result.returncode:
        sys.exit(result.returncode)

for arch in ['x64', 'x86']:
    with tempfile.TemporaryDirectory(prefix='global_hook_' + arch + '_', dir=HERE) as temporary:
        out = Path(temporary)
        process = (ROOT / 'reconduck/os/win32/win32_process.cpp').read_text(encoding='utf-8')
        activity = re.search(r'bool Process::IsGlobalHookActive\(\)\s*\{.*?\n\}', process, re.S)
        assert activity, 'Global Hook activity query is missing'
        (out / 'global_hook_activity.inc').write_text(activity.group().replace('Process::', ''), encoding='utf-8')
        state = process[process.index('static GlobalHook::Session *globalHook = NULL;'):
                        process.index('RDResult Process::StartGlobalHook(')]
        (out / 'global_hook_state.inc').write_text(state, encoding='utf-8')
        run([MSVC / ('bin/Hostx64/' + arch + '/cl.exe'), '/nologo', '/c', '/GS-', '/O1', '/EHsc', '/MT',
             '/std:c++14', '/W4', '/WX', '/I' + str(MSVC / 'include'), '/I' + str(out),
             *['/I' + str(INC / x) for x in ['um', 'shared', 'ucrt']],
             HERE / 'global_hook_native.cpp', '/Fo' + str(out / 'tests.obj')])
        run([LLVM / 'lld-link.exe', '/machine:' + arch,
             '/libpath:' + str(MSVC / 'lib' / arch),
             '/libpath:' + str(LIB / 'um' / arch), '/libpath:' + str(LIB / 'ucrt' / arch),
             'kernel32.lib', out / 'tests.obj', '/subsystem:console',
             '/out:' + str(out / 'tests.exe')])
        run([out / 'tests.exe'])
        # Check integration with the real Process translation unit as well as the isolated header.
        run([MSVC / ('bin/Hostx64/' + arch + '/cl.exe'), '/nologo', '/Zs', '/std:c++14',
             '/W4', '/WX', '/wd4100', '/EHsc', '/GR-',
             '/FI' + str(ROOT / 'reconduck/core/precompiled.h'),
             '/I' + str(ROOT / 'reconduck'), '/I' + str(ROOT / 'reconduck/3rdparty'),
             '/I' + str(MSVC / 'include'),
             *['/I' + str(INC / x) for x in ['um', 'shared', 'ucrt']],
             *['/D' + x for x in ['UNICODE', '_UNICODE', 'RECONDUCK_PLATFORM_WIN32',
                                 'RECONDUCK_EXPORTS', 'RDOC_BASE_NAME=reconduck', 'WIN32',
                                 'NDEBUG', '_WINDOWS', '_USRDLL']],
             ROOT / 'reconduck/os/win32/win32_process.cpp'])
