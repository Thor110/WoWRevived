#!/usr/bin/env python3
"""Makes a WoWEngine release: out/WoWEngine-v<version>/ and out/WoWEngine-v<version>.zip.

    python make_release.py [--build] [--debug] [--build-dir DIR] [--seven-zip]

The version comes from project(WoWEngine VERSION ...) in CMakeLists.txt, the one place it is set.
The programs come from the Release build in out/build/x64-Release (out/build/release on Linux and macOS),
the folder Visual Studio's x64-Release preset builds into. If that build isn't there, or with --build, the
script builds it first with the CMake presets; on Windows that needs CMake and the Visual Studio compiler
on the PATH (Visual Studio's "x64 Native Tools Command Prompt"), otherwise build x64-Release in Visual Studio
and run this again.

  --build       build (or rebuild) before packaging
  --debug       package the Debug build instead (x64-Debug / debug)
  --build-dir   package the programs from this folder instead
  --seven-zip   also make a .7z with 7-Zip (7z or 7z.exe on the PATH, or in Program Files)
"""
import argparse, os, re, shutil, subprocess, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent      # the WoWEngine folder
REPO = ROOT.parent                                 # the WoWRevived repository, for its LICENSE
WINDOWS = os.name == 'nt'
MACOS = sys.platform == 'darwin'
EXE = '.exe' if WINDOWS else ''


def version():
    text = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
    match = re.search(r'project\s*\(\s*WoWEngine\s+VERSION\s+([0-9.]+)', text)
    if not match:
        sys.exit('Couldn\'t find "project(WoWEngine VERSION x.y.z" in CMakeLists.txt')
    return match.group(1)


def run(args):
    print('>', ' '.join(args))
    subprocess.run(args, cwd=ROOT, check=True)


def build(preset):
    if not shutil.which('cmake'):
        sys.exit('CMake isn\'t on the PATH. Build the ' + preset + ' configuration in Visual Studio (open the WoWEngine '
                 'folder, pick ' + preset + ', Build All), or run this from the "x64 Native Tools Command Prompt".')
    run(['cmake', '--preset', preset] + (['-DWOW_MACOS_BUNDLE=OFF'] if MACOS else []))
    run(['cmake', '--build', '--preset', preset])


def find_seven_zip():
    for name in ('7z', '7z.exe', '7za'):
        if shutil.which(name):
            return shutil.which(name)
    for folder in (os.environ.get('ProgramFiles', ''), os.environ.get('ProgramFiles(x86)', '')):
        candidate = Path(folder) / '7-Zip' / '7z.exe'
        if folder and candidate.exists():
            return str(candidate)
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--build', action='store_true')
    parser.add_argument('--debug', action='store_true')
    parser.add_argument('--build-dir')
    parser.add_argument('--seven-zip', action='store_true')
    args = parser.parse_args()

    ver = version()
    preset = ('x64-Debug' if args.debug else 'x64-Release') if WINDOWS else ('debug' if args.debug else 'release')
    build_dir = Path(args.build_dir) if args.build_dir else ROOT / 'out' / 'build' / preset
    print(f'WoWEngine v{ver}, {preset} build in {build_dir}')

    if MACOS and (build_dir / 'WoWEngine.app').exists() and not (build_dir / 'WoWEngine').exists():
        sys.exit('The build has the programs as .app bundles; a release is the plain programs in one folder. Configure '
                 'with -DWOW_MACOS_BUNDLE=OFF (cmake --preset release -DWOW_MACOS_BUNDLE=OFF), build, and run this again.')
    if args.build or not (build_dir / ('WoWEngine' + EXE)).exists():
        if args.build_dir:
            sys.exit(f'There\'s no WoWEngine{EXE} in {build_dir}')
        build(preset)

    name = f'WoWEngine-v{ver}' + ('-debug' if args.debug else '')
    out = ROOT / 'out' / name
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    files = [
        (build_dir / ('WoWEngine' + EXE), True),
        (build_dir / ('WoWLauncher' + EXE), True),
        (build_dir / 'launcher.bmp', True),
        (ROOT / 'README.md', True),
        # the players' and modders' guides (update 247): plain text, beside the programs
        (ROOT / 'readme' / 'README-OVERRIDE.txt', True),
        (ROOT / 'readme' / 'README-CUSTOM-CAMPAIGNS.txt', True),
        (ROOT / 'readme' / 'README-TEXTURE-PACKS.txt', True),
        (ROOT / 'readme' / 'README-SOUNDS.txt', True),
        # every setting with its description and default, nothing set (update 248): beside, not in place of, a
        # player's WoWRevived.ini, which WoWLauncher documents the same way
        (ROOT / 'readme' / 'WoWRevived.default.ini', True),
        (ROOT / 'readme' / 'THIRD-PARTY.txt', True),          # the licences of SDL3, stb_vorbis and cgltf
        (REPO / 'LICENSE', False),
    ]
    if MACOS:
        files.append((ROOT / 'readme' / 'README-MACOS.txt', True))   # opening unsigned programs, ffmpeg
    if WINDOWS:
        # the programs' debug information (update 258): a crash's lines in WoWEngine.log name the routines with it
        files.append((build_dir / 'WoWEngine.pdb', False))
        files.append((build_dir / 'WoWLauncher.pdb', False))
        files.append((build_dir / ('SDL3d.dll' if args.debug else 'SDL3.dll'), False))
        files.append((build_dir / 'SDL3.dll', False))
    if WINDOWS:
        # Discord's rich presence: the DLL and its licence (from the build folder, where the build puts them, or
        # straight from resources)
        for extra in ('discord-rpc.dll', 'discord-rpc-LICENSE.txt'):
            files.append((build_dir / extra if (build_dir / extra).exists() else ROOT / 'resources' / extra, False))
    else:
        # SDL3 built by the project sits in _deps; an installed SDL3 is found by the system instead
        for lib in sorted((build_dir / '_deps').glob('sdl3-build/libSDL3.so*')) + sorted((build_dir / '_deps').glob('sdl3-build/libSDL3*.dylib')):
            files.append((lib, False))

    missing = []
    for path, required in files:
        if path.exists():
            if path.is_symlink():
                target = out / path.name
                target.symlink_to(os.readlink(path))
            else:
                shutil.copy2(path, out / path.name)
            print('  ' + path.name)
        elif required:
            missing.append(str(path))
    if missing:
        sys.exit('Missing from the build: ' + ', '.join(missing))

    # WoWRevived's updated TEXT.ojd for each language, in "resources": WoWLauncher copies the one for the discs'
    # language in as the game's TEXT.ojd
    (out / 'resources').mkdir(exist_ok=True)
    for code in ('EN', 'FR', 'DE', 'IT', 'ES'):
        source = ROOT / 'resources' / f'TEXT-{code}.ojd'
        if source.exists():
            shutil.copy2(source, out / 'resources' / source.name)
            print('  resources/' + source.name)

    # the engine's loose files: into DAT and MAPS of the release folder, which is where the game is then installed
    # (WoWLauncher adds the game's own files beside them), so they are read as the game's loose files are
    for source, target in (('dat', 'DAT'), ('maps', 'MAPS'), ('sfx', 'SFX'), ('vox', 'VOX')):
        if (ROOT / source).is_dir():
            shutil.copytree(ROOT / source, out / target, dirs_exist_ok=True)
            print('  ' + target + '/')

    # the custom campaigns' examples (docs/CUSTOM.md, update 247): Campaigns in the main folder, beside Human.000
    if (ROOT / 'Campaigns').is_dir():
        shutil.copytree(ROOT / 'Campaigns', out / 'Campaigns', dirs_exist_ok=True)
        print('  Campaigns/')

    archive = ROOT / 'out' / (name + '.zip')
    if archive.exists():
        archive.unlink()
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for item in sorted(out.rglob('*')):
            z.write(item, Path(name) / item.relative_to(out))
    print(f'Made {out}')
    print(f'Made {archive} ({archive.stat().st_size // 1024} KB)')

    if args.seven_zip:
        seven = find_seven_zip()
        if not seven:
            print('7-Zip wasn\'t found, so there\'s no .7z')
        else:
            target = ROOT / 'out' / (name + '.7z')
            if target.exists():
                target.unlink()
            subprocess.run([seven, 'a', '-t7z', '-mx=9', str(target), name], cwd=ROOT / 'out', check=True)
            print(f'Made {target}')


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as e:
        sys.exit(f'Stopped: {" ".join(e.cmd)} failed')
