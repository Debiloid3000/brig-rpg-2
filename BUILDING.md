# Building this SDK

## Brig RPG 2 for Steam legacy (Windows x86)

The installable mod is `_folder/brig`. The repository contains assets; game DLLs
are compiled from this source tree and supplied in CI artifacts and Releases.
Use Visual Studio 2022 Desktop development with C++ (v143 toolset and Windows SDK),
Python 3.11 or newer, and a Developer PowerShell prompt:

```powershell
msbuild projects/vs2019/projects.sln -t:rebuild -p:Configuration=Release -p:Platform=Win32 -p:PlatformToolset=v143 -p:PostBuildEventUseInBuild=false -maxcpucount:2
python -m unittest discover -s tests -v
python tools/validate_brig.py
python tools/package_brig.py --client projects/vs2019/Release/hl_cdll/client.dll --server projects/vs2019/Release/hldll/hl.dll --version v1.0.0
```

The resulting `dist/brig-v1.0.0-windows-x86.zip` contains one `brig/` directory,
including `cl_dlls/client.dll`, `dlls/hl.dll` and build metadata. The neighboring
`.sha256` file checks the complete archive. No engine executable is bundled.
See [docs/brig-packaging.md](docs/brig-packaging.md) for asset policy and release
automation, and `_folder/brig/README_INSTALL.txt` for installation instructions.

For a local unpacked installation, copy `_folder/brig` beside Steam's `hl.exe`.
To make ordinary Visual Studio builds install DLLs into that exact mod folder, set the
explicit destination before building:

```powershell
$env:BRIG_GAME_DIR = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Half-Life\\brig"
msbuild projects/vs2019/projects.sln -t:rebuild -p:Configuration=Release -p:Platform=Win32 -p:PlatformToolset=v143
```

Without `BRIG_GAME_DIR`, automatic installation is skipped and DLLs remain in the
project output directories. Release packaging deliberately disables post-build
installation. Select `steam_legacy` in Half-Life's Steam beta settings and run
`hl.exe -game brig`.

The original SDK notes below describe general SDK development. The old
`halflife_updated/scripts/packager/CreatePackage.csx` referenced below is not the
Brig packager; use `tools/package_brig.py` for this repository.

## Requirements

### Windows

* Windows 7 or newer
* Visual Studio 2019 or newer

### Linux

* GCC 9 or newer (C++17 support or newer required)
* Packages:
    * `g++-multilib`
    * `libgl1-mesa-dev` (or an equivalent package that provides the OpenGL headers)

## Building

The TWHL tutorial on setting up the Half-Life SDK for C++ mod development explains how to set up the source code and how to configure the mod installation: https://twhl.info/wiki/page/Half-Life_Programming_-_Getting_Started

## Packaging mod files

To package a mod for distribution as an archive, use the `CreatePackage` script:

```
cd path/to/Half-Life
dotnet script halflife_updated/scripts/packager/CreatePackage.csx --package-name <name of archive file excluding file extension>
```
