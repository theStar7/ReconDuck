"""Compile the actual manual-launch worker with real Qt and isolated core boundaries.

Pass --capture-source to test a saved older source and verify the regression fails.
No global registry writes or application injection are performed.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--capture-source', type=Path,
                    default=ROOT / 'qreconduck/Windows/Dialogs/CaptureDialog.cpp')
parser.add_argument('--main-source', type=Path,
                    default=ROOT / 'qreconduck/Windows/MainWindow.cpp')
args = parser.parse_args()
capture = args.capture_source.read_text(encoding='utf-8')
worker = capture[capture.index('struct ManualLaunchState'):capture.index('\nstatic QString GetDescription')]

def function(source, signature):
    begin = source.index(signature)
    brace = source.index('{', begin)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[begin:end] + '\n'

close = function(capture, 'bool CaptureDialog::checkAllowClose()')
close += function(args.main_source.read_text(encoding='utf-8'),
                  'void MainWindow::closeEvent(QCloseEvent *event)')
QT = ROOT / 'qreconduck/3rdparty/qt'
MSVC = Path(r'D:/Code/VS2026/VC/Tools/MSVC/14.44.35207')
SDK = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Windows Kits/10'
VERSION = sorted((SDK / 'Include').iterdir())[-1].name

with tempfile.TemporaryDirectory(prefix='manual-launch-lifecycle-') as directory:
    directory = Path(directory)
    (directory / 'manual_launch_worker.inc').write_text(worker, encoding='utf-8')
    (directory / 'manual_launch_close.inc').write_text(close, encoding='utf-8')
    includes = [directory, MSVC / 'include', QT / 'include', QT / 'include/QtCore', QT / 'include/QtGui']
    includes += [SDK / 'Include' / VERSION / part for part in ['um', 'shared', 'ucrt']]
    for arch, qt_arch in [('x64', 'x64'), ('x86', 'Win32')]:
        subprocess.run([str(QT / qt_arch / 'bin/moc.exe'),
                        str(HERE / 'manual_launch_close.cpp'),
                        '-o', str(directory / 'manual_launch_close.moc')], check=True)
        environment = os.environ.copy()
        environment['PATH'] = str(QT / qt_arch / 'bin') + os.pathsep + environment['PATH']
        for test in ['manual_launch_lifecycle', 'manual_launch_close']:
            exe = directory / f'{test}-{arch}.exe'
            command = [str(MSVC / f'bin/Hostx64/{arch}/cl.exe'), '/nologo', '/MD', '/std:c++14',
                       '/EHsc', '/W4', '/WX', '/DUNICODE', '/DQT_CORE_LIB', '/DQT_GUI_LIB',
                       *[f'/I{path}' for path in includes], str(HERE / (test + '.cpp')),
                       f'/Fo{directory / (test + arch + ".obj")}', f'/Fe{exe}', '/link',
                       f'/LIBPATH:{MSVC / "lib" / arch}',
                       f'/LIBPATH:{SDK / "Lib" / VERSION / "um" / arch}',
                       f'/LIBPATH:{SDK / "Lib" / VERSION / "ucrt" / arch}',
                       f'/LIBPATH:{QT / qt_arch / "lib"}', 'Qt5Core.lib', 'Qt5Gui.lib', 'kernel32.lib']
            subprocess.run(command, check=True)
            subprocess.run([str(exe)], env=environment, check=True, timeout=10)
            print(f'PASS actual {test} ({arch})')
