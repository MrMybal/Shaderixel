# Shaderixel

GPU image editor with a Photoshop-style **layer** workflow and a **node graph**
workflow. Effects are written in HLSL/Slang and compiled at runtime.

## Interface language

The interface defaults to **English** on a fresh installation. Select another
language from **View > Language**; existing language preferences are preserved.
Interface dictionaries are loaded from `lang/` next to the executable, or from
`library/lang/` when running a development build. An unavailable saved language
falls back to English.

## Build (Windows, Visual Studio 2022)

```bat
build.bat            (Release)
build.bat Debug
```

The Release executables are generated in `build/Release/`: `Shaderixel.exe`
(D3D12) and, when the Vulkan headers are available, `ShaderixelVk.exe` (Vulkan).
Use `build_target.bat <target>` to build a single target. The build copies the
interface dictionaries into `lang/` alongside the executables.

## Dependencies (`external/`, not tracked by Git)

- Dear ImGui, docking branch: `git clone -b docking https://github.com/ocornut/imgui external/imgui`
- stb: `git clone https://github.com/nothings/stb external/stb`
- xatlas (automatic UV unwrapping, MIT): `git clone --depth 1 https://github.com/jpcy/xatlas external/xatlas`
- ufbx (FBX loading, MIT): `git clone --depth 1 https://github.com/ufbx/ufbx external/ufbx`
- LibRaw (camera RAW decoding, LGPL 2.1 / CDDL 1.0, optional; WIC is used when unavailable): `git clone --depth 1 --branch 0.22.2 https://github.com/LibRaw/LibRaw external/LibRaw`
- Lens profiles: the [lensfun](https://github.com/lensfun/lensfun) database (`data/db`, CC BY-SA 3.0) is not bundled. Place its XML files in `%LOCALAPPDATA%\Shaderixel\lensfun` or select a folder in the application. Installed darktable and RawTherapee databases are also detected.
- Unreal textures (`.uasset`) are read by `src/io/uasset`, adapted from CyImgView by the same author. Meshes stored in `.uasset` files are exported through an installed Unreal Editor using a commandlet and glTF.
- Slang: extract the Windows x64 release into `external/slang`.
- DXC (`dxcompiler.dll` / `dxil.dll`): copied automatically from the Windows SDK.
- Vulkan (optional): `git clone --depth 1 https://github.com/KhronosGroup/Vulkan-Headers external/vulkan-headers`
  and `git clone --depth 1 https://github.com/zeux/volk external/volk`. No Vulkan SDK is required; a compatible driver is needed at runtime.

See also [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md),
[docs/ROADMAP.md](docs/ROADMAP.md) and the
[Unreal Engine plugin README](unreal/ShaderixelLink/README.md).
