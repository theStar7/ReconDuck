"""Native process selection tests; no capture DLL or registry writes."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
SDK = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Windows Kits/10'
VERSION = sorted((SDK / 'Include').iterdir())[-1].name

with tempfile.TemporaryDirectory(prefix='manual-launch-') as tmp:
    tmp = Path(tmp)
    for arch, machine in [('x86_64', 'x64'), ('i686', 'x86')]:
        obj = tmp / f'tests-{machine}.obj'
        exe = tmp / f'tests-{machine}.exe'
        subprocess.run(['clang-cl', f'--target={arch}-pc-windows-msvc', '/nologo', '/c',
                        '/GS-', '/O2', '/W4', '/WX', '/std:c++14',
                        *[f'/I{SDK / "Include" / VERSION / part}' for part in ['um', 'shared', 'ucrt']],
                        str(Path(__file__).with_name('manual_launch_process_tests.cpp')),
                        f'/Fo{obj}'], check=True)
        subprocess.run(['lld-link', '/nologo', '/nodefaultlib', f'/machine:{machine}',
                        '/entry:entry', '/subsystem:console',
                        f'/libpath:{SDK / "Lib" / VERSION / "um" / machine}',
                        f'/libpath:{SDK / "Lib" / VERSION / "ucrt" / machine}',
                        str(obj), 'kernel32.lib', 'ucrt.lib', f'/out:{exe}'], check=True)
        subprocess.run([str(exe)], check=True)
        print(f'PASS manual launch process policy ({machine})')
