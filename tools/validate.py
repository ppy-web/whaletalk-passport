#!/usr/bin/env python3
"""Build and verify the WhaleTalk Passport app on Windows, Linux or macOS. Never flashes."""
import argparse
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def run(*args, cwd=ROOT):
    subprocess.run([str(arg) for arg in args], cwd=cwd, check=True)


def host_tests():
    run(sys.executable, ROOT / 'tools/generate_online_font.py', '--check')
    compiler = os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc')
    if compiler:
        command = [compiler]
    else:
        zig = shutil.which('zig')
        if not zig:
            raise RuntimeError('Host C compiler required: set CC or add zig to PATH')
        command = [zig, 'cc']
    build = ROOT / 'build/host'
    build.mkdir(parents=True, exist_ok=True)
    executable = build / ('state-test.exe' if os.name == 'nt' else 'state-test')
    run(*command, '-std=c11', '-Wall', '-Wextra', '-Werror', '-Imain',
        'tests/test_online_state.c', 'main/online_state.c', '-o', executable)
    run(executable)
    cue_test = build / ('cue-test.exe' if os.name == 'nt' else 'cue-test')
    run(*command, '-std=c11', '-Wall', '-Wextra', '-Werror', '-Imain',
        'tests/test_online_cue.c', 'main/online_cue.c', '-o', cue_test)
    run(cue_test)
    theme_test = build / ('theme-test.exe' if os.name == 'nt' else 'theme-test')
    run(*command, '-std=c11', '-Wall', '-Wextra', '-Werror', '-Itests/theme_stubs', '-Imain',
        'tests/test_online_theme.c', 'main/online_theme.c', 'main/online_state.c', '-o', theme_test)
    run(theme_test)
    background_test = build / ('background-test.exe' if os.name == 'nt' else 'background-test')
    run(*command, '-std=c11', '-Wall', '-Wextra', '-Werror', '-Itests/theme_stubs', '-Imain',
        'tests/test_online_background.c', 'main/online_background.c', 'main/online_background_format.c',
        'main/online_state.c', '-o', background_test)
    run(background_test)
    scan_test = build / ('setup-scan-test.exe' if os.name == 'nt' else 'setup-scan-test')
    run(*command, '-std=c11', '-Wall', '-Wextra', '-Werror', '-Itests/setup_stubs', '-Imain',
        'tests/test_online_setup_scan.c', 'main/online_setup_scan.c', '-o', scan_test)
    run(scan_test)
    print('Host tests: PASS', flush=True)


def firmware():
    idf = Path(os.environ['IDF_PATH']) / 'tools/idf.py'
    version = subprocess.check_output([sys.executable, str(idf), '--version'], text=True)
    if version.strip() != 'ESP-IDF v5.5.3':
        raise RuntimeError(f'Expected ESP-IDF v5.5.3, got {version.strip()}')
    build = ROOT / 'build/firmware-build'
    config = build / 'sdkconfig'
    common = [sys.executable, idf, '-B', build, '-D', f'SDKCONFIG={config}']
    run(*common, 'reconfigure', 'build')
    image = build / 'WhaleTalk-Passport-full.bin'
    run(*common, 'merge-bin', '-o', image)
    run(sys.executable, ROOT / 'tools/verify_firmware.py', build)
    run(sys.executable, ROOT / 'tools/archive_firmware.py', 'create', build)
    shutil.copyfile(image, ROOT / 'build/WhaleTalk-Passport-full.bin')
    print('Build: PASS; build/WhaleTalk-Passport-full.bin (flash at 0x0)', flush=True)


def preview():
    build = ROOT / 'build/ui'
    compiler = []
    if not os.environ.get('CC') and not shutil.which('cc') and shutil.which('zig'):
        zig = shutil.which('zig')
        build.mkdir(parents=True, exist_ok=True)
        wrappers = []
        for command in ('ar', 'ranlib'):
            wrapper = build / f'zig-{command}{".cmd" if os.name == "nt" else ".sh"}'
            text = f'@"{zig}" {command} %*\n' if os.name == 'nt' else f'#!/bin/sh\nexec {shlex.quote(zig)} {command} "$@"\n'
            wrapper.write_text(text, encoding='utf-8')
            if os.name != 'nt':
                wrapper.chmod(0o755)
            wrappers.append(wrapper)
        compiler = [f'-DCMAKE_C_COMPILER={zig}', '-DCMAKE_C_COMPILER_ARG1=cc',
                    f'-DCMAKE_CXX_COMPILER={zig}', '-DCMAKE_CXX_COMPILER_ARG1=c++',
                    f'-DCMAKE_ASM_COMPILER={zig}', '-DCMAKE_ASM_COMPILER_ARG1=cc',
                    f'-DCMAKE_AR={wrappers[0]}', f'-DCMAKE_RANLIB={wrappers[1]}']
    run('cmake', '-S', ROOT / 'tests/online_ui', '-B', build, '-G', 'Ninja', *compiler)
    run('cmake', '--build', build, '--parallel', '4')
    output = ROOT / 'build/preview'
    output.mkdir(parents=True, exist_ok=True)
    run(build / ('preview.exe' if os.name == 'nt' else 'preview'), cwd=output)
    print('LVGL preview: PASS (synthetic states, not device captures)', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--static', action='store_true')
    parser.add_argument('--firmware', action='store_true')
    parser.add_argument('--preview', action='store_true')
    args = parser.parse_args()
    try:
        if args.preview:
            preview()
        else:
            if not args.firmware:
                host_tests()
            if not args.static:
                firmware()
    except (OSError, KeyError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f'Validation failed: {error}', file=sys.stderr)
        sys.exit(1)
