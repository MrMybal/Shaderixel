#include "doc/DocFile.h"
#include "ai/Net.h"

namespace sx {

Json MeshToJson(const MeshData& m, bool embedGeometry) {
    Json j = Json::Object();
    j["name"] = m.name;
    if (embedGeometry) {
        j["vertices"] = net::Base64Encode(m.vertices.data(), m.vertices.size() * sizeof(MeshVertex));
        j["indices"] = net::Base64Encode(m.indices.data(), m.indices.size() * sizeof(u32));
    } else {
        j["binaryGeometry"] = true;
    }
    if (!m.partNames.empty() && m.triPart.size() == m.indices.size() / 3) {
        Json names = Json::Array();
        for (const auto& name : m.partNames) names.Push(name);
        j["partNames"] = std::move(names);
        if (embedGeometry) j["triPart"] = net::Base64Encode(m.triPart.data(), m.triPart.size() * sizeof(u32));
    }
    // Placement of the original model (exports in its own units).
    Json c = Json::Array();
    c.Push(m.srcCenter.x); c.Push(m.srcCenter.y); c.Push(m.srcCenter.z);
    j["srcCenter"] = c;
    j["srcScale"] = m.srcScale;
    // Materials of the parts (names, colors) with their triangles, and their UV tiles.
    if (!m.materials.empty() && m.triMaterial.size() * 3 == m.indices.size()) {
        Json mats = Json::Array();
        for (size_t i = 0; i < m.materials.size(); ++i) {
            const auto& mi = m.materials[i];
            Json o = Json::Object();
            o["name"] = mi.name;
            Json c = Json::Array();
            c.Push(mi.baseColor.x); c.Push(mi.baseColor.y); c.Push(mi.baseColor.z); c.Push(mi.baseColor.w);
            o["color"] = c;
            o["roughness"] = mi.roughness;
            o["metallic"] = mi.metallic;
            if (i < m.uvTiles.size()) {
                Json t = Json::Array();
                t.Push(m.uvTiles[i].x); t.Push(m.uvTiles[i].y); t.Push(m.uvTiles[i].z); t.Push(m.uvTiles[i].w);
                o["tile"] = t;
            }
            mats.Push(o);
        }
        j["materials"] = mats;
        if (embedGeometry) j["triMaterial"] = net::Base64Encode(m.triMaterial.data(), m.triMaterial.size() * sizeof(u16));
    }
    if (!m.udims.empty()) {
        Json u = Json::Array();
        for (u16 t : m.udims) u.Push((int)t);
        j["udims"] = u;
        j["udimCols"] = m.udimCols;
        for (u16 material : m.udimMaterials) j["udimMaterials"].Push((int)material);
        for (float scale : m.udimScale) j["udimScale"].Push(scale);
    }
    return j;
}

bool MeshFromJson(const Json& j, MeshData& m, SavedMesh* geometry) {
    if (j["binaryGeometry"].B()) {
        if (!geometry || geometry->vertices.empty() || geometry->indices.empty()) return false;
        m.vertices = std::move(geometry->vertices);
        m.indices = std::move(geometry->indices);
        m.triMaterial = std::move(geometry->triangleMaterials);
        m.triPart = std::move(geometry->triangleParts);
    } else {
        auto decode = [](const Json& field, auto& out) {
            const auto bytes = net::Base64Decode(field.S());
            using Element = typename std::decay_t<decltype(out)>::value_type;
            if (bytes.size() % sizeof(Element)) return false;
            out.resize(bytes.size() / sizeof(Element));
            if (!bytes.empty()) memcpy(out.data(), bytes.data(), bytes.size());
            return true;
        };
        if (!decode(j["vertices"], m.vertices) || !decode(j["indices"], m.indices) ||
            !decode(j["triMaterial"], m.triMaterial) || !decode(j["triPart"], m.triPart) ||
            m.vertices.empty() || m.indices.empty()) return false;
    }
    m.name = j["name"].S("Maillage");
    if (m.indices.size() % 3) return false;
    for (const Json& name : j["partNames"].Elements()) m.partNames.push_back(name.S());
    if (!m.triPart.empty() && (m.triPart.size() != m.indices.size() / 3 ||
        *std::max_element(m.triPart.begin(), m.triPart.end()) >= m.partNames.size())) return false;
    for (u32 ix : m.indices)
        if (ix >= m.vertices.size()) return false;  // corrupt or hand-edited file: would read out of bounds
    if (j["srcCenter"].Size() == 3) m.srcCenter = {j["srcCenter"].At(0).F(), j["srcCenter"].At(1).F(), j["srcCenter"].At(2).F()};
    m.srcScale = j["srcScale"].F(1.0f);
    if (j.Has("materials")) {
        if (m.triMaterial.size() == m.indices.size() / 3) {
            bool tiles = false;
            for (size_t i = 0; i < j["materials"].Size(); ++i) {
                const Json& o = j["materials"].At(i);
                MeshMaterialInfo mi;
                mi.name = o["name"].S("");
                if (o["color"].Size() == 4) mi.baseColor = {o["color"].At(0).F(), o["color"].At(1).F(), o["color"].At(2).F(), o["color"].At(3).F()};
                mi.roughness = o["roughness"].F(0.5f);
                mi.metallic = o["metallic"].F(0.0f);
                m.materials.push_back(mi);
                float4 t = {0, 0, 0, 0};
                if (o["tile"].Size() == 4) { t = {o["tile"].At(0).F(), o["tile"].At(1).F(), o["tile"].At(2).F(), o["tile"].At(3).F()}; tiles = true; }
                m.uvTiles.push_back(t);
            }
            if (!tiles) m.uvTiles.clear();
        }
    }
    for (auto& u : j["udims"].Elements()) m.udims.push_back((u16)std::clamp(u.I(1001), 1001, 2000));
    m.udimCols = m.udims.empty() ? 0 : std::max(1, j["udimCols"].I(1));
    if (!m.udims.empty() && (size_t)m.udimCols * m.udimCols < m.udims.size()) m.udims.clear(), m.udimCols = 0;  // inconsistent
    if (j.Has("udimMaterials")) {
        if (j["udimMaterials"].Size() != m.udims.size()) return false;
        for (const auto& value : j["udimMaterials"].Elements()) {
            const int material = value.I(-1);
            if (material < 0 || (size_t)material >= m.materials.size() || material > 65535) return false;
            m.udimMaterials.push_back((u16)material);
        }
    }
    if (j.Has("udimScale")) {
        if (j["udimScale"].Size() != m.udims.size()) return false;
        for (const auto& value : j["udimScale"].Elements()) {
            const float scale = value.F();
            if (!(scale > 0 && scale <= 1)) return false;
            m.udimScale.push_back(scale);
        }
    }
    m.boundsMin = {1e30f, 1e30f, 1e30f};
    m.boundsMax = {-1e30f, -1e30f, -1e30f};
    for (auto& p : m.vertices) {
        m.boundsMin = {std::min(m.boundsMin.x, p.pos.x), std::min(m.boundsMin.y, p.pos.y), std::min(m.boundsMin.z, p.pos.z)};
        m.boundsMax = {std::max(m.boundsMax.x, p.pos.x), std::max(m.boundsMax.y, p.pos.y), std::max(m.boundsMax.z, p.pos.z)};
    }
    return true;
}

}  // namespace sx
