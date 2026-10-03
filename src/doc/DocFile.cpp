#include "doc/DocFile.h"
#include "ai/Net.h"  // Base64

#include <stb_image.h>

#include "core/Deflate.h"

#ifdef _WIN32
#include <windows.h>
#include <compressapi.h>
#pragma comment(lib, "Cabinet.lib")
#endif

#include <cstdio>
#include <cstring>
#include <future>
#include <set>
#include <filesystem>
#include <fstream>
#include <thread>
#include <functional>
#include <atomic>


namespace sx {

namespace {
// Version 1: XPRESS_HUFF (Windows only, still readable on Windows).
// Version 2: byte-plane shuffle, zlib (deflate, own encoder / stb inflate) in 8 MB chunks (portable):
//            [u32 chunks]{[u32 raw][u32 packed][bytes]}...
constexpr u32 kVersion = 2;
constexpr size_t kChunk = 8u << 20;

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
const char* kNodeKinds[] = {"output", "surface", "effect", "blend", "split"};

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

#ifdef _WIN32
bool DecompressXpress(const std::vector<u8>& in, size_t rawSize, std::vector<u8>& out) {
    DECOMPRESSOR_HANDLE h = nullptr;
    if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &h)) return false;
    out.resize(rawSize);
    SIZE_T got = 0;
    bool ok = ::Decompress(h, in.data(), in.size(), out.data(), out.size(), &got) != FALSE && got == rawSize;
    CloseDecompressor(h);
    return ok;
}
#endif

// Runs fn(i) for i in [0, count) on all hardware threads.
void ParallelFor(size_t count, const std::function<void(size_t)>& fn) {
    std::atomic<size_t> next{0};
    unsigned n = std::max(1u, std::min<unsigned>(std::thread::hardware_concurrency(), (unsigned)count));
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < n; ++t)
        pool.emplace_back([&] {
            for (size_t i; (i = next.fetch_add(1)) < count;) fn(i);
        });
    for (auto& th : pool) th.join();
}

// Chunked deflate of every buffer, all chunks of all buffers in parallel.
std::vector<std::vector<u8>> PackAll(const std::vector<std::vector<u8>>& raws) {
    struct Job { size_t buf, off, len; std::vector<u8> out; };
    std::vector<Job> jobs;
    for (size_t i = 0; i < raws.size(); ++i)
        for (size_t off = 0; off < raws[i].size() || (off == 0 && raws[i].empty()); off += kChunk) {
            jobs.push_back({i, off, std::min(kChunk, raws[i].size() - off), {}});
            if (raws[i].empty()) break;
        }
    ParallelFor(jobs.size(), [&](size_t j) {
        Job& jb = jobs[j];
        jb.out = deflate::ZlibCompress(raws[jb.buf].data() + jb.off, jb.len, 6);
    });
    std::vector<std::vector<u8>> packed(raws.size());
    std::vector<u32> counts(raws.size(), 0);
    for (auto& jb : jobs) ++counts[jb.buf];
    for (size_t i = 0; i < raws.size(); ++i) {
        packed[i].resize(4);
        memcpy(packed[i].data(), &counts[i], 4);
    }
    for (auto& jb : jobs) {
        auto& p = packed[jb.buf];
        u32 hdr[2] = {(u32)jb.len, (u32)jb.out.size()};
        p.insert(p.end(), (const u8*)hdr, (const u8*)hdr + 8);
        p.insert(p.end(), jb.out.begin(), jb.out.end());
    }
    return packed;
}

// Inverse of PackAll for one buffer whose raw size is known.
bool UnpackChunks(const std::vector<u8>& in, size_t rawSize, std::vector<u8>& out) {
    out.resize(rawSize);
    if (in.size() < 4) return false;
    u32 count;
    memcpy(&count, in.data(), 4);
    struct Chunk { size_t src, srcLen, dst, dstLen; };
    std::vector<Chunk> chunks;
    size_t pos = 4, dst = 0;
    for (u32 c = 0; c < count; ++c) {
        if (pos + 8 > in.size()) return false;
        u32 hdr[2];
        memcpy(hdr, in.data() + pos, 8);
        pos += 8;
        if (pos + hdr[1] > in.size() || dst + hdr[0] > rawSize) return false;
        chunks.push_back({pos, hdr[1], dst, hdr[0]});
        pos += hdr[1];
        dst += hdr[0];
    }
    if (dst != rawSize) return false;
    std::atomic<bool> ok{true};
    ParallelFor(chunks.size(), [&](size_t i) {
        const Chunk& ch = chunks[i];
        int got = stbi_zlib_decode_buffer((char*)out.data() + ch.dst, (int)ch.dstLen, (const char*)in.data() + ch.src, (int)ch.srcLen);
        if (got != (int)ch.dstLen) ok = false;
    });
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
    if (!s.layerNodePos.empty()) {
        Json np = Json::Array();
        for (auto& [id, p] : s.layerNodePos) {
            np.Push(id);
            np.Push(ToJson(p));
        }
        j["layerNodePos"] = np;
    }

    Json layers = Json::Array();
    for (auto& l : s.layers) {
        Json o = Json::Object();
        o["id"] = l.id;
        o["name"] = l.name;
        o["kind"] = kLayerKinds[(int)l.kind];
        if (l.surface) o["surface"] = l.surface;
        if (l.kind == LayerKind::Fill || l.kind == LayerKind::Adjustment) o["effect"] = EffectToJson(l.effect);
        if (l.channelOff) o["channelOff"] = (int)l.channelOff;
        if (!l.blendFx.customCode.empty()) o["blendFx"] = EffectToJson(l.blendFx);
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
            for (u8 x : l.blendIf.self[0]) b.Push((int)x);
            for (u8 x : l.blendIf.under[0]) b.Push((int)x);
            o["blendIf"] = b;
            if (l.blendIf.Active(1) || l.blendIf.Active(2) || l.blendIf.Active(3)) {
                Json c = Json::Array();
                for (int ch = 1; ch < 4; ++ch) {
                    for (u8 x : l.blendIf.self[ch]) c.Push((int)x);
                    for (u8 x : l.blendIf.under[ch]) c.Push((int)x);
                }
                o["blendIfRGB"] = c;
            }
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
        if (std::any_of(n.inputChannel.begin(), n.inputChannel.end(), [](u8 c) { return c != 0; })) {
            Json ch = Json::Array();
            for (u8 c : n.inputChannel) ch.Push((int)c);
            o["inputChannels"] = ch;
        }
        if (n.kind == NodeKind::Blend) { o["blend"] = (int)n.blend; o["opacity"] = n.opacity; o["clip"] = n.clip; if (n.channelOff) o["channelOff"] = (int)n.channelOff; }
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
        l.channelOff = (u8)(o["channelOff"].I(0) & 15);
        if (o.Has("blendFx")) l.blendFx = EffectFromJson(o["blendFx"]);
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
                if (i < 4) l.blendIf.self[0][i] = v;
                else if (i < 8) l.blendIf.under[0][i - 4] = v;
                ++i;
            }
        }
        if (o.Has("blendIfRGB")) {
            int i = 0;
            for (auto& x : o["blendIfRGB"].Elements()) {
                if (i >= 24) break;
                u8 v = (u8)std::clamp(x.I(), 0, 255);
                int ch = 1 + i / 8, k = i % 8;
                if (k < 4) l.blendIf.self[ch][k] = v;
                else l.blendIf.under[ch][k - 4] = v;
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
        n.channelOff = (u8)(o["channelOff"].I(0) & 15);
        for (auto& c : o["inputChannels"].Elements()) n.inputChannel.push_back((u8)std::clamp(c.I(0), 0, 4));
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
    {
        const auto& np = j["layerNodePos"].Elements();
        for (size_t k = 0; k + 1 < np.size(); k += 2) s.layerNodePos[np[k].U()] = Float2FromJson(np[k + 1]);
    }
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
    std::vector<std::vector<u8>> shuffled(surfaces.size());
    ParallelFor(surfaces.size(), [&](size_t i) { shuffled[i] = Shuffle(surfaces[i].data, ElemSize(surfaces[i].format)); });
    std::vector<std::vector<u8>> packed = PackAll(shuffled);
    shuffled.clear();
    std::string json = meta.Dump(1);
    auto tmp = Utf8Path(path + ".tmp");
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            if (error) *error = "impossible d'écrire " + path;
            return false;
        }
        auto put = [&](const void* p, size_t n) { f.write((const char*)p, (std::streamsize)n); };
        put("SXL1", 4);
        u32 version = kVersion;
        put(&version, 4);
        u64 js = json.size();
        put(&js, 8);
        put(json.data(), json.size());
        for (size_t i = 0; i < surfaces.size(); ++i) {
            auto& s = surfaces[i];
            u32 hdr[4] = {s.id, s.width, s.height, (u32)s.format};
            put(hdr, sizeof(hdr));
            u64 raw = s.data.size(), pk = packed[i].size();
            put(&raw, 8);
            put(&pk, 8);
            put(packed[i].data(), packed[i].size());
        }
        if (!f) {
            f.close();
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            if (error) *error = "échec de l'écriture";
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp, Utf8Path(path), ec);  // replaces the destination atomically where supported
    if (ec) {
        if (error) *error = "impossible de remplacer " + path;
        return false;
    }
    return true;
}

bool ReadSxl(const std::string& path, Json& meta, std::vector<SurfaceBlob>& surfaces, std::string* error) {
    std::ifstream f(Utf8Path(path), std::ios::binary);
    if (!f) {
        if (error) *error = "impossible d'ouvrir " + path;
        return false;
    }
    auto get = [&](void* p, size_t n) { return (bool)f.read((char*)p, (std::streamsize)n); };
    char magic[4];
    u32 version = 0;
    u64 js = 0;
    if (!get(magic, 4) || memcmp(magic, "SXL1", 4) != 0 || !get(&version, 4) || !get(&js, 8) || js > (256ull << 20)) {
        if (error) *error = "en-tête invalide";
        return false;
    }
    if (version < 1 || version > kVersion) {
        if (error) *error = "version de fichier non prise en charge (" + std::to_string(version) + ")";
        return false;
    }
#ifndef _WIN32
    if (version == 1) {
        if (error) *error = "fichier .sxl v1 (XPRESS) : réenregistrez-le sous Windows";
        return false;
    }
#endif
    std::string json(js, '\0');
    get(json.data(), js);
    std::string perr;
    if (!Json::Parse(json, meta, &perr)) {
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
    f.close();
    bool ok = true;
    for (size_t i = 0; i < surfaces.size() && ok; ++i) {
        std::vector<u8> tmp;
        size_t raw = surfaces[i].data.size();
#ifdef _WIN32
        if (version == 1) {
            ok = DecompressXpress(packed[i], raw, tmp);
            if (ok) surfaces[i].data = Unshuffle(tmp, ElemSize(surfaces[i].format));
        } else
#endif
        {
            ok = UnpackChunks(packed[i], raw, tmp);
            if (ok) surfaces[i].data = Unshuffle(tmp, ElemSize(surfaces[i].format));
        }
        packed[i].clear();
    }
    if (!ok && error) *error = "données de pixels corrompues";
    return ok;
}

}  // namespace sx
