#include "io/PsdExport.h"
#include "render/Renderer.h"

#include <cstdio>
#include <cstring>
#include <functional>

namespace sx {

namespace {
const char* BlendKey(BlendMode m) {
    static const char* keys[] = {"norm", "mul ", "scrn", "over", "lddg", "fsub", "dark", "lite", "diff", "div ", "idiv", "hLit", "sLit", "diss",
                                 "lbrn", "vLit", "lLit", "pLit", "hMix", "smud", "fdiv", "hue ", "sat ", "colr", "lum ", "dkCl", "lgCl"};
    static_assert(sizeof(keys) / sizeof(keys[0]) == (size_t)BlendMode::Count);
    return (size_t)m < (size_t)BlendMode::Count ? keys[(size_t)m] : "norm";
}

struct Writer {
    std::vector<u8> b;
    void U8(u8 v) { b.push_back(v); }
    void U16(u16 v) { U8((u8)(v >> 8)); U8((u8)v); }
    void U32(u32 v) { U16((u16)(v >> 16)); U16((u16)v); }
    void I32(i32 v) { U32((u32)v); }
    void Tag(const char* t) { b.insert(b.end(), t, t + 4); }
    void Bytes(const void* p, size_t n) { b.insert(b.end(), (const u8*)p, (const u8*)p + n); }
    size_t Reserve32() { size_t at = b.size(); U32(0); return at; }
    void Patch32(size_t at, u32 v) { b[at] = (u8)(v >> 24); b[at + 1] = (u8)(v >> 16); b[at + 2] = (u8)(v >> 8); b[at + 3] = (u8)v; }
    void Pad(size_t from, size_t multiple) { while ((b.size() - from) % multiple) U8(0); }
};

// PackBits one row.
void PackRow(const u8* p, int n, std::vector<u8>& out) {
    int i = 0;
    while (i < n) {
        int run = 1;
        while (i + run < n && run < 128 && p[i + run] == p[i]) ++run;
        if (run >= 2) {
            out.push_back((u8)(1 - run));
            out.push_back(p[i]);
            i += run;
            continue;
        }
        int start = i, lit = 0;
        while (i < n && lit < 128) {
            if (i + 1 < n && p[i] == p[i + 1]) break;
            ++i;
            ++lit;
        }
        out.push_back((u8)(lit - 1));
        out.insert(out.end(), p + start, p + start + lit);
    }
}

// RLE-compressed channel block: u16 compression, row lengths, data.
std::vector<u8> EncodeChannel(const std::vector<u8>& plane, int w, int h) {
    std::vector<u8> out;
    out.push_back(0);
    out.push_back(1);
    if (w <= 0 || h <= 0) return out;
    std::vector<u8> data;
    std::vector<u16> lens(h);
    for (int y = 0; y < h; ++y) {
        size_t before = data.size();
        PackRow(&plane[(size_t)y * w], w, data);
        lens[y] = (u16)(data.size() - before);
    }
    for (u16 l : lens) { out.push_back((u8)(l >> 8)); out.push_back((u8)l); }
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

Rect AlphaBounds(const gpu::Readback& rb, int channel, u8 background) {
    Rect r{(int)rb.width, (int)rb.height, 0, 0};
    for (u32 y = 0; y < rb.height; ++y) {
        const u8* row = &rb.data[(size_t)y * rb.width * 4];
        for (u32 x = 0; x < rb.width; ++x)
            if (row[x * 4 + channel] != background) {
                r.x0 = std::min(r.x0, (int)x); r.x1 = std::max(r.x1, (int)x + 1);
                r.y0 = std::min(r.y0, (int)y); r.y1 = std::max(r.y1, (int)y + 1);
            }
    }
    return r.Empty() ? Rect{} : r;
}

std::vector<u8> Plane(const gpu::Readback& rb, const Rect& r, int channel) {
    std::vector<u8> p((size_t)r.Width() * r.Height());
    for (int y = 0; y < r.Height(); ++y)
        for (int x = 0; x < r.Width(); ++x) p[(size_t)y * r.Width() + x] = rb.data[((size_t)(r.y0 + y) * rb.width + r.x0 + x) * 4 + channel];
    return p;
}

void PascalName(Writer& w, const std::string& utf8) {
    // Latin-1 approximation of the name (the UTF-16 'luni' block carries the real one).
    std::wstring ws = Widen(utf8);
    std::string s;
    for (wchar_t c : ws) s += c < 256 ? (char)c : '?';
    if (s.size() > 255) s.resize(255);
    size_t start = w.b.size();
    w.U8((u8)s.size());
    w.Bytes(s.data(), s.size());
    w.Pad(start, 4);
}

void Luni(Writer& w, const std::string& utf8) {
    std::wstring ws = Widen(utf8);
    w.Tag("8BIM");
    w.Tag("luni");
    u32 len = 4 + 2 * (u32)ws.size();
    u32 padded = (len + 3) & ~3u;
    w.U32(padded);
    w.U32((u32)ws.size());
    for (wchar_t c : ws) w.U16((u16)c);
    for (u32 i = len; i < padded; ++i) w.U8(0);
}
}  // namespace

bool StartPsdExport(Document& doc, Renderer& r, gpu::CommandContext& ctx, PsdExportJob& job) {
    job.width = doc.Width();
    job.height = doc.Height();
    job.layers.clear();
    job.composite = r.ExportRGBA8(ctx, r.EvaluateComplete(doc, ctx));
    if (!job.composite) return false;
    const auto& S = doc.S();
    if (S.mode != DocMode::Layers) {
        // Node mode: a single flattened layer.
        PsdExportLayer l;
        l.name = "Image";
        l.pixels = job.composite;
        job.layers.push_back(l);
        return true;
    }
    // Content node of each layer = top input of its blend node.
    auto contentOf = [&](Id layer) -> Id {
        for (auto& n : S.graph.nodes)
            if (n.kind == NodeKind::Blend && n.layer == layer && n.inputs.size() > 1) return n.inputs[1];
        return 0;
    };
    std::function<void(Id)> emit = [&](Id parent) {
        for (const Layer& l : S.layers) {
            if (l.parent != parent) continue;
            PsdExportLayer e;
            e.name = l.name;
            e.blend = l.blend;
            e.opacity = (u8)std::clamp((int)std::lround(l.opacity * 255.0f), 0, 255);
            e.visible = l.visible;
            e.clipped = l.clipped;
            e.maskDisabled = !l.maskEnabled;
            if (l.mask)
                if (Surface* m = doc.GetSurface(l.mask)) e.mask = r.ExportRGBA8(ctx, m->tex.get(), false);
            if (l.kind == LayerKind::Group) {
                PsdExportLayer end;
                end.kind = PsdExportLayer::GroupEnd;
                end.name = "</Layer group>";
                job.layers.push_back(end);
                emit(l.id);
                e.kind = PsdExportLayer::GroupBegin;
                e.passThrough = l.passThrough;
                job.layers.push_back(e);
                continue;
            }
            if (Id c = contentOf(l.id))
                if (gpu::Texture* t = r.EvaluateNode(doc, ctx, c)) e.pixels = r.ExportRGBA8(ctx, t);
            job.layers.push_back(e);
        }
    };
    emit(0);
    r.DropNodeCaches(doc);
    return true;
}

bool PsdExportReady(const PsdExportJob& job) {
    if (!job.composite || !job.composite->ready) return false;
    for (auto& l : job.layers) {
        if (l.pixels && !l.pixels->ready) return false;
        if (l.mask && !l.mask->ready) return false;
    }
    return true;
}

bool WritePsd(const PsdExportJob& job, std::string* error) {
    const int W = (int)job.width, H = (int)job.height;
    if (W > 30000 || H > 30000) {
        if (error) *error = "PSD limité à 30000 px";
        return false;
    }
    Writer w;
    w.Tag("8BPS");
    w.U16(1);
    for (int i = 0; i < 6; ++i) w.U8(0);
    w.U16(4);  // RGB + transparency
    w.U32((u32)H);
    w.U32((u32)W);
    w.U16(8);
    w.U16(3);  // RGB
    w.U32(0);  // color mode data
    w.U32(0);  // image resources

    // Layer and mask information.
    size_t lmiLen = w.Reserve32();
    size_t lmiStart = w.b.size();
    size_t liLen = w.Reserve32();
    size_t liStart = w.b.size();
    w.U16((u16)(-(i16)job.layers.size()));  // negative: merged image has transparency

    struct Encoded {
        Rect r, mr;
        std::vector<std::pair<i16, std::vector<u8>>> channels;
    };
    std::vector<Encoded> enc(job.layers.size());
    for (size_t i = 0; i < job.layers.size(); ++i) {
        const auto& l = job.layers[i];
        Encoded& e = enc[i];
        if (l.kind == PsdExportLayer::Pixels && l.pixels && l.pixels->width == (u32)W) e.r = AlphaBounds(*l.pixels, 3, 0);
        const int cw = e.r.Width(), ch = e.r.Height();
        static const int order[4] = {3, 0, 1, 2};
        static const i16 ids[4] = {-1, 0, 1, 2};
        for (int c = 0; c < 4; ++c) {
            std::vector<u8> plane = e.r.Empty() ? std::vector<u8>{} : Plane(*l.pixels, e.r, order[c]);
            e.channels.push_back({ids[c], EncodeChannel(plane, cw, ch)});
        }
        if (l.mask && l.mask->width == (u32)W) {
            e.mr = AlphaBounds(*l.mask, 0, 255);
            std::vector<u8> plane = e.mr.Empty() ? std::vector<u8>{} : Plane(*l.mask, e.mr, 0);
            e.channels.push_back({-2, EncodeChannel(plane, e.mr.Width(), e.mr.Height())});
        }
    }
    for (size_t i = 0; i < job.layers.size(); ++i) {
        const auto& l = job.layers[i];
        const Encoded& e = enc[i];
        w.I32(e.r.y0);
        w.I32(e.r.x0);
        w.I32(e.r.y1);
        w.I32(e.r.x1);
        w.U16((u16)e.channels.size());
        for (auto& [id, data] : e.channels) {
            w.U16((u16)id);
            w.U32((u32)data.size());
        }
        w.Tag("8BIM");
        w.Tag(l.kind == PsdExportLayer::GroupBegin && l.passThrough ? "pass" : BlendKey(l.blend));
        w.U8(l.opacity);
        w.U8(l.clipped ? 1 : 0);
        u8 flags = 8;  // bit 4 is meaningful
        if (!l.visible) flags |= 2;
        if (l.kind != PsdExportLayer::Pixels) flags |= 16;  // pixel data irrelevant
        w.U8(flags);
        w.U8(0);
        size_t extraLen = w.Reserve32();
        size_t extraStart = w.b.size();
        if (l.mask && l.mask->width == (u32)W) {
            w.U32(20);
            w.I32(e.mr.y0);
            w.I32(e.mr.x0);
            w.I32(e.mr.y1);
            w.I32(e.mr.x1);
            w.U8(255);                        // default color outside the rect
            w.U8(l.maskDisabled ? 2 : 0);     // flags
            w.U16(0);
        } else {
            w.U32(0);
        }
        w.U32(0);  // blending ranges
        PascalName(w, l.name);
        Luni(w, l.name);
        if (l.kind != PsdExportLayer::Pixels) {
            w.Tag("8BIM");
            w.Tag("lsct");
            w.U32(12);
            w.U32(l.kind == PsdExportLayer::GroupBegin ? 1u : 3u);
            w.Tag("8BIM");
            w.Tag(l.kind == PsdExportLayer::GroupBegin && l.passThrough ? "pass" : BlendKey(l.blend));
        }
        w.Patch32(extraLen, (u32)(w.b.size() - extraStart));
    }
    for (auto& e : enc)
        for (auto& [id, data] : e.channels) w.Bytes(data.data(), data.size());
    w.Pad(liStart, 2);
    w.Patch32(liLen, (u32)(w.b.size() - liStart));
    w.U32(0);  // global layer mask info
    w.Patch32(lmiLen, (u32)(w.b.size() - lmiStart));

    // Merged image: RLE, all channels' row lengths first, then data.
    const gpu::Readback& c = *job.composite;
    w.U16(1);
    std::vector<std::vector<u8>> rows;
    std::vector<u16> lens;
    for (int ch : {0, 1, 2, 3}) {
        std::vector<u8> row(W);
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) row[x] = c.data[((size_t)y * W + x) * 4 + ch];
            std::vector<u8> packed;
            PackRow(row.data(), W, packed);
            lens.push_back((u16)packed.size());
            rows.push_back(std::move(packed));
        }
    }
    for (u16 l : lens) w.U16(l);
    for (auto& r : rows) w.Bytes(r.data(), r.size());

    FILE* f = nullptr;
    if (_wfopen_s(&f, Widen(job.path).c_str(), L"wb") != 0 || !f) {
        if (error) *error = "Impossible d'écrire " + job.path;
        return false;
    }
    bool ok = fwrite(w.b.data(), 1, w.b.size(), f) == w.b.size();
    fclose(f);
    if (!ok && error) *error = "Erreur d'écriture";
    return ok;
}

}  // namespace sx
