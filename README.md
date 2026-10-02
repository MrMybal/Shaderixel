# Shaderixel

Éditeur d'images entièrement GPU, avec un mode **calques** (façon Photoshop) et un
mode **nodal**. Les effets s'écrivent en HLSL/Slang et sont compilés à chaud.

## Compiler (Windows, VS 2022)

```
build.bat            (Release)
build.bat Debug
```

Le binaire est généré dans `build/Release/Shaderixel.exe`.

## Dépendances (`external/`, non versionné)

- Dear ImGui, branche docking : `git clone -b docking https://github.com/ocornut/imgui external/imgui`
- stb : `git clone https://github.com/nothings/stb external/stb`
- Slang (release Windows x64), à décompresser dans `external/slang`
- DXC (`dxcompiler.dll` / `dxil.dll`) : copié automatiquement depuis le Windows SDK

Voir aussi [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) et [docs/ROADMAP.md](docs/ROADMAP.md).
