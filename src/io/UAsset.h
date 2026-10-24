#pragma once
// Unreal Engine .uasset files.
//  - Textures (Texture2D, cube, arrays...; editor and cooked, UE 4.x to 5.8): read natively
//    (src/io/uasset, from CyImgView), Oodle loaded from an installed Unreal Engine when needed.
//  - Meshes (StaticMesh / SkeletalMesh, editor assets): exported to glTF by the matching Unreal
//    Editor (commandlet, temporary project with the asset's Content mounted read-only), cached.
#include "core/Common.h"

#include <functional>
#include <string>
#include <vector>

namespace sx {

struct UAssetInfo {
    std::string cls;     // class of the main asset ("Texture2D", "StaticMesh"...)
    std::string engine;  // engine version saved in the package ("5.3.2-...")
    bool texture = false, mesh = false;
};
bool ReadUAssetInfo(const std::string& path, UAssetInfo& out);

// Mip 0 as straight (non premultiplied) RGBA8.
bool LoadUAssetImage(const std::string& path, std::vector<u8>& rgba, u32& w, u32& h, std::string* error, std::string* format = nullptr);

// Glb of a mesh asset: the cached file when fresh (true), otherwise "" (convert first).
std::string UAssetMeshCached(const std::string& path);
// Unreal materials of a converted mesh (base color texture, roughness, metallic by material name).
struct MeshData;
void ApplyUAssetMaterials(const std::string& glb, MeshData& mesh);
// Converts with Unreal (blocking, minutes on the first run of an engine); `progress` receives status lines.
bool ConvertUAssetMesh(const std::string& path, std::string& glbOut, std::string* error, const std::function<void(const std::string&)>& progress = {});
// Every conversion running stops (Unreal ended): the application closes.
void CancelUAssetConversions();

}  // namespace sx
