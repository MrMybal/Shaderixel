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
- xatlas (dépliage UV automatique, MIT) : `git clone --depth 1 https://github.com/jpcy/xatlas external/xatlas`
- ufbx (lecture FBX, MIT) : `git clone --depth 1 https://github.com/ufbx/ufbx external/ufbx`
- LibRaw (décodage de tous les formats RAW d'appareils photo, LGPL 2.1 / CDDL 1.0, optionnel : sans lui, les RAW passent par WIC) : `git clone --depth 1 --branch 0.22.2 https://github.com/LibRaw/LibRaw external/LibRaw`
- Profils d'objectifs : la base [lensfun](https://github.com/lensfun/lensfun) (dossier `data/db`, CC BY-SA 3.0) n'est pas livrée ; l'utilisateur place ses fichiers XML dans `%LOCALAPPDATA%\Shaderixel\lensfun` (ou choisit un dossier ; une installation darktable / RawTherapee est aussi détectée).
- Les textures Unreal (.uasset) sont lues par `src/io/uasset` (repris de CyImgView, même auteur) ; les maillages .uasset sont exportés par l'Unreal Editor installé (commandlet, glTF).
- Slang (release Windows x64), à décompresser dans `external/slang`
- DXC (`dxcompiler.dll` / `dxil.dll`) : copié automatiquement depuis le Windows SDK
- Vulkan (optionnel) : `git clone --depth 1 https://github.com/KhronosGroup/Vulkan-Headers external/vulkan-headers`
  et `git clone --depth 1 https://github.com/zeux/volk external/volk` (aucun SDK requis, seul le pilote à l'exécution)

Voir aussi [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) et [docs/ROADMAP.md](docs/ROADMAP.md).
