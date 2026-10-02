#include "doc/History.h"

namespace sx {

namespace {
constexpr size_t kMaxEntries = 300;
constexpr u64 kMaxPaintBytes = 2ull << 30;  // 2 GB of VRAM for paint undo
}

void History::Push(Entry&& e) {
    // Drop the redo branch.
    while (entries.size() > cursor) {
        paintBytes -= entries.back().bytes;
        entries.pop_back();
    }
    paintBytes += e.bytes;
    entries.push_back(std::move(e));
    // Trim oldest entries.
    while (entries.size() > kMaxEntries || (paintBytes > kMaxPaintBytes && entries.size() > 1)) {
        paintBytes -= entries.front().bytes;
        entries.erase(entries.begin());
    }
    cursor = entries.size();
}

void History::PushState(const std::string& label, const DocState& before, const DocState& after) {
    Entry e;
    e.label = label;
    e.before = before;
    e.after = after;
    Push(std::move(e));
}

void History::PushPaint(const std::string& label, Id surface, const Rect& r, gpu::TexturePtr before, gpu::TexturePtr after) {
    Entry e;
    e.label = label;
    e.paint = true;
    e.surface = surface;
    e.rect = r;
    e.bytes = (u64)r.Width() * r.Height() * gpu::BytesPerPixel(before->GetFormat()) * 2;
    e.pixelsBefore = std::move(before);
    e.pixelsAfter = std::move(after);
    Push(std::move(e));
}

static void ApplyPaint(Document& doc, gpu::CommandContext& ctx, Id surface, const Rect& r, gpu::Texture* pixels) {
    Surface* s = doc.GetSurface(surface);
    if (!s || !pixels) return;
    ctx.CopyRegion(s->tex.get(), r.x0, r.y0, pixels, {0, 0, r.Width(), r.Height()});
    doc.TouchSurface(surface);
}

void History::Undo(Document& doc, gpu::CommandContext& ctx) {
    if (!CanUndo()) return;
    Entry& e = entries[--cursor];
    if (e.paint) ApplyPaint(doc, ctx, e.surface, e.rect, e.pixelsBefore.get());
    else { doc.S() = e.before; doc.InvalidateSurfaces(); doc.SyncGraph(); }
    doc.dirty = true;
}

void History::Redo(Document& doc, gpu::CommandContext& ctx) {
    if (!CanRedo()) return;
    Entry& e = entries[cursor++];
    if (e.paint) ApplyPaint(doc, ctx, e.surface, e.rect, e.pixelsAfter.get());
    else { doc.S() = e.after; doc.InvalidateSurfaces(); doc.SyncGraph(); }
    doc.dirty = true;
}

}  // namespace sx
