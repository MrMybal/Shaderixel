#include "doc/History.h"
#include "core/Deflate.h"

#include <stb_image.h>

namespace sx {

namespace {
constexpr size_t kMaxEntries = 300;
constexpr size_t kKeepOnGpu = 2;           // the latest steps always stay on the GPU (instant undo)

u64 TextureBytes(const gpu::TexturePtr& t) {
    return t ? (u64)t->Width() * t->Height() * gpu::BytesPerPixel(t->GetFormat()) : 0;
}

// Pixels grouped by byte position (all first bytes, all second bytes...): half floats compress far better.
std::vector<u8> Shuffle(const std::vector<u8>& in, u32 bpp) {
    std::vector<u8> out(in.size());
    const size_t n = bpp ? in.size() / bpp : 0;
    for (u32 b = 0; b < bpp; ++b)
        for (size_t i = 0; i < n; ++i) out[b * n + i] = in[i * bpp + b];
    return out;
}

std::vector<u8> Unshuffle(const std::vector<u8>& in, u32 bpp) {
    std::vector<u8> out(in.size());
    const size_t n = bpp ? in.size() / bpp : 0;
    for (u32 b = 0; b < bpp; ++b)
        for (size_t i = 0; i < n; ++i) out[i * bpp + b] = in[b * n + i];
    return out;
}
}  // namespace

HistoryLimits& History::Limits() {
    static HistoryLimits limits;
    return limits;
}

History::CpuPixels History::Pack(const gpu::Readback& rb) {
    CpuPixels c;
    c.w = rb.width;
    c.h = rb.height;
    c.format = rb.format;
    c.rawSize = rb.data.size();
    // Compressed in chunks (each: raw size, compressed size, zlib stream): the decoder takes int sizes.
    const std::vector<u8> sh = Shuffle(rb.data, gpu::BytesPerPixel(rb.format));
    constexpr size_t kChunk = 256u << 20;
    for (size_t off = 0; off < sh.size(); off += kChunk) {
        const u32 raw = (u32)std::min(kChunk, sh.size() - off);
        const std::vector<u8> z = deflate::ZlibCompress(sh.data() + off, raw, 1);
        const u32 zs = (u32)z.size();
        const size_t at = c.z.size();
        c.z.resize(at + 8 + z.size());
        memcpy(&c.z[at], &raw, 4);
        memcpy(&c.z[at + 4], &zs, 4);
        memcpy(&c.z[at + 8], z.data(), z.size());
    }
    return c;
}

gpu::TexturePtr History::Unpack(const CpuPixels& c, gpu::CommandContext& ctx) {
    std::vector<u8> raw(c.rawSize);
    size_t in = 0, out = 0;
    while (in + 8 <= c.z.size() && out < raw.size()) {
        u32 rs, zs;
        memcpy(&rs, &c.z[in], 4);
        memcpy(&zs, &c.z[in + 4], 4);
        if (in + 8 + zs > c.z.size() || out + rs > raw.size()) return nullptr;
        const int got = stbi_zlib_decode_buffer((char*)raw.data() + out, (int)rs, (const char*)c.z.data() + in + 8, (int)zs);
        if (got != (int)rs) return nullptr;
        in += 8 + zs;
        out += rs;
    }
    if (out != raw.size()) return nullptr;
    raw = Unshuffle(raw, gpu::BytesPerPixel(c.format));
    auto t = gpu::Dev().CreateTexture(c.w, c.h, c.format, "UndoPixels");
    ctx.Upload(t.get(), raw.data(), c.w * gpu::BytesPerPixel(c.format));
    return t;
}

void History::Push(Entry&& e) {
    e.group = currentGroup;
    // Drop the redo branch.
    while (entries.size() > cursor) {
        Account(entries.back(), -1);
        entries.pop_back();
    }
    Account(e, 1);
    entries.push_back(std::move(e));
    // Trim oldest entries (whole groups).
    const HistoryLimits& lim = Limits();
    while (entries.size() > 1 && (entries.size() > kMaxEntries || gpuBytes > lim.gpuMax || ramBytes > lim.ramMax)) {
        u32 g = entries.front().group;
        if (g && g == currentGroup) break;  // never the start of the group being recorded (one step for every layer)
        do {
            Account(entries.front(), -1);
            entries.erase(entries.begin());
        } while (g && entries.size() > 1 && entries.front().group == g);
    }
    cursor = entries.size();
}

void History::Account(const Entry& e, int sign) {
    const u64 gpu = e.bytes, ram = e.cpuBefore.z.size() + e.cpuAfter.z.size();
    if (sign > 0) {
        gpuBytes += gpu;
        ramBytes += ram;
    } else {
        gpuBytes -= std::min(gpuBytes, gpu);
        ramBytes -= std::min(ramBytes, ram);
    }
}

void History::PushState(const std::string& label, const DocState& before, const DocState& after) {
    Entry e;
    e.label = label;
    e.before = before;
    e.after = after;
    // Surfaces created by this step are owned (memory-wise) by it.
    for (auto& [id, s] : after.surfaces)
        if (!before.surfaces.count(id)) e.bytes += TextureBytes(s.tex);
    Push(std::move(e));
}

void History::PushPaint(const std::string& label, Id surface, const Rect& r, gpu::TexturePtr before, gpu::TexturePtr after) {
    Entry e;
    e.label = label;
    e.paint = true;
    e.surface = surface;
    e.rect = r;
    e.bytes = TextureBytes(before) + TextureBytes(after);
    e.pixelsBefore = std::move(before);
    e.pixelsAfter = std::move(after);
    Push(std::move(e));
}

void History::Maintain(gpu::CommandContext& ctx) {
    // Older paint steps beyond the GPU budget: read back, compressed in the background, then their GPU copies freed.
    int started = 0;
    // GPU bytes that stay once the transfers in flight are done.
    u64 staying = gpuBytes;
    for (const Entry& e : entries)
        if (e.offload == Offload::Reading || e.offload == Offload::Packing) staying -= std::min(staying, e.bytes);
    for (size_t i = 0; i < entries.size(); ++i) {
        Entry& e = entries[i];
        if (!e.paint) continue;
        if (e.offload == Offload::Reading && (!e.rbBefore || e.rbBefore->ready) && (!e.rbAfter || e.rbAfter->ready)) {
            auto rbB = e.rbBefore, rbA = e.rbAfter;
            e.packing = std::async(std::launch::async, [rbB, rbA] {
                std::pair<CpuPixels, CpuPixels> out;
                if (rbB) out.first = Pack(*rbB);
                if (rbA) out.second = Pack(*rbA);
                return out;
            });
            e.offload = Offload::Packing;
        } else if (e.offload == Offload::Packing && e.packing.valid() && e.packing.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            Account(e, -1);
            auto packed = e.packing.get();
            e.cpuBefore = std::move(packed.first);
            e.cpuAfter = std::move(packed.second);
            e.pixelsBefore.reset();
            e.pixelsAfter.reset();
            e.rbBefore.reset();
            e.rbAfter.reset();
            e.bytes = 0;
            e.offload = Offload::Cpu;
            Account(e, 1);
        } else if (e.offload == Offload::Gpu && staying > Limits().gpuTarget && i + kKeepOnGpu < entries.size() && started < 2) {
            staying -= std::min(staying, e.bytes);
            if (e.pixelsBefore) e.rbBefore = ctx.ReadbackTexture(e.pixelsBefore.get());
            if (e.pixelsAfter) e.rbAfter = ctx.ReadbackTexture(e.pixelsAfter.get());
            e.offload = Offload::Reading;
            ++started;
        }
    }
}

static void ApplyPaint(Document& doc, gpu::CommandContext& ctx, Id surface, const Rect& r, gpu::Texture* pixels) {
    Surface* s = doc.GetSurface(surface);
    if (!s || !pixels) return;
    ctx.CopyRegion(s->tex.get(), r.x0, r.y0, pixels, {0, 0, r.Width(), r.Height()});
    doc.TouchSurface(surface, &r);
}

// The pixels of a step: on the GPU, or uploaded back from system memory.
static gpu::TexturePtr StepPixels(const gpu::TexturePtr& gpuCopy, const History::CpuPixels& cpu, gpu::CommandContext& ctx) {
    if (gpuCopy) return gpuCopy;
    if (!cpu.z.empty()) return History::Unpack(cpu, ctx);
    return nullptr;
}

void History::UndoOne(Document& doc, gpu::CommandContext& ctx) {
    Entry& e = entries[--cursor];
    if (e.paint) {
        gpu::TexturePtr t = StepPixels(e.pixelsBefore, e.cpuBefore, ctx);
        ApplyPaint(doc, ctx, e.surface, e.rect, t.get());
    } else {
        doc.S() = e.before;
        doc.InvalidateSurfaces();
        doc.SyncGraph();
    }
}

void History::RedoOne(Document& doc, gpu::CommandContext& ctx) {
    Entry& e = entries[cursor++];
    if (e.paint) {
        gpu::TexturePtr t = StepPixels(e.pixelsAfter, e.cpuAfter, ctx);
        ApplyPaint(doc, ctx, e.surface, e.rect, t.get());
    } else {
        doc.S() = e.after;
        doc.InvalidateSurfaces();
        doc.SyncGraph();
    }
}

void History::Undo(Document& doc, gpu::CommandContext& ctx) {
    if (!CanUndo()) return;
    u32 g = entries[cursor - 1].group;
    UndoOne(doc, ctx);
    while (g && cursor > 0 && entries[cursor - 1].group == g) UndoOne(doc, ctx);
    doc.dirty = true;
}

void History::Redo(Document& doc, gpu::CommandContext& ctx) {
    if (!CanRedo()) return;
    u32 g = entries[cursor].group;
    RedoOne(doc, ctx);
    while (g && cursor < entries.size() && entries[cursor].group == g) RedoOne(doc, ctx);
    doc.dirty = true;
}

}  // namespace sx
