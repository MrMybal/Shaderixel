# Shaderixel

Éditeur d'images entièrement GPU, avec un mode **calques** (façon Photoshop) et un
mode **nodal**. Les effets s'écrivent en HLSL/Slang et sont compilés à chaud.

## Compiler (Windows, VS 2022)

```
build.bat            (Release)
build.bat Debug
```

Les binaires sont générés dans `build/Release/` : `Shaderixel.exe` (D3D12) et, si les
en-têtes Vulkan sont présents, `ShaderixelVk.exe` (Vulkan). `build_target.bat <cible>` compile une seule cible.

## Dépendances (`external/`, non versionné)

- Dear ImGui, branche docking : `git clone -b docking https://github.com/ocornut/imgui external/imgui`
- stb : `git clone https://github.com/nothings/stb external/stb`
- Slang (release Windows x64), à décompresser dans `external/slang`
- DXC (`dxcompiler.dll` / `dxil.dll`) : copié automatiquement depuis le Windows SDK
- Vulkan (optionnel) : `git clone --depth 1 https://github.com/KhronosGroup/Vulkan-Headers external/vulkan-headers`
  et `git clone --depth 1 https://github.com/zeux/volk external/volk` (aucun SDK requis, seul le pilote à l'exécution)

Voir aussi [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) et [docs/ROADMAP.md](docs/ROADMAP.md).
