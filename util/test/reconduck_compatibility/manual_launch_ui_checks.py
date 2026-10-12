"""Compile the changed Qt translation units and generated UI/metaobject sources."""
import argparse
from pathlib import Path
import os
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', type=Path, required=True, help='Hostx64/<arch>/cl.exe')
parser.add_argument('--arch', choices=['x64', 'x86'], default='x64')
args = parser.parse_args()
ROOT = Path(__file__).resolve().parents[3]
QT = ROOT / 'qreconduck/3rdparty/qt'
SDK = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Windows Kits/10'
VERSION = sorted((SDK / 'Include').iterdir())[-1].name
MSVC = args.compiler.resolve().parents[3]
qt_arch = 'x64' if args.arch == 'x64' else 'Win32'

with tempfile.TemporaryDirectory(prefix='manual-launch-ui-') as directory:
    directory = Path(directory)
    subprocess.run([str(QT / qt_arch / 'bin/uic.exe'),
                    str(ROOT / 'qreconduck/Windows/Dialogs/CaptureDialog.ui'),
                    '-o', str(directory / 'ui_CaptureDialog.h')], check=True)
    subprocess.run([str(QT / qt_arch / 'bin/uic.exe'),
                    str(ROOT / 'qreconduck/Windows/MainWindow.ui'),
                    '-o', str(directory / 'ui_MainWindow.h')], check=True)
    subprocess.run([str(QT / qt_arch / 'bin/moc.exe'),
                    '-I', str(ROOT / 'qreconduck'), '-I', str(QT / 'include'),
                    str(ROOT / 'qreconduck/Windows/Dialogs/CaptureDialog.h'),
                    '-o', str(directory / 'moc_CaptureDialog.cpp')], check=True)
    includes = [MSVC / 'include', ROOT / 'qreconduck', ROOT / 'qreconduck/3rdparty',
                ROOT / 'reconduck/api/replay', directory, QT / 'include']
    includes += [QT / 'include' / name for name in ['QtCore', 'QtGui', 'QtWidgets', 'QtNetwork', 'QtSvg']]
    includes += [SDK / 'Include' / VERSION / name for name in ['um', 'shared', 'ucrt']]
    sources = [ROOT / 'qreconduck/Code/qprocessinfo.cpp',
               ROOT / 'qreconduck/Windows/Dialogs/CaptureDialog.cpp',
               ROOT / 'qreconduck/Windows/MainWindow.cpp', directory / 'moc_CaptureDialog.cpp']
    compiler_flags = [str(args.compiler), '/nologo', '/MD', '/std:c++14', '/EHsc',
                        '/DUNICODE', '/D_WIN32_WINNT=0x0601', '/DPYSIDE2_ENABLED=0',
                        '/DRECONDUCK_PLATFORM_WIN32', '/DQT_NO_CAST_FROM_ASCII',
                        '/DQT_NO_CAST_TO_ASCII', '/DQT_WIDGETS_LIB', '/DQT_GUI_LIB',
                        '/DQT_CORE_LIB', '/DQT_SVG_LIB',
                        *[f'/I{path}' for path in includes],
                        f'/FI{ROOT / "qreconduck/Code/precompiled.h"}']
    for source in sources:
        subprocess.run([*compiler_flags, '/c',
                        f'/Fo{directory / (source.stem + ".obj")}', str(source)], check=True)
    smoke = Path(__file__).with_name('manual_launch_qprocess_tests.cpp')
    executable = directory / 'qprocess_tests.exe'
    subprocess.run([*compiler_flags, str(smoke), str(directory / 'qprocessinfo.obj'),
                    f'/Fo{directory / "qprocess_tests.obj"}', f'/Fe{executable}', '/link',
                    f'/LIBPATH:{MSVC / "lib" / args.arch}',
                    f'/LIBPATH:{SDK / "Lib" / VERSION / "um" / args.arch}',
                    f'/LIBPATH:{SDK / "Lib" / VERSION / "ucrt" / args.arch}',
                    f'/LIBPATH:{QT / qt_arch / "lib"}', 'Qt5Core.lib', 'kernel32.lib'], check=True)
    environment = os.environ.copy()
    environment['PATH'] = str(QT / qt_arch / 'bin') + os.pathsep + environment['PATH']
    subprocess.run([str(executable)], env=environment, check=True, timeout=15)
    print(f'PASS Qt UI, metaobject compilation and live child process discovery ({args.arch})')
