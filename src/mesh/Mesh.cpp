#include "mesh/Mesh.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

namespace sx {

Mat4 Mat4::Identity() {
    Mat4 r;
    for (int i = 0; i < 4; ++i) r.m[i][i] = 1;
    return r;
}

Mat4 Mat4::LookAtLH(float3 eye, float3 at, float3 up) {
    float3 z = Normalize(at - eye);
    float3 x = Normalize(Cross(up, z));
    float3 y = Cross(z, x);
    Mat4 r = Identity();
    r.m[0][0] = x.x; r.m[0][1] = x.y; r.m[0][2] = x.z; r.m[0][3] = -Dot(x, eye);
    r.m[1][0] = y.x; r.m[1][1] = y.y; r.m[1][2] = y.z; r.m[1][3] = -Dot(y, eye);
    r.m[2][0] = z.x; r.m[2][1] = z.y; r.m[2][2] = z.z; r.m[2][3] = -Dot(z, eye);
    return r;
}

Mat4 Mat4::PerspectiveLH(float fovY, float aspect, float zn, float zf) {
    float f = 1.0f / std::tan(fovY * 0.5f);
    Mat4 r;
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = zf / (zf - zn);
    r.m[2][3] = -zn * zf / (zf - zn);
    r.m[3][2] = 1;
    return r;
}

Mat4 Mat4::operator*(const Mat4& b) const {
    Mat4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k) r.m[i][j] += m[i][k] * b.m[k][j];
    return r;
}

float4 Mat4::Transform(float4 v) const {
    float4 r;
    for (int i = 0; i < 4; ++i) r[i] = m[i][0] * v.x + m[i][1] * v.y + m[i][2] * v.z + m[i][3] * v.w;
    return r;
}

static void ComputeBounds(MeshData& m) {
    m.boundsMin = {1e30f, 1e30f, 1e30f};
    m.boundsMax = {-1e30f, -1e30f, -1e30f};
    for (auto& v : m.vertices) {
        m.boundsMin = {std::min(m.boundsMin.x, v.pos.x), std::min(m.boundsMin.y, v.pos.y), std::min(m.boundsMin.z, v.pos.z)};
        m.boundsMax = {std::max(m.boundsMax.x, v.pos.x), std::max(m.boundsMax.y, v.pos.y), std::max(m.boundsMax.z, v.pos.z)};
    }
}

void NormalizeMesh(MeshData& m) {
    ComputeBounds(m);
    float3 c = (m.boundsMin + m.boundsMax) * 0.5f;
    float r = 0;
    for (auto& v : m.vertices) r = std::max(r, Length(v.pos - c));
    float s = r > 0 ? 1.0f / r : 1.0f;
    for (auto& v : m.vertices) v.pos = (v.pos - c) * s;
    // Combined with an earlier normalization (meshes normalized twice keep their first origin).
    m.srcCenter = m.srcCenter + c * (1.0f / m.srcScale);
    m.srcScale *= s;
    ComputeBounds(m);
}

bool SaveObj(const std::string& path, const MeshData& m, bool original, std::string* error) {
    std::ofstream f(Utf8Path(path), std::ios::binary);
    if (!f) { if (error) *error = "écriture impossible : " + path; return false; }
    f << "# Shaderixel: " << m.name << " (Y up" << (original ? ", original units" : ", normalized") << ")\n";
    f << "o " << (m.name.empty() ? std::string("Mesh") : m.name) << "\n";
    char b[128];
    const float k = original && m.srcScale > 0 ? 1.0f / m.srcScale : 1.0f;
    for (auto& v : m.vertices) {
        float3 p = original ? v.pos * k + m.srcCenter : v.pos;
        snprintf(b, sizeof(b), "v %.6f %.6f %.6f\n", p.x, p.y, -p.z);  // back to right-handed, like LoadObj reads it
        f << b;
    }
    for (auto& v : m.vertices) {
        snprintf(b, sizeof(b), "vt %.6f %.6f\n", v.uv.x, 1.0f - v.uv.y);
        f << b;
    }
    for (auto& v : m.vertices) {
        snprintf(b, sizeof(b), "vn %.5f %.5f %.5f\n", v.normal.x, v.normal.y, -v.normal.z);
        f << b;
    }
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        const u32 a = m.indices[i] + 1, c = m.indices[i + 1] + 1, d = m.indices[i + 2] + 1;
        snprintf(b, sizeof(b), "f %u/%u/%u %u/%u/%u %u/%u/%u\n", a, a, a, d, d, d, c, c, c);  // winding of the file
        f << b;
    }
    return (bool)f;
}

bool LoadObj(const std::string& path, MeshData& out, std::string* error) {
    std::ifstream f(Utf8Path(path));
    if (!f) { if (error) *error = "fichier introuvable"; return false; }
    std::vector<float3> pos, nrm;
    std::vector<float2> uvs;
    std::unordered_map<u64, u32> dedup;
    out = MeshData{};
    out.name = Narrow(Utf8Path(path).stem().wstring());
    bool hasNormals = false;
    std::string line;
    auto key = [](int a, int b, int c) { return ((u64)(u32)a << 42) ^ ((u64)(u32)b << 21) ^ (u64)(u32)c; };
    std::unordered_map<std::string, int> matByName;
    int curMat = -1;
    auto loadMtl = [&](const std::string& file) {
        auto mp = Utf8Path(path).parent_path() / Utf8Path(file);
        std::ifstream mf(mp);
        std::string ml;
        MeshMaterialInfo* m = nullptr;
        while (std::getline(mf, ml)) {
            std::istringstream ms(ml);
            std::string t;
            ms >> t;
            if (t == "newmtl") {
                MeshMaterialInfo info;
                std::getline(ms >> std::ws, info.name);
                matByName[info.name] = (int)out.materials.size();
                out.materials.push_back(info);
                m = &out.materials.back();
            } else if (!m) continue;
            else if (t == "Kd") { float r, g, b; ms >> r >> g >> b; m->baseColor = {SrgbToLinear(r), SrgbToLinear(g), SrgbToLinear(b), 1}; }
            else if (t == "Ke") { float r, g, b; ms >> r >> g >> b; m->emissive = {r, g, b}; }
            else if (t == "Ns") { float ns; ms >> ns; m->roughness = std::clamp(std::sqrt(2.0f / (ns + 2.0f)), 0.02f, 1.0f); }
            else if (t == "Pr") ms >> m->roughness;
            else if (t == "Pm") ms >> m->metallic;
            else if (t == "map_Kd") {
                std::string rest;
                std::getline(ms >> std::ws, rest);
                size_t sp = rest.find_last_of(' ');  // options (-bm ...) before the file name
                std::string tex = rest[0] == '-' && sp != std::string::npos ? rest.substr(sp + 1) : rest;
                m->baseColorPath = Narrow((mp.parent_path() / Utf8Path(tex)).wstring());
            }
        }
    };
    while (std::getline(f, line)) {
        if (line.size() < 2) continue;
        std::istringstream s(line);
        std::string tag;
        s >> tag;
        if (tag == "mtllib") { std::string file; std::getline(s >> std::ws, file); loadMtl(file); }
        else if (tag == "usemtl") {
            std::string name;
            std::getline(s >> std::ws, name);
            auto it = matByName.find(name);
            curMat = it == matByName.end() ? -1 : it->second;
        }
        else if (tag == "v") { float3 p; s >> p.x >> p.y >> p.z; pos.push_back({p.x, p.y, -p.z}); }  // RH -> LH
        else if (tag == "vt") { float2 t; s >> t.x >> t.y; uvs.push_back({t.x, 1.0f - t.y}); }
        else if (tag == "vn") { float3 n; s >> n.x >> n.y >> n.z; nrm.push_back({n.x, n.y, -n.z}); }
        else if (tag == "f") {
            std::vector<u32> poly;
            std::string tok;
            while (s >> tok) {
                int vi = 0, ti = 0, ni = 0;
                size_t a = tok.find('/');
                vi = std::atoi(tok.substr(0, a).c_str());
                if (a != std::string::npos) {
                    size_t b = tok.find('/', a + 1);
                    std::string t = tok.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
                    if (!t.empty()) ti = std::atoi(t.c_str());
                    if (b != std::string::npos) ni = std::atoi(tok.substr(b + 1).c_str());
                }
                if (vi < 0) vi = (int)pos.size() + vi + 1;
                if (ti < 0) ti = (int)uvs.size() + ti + 1;
                if (ni < 0) ni = (int)nrm.size() + ni + 1;
                u64 k = key(vi, ti, ni);
                auto it = dedup.find(k);
                if (it == dedup.end()) {
                    MeshVertex v;
                    if (vi > 0 && vi <= (int)pos.size()) v.pos = pos[vi - 1];
                    if (ti > 0 && ti <= (int)uvs.size()) v.uv = uvs[ti - 1];
                    if (ni > 0 && ni <= (int)nrm.size()) { v.normal = nrm[ni - 1]; hasNormals = true; }
                    it = dedup.emplace(k, (u32)out.vertices.size()).first;
                    out.vertices.push_back(v);
                }
                poly.push_back(it->second);
            }
            // Reverse winding together with the Z flip so faces stay front-facing.
            for (size_t i = 1; i + 1 < poly.size(); ++i) {
                out.indices.push_back(poly[0]);
                out.indices.push_back(poly[i + 1]);
                out.indices.push_back(poly[i]);
                out.triMaterial.push_back((u16)std::max(curMat, 0));
            }
        }
    }
    if (out.indices.empty()) { if (error) *error = "aucune face"; return false; }
    if (out.materials.empty()) out.triMaterial.clear();
    out.hasUvs = !uvs.empty();  // without UVs: loaded anyway, the UV tool (xatlas) unwraps it
    if (!hasNormals) {
        for (auto& v : out.vertices) v.normal = {};
        for (size_t i = 0; i + 2 < out.indices.size(); i += 3) {
            MeshVertex &a = out.vertices[out.indices[i]], &b = out.vertices[out.indices[i + 1]], &c = out.vertices[out.indices[i + 2]];
            float3 n = Cross(b.pos - a.pos, c.pos - a.pos);
            a.normal = a.normal + n; b.normal = b.normal + n; c.normal = c.normal + n;
        }
        for (auto& v : out.vertices) v.normal = Normalize(v.normal);
    }
    NormalizeMesh(out);
    return true;
}

MeshData MakeSphere(int seg, int rings) {
    MeshData m;
    m.name = "Sphère";
    for (int r = 0; r <= rings; ++r) {
        float v = (float)r / rings, phi = v * 3.14159265f;
        for (int s = 0; s <= seg; ++s) {
            float u = (float)s / seg, th = u * 6.2831853f;
            float3 n = {std::sin(phi) * std::cos(th), std::cos(phi), std::sin(phi) * std::sin(th)};
            m.vertices.push_back({n, n, {u, v}});
        }
    }
    for (int r = 0; r < rings; ++r)
        for (int s = 0; s < seg; ++s) {
            u32 a = r * (seg + 1) + s, b = a + seg + 1;
            m.indices.insert(m.indices.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    NormalizeMesh(m);
    return m;
}

MeshData MakeCube() {
    MeshData m;
    m.name = "Cube";
    // Each face gets its own cell of a 3x2 UV atlas.
    const float3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int f = 0; f < 6; ++f) {
        float3 n = dirs[f];
        float3 up = std::abs(n.y) > 0.5f ? float3{0, 0, 1} : float3{0, 1, 0};
        float3 right = Cross(up, n);
        float cu = (f % 3) / 3.0f, cv = (f / 3) / 2.0f;
        u32 base = (u32)m.vertices.size();
        for (int k = 0; k < 4; ++k) {
            float a = (k == 1 || k == 2) ? 1.0f : -1.0f, b = (k >= 2) ? 1.0f : -1.0f;
            float3 p = n + right * a + up * b;
            float2 uv = {cu + (a * 0.5f + 0.5f) / 3.0f * 0.96f + 0.0066f, cv + (0.5f - b * 0.5f) / 2.0f * 0.96f + 0.01f};
            m.vertices.push_back({p * 0.6f, n, uv});
        }
        m.indices.insert(m.indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
    }
    NormalizeMesh(m);
    return m;
}

MeshData MakePlane() {
    MeshData m;
    m.name = "Plan";
    m.vertices = {{{-1, 0, -1}, {0, 1, 0}, {0, 1}}, {{1, 0, -1}, {0, 1, 0}, {1, 1}}, {{1, 0, 1}, {0, 1, 0}, {1, 0}}, {{-1, 0, 1}, {0, 1, 0}, {0, 0}}};
    m.indices = {0, 2, 1, 0, 3, 2};
    NormalizeMesh(m);
    return m;
}

}  // namespace sx
