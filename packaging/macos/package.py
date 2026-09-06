#!/usr/bin/env python3
"""Build an Apple Silicon macOS alpha; run from any working directory."""
import hashlib
from pathlib import Path
import plistlib
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'build/macos-deps'
BUILD = ROOT / 'build/macos-release'
DIST = ROOT / 'dist'
VERSION = '0.1.0'
NAME = 'AI PvP FPS'

def run(*args):
    subprocess.run([str(a) for a in args], check=True)

def main():
    WORK.mkdir(parents=True, exist_ok=True)
    archive = WORK / 'SDL2-2.32.10.tar.gz'
    if not archive.exists():
        run('curl', '-fL', 'https://www.libsdl.org/release/SDL2-2.32.10.tar.gz', '-o', archive)
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == '5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165', 'SDL archive checksum mismatch'
    source = WORK / 'SDL2-2.32.10'
    if not source.exists():
        run('tar', '-xzf', archive, '-C', WORK)
    prefix = WORK / 'install'
    run('cmake', '-S', source, '-B', WORK / 'sdl-build',
        '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0',
        '-DCMAKE_OSX_ARCHITECTURES=arm64', f'-DCMAKE_INSTALL_PREFIX={prefix}',
        '-DSDL_SHARED=ON', '-DSDL_STATIC=OFF', '-DSDL_TEST=OFF', '-DSDL_TESTS=OFF')
    run('cmake', '--build', WORK / 'sdl-build', '-j', '4')
    run('cmake', '--install', WORK / 'sdl-build')
    args = ['cmake', '-S', ROOT, '-B', BUILD, '-DCMAKE_BUILD_TYPE=Release',
            '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0',
            f'-DSDL2_DIR={prefix}/lib/cmake/SDL2']
    # Reuse the already fetched, pinned GLM source when available.
    glm = ROOT / 'build/_deps/glm-src'
    if glm.exists():
        args.append(f'-DFETCHCONTENT_SOURCE_DIR_GLM={glm}')
    run(*args)
    run('cmake', '--build', BUILD, '-j', '4')
    run('ctest', '--test-dir', BUILD, '--output-on-failure')
    stage = DIST / f'ai-pvp-fps-{VERSION}-macos-arm64'
    if stage.exists():
        shutil.rmtree(stage)
    app = stage / f'{NAME}.app'
    contents = app / 'Contents'
    resources = contents / 'Resources'
    frameworks = contents / 'Frameworks'
    for path in (resources, frameworks, contents / 'MacOS'):
        path.mkdir(parents=True, exist_ok=True)
    binary = contents / 'MacOS/game'
    shutil.copy2(BUILD / 'game', binary)
    for folder in ('shaders', 'textures', 'sounds'):
        shutil.copytree(ROOT / folder, resources / folder)
    # Repository attribution explicitly requires verification before redistribution.
    (resources / 'textures/ground.jpg').unlink(missing_ok=True)
    iconset = WORK / 'AppIcon.iconset'
    iconset.mkdir(exist_ok=True)
    for size in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            suffix = '@2x' if scale == 2 else ''
            run('sips', '-z', size * scale, size * scale, ROOT / 'packaging/macos/icon.png',
                '--out', iconset / f'icon_{size}x{size}{suffix}.png')
    run('iconutil', '-c', 'icns', iconset, '-o', resources / 'AppIcon.icns')
    info = dict(CFBundleExecutable='game', CFBundleName=NAME,
                CFBundleDisplayName=NAME, CFBundleIdentifier='com.taavisaft.aipvpfps',
                CFBundleVersion='1', CFBundleShortVersionString=VERSION,
                CFBundlePackageType='APPL', CFBundleIconFile='AppIcon',
                LSMinimumSystemVersion='12.0', NSHighResolutionCapable=True,
                NSLocalNetworkUsageDescription='Connect to FPS servers on your local network.')
    (contents / 'Info.plist').write_bytes(plistlib.dumps(info))
    dependencies = subprocess.check_output(['otool', '-L', str(binary)], text=True)
    sdl = next(line.strip().split(' (')[0] for line in dependencies.splitlines()[1:] if 'libSDL2' in line)
    lib = frameworks / Path(sdl).name
    shutil.copy2((prefix / 'lib' / Path(sdl).name).resolve(), lib)
    run('install_name_tool', '-change', sdl, f'@executable_path/../Frameworks/{lib.name}', binary)
    run('install_name_tool', '-id', f'@rpath/{lib.name}', lib)
    for item in (binary, lib):
        linked = subprocess.check_output(['otool', '-L', str(item)], text=True)
        assert '/opt/homebrew' not in linked and str(WORK) not in linked, linked
    licenses = resources / 'Licenses'
    licenses.mkdir()
    shutil.copy2(source / 'LICENSE.txt', licenses / 'SDL2.txt')
    stb = (ROOT / 'src/stb_image.h').read_text()
    (licenses / 'stb_image.txt').write_text(stb[stb.rfind('ALTERNATIVE A'):].removesuffix('*/\n'))
    shutil.copy2(BUILD / '_deps/glm-src/copying.txt' if not glm.exists() else glm / 'copying.txt', licenses / 'GLM.txt')
    shutil.copy2(ROOT / 'README.md', stage / 'CONTROLS.md')
    shutil.copy2(ROOT / 'packaging/macos/PLAYER-README.txt', stage / 'README.txt')
    revision = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
    (resources / 'build-info.txt').write_text(f'Version: {VERSION} alpha\nBase commit: {revision}\nArchitecture: arm64\nDeployment target: macOS 12.0\nMay include uncommitted packaging changes.\n')
    run('codesign', '--force', '--sign', '-', lib)
    run('codesign', '--force', '--sign', '-', app)
    run('codesign', '--verify', '--deep', '--strict', '--verbose=2', app)
    output = DIST / f'{stage.name}.zip'
    output.unlink(missing_ok=True)
    run('ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', stage, output)
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_suffix('.zip.sha256').write_text(f'{digest}  {output.name}\n')
    print(f'Packaged: {output}')

if __name__ == '__main__':
    main()
