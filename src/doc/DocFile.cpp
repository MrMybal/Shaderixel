#include "doc/DocFile.h"
#include "doc/GraphTypes.h"
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

const char* kLayerKinds[] = {"pixel", "fill", "adjustment", "group", "text", "shape", "smart"};
const char* kNodeKinds[] = {"output", "surface", "effect", "blend", "split", "tool", "function", "subinput", "suboutput", "value", "math", "varget", "varset", "event", "action"};

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
    CollectGraphSurfaces(s.graph, ids);
    for (auto& n : s.graph.nodes) if (n.kind == NodeKind::SubInput && n.surface) ids.insert(n.surface);  // function editor previews
    if (s.selection) ids.insert(s.selection);
    std::vector<Id> out;
    for (Id id : ids) if (s.surfaces.count(id)) out.push_back(id);
    return out;
}

Json GraphToJson(const Graph& g) {
    Json nodes = Json::Array();
    for (auto& n : g.nodes) {
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
        if (n.kind == NodeKind::Tool) {
            o["tool"] = n.tool;
            o["passLive"] = n.passLive;
            if (n.smart) o["smart"] = n.smart;
            if (n.bakedHash) o["bakedHash"] = std::to_string(n.bakedHash);
            Json pr = Json::Object();
            for (auto& [k, val] : n.props) pr[k] = val;
            o["props"] = pr;
        }
        Json in = Json::Array();
        for (Id x : n.inputs) in.Push(x);
        o["inputs"] = in;
        if (n.kind == NodeKind::Function) {
            if (n.sub) o["sub"] = GraphToJson(*n.sub);
            o["funcKind"] = n.funcKind;
            o["applyMode"] = n.applyMode;
            Json os = Json::Array();
            for (Id x : n.outSurf) os.Push(x);
            o["outSurf"] = os;
        }
        if (n.kind == NodeKind::SubInput || n.kind == NodeKind::SubOutput) o["port"] = n.port;
        if (PinIsValue(n.value.type)) o["value"] = ValueToJson(n.value);
        if (!n.op.empty()) o["op"] = n.op;
        if (!n.pinValues.empty()) {
            Json pv = Json::Array();
            for (auto& v : n.pinValues) pv.Push(ValueToJson(v));
            o["pinValues"] = pv;
        }
        if (!n.paramPins.empty()) {
            Json pp = Json::Array();
            for (int k : n.paramPins) pp.Push(k);
            o["paramPins"] = pp;
        }
        if (!n.execResults.empty()) {
            Json er = Json::Array();
            for (auto& [inner, s] : n.execResults) { Json e = Json::Array(); e.Push(inner); e.Push(s); er.Push(e); }
            o["execResults"] = er;
        }
        if (!n.execSmart.empty()) {
            Json es = Json::Array();
            for (auto& [inner, s] : n.execSmart) { Json e = Json::Array(); e.Push(inner); e.Push(s); es.Push(e); }
            o["execSmart"] = es;
        }
        if (!n.varValues.empty()) {
            Json vv = Json::Object();
            for (auto& [k, v] : n.varValues) vv[k] = ValueToJson(v);
            o["varValues"] = vv;
        }
        nodes.Push(o);
    }
    Json graph = Json::Object();
    graph["nodes"] = nodes;
    graph["output"] = g.output;
    if (!g.description.empty()) graph["description"] = g.description;
    if (!g.variables.empty()) {
        Json vars = Json::Array();
        for (auto& v : g.variables) {
            Json o = Json::Object();
            o["name"] = v.name;
            o["value"] = ValueToJson(v.value);
            o["exposed"] = v.exposed;
            if (!v.desc.empty()) o["desc"] = v.desc;
            vars.Push(o);
        }
        graph["variables"] = vars;
    }
    return graph;
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
    if (!doc.iccProfile.empty()) j["iccProfile"] = doc.iccProfile;
    if (!doc.cmykProfile.empty()) j["cmykProfile"] = doc.cmykProfile;
    if (doc.colorMode) j["colorMode"] = doc.colorMode;
    if (doc.kind) {
        j["kind"] = doc.kind == 1 ? "paint3d" : "image";
        j["pbrChannels"] = doc.pbrChannels;
        j["exportTemplate"] = doc.exportTemplate;
    }
    j["nextId"] = s.nextId;
    j["activeLayer"] = s.activeLayer;
    j["activeNode"] = s.activeNode;
    j["selection"] = s.selection;
    if (!s.charStyles.empty() || !s.paraStyles.empty()) {
        Json cs = Json::Array(), ps = Json::Array();
        for (auto& c : s.charStyles) {
            Json o = Json::Object();
            o["id"] = c.id; o["name"] = c.name; o["font"] = c.font; o["size"] = c.size; o["color"] = ToJson(c.color);
            o["useColor"] = c.useColor; o["tracking"] = c.tracking; o["faux"] = c.faux; o["baseline"] = c.baseline;
            o["underline"] = c.underline; o["allCaps"] = c.allCaps;
            cs.Push(o);
        }
        for (auto& p : s.paraStyles) {
            Json o = Json::Object();
            o["id"] = p.id; o["name"] = p.name; o["align"] = p.align; o["lineSpacing"] = p.lineSpacing; o["spaceBefore"] = p.spaceBefore;
            o["indentLeft"] = p.indentLeft; o["indentFirst"] = p.indentFirst; o["charStyle"] = p.charStyle;
            ps.Push(o);
        }
        j["charStyles"] = cs;
        j["paraStyles"] = ps;
    }
    if (!s.comps.empty()) {
        Json cs = Json::Array();
        for (auto& c : s.comps) {
            Json o = Json::Object();
            o["id"] = c.id;
            o["name"] = c.name;
            o["comment"] = c.comment;
            o["visibility"] = c.useVisibility;
            o["appearance"] = c.useAppearance;
            o["position"] = c.usePosition;
            Json es = Json::Array();
            for (auto& e : c.entries) {
                Json eo = Json::Object();
                eo["layer"] = e.layer;
                eo["visible"] = e.visible;
                eo["opacity"] = e.opacity;
                eo["blend"] = (int)e.blend;
                Json f = Json::Array();
                for (u8 b : e.filterOn) f.Push((int)b);
                eo["filters"] = f;
                Json cr = Json::Array();
                for (auto& p : e.corners) { cr.Push(p.x); cr.Push(p.y); }
                eo["corners"] = cr;
                es.Push(eo);
            }
            o["entries"] = es;
            cs.Push(o);
        }
        j["layerComps"] = cs;
        j["activeComp"] = s.activeComp;
    }
    if (!s.trimSheet.trims.empty()) {
        Json ts = Json::Object();
        ts["padding"] = s.trimSheet.padding;
        ts["show"] = s.trimSheet.show;
        Json arr = Json::Array();
        for (auto& t : s.trimSheet.trims) {
            Json o = Json::Object();
            o["name"] = t.name;
            Json r = Json::Array();
            r.Push(t.r.x0); r.Push(t.r.y0); r.Push(t.r.x1); r.Push(t.r.y1);
            o["rect"] = r;
            o["tileX"] = t.tileX;
            o["tileY"] = t.tileY;
            arr.Push(o);
        }
        ts["trims"] = arr;
        j["trimSheet"] = ts;
    }
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
        if (l.HasCustomBlend()) o["blendFx"] = EffectToJson(l.blendFx);
        if (l.artboard) {
            Json b = Json::Array();
            b.Push(l.board.x0); b.Push(l.board.y0); b.Push(l.board.x1); b.Push(l.board.y1);
            o["artboard"] = b;
        }
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
        if (l.kind == LayerKind::Smart) {
            o["smart"] = l.smart;
            Json c = Json::Array();
            for (auto& p : l.smartCorners) { c.Push(p.x); c.Push(p.y); }
            o["smartCorners"] = c;
            if (l.smartPivot.x != 0.5f || l.smartPivot.y != 0.5f) {
                Json pv = Json::Array();
                pv.Push(l.smartPivot.x);
                pv.Push(l.smartPivot.y);
                o["smartPivot"] = pv;
            }
            if (l.smartInterp != 2) o["smartInterp"] = (int)l.smartInterp;
        }
        if (l.material) o["material"] = l.material;
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
            if (!l.shape.parts.empty()) {
                Json ps = Json::Array();
                for (auto& p : l.shape.parts) {
                    Json po = Json::Object();
                    po["op"] = p.op; po["type"] = p.type; po["rect"] = ToJson(p.rect); po["radius"] = p.radius; po["sides"] = p.sides; po["rotation"] = p.rotation;
                    ps.Push(po);
                }
                t["parts"] = ps;
            }
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
            if (!l.text.runs.empty()) {
                Json rs = Json::Array();
                for (auto& r : l.text.runs) { Json a = Json::Array(); a.Push(r.start); a.Push(r.end); a.Push(r.style); rs.Push(a); }
                t["runs"] = rs;
            }
            if (!l.text.paraStyles.empty()) {
                Json ps = Json::Array();
                for (Id p : l.text.paraStyles) ps.Push(p);
                t["paraStyles"] = ps;
            }
            if (!l.text.path.empty()) {
                Json pp = Json::Array();
                for (auto& p : l.text.path) { pp.Push(p.x); pp.Push(p.y); }
                t["path"] = pp;
                t["pathOffset"] = l.text.pathOffset;
                t["pathFlip"] = l.text.pathFlip;
            }
            o["text"] = t;
        }
        layers.Push(o);
    }
    j["layers"] = layers;

    j["graph"] = GraphToJson(s.graph);
    if (!doc.functionKind.empty()) j["functionKind"] = doc.functionKind;
    return j;
}

void GraphFromJson(const Json& jg, Graph& g) {
    for (auto& o : jg["nodes"].Elements()) {
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
        for (auto& c : o["inputChannels"].Elements()) n.inputChannel.push_back((u8)std::clamp(c.I(0), 0, 255));  // channel (0-4) | output port << 3
        n.bypass = o["bypass"].B(false);
        n.layer = o["layer"].U();
        if (n.kind == NodeKind::Tool) {
            n.tool = o["tool"].S("bake");
            n.passLive = o["passLive"].B(true);
            n.smart = o["smart"].U();
            n.bakedHash = strtoull(o["bakedHash"].S("0").c_str(), nullptr, 10);
            for (auto& [k, val] : o["props"].Items()) n.props[k] = val.S("");
        }
        for (auto& x : o["inputs"].Elements()) n.inputs.push_back(x.U());
        if (n.kind == NodeKind::Function) {
            if (o.Has("sub")) {
                auto sub = std::make_shared<Graph>();
                GraphFromJson(o["sub"], *sub);
                n.sub = sub;
            }
            n.funcKind = o["funcKind"].S("subgraph");
            n.applyMode = o["applyMode"].B(false);
            for (auto& x : o["outSurf"].Elements()) n.outSurf.push_back(x.U());
        }
        n.port = o["port"].I(0);
        if (o.Has("value")) n.value = ValueFromJson(o["value"]);
        n.op = o["op"].S("");
        // Hand-written graphs (.sxfn): parameters by id ("set"), missing parameters, names and pins.
        if (n.kind == NodeKind::Effect) {
            if (auto def = n.fx.Def()) {
                while (n.fx.params.size() < def->params.size()) n.fx.params.push_back(def->params[n.fx.params.size()].def);
                for (auto& [pid, val] : o["effect"]["set"].Items())
                    for (size_t k = 0; k < def->params.size(); ++k)
                        if (def->params[k].id == pid) {
                            if (val.IsArray()) n.fx.params[k] = Float4FromJson(val);
                            else n.fx.params[k] = {val.F(), 0, 0, 0};
                        }
                for (auto& pid : o["paramPinNames"].Elements())
                    for (size_t k = 0; k < def->params.size(); ++k)
                        if (def->params[k].id == pid.S("")) n.paramPins.push_back((int)k);
                if (n.name.empty()) n.name = def->name;
            }
        }
        for (auto& v : o["pinValues"].Elements()) n.pinValues.push_back(ValueFromJson(v));
        for (auto& k : o["paramPins"].Elements()) n.paramPins.push_back(k.I(0));
        for (auto& e : o["execResults"].Elements()) if (e.Size() >= 2) n.execResults[e.At(0).U()] = e.At(1).U();
        for (auto& [k, v] : o["varValues"].Items()) n.varValues[k] = ValueFromJson(v);
        for (auto& e : o["execSmart"].Elements()) if (e.Size() >= 2) n.execSmart[e.At(0).U()] = e.At(1).U();
        if (n.name.empty()) {
            if (auto* m = FindMathOp(n.op); m && n.kind == NodeKind::Math) n.name = m->label;
            if (auto* a = FindExecAction(n.op); a && n.kind == NodeKind::Action) n.name = a->label;
            if (n.kind == NodeKind::VarGet || n.kind == NodeKind::VarSet) n.name = (n.kind == NodeKind::VarGet ? "Lire " : "Définir ") + n.op;
        }
        if ((int)n.inputs.size() < NodeInputCount(n)) n.inputs.resize(NodeInputCount(n), 0);
        g.nodes.push_back(n);
    }
    g.output = jg["output"].U(0);
    g.description = jg["description"].S("");
    for (auto& o : jg["variables"].Elements()) {
        GraphVariable v;
        v.name = o["name"].S("Variable");
        v.value = ValueFromJson(o["value"]);
        v.exposed = o["exposed"].B(true);
        v.desc = o["desc"].S("");
        g.variables.push_back(v);
    }
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
    doc->iccProfile = j["iccProfile"].S("");
    doc->cmykProfile = j["cmykProfile"].S("");
    doc->colorMode = j["colorMode"].I(0);
    doc->kind = j["kind"].S("image") == "paint3d" ? 1 : 0;
    doc->pbrChannels = j["pbrChannels"].U(0x7F);
    doc->exportTemplate = j["exportTemplate"].I(0);
    DocState& s = doc->S();
    // Smart object contents and their inner documents are not surfaces of this document.
    std::set<Id> notSurfaces;
    for (auto& so : j["smartObjects"].Elements()) notSurfaces.insert(so["id"].U());
    for (auto& b : blobs)
        if (!notSurfaces.count(b.id) && b.id < kSmartChildBlobBit) doc->CreateSurfaceWithId(b.id, b.format);

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
        if (o["artboard"].Size() == 4) {
            l.artboard = true;
            l.board = {o["artboard"].At(0).I(), o["artboard"].At(1).I(), o["artboard"].At(2).I(), o["artboard"].At(3).I()};
        }
        l.blend = (BlendMode)std::clamp(o["blend"].I(), 0, (int)BlendMode::Count - 1);
        l.opacity = o["opacity"].F(1);
        l.visible = o["visible"].B(true);
        l.locked = o["locked"].B(false);
        l.mask = o["mask"].U();
        l.maskEnabled = o["maskEnabled"].B(true);
        l.parent = o["parent"].U();
        l.smart = o["smart"].U();
        for (int k = 0; k < 4 && (size_t)k * 2 + 1 < o["smartCorners"].Size(); ++k)
            l.smartCorners[k] = {o["smartCorners"].At(k * 2).F(), o["smartCorners"].At(k * 2 + 1).F()};
        if (o["smartPivot"].Size() == 2) l.smartPivot = {o["smartPivot"].At(0).F(0.5f), o["smartPivot"].At(1).F(0.5f)};
        l.smartInterp = (u8)std::clamp(o["smartInterp"].I(2), 0, 2);
        l.material = o["material"].U();
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
            for (auto& po : t["parts"].Elements()) {
                ShapePart p;
                p.op = po["op"].I(0); p.type = po["type"].I(1); p.rect = Float4FromJson(po["rect"]); p.radius = po["radius"].F(0);
                p.sides = po["sides"].I(5); p.rotation = po["rotation"].F(0);
                l.shape.parts.push_back(p);
            }
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
            for (auto& r : t["runs"].Elements())
                if (r.Size() >= 3) l.text.runs.push_back({r.At(0).I(), r.At(1).I(), r.At(2).U()});
            for (auto& p : t["paraStyles"].Elements()) l.text.paraStyles.push_back(p.U());
            for (size_t k = 0; k + 1 < t["path"].Size(); k += 2) l.text.path.push_back({t["path"].At(k).F(), t["path"].At(k + 1).F()});
            l.text.pathOffset = t["pathOffset"].F(0);
            l.text.pathFlip = t["pathFlip"].B(false);
        }
        if (l.surface && !s.surfaces.count(l.surface)) l.surface = doc->CreateSurface({0, 0, 0, 0});
        if (l.mask && !s.surfaces.count(l.mask)) l.mask = 0;
        s.layers.push_back(l);
    }

    Graph g;
    GraphFromJson(j["graph"], g);
    for (auto& n : g.nodes)
        if (n.surface && n.kind != NodeKind::SubInput && !s.surfaces.count(n.surface)) n.surface = doc->CreateSurface({0, 0, 0, 0});
    doc->functionKind = j["functionKind"].S("");
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
    if (j.Has("trimSheet")) {
        const Json& ts = j["trimSheet"];
        s.trimSheet.padding = ts["padding"].I(0);
        s.trimSheet.show = ts["show"].B(true);
        for (auto& o : ts["trims"].Elements()) {
            Trim t;
            t.name = o["name"].S("Bande");
            const Json& r = o["rect"];
            if (r.Size() >= 4) t.r = {r.At(0).I(), r.At(1).I(), r.At(2).I(), r.At(3).I()};
            t.tileX = o["tileX"].B(true);
            t.tileY = o["tileY"].B(false);
            s.trimSheet.trims.push_back(t);
        }
    }
    for (auto& o : j["charStyles"].Elements()) {
        CharStyle c;
        c.id = o["id"].U(); c.name = o["name"].S("Style"); c.font = o["font"].S(""); c.size = o["size"].F(0);
        c.color = Float4FromJson(o["color"], {0, 0, 0, 1}); c.useColor = o["useColor"].B(true); c.tracking = o["tracking"].F(0);
        c.faux = o["faux"].B(false); c.baseline = o["baseline"].F(0); c.underline = o["underline"].B(false); c.allCaps = o["allCaps"].B(false);
        s.charStyles.push_back(c);
    }
    for (auto& o : j["paraStyles"].Elements()) {
        ParaStyle p;
        p.id = o["id"].U(); p.name = o["name"].S("Style"); p.align = o["align"].I(0); p.lineSpacing = o["lineSpacing"].F(1.2f);
        p.spaceBefore = o["spaceBefore"].F(0); p.indentLeft = o["indentLeft"].F(0); p.indentFirst = o["indentFirst"].F(0); p.charStyle = o["charStyle"].U();
        s.paraStyles.push_back(p);
    }
    for (auto& o : j["layerComps"].Elements()) {
        LayerComp c;
        c.id = o["id"].U();
        c.name = o["name"].S("Composition");
        c.comment = o["comment"].S("");
        c.useVisibility = o["visibility"].B(true);
        c.useAppearance = o["appearance"].B(true);
        c.usePosition = o["position"].B(true);
        for (auto& eo : o["entries"].Elements()) {
            LayerComp::Entry e;
            e.layer = eo["layer"].U();
            e.visible = eo["visible"].B(true);
            e.opacity = eo["opacity"].F(1);
            e.blend = (BlendMode)std::clamp(eo["blend"].I(), 0, (int)BlendMode::Count - 1);
            for (auto& b : eo["filters"].Elements()) e.filterOn.push_back((u8)b.I(1));
            for (int k = 0; k < 4 && (size_t)k * 2 + 1 < eo["corners"].Size(); ++k) e.corners[k] = {eo["corners"].At(k * 2).F(), eo["corners"].At(k * 2 + 1).F()};
            c.entries.push_back(std::move(e));
        }
        s.comps.push_back(std::move(c));
    }
    s.activeComp = j["activeComp"].U();
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
