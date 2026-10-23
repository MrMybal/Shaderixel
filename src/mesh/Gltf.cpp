// glTF 2.0 loader (.gltf with external/base64 buffers, and binary .glb).
// Flattens the default scene: every triangle primitive is transformed by its
// node's world matrix into one mesh (positions, normals, first UV set).
// Converted to the engine's left-handed space like the OBJ loader (z flip +
// reversed winding). glTF UVs already have v pointing down.
#include "mesh/Mesh.h"
#include "core/Json.h"
#include "ai/Net.h"  // Base64

#include <cstring>
#include <fstream>
#include <functional>

namespace sx {

namespace {
struct M4 {
    float m[16];  // column-major like glTF
    static M4 Identity() { M4 r{}; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; return r; }
    M4 operator*(const M4& b) const {
        M4 r{};
        for (int c = 0; c < 4; ++c)
            for (int rr = 0; rr < 4; ++rr) {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += m[k * 4 + rr] * b.m[c * 4 + k];
                r.m[c * 4 + rr] = s;
            }
        return r;
    }
    float3 Point(float3 p) const { return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12], m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13], m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]}; }
    float3 Dir(float3 p) const { return {m[0] * p.x + m[4] * p.y + m[8] * p.z, m[1] * p.x + m[5] * p.y + m[9] * p.z, m[2] * p.x + m[6] * p.y + m[10] * p.z}; }
    // Determinant of the upper 3x3 (negative: mirrored transform, triangle winding flips).
    float Det3() const {
        return m[0] * (m[5] * m[10] - m[9] * m[6]) - m[4] * (m[1] * m[10] - m[9] * m[2]) + m[8] * (m[1] * m[6] - m[5] * m[2]);
    }
    // Normal transformed by the inverse-transpose of the upper 3x3 (cofactor matrix, sign of the
    // determinant restored), normalized: right under non-uniform scale and mirroring.
    float3 Normal(float3 n) const {
        const float a = m[0], b = m[4], c = m[8], d = m[1], e = m[5], f = m[9], g = m[2], h = m[6], k = m[10];  // rows
        float3 r = {(e * k - f * h) * n.x + (f * g - d * k) * n.y + (d * h - e * g) * n.z,
                    (c * h - b * k) * n.x + (a * k - c * g) * n.y + (b * g - a * h) * n.z,
                    (b * f - c * e) * n.x + (c * d - a * f) * n.y + (a * e - b * d) * n.z};
        if (Det3() < 0) r = {-r.x, -r.y, -r.z};
        return Normalize(r);
    }
};

M4 NodeMatrix(const Json& n) {
    if (n.Has("matrix") && n["matrix"].Size() == 16) {
        M4 r;
        for (int i = 0; i < 16; ++i) r.m[i] = n["matrix"].At(i).F();
        return r;
    }
    float t[3] = {0, 0, 0}, q[4] = {0, 0, 0, 1}, s[3] = {1, 1, 1};
    if (n.Has("translation")) for (int i = 0; i < 3; ++i) t[i] = n["translation"].At(i).F();
    if (n.Has("rotation")) for (int i = 0; i < 4; ++i) q[i] = n["rotation"].At(i).F();
    if (n.Has("scale")) for (int i = 0; i < 3; ++i) s[i] = n["scale"].At(i).F();
    float x = q[0], y = q[1], z = q[2], w = q[3];
    M4 r = M4::Identity();
    r.m[0] = (1 - 2 * (y * y + z * z)) * s[0]; r.m[1] = (2 * (x * y + z * w)) * s[0]; r.m[2] = (2 * (x * z - y * w)) * s[0];
    r.m[4] = (2 * (x * y - z * w)) * s[1]; r.m[5] = (1 - 2 * (x * x + z * z)) * s[1]; r.m[6] = (2 * (y * z + x * w)) * s[1];
    r.m[8] = (2 * (x * z + y * w)) * s[2]; r.m[9] = (2 * (y * z - x * w)) * s[2]; r.m[10] = (1 - 2 * (x * x + y * y)) * s[2];
    r.m[12] = t[0]; r.m[13] = t[1]; r.m[14] = t[2];
    return r;
}

struct Gltf {
    Json doc;
    std::vector<std::vector<u8>> buffers;
    std::string error;

    // Reads accessor `index` as floats (normalized ints mapped to 0..1) or as u32 indices.
    bool Read(int index, int wantComponents, std::vector<float>* f, std::vector<u32>* u) {
        const Json& acc = doc["accessors"].At(index);
        int count = acc["count"].I();
        int ctype = acc["componentType"].I();
        std::string type = acc["type"].S("SCALAR");
        int comps = type == "SCALAR" ? 1 : type == "VEC2" ? 2 : type == "VEC3" ? 3 : type == "VEC4" ? 4 : 0;
        if (!comps || comps < wantComponents) { error = "accesseur de type inattendu"; return false; }
        int csize = ctype == 5126 || ctype == 5125 ? 4 : ctype == 5123 || ctype == 5122 ? 2 : 1;
        if (!acc.Has("bufferView")) {  // all zeros (sparse accessors are not supported)
            if (f) f->assign((size_t)count * wantComponents, 0.0f);
            if (u) u->assign(count, 0);
            return true;
        }
        const Json& bv = doc["bufferViews"].At(acc["bufferView"].I());
        int buf = bv["buffer"].I();
        if (buf < 0 || buf >= (int)buffers.size()) { error = "tampon manquant"; return false; }
        size_t base = (size_t)bv["byteOffset"].I() + (size_t)acc["byteOffset"].I();
        size_t stride = bv.Has("byteStride") ? (size_t)bv["byteStride"].I() : (size_t)comps * csize;
        const std::vector<u8>& b = buffers[buf];
        if (count > 0 && base + stride * (count - 1) + (size_t)comps * csize > b.size()) { error = "accesseur hors du tampon"; return false; }
        bool normalized = acc["normalized"].B(false);
        for (int i = 0; i < count; ++i) {
            const u8* p = &b[base + stride * i];
            for (int c = 0; c < (f ? wantComponents : 1); ++c) {
                const u8* q = p + (size_t)c * csize;
                double v = 0;
                switch (ctype) {
                    case 5126: { float x; memcpy(&x, q, 4); v = x; break; }
                    case 5125: { u32 x; memcpy(&x, q, 4); v = x; break; }
                    case 5123: { u16 x; memcpy(&x, q, 2); v = normalized ? x / 65535.0 : x; break; }
                    case 5122: { i16 x; memcpy(&x, q, 2); v = normalized ? std::max(x / 32767.0, -1.0) : x; break; }
                    case 5121: { v = normalized ? q[0] / 255.0 : q[0]; break; }
                    case 5120: { i8 x = (i8)q[0]; v = normalized ? std::max(x / 127.0, -1.0) : x; break; }
                    default: error = "type de composante inconnu"; return false;
                }
                if (f) f->push_back((float)v);
                else u->push_back((u32)v);
            }
        }
        return true;
    }
};
}  // namespace

bool LoadGltf(const std::string& path, MeshData& out, std::string* error) {
    auto fail = [&](const std::string& e) { if (error) *error = e; return false; };
    std::ifstream f(Utf8Path(path), std::ios::binary);
    if (!f) return fail("fichier introuvable");
    std::vector<u8> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    Gltf g;
    std::string jsonText;
    std::vector<u8> glbBin;
    if (bytes.size() >= 12 && memcmp(bytes.data(), "glTF", 4) == 0) {
        size_t pos = 12;
        while (pos + 8 <= bytes.size()) {
            u32 len, type;
            memcpy(&len, &bytes[pos], 4);
            memcpy(&type, &bytes[pos + 4], 4);
            if (pos + 8 + len > bytes.size()) return fail("GLB tronqué");
            if (type == 0x4E4F534A) jsonText.assign((const char*)&bytes[pos + 8], len);        // JSON
            else if (type == 0x004E4942) glbBin.assign(bytes.begin() + pos + 8, bytes.begin() + pos + 8 + len);  // BIN
            pos += 8 + ((len + 3) & ~3u);
        }
    } else {
        jsonText.assign(bytes.begin(), bytes.end());
    }
    std::string perr;
    if (!Json::Parse(jsonText, g.doc, &perr)) return fail("JSON glTF invalide : " + perr);
    auto dir = Utf8Path(path).parent_path();
    for (auto& b : g.doc["buffers"].Elements()) {
        std::string uri = b["uri"].S("");
        if (uri.empty()) { g.buffers.push_back(glbBin); continue; }
        if (uri.rfind("data:", 0) == 0) {
            size_t comma = uri.find(',');
            g.buffers.push_back(net::Base64Decode(comma == std::string::npos ? "" : uri.substr(comma + 1)));
            continue;
        }
        std::ifstream bf(dir / Utf8Path(uri), std::ios::binary);
        if (!bf) return fail("tampon introuvable : " + uri);
        g.buffers.emplace_back((std::istreambuf_iterator<char>(bf)), std::istreambuf_iterator<char>());
    }

    out = MeshData{};
    out.name = Narrow(Utf8Path(path).stem().wstring());
    for (auto& m : g.doc["materials"].Elements()) {
        MeshMaterialInfo info;
        info.name = m["name"].S("");
        const Json& pbr = m["pbrMetallicRoughness"];
        if (pbr["baseColorFactor"].Size() == 4)
            info.baseColor = {pbr["baseColorFactor"].At(0).F(), pbr["baseColorFactor"].At(1).F(), pbr["baseColorFactor"].At(2).F(), 1.0f};
        info.roughness = pbr["roughnessFactor"].F(1.0f);
        info.metallic = pbr["metallicFactor"].F(1.0f);
        if (m["emissiveFactor"].Size() == 3) info.emissive = {m["emissiveFactor"].At(0).F(), m["emissiveFactor"].At(1).F(), m["emissiveFactor"].At(2).F()};
        if (pbr.Has("baseColorTexture")) {
            int ti = pbr["baseColorTexture"]["index"].I(-1);
            int si = ti >= 0 && (size_t)ti < g.doc["textures"].Size() ? g.doc["textures"].At(ti)["source"].I(-1) : -1;
            if (si >= 0 && (size_t)si < g.doc["images"].Size()) {
                const Json& img = g.doc["images"].At(si);
                std::string uri = img["uri"].S("");
                if (uri.rfind("data:", 0) == 0) {
                    size_t comma = uri.find(',');
                    info.baseColorData = net::Base64Decode(comma == std::string::npos ? "" : uri.substr(comma + 1));
                } else if (!uri.empty()) {
                    info.baseColorPath = Narrow((dir / Utf8Path(uri)).wstring());
                } else if (img.Has("bufferView")) {
                    const Json& bv = g.doc["bufferViews"].At(img["bufferView"].I());
                    size_t b = (size_t)bv["buffer"].I(), off = (size_t)bv["byteOffset"].I(0), len = (size_t)bv["byteLength"].I(0);
                    if (b < g.buffers.size() && off + len <= g.buffers[b].size())
                        info.baseColorData.assign(g.buffers[b].begin() + off, g.buffers[b].begin() + off + len);
                }
            }
        }
        out.materials.push_back(info);
    }
    bool hasUv = false;
    std::function<void(int, const M4&, int)> visit = [&](int ni, const M4& parent, int depth) {
        if (depth > 64) return;  // malformed file with a cycle in the node hierarchy
        const Json& node = g.doc["nodes"].At(ni);
        M4 world = parent * NodeMatrix(node);
        const bool mirrored = world.Det3() < 0;  // the transform already reverses the winding
        if (node.Has("mesh")) {
            const Json& mesh = g.doc["meshes"].At(node["mesh"].I());
            for (auto& prim : mesh["primitives"].Elements()) {
                if (prim["mode"].I(4) != 4) continue;  // triangles only
                const Json& at = prim["attributes"];
                if (!at.Has("POSITION")) continue;
                std::vector<float> P, N, T;
                std::vector<u32> I;
                if (!g.Read(at["POSITION"].I(), 3, &P, nullptr)) return;
                if (at.Has("NORMAL") && !g.Read(at["NORMAL"].I(), 3, &N, nullptr)) return;
                if (at.Has("TEXCOORD_0")) { if (!g.Read(at["TEXCOORD_0"].I(), 2, &T, nullptr)) return; hasUv = true; }
                size_t nv = P.size() / 3;
                if (N.size() / 3 < nv) N.clear();  // attributes shorter than POSITION: ignored
                if (T.size() / 2 < nv) T.clear();
                if (prim.Has("indices")) { if (!g.Read(prim["indices"].I(), 1, nullptr, &I)) return; }
                else for (u32 i = 0; i < nv; ++i) I.push_back(i);
                u32 base = (u32)out.vertices.size();
                for (size_t i = 0; i < nv; ++i) {
                    MeshVertex v;
                    float3 p = world.Point({P[i * 3], P[i * 3 + 1], P[i * 3 + 2]});
                    v.pos = {p.x, p.y, -p.z};
                    if (!N.empty()) {
                        float3 n = world.Normal({N[i * 3], N[i * 3 + 1], N[i * 3 + 2]});
                        v.normal = {n.x, n.y, -n.z};
                    }
                    if (!T.empty()) v.uv = {T[i * 2], T[i * 2 + 1]};
                    out.vertices.push_back(v);
                }
                const int mat = std::clamp(prim["material"].I(0), 0, std::max((int)out.materials.size() - 1, 0));
                // Reversed with the z flip, unless a mirrored node transform reversed it already.
                const size_t o1 = mirrored ? 1 : 2, o2 = mirrored ? 2 : 1;
                for (size_t i = 0; i + 2 < I.size(); i += 3) {
                    if (I[i] >= nv || I[i + 1] >= nv || I[i + 2] >= nv) continue;
                    out.indices.push_back(base + I[i]);
                    out.indices.push_back(base + I[i + o1]);
                    out.indices.push_back(base + I[i + o2]);
                    out.triMaterial.push_back((u16)mat);
                }
                if (N.empty()) {  // flat-ish normals from faces
                    for (size_t i = 0; i + 2 < I.size(); i += 3) {
                        if (I[i] >= nv || I[i + 1] >= nv || I[i + 2] >= nv) continue;
                        MeshVertex &a = out.vertices[base + I[i]], &b = out.vertices[base + I[i + o1]], &c = out.vertices[base + I[i + o2]];
                        float3 n = Cross(b.pos - a.pos, c.pos - a.pos);
                        a.normal = a.normal + n; b.normal = b.normal + n; c.normal = c.normal + n;
                    }
                    for (size_t i = base; i < out.vertices.size(); ++i) out.vertices[i].normal = Normalize(out.vertices[i].normal);
                }
            }
        }
        for (auto& c : node["children"].Elements()) visit(c.I(), world, depth + 1);
    };
    int scene = g.doc["scene"].I(0);
    if (g.doc["scenes"].Size() > (size_t)scene) {
        for (auto& n : g.doc["scenes"].At(scene)["nodes"].Elements()) visit(n.I(), M4::Identity(), 0);
    } else {
        for (size_t i = 0; i < g.doc["nodes"].Size(); ++i) visit((int)i, M4::Identity(), 0);
    }
    if (!g.error.empty()) return fail(g.error);
    if (out.indices.empty()) return fail("aucun triangle");
    out.hasUvs = hasUv;  // without UVs: loaded anyway, the UV tool (xatlas) unwraps it
    if (out.materials.empty()) out.triMaterial.clear();
    NormalizeMesh(out);
    return true;
}

bool LoadMesh(const std::string& path, MeshData& out, std::string* error) {
    std::string ext = Narrow(Utf8Path(path).extension().wstring());
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    if (ext == ".gltf" || ext == ".glb") return LoadGltf(path, out, error);
    if (ext == ".fbx") return LoadFbx(path, out, error);
    return LoadObj(path, out, error);
}

}  // namespace sx
