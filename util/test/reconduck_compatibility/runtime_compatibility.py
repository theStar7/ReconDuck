"""Reproduce the old-CRT mutex failure and load the actual rebuilt capture DLL.

An existing x64 runtime directory is required; no runtime binaries are vendored
and the source application directory is never changed.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET

from d3d11_tests import HERE, ROOT, SDK, VERSION

parser = argparse.ArgumentParser()
parser.add_argument('--runtime-dir', type=Path, required=True, help='Older x64 VC runtime DLL directory')
parser.add_argument('--dll', type=Path, default=ROOT / 'x64/Development/reconduck.dll')
args = parser.parse_args()
OUT = ROOT.parent / 'build-temp/runtime-compatibility-validation'
OUT.mkdir(exist_ok=True)
MSVC = Path('D:/Code/VS2026/VC/Tools/MSVC/14.44.35207')

for name in ['msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll']:
    shutil.copy2(args.runtime_dir / name, OUT / name)

namespace = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
project = ET.parse(ROOT / 'reconduck/driver/shaders/spirv/reconduck_spirv.vcxproj')
definitions = ';'.join(node.text or '' for node in project.findall('.//m:PreprocessorDefinitions', namespace))
compatibility = '_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR'
configured = [compatibility] if compatibility in definitions.split(';') else []

for variant in ['baseline', 'configured', 'load']:
    exe = OUT / (variant + '.exe')
    options = [MSVC / 'bin/Hostx64/x64/cl.exe', '/nologo', '/EHsc', '/std:c++14', '/W4', '/WX',
               '/MT' if variant == 'load' else '/MD', '/I' + str(MSVC / 'include'),
               *['/I' + str(SDK / 'Include' / VERSION / part) for part in ['um', 'shared', 'ucrt']],
               HERE / 'runtime_compatibility.cpp', '/Fo' + str(OUT / (variant + '.obj')), '/Fe' + str(exe)]
    if variant == 'configured':
        options += ['/D' + define for define in configured]
    if variant == 'load':
        options += ['/DRUNTIME_LOAD_PROBE']
    options += ['/link', '/LIBPATH:' + str(MSVC / 'lib/x64'),
                *['/LIBPATH:' + str(SDK / 'Lib' / VERSION / part / 'x64') for part in ['um', 'ucrt']]]
    result = subprocess.run(list(map(str, options)), capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    command = [str(exe)] + ([str(args.dll.resolve())] if variant == 'load' else [])
    result = subprocess.run(command, cwd=OUT, capture_output=True, text=True, timeout=45)
    print(variant, 'exit=' + hex(result.returncode & 0xffffffff), result.stdout, result.stderr)
    if variant == 'baseline':
        assert result.returncode & 0xffffffff == 0xc0000005, 'Expected the newer mutex/older runtime incompatibility'
    else:
        assert result.returncode == 0, 'Old-runtime compatibility regression'
