#include "doc/DocFile.h"
#include "ai/Net.h"  // Base64

#include <windows.h>
#include <compressapi.h>

#include <cstdio>
#include <cstring>
#include <future>
#include <set>

#pragma comment(lib, "Cabinet.lib")

namespace sx {

namespace {
constexpr u32 kVersion = 1;

Json EffectToJson(const EffectInstance& fx) {
    Json j = Json::Object();
    if (!fx.effectId.empty()) j["effect"] = fx.effectId;
    if (!fx.customCode.empty()) j["code"] = fx.customCode;
    if (!fx.graph.empty()) j["graph"] = fx.graph;
    Json p = Json::Array();
    for (auto& v : fx.params) p.Push(ToJson(v));
    j["params"] = p;
    j["enabled"] = fx.enabled;
    bool anyCurve = false;
    for (auto& c : fx.curves) if (!c.empty()) anyCurve = true;
    if (anyCurve) {
        Json cs = Json::Array();
        for (auto& c : fx.curves) {
            Json pts = Json::Array();
            for (auto& pt : c) pts.Push(ToJson(pt));
            cs.Push(pts);
        }
        j["curves"] = cs;
    }
    Json ls = Json::Array();
    for (size_t i = 0; i < fx.luts.size(); ++i) {
        if (!fx.luts[i]) continue;
        Json l = Json::Object();
        l["param"] = (int)i;
        l["name"] = fx.luts[i]->name;
        l["size"] = fx.luts[i]->size;
        l["data"] = net::Base64Encode(fx.luts[i]->rgb.data(), fx.luts[i]->rgb.size() * sizeof(float));
        ls.Push(l);
    }
    if (ls.Size()) j["luts"] = ls;
    return j;
}

EffectInstance EffectFromJson(const Json& j) {
    EffectInstance fx;
    fx.effectId = j["effect"].S("");
    fx.customCode = j["code"].S("");
    fx.graph = j["graph"].S("");
    for (auto& p : j["params"].Elements()) fx.params.push_back(Float4FromJson(p));
    fx.enabled = j["enabled"].B(true);
    for (auto& c : j["curves"].Elements()) {
        CurvePoints pts;
        for (auto& p : c.Elements()) pts.push_back(Float2FromJson(p));
        fx.curves.push_back(pts);
    }
    for (auto& l : j["luts"].Elements()) {
        int idx = l["param"].I();
        int size = l["size"].I();
        std::vector<u8> raw = net::Base64Decode(l["data"].S(""));
        if (idx < 0 || idx > 64 || size < 2 || raw.size() != (size_t)size * size * size * 3 * sizeof(float)) continue;
        std::vector<float> rgb(raw.size() / sizeof(float));
        memcpy(rgb.data(), raw.data(), raw.size());
        if (fx.luts.size() <= (size_t)idx) fx.luts.resize(idx + 1);
        fx.luts[idx] = MakeLut(l["name"].S(""), size, std::move(rgb));
    }
    return fx;
}

const char* kLayerKinds[] = {"pixel", "fill", "adjustment", "group", "text", "shape"};
const char* kNodeKinds[] = {"output", "surface", "effect", "blend"};

template <size_t N>
int IndexOf(const char* (&names)[N], const std::string& s, int def) {
    for (size_t i = 0; i < N; ++i) if (s == names[i]) return (int)i;
    return def;
}

u32 ElemSize(gpu::Format f) {
    switch (f) {
        case gpu::Format::RGBA16F: case gpu::Format::R16F: return 2;
        case gpu::Format::RGBA32F: case gpu::Format::R32F: case gpu::Format::R32U: return 4;
        default: return 1;
    }
}

// Byte-plane shuffle: group byte k of every element together.
std::vector<u8> Shuffle(const std::vector<u8>& in, u32 elem) {
    if (elem <= 1) return in;
    size_t n = in.size() / elem;
    std::vector<u8> out(in.size());
    for (u32 b = 0; b < elem; ++b) {
        u8* dst = out.data() + b * n;
        for (size_t i = 0; i < n; ++i) dst[i] = in[i * elem + b];
    }
    return out;
}

std::vector<u8> Unshuffle(const std::vector<u8>& in, u32 elem) {
    if (elem <= 1) return in;
    size_t n = in.size() / elem;
    std::vector<u8> out(in.size());
    for (u32 b = 0; b < elem; ++b) {
        const u8* src = in.data() + b * n;
        for (size_t i = 0; i < n; ++i) out[i * elem + b] = src[i];
    }
    return out;
}

bool Compress(const std::vector<u8>& in, std::vector<u8>& out) {
    COMPRESSOR_HANDLE h = nullptr;
    if (!CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &h)) return false;
    SIZE_T need = 0;
    ::Compress(h, in.data(), in.size(), nullptr, 0, &need);
    out.resize(need);
    SIZE_T got = 0;
    bool ok = ::Compress(h, in.data(), in.size(), out.data(), out.size(), &got) != FALSE;
    out.resize(got);
    CloseCompressor(h);
    return ok;
}

bool Decompress(const std::vector<u8>& in, size_t rawSize, std::vector<u8>& out) {
    DECOMPRESSOR_HANDLE h = nullptr;
    if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &h)) return false;
    out.resize(rawSize);
    SIZE_T got = 0;
    bool ok = ::Decompress(h, in.data(), in.size(), out.data(), out.size(), &got) != FALSE && got == rawSize;
    CloseDecompressor(h);
    return ok;
}
}  // namespace

std::vector<Id> ReferencedSurfaces(const Document& doc) {
    std::set<Id> ids;
    const DocState& s = doc.S();
    for (auto& l : s.layers) {
        if (l.surface) ids.insert(l.surface);
        if (l.mask) ids.insert(l.mask);
    }
    for (auto& n : s.graph.nodes) if (n.surface) ids.insert(n.surface);
    if (s.selection) ids.insert(s.selection);
    std::vector<Id> out;
    for (Id id : ids) if (s.surfaces.count(id)) out.push_back(id);
    return out;
}

Json SerializeDocument(const Document& doc) {
    const DocState& s = doc.S();
    Json j = Json::Object();
    j["format"] = "shaderixel";
    j["version"] = kVersion;
    j["name"] = doc.Name();
    j["width"] = doc.Width();
    j["height"] = doc.Height();
    j["precision"] = doc.GetPrecision() == Precision::Float32 ? "float32" : "half16";
    j["mode"] = s.mode == DocMode::Layers ? "layers" : "nodes";
    j["blendInGamma"] = doc.blendInGamma;
    j["nextId"] = s.nextId;
    j["activeLayer"] = s.activeLayer;
    j["activeNode"] = s.activeNode;
    j["selection"] = s.selection;
    if (!s.guidesX.empty() || !s.guidesY.empty()) {
        Json gx = Json::Array(), gy = Json::Array();
        for (float g : s.guidesX) gx.Push(g);
        for (float g : s.guidesY) gy.Push(g);
        j["guidesX"] = gx;
        j["guidesY"] = gy;
    }
    if (!s.paths.empty()) {
        Json ps = Json::Array();
        for (auto& p : s.paths) {
            Json o = Json::Object();
            o["id"] = p.id;
            o["name"] = p.name;
            Json subs = Json::Array();
            for (auto& sp : p.subpaths) {
                Json so = Json::Object();
                so["closed"] = sp.closed;
                Json pts = Json::Array();
                for (auto& a : sp.pts) {
                    pts.Push(ToJson(a.p));
                    pts.Push(ToJson(a.in));
                    pts.Push(ToJson(a.out));
                }
                so["pts"] = pts;
                subs.Push(so);
            }
            o["subpaths"] = subs;
            ps.Push(o);
        }
        j["paths"] = ps;
        j["activePath"] = s.activePath;
    }

    Json layers = Json::Array();
    for (auto& l : s.layers) {
        Json o = Json::Object();
        o["id"] = l.id;
        o["name"] = l.name;
        o["kind"] = kLayerKinds[(int)l.kind];
        if (l.surface) o["surface"] = l.surface;
        if (l.kind == LayerKind::Fill || l.kind == LayerKind::Adjustment) o["effect"] = EffectToJson(l.effect);
        if (!l.filters.empty()) {
            Json f = Json::Array();
            for (auto& x : l.filters) f.Push(EffectToJson(x));
            o["filters"] = f;
        }
        o["blend"] = (int)l.blend;
        o["opacity"] = l.opacity;
        o["visible"] = l.visible;
        o["locked"] = l.locked;
        if (l.mask) { o["mask"] = l.mask; o["maskEnabled"] = l.maskEnabled; }
        if (l.parent) o["parent"] = l.parent;
        if (l.clipped) o["clipped"] = true;
        if (l.blendIf.Active()) {
            Json b = Json::Array();
            for (u8 x : l.blendIf.self) b.Push((int)x);
            for (u8 x : l.blendIf.under) b.Push((int)x);
            o["blendIf"] = b;
        }
        if (l.kind == LayerKind::Group) { o["passThrough"] = l.passThrough; o["collapsed"] = l.collapsed; }
        if (l.kind == LayerKind::Shape) {
            Json t = Json::Object();
            t["type"] = l.shape.type;
            t["rect"] = ToJson(l.shape.rect);
            t["radius"] = l.shape.radius;
            t["sides"] = l.shape.sides;
            t["rotation"] = l.shape.rotation;
            t["fillOn"] = l.shape.fillOn;
            t["strokeOn"] = l.shape.strokeOn;
            t["fill"] = ToJson(l.shape.fill);
            t["stroke"] = ToJson(l.shape.stroke);
            t["strokeWidth"] = l.shape.strokeWidth;
            o["shape"] = t;
        }
        if (l.kind == LayerKind::Text) {
            Json t = Json::Object();
            t["content"] = l.text.content;
            t["font"] = l.text.font;
            t["size"] = l.text.size;
            t["color"] = ToJson(l.text.color);
            t["pos"] = ToJson(l.text.pos);
            t["align"] = l.text.align;
            t["lineSpacing"] = l.text.lineSpacing;
            t["tracking"] = l.text.tracking;
            t["faux"] = l.text.faux;
            o["text"] = t;
        }
        layers.Push(o);
    }
    j["layers"] = layers;

    Json nodes = Json::Array();
    for (auto& n : s.graph.nodes) {
        Json o = Json::Object();
        o["id"] = n.id;
        o["kind"] = kNodeKinds[(int)n.kind];
        o["name"] = n.name;
        o["pos"] = ToJson(n.pos);
        if (n.surface) o["surface"] = n.surface;
        if (n.kind == NodeKind::Effect) o["effect"] = EffectToJson(n.fx);
        if (n.kind == NodeKind::Blend) { o["blend"] = (int)n.blend; o["opacity"] = n.opacity; o["clip"] = n.clip; }
        if (n.bypass) o["bypass"] = true;
        if (n.layer) o["layer"] = n.layer;
        Json in = Json::Array();
        for (Id x : n.inputs) in.Push(x);
        o["inputs"] = in;
        nodes.Push(o);
    }
    Json graph = Json::Object();
    graph["nodes"] = nodes;
    graph["output"] = s.graph.output;
    j["graph"] = graph;
    return j;
}

std::unique_ptr<Document> DeserializeDocument(const Json& j, const std::vector<SurfaceBlob>& blobs, std::string* error) {
    if (j["format"].S("") != "shaderixel") {
        if (error) *error = "fichier non reconnu";
        return nullptr;
    }
    u32 w = j["width"].U(), h = j["height"].U();
    if (!w || !h || w > 32768 || h > 32768) {
        if (error) *error = "dimensions invalides";
        return nullptr;
    }
    Precision prec = j["precision"].S("") == "float32" ? Precision::Float32 : Precision::Half16;
    DocMode mode = j["mode"].S("") == "nodes" ? DocMode::Nodes : DocMode::Layers;
    auto doc = std::make_unique<Document>(w, h, prec, mode, j["name"].S("Sans titre"));
    doc->blendInGamma = j["blendInGamma"].B(true);
    DocState& s = doc->S();
    for (auto& b : blobs) doc->CreateSurfaceWithId(b.id, b.format);

    for (auto& o : j["layers"].Elements()) {
        Layer l;
        l.id = o["id"].U();
        l.name = o["name"].S("Calque");
        l.kind = (LayerKind)IndexOf(kLayerKinds, o["kind"].S(""), 0);
        l.surface = o["surface"].U();
        if (o.Has("effect")) l.effect = EffectFromJson(o["effect"]);
        for (auto& f : o["filters"].Elements()) l.filters.push_back(EffectFromJson(f));
        l.blend = (BlendMode)std::clamp(o["blend"].I(), 0, (int)BlendMode::Count - 1);
        l.opacity = o["opacity"].F(1);
        l.visible = o["visible"].B(true);
        l.locked = o["locked"].B(false);
        l.mask = o["mask"].U();
        l.maskEnabled = o["maskEnabled"].B(true);
        l.parent = o["parent"].U();
        l.clipped = o["clipped"].B(false);
        if (o.Has("blendIf")) {
            int i = 0;
            for (auto& x : o["blendIf"].Elements()) {
                u8 v = (u8)std::clamp(x.I(), 0, 255);
                if (i < 4) l.blendIf.self[i] = v;
                else if (i < 8) l.blendIf.under[i - 4] = v;
                ++i;
            }
        }
        l.passThrough = o["passThrough"].B(true);
        l.collapsed = o["collapsed"].B(false);
        if (o.Has("shape")) {
            const Json& t = o["shape"];
            l.shape.type = t["type"].I(0);
            l.shape.rect = Float4FromJson(t["rect"]);
            l.shape.radius = t["radius"].F(0);
            l.shape.sides = t["sides"].I(5);
            l.shape.rotation = t["rotation"].F(0);
            l.shape.fillOn = t["fillOn"].B(true);
            l.shape.strokeOn = t["strokeOn"].B(false);
            l.shape.fill = Float4FromJson(t["fill"], {0, 0, 0, 1});
            l.shape.stroke = Float4FromJson(t["stroke"], {0, 0, 0, 1});
            l.shape.strokeWidth = t["strokeWidth"].F(4);
        }
        if (o.Has("text")) {
            const Json& t = o["text"];
            l.text.content = t["content"].S("");
            l.text.font = t["font"].S("");
            l.text.size = t["size"].F(72);
            l.text.color = Float4FromJson(t["color"], {0, 0, 0, 1});
            l.text.pos = Float2FromJson(t["pos"]);
            l.text.align = t["align"].I(0);
            l.text.lineSpacing = t["lineSpacing"].F(1.2f);
            l.text.tracking = t["tracking"].F(0);
            l.text.faux = t["faux"].B(false);
        }
        if (l.surface && !s.surfaces.count(l.surface)) l.surface = doc->CreateSurface({0, 0, 0, 0});
        if (l.mask && !s.surfaces.count(l.mask)) l.mask = 0;
        s.layers.push_back(l);
    }

    Graph g;
    for (auto& o : j["graph"]["nodes"].Elements()) {
        Node n;
        n.id = o["id"].U();
        n.kind = (NodeKind)IndexOf(kNodeKinds, o["kind"].S(""), 2);
        n.name = o["name"].S("");
        n.pos = Float2FromJson(o["pos"]);
        n.surface = o["surface"].U();
        if (o.Has("effect")) n.fx = EffectFromJson(o["effect"]);
        n.blend = (BlendMode)std::clamp(o["blend"].I(), 0, (int)BlendMode::Count - 1);
        n.opacity = o["opacity"].F(1);
        n.clip = o["clip"].B(false);
        n.bypass = o["bypass"].B(false);
        n.layer = o["layer"].U();
        for (auto& x : o["inputs"].Elements()) n.inputs.push_back(x.U());
        if (n.surface && !s.surfaces.count(n.surface)) n.surface = doc->CreateSurface({0, 0, 0, 0});
        g.nodes.push_back(n);
    }
    g.output = j["graph"]["output"].U(1);
    if (!g.Find(g.output)) {
        if (error) *error = "graphe sans sortie";
        return nullptr;
    }
    s.graph = std::move(g);
    s.activeLayer = j["activeLayer"].U();
    s.activeNode = j["activeNode"].U();
    s.selection = s.surfaces.count(j["selection"].U()) ? j["selection"].U() : 0;
    s.lastSelection = s.selection;
    for (auto& o : j["paths"].Elements()) {
        VectorPath p;
        p.id = o["id"].U();
        p.name = o["name"].S("Tracé");
        for (auto& so : o["subpaths"].Elements()) {
            SubPath sp;
            sp.closed = so["closed"].B(false);
            std::vector<float2> v;
            for (auto& e : so["pts"].Elements()) v.push_back(Float2FromJson(e));
            for (size_t k = 0; k + 2 < v.size(); k += 3) sp.pts.push_back({v[k], v[k + 1], v[k + 2]});
            p.subpaths.push_back(sp);
        }
        s.paths.push_back(p);
    }
    s.activePath = j["activePath"].U();
    for (auto& gv : j["guidesX"].Elements()) s.guidesX.push_back(gv.F());
    for (auto& gv : j["guidesY"].Elements()) s.guidesY.push_back(gv.F());
    // nextId must stay above every id in the file.
    Id maxId = j["nextId"].U(100);
    for (auto& l : s.layers) maxId = std::max(maxId, l.id + 1);
    for (auto& p : s.paths) maxId = std::max(maxId, p.id + 1);
    for (auto& [id, sf] : s.surfaces) maxId = std::max(maxId, id + 1);
    for (auto& n : s.graph.nodes) if (n.id < 0x40000000u) maxId = std::max(maxId, n.id + 1);
    s.nextId = std::max(s.nextId, maxId);
    doc->SyncGraph();
    doc->dirty = false;
    return doc;
}

bool WriteSxl(const std::string& path, const Json& meta, const std::vector<SurfaceBlob>& surfaces, std::string* error) {
    // Compress all surfaces in parallel.
    std::vector<std::future<std::vector<u8>>> jobs;
    for (auto& s : surfaces) {
        jobs.push_back(std::async(std::launch::async, [&s] {
            std::vector<u8> packed;
            if (!Compress(Shuffle(s.data, ElemSize(s.format)), packed)) packed.clear();
            return packed;
        }));
    }
    std::string json = meta.Dump(1);
    std::string tmp = path + ".tmp";
    FILE* f = nullptr;
    _wfopen_s(&f, Widen(tmp).c_str(), L"wb");
    if (!f) {
        if (error) *error = "impossible d'écrire " + path;
        return false;
    }
    auto put = [&](const void* p, size_t n) { fwrite(p, 1, n, f); };
    put("SXL1", 4);
    u32 version = kVersion;
    put(&version, 4);
    u64 js = json.size();
    put(&js, 8);
    put(json.data(), json.size());
    bool ok = true;
    for (size_t i = 0; i < surfaces.size(); ++i) {
        auto& s = surfaces[i];
        std::vector<u8> packed = jobs[i].get();
        if (packed.empty() && !s.data.empty()) ok = false;
        u32 hdr[4] = {s.id, s.width, s.height, (u32)s.format};
        put(hdr, sizeof(hdr));
        u64 raw = s.data.size(), pk = packed.size();
        put(&raw, 8);
        put(&pk, 8);
        put(packed.data(), packed.size());
    }
    ok = ok && !ferror(f);
    fclose(f);
    if (!ok) {
        DeleteFileW(Widen(tmp).c_str());
        if (error) *error = "échec de la compression ou de l'écriture";
        return false;
    }
    // Atomic-ish replace.
    if (!MoveFileExW(Widen(tmp).c_str(), Widen(path).c_str(), MOVEFILE_REPLACE_EXISTING)) {
        if (error) *error = "impossible de remplacer " + path;
        return false;
    }
    return true;
}

bool ReadSxl(const std::string& path, Json& meta, std::vector<SurfaceBlob>& surfaces, std::string* error) {
    FILE* f = nullptr;
    _wfopen_s(&f, Widen(path).c_str(), L"rb");
    if (!f) {
        if (error) *error = "impossible d'ouvrir " + path;
        return false;
    }
    auto get = [&](void* p, size_t n) { return fread(p, 1, n, f) == n; };
    char magic[4];
    u32 version = 0;
    u64 js = 0;
    if (!get(magic, 4) || memcmp(magic, "SXL1", 4) != 0 || !get(&version, 4) || !get(&js, 8) || js > (256ull << 20)) {
        fclose(f);
        if (error) *error = "en-tête invalide";
        return false;
    }
    std::string json(js, '\0');
    get(json.data(), js);
    std::string perr;
    if (!Json::Parse(json, meta, &perr)) {
        fclose(f);
        if (error) *error = "JSON invalide : " + perr;
        return false;
    }
    std::vector<std::vector<u8>> packed;
    for (;;) {
        u32 hdr[4];
        if (!get(hdr, sizeof(hdr))) break;
        u64 raw = 0, pk = 0;
        if (!get(&raw, 8) || !get(&pk, 8) || pk > (8ull << 30) || raw > (16ull << 30)) break;
        SurfaceBlob b;
        b.id = hdr[0];
        b.width = hdr[1];
        b.height = hdr[2];
        b.format = (gpu::Format)hdr[3];
        b.data.resize((size_t)raw);  // placeholder for size
        std::vector<u8> p((size_t)pk);
        if (!get(p.data(), p.size())) break;
        surfaces.push_back(std::move(b));
        packed.push_back(std::move(p));
    }
    fclose(f);
    std::vector<std::future<bool>> jobs;
    for (size_t i = 0; i < surfaces.size(); ++i) {
        jobs.push_back(std::async(std::launch::async, [&, i] {
            std::vector<u8> tmp;
            size_t raw = surfaces[i].data.size();
            if (!Decompress(packed[i], raw, tmp)) return false;
            surfaces[i].data = Unshuffle(tmp, ElemSize(surfaces[i].format));
            return true;
        }));
    }
    bool ok = true;
    for (auto& jb : jobs) ok &= jb.get();
    if (!ok && error) *error = "données de pixels corrompues";
    return ok;
}

}  // namespace sx
