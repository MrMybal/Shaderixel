#include "doc/History.h"

namespace sx {

namespace {
constexpr size_t kMaxEntries = 300;
constexpr u64 kMaxBytes = 3ull << 30;  // VRAM budget for undo data

u64 TextureBytes(const gpu::TexturePtr& t) {
    return t ? (u64)t->Width() * t->Height() * gpu::BytesPerPixel(t->GetFormat()) : 0;
}
}  // namespace

void History::Push(Entry&& e) {
    e.group = currentGroup;
    // Drop the redo branch.
    while (entries.size() > cursor) {
        bytes -= entries.back().bytes;
        entries.pop_back();
    }
    bytes += e.bytes;
    entries.push_back(std::move(e));
    // Trim oldest entries (whole groups).
    while (entries.size() > 1 && (entries.size() > kMaxEntries || bytes > kMaxBytes)) {
        u32 g = entries.front().group;
        do {
            bytes -= entries.front().bytes;
            entries.erase(entries.begin());
        } while (g && entries.size() > 1 && entries.front().group == g);
    }
    cursor = entries.size();
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

static void ApplyPaint(Document& doc, gpu::CommandContext& ctx, Id surface, const Rect& r, gpu::Texture* pixels) {
    Surface* s = doc.GetSurface(surface);
    if (!s || !pixels) return;
    ctx.CopyRegion(s->tex.get(), r.x0, r.y0, pixels, {0, 0, r.Width(), r.Height()});
    doc.TouchSurface(surface);
}

void History::UndoOne(Document& doc, gpu::CommandContext& ctx) {
    Entry& e = entries[--cursor];
    if (e.paint) ApplyPaint(doc, ctx, e.surface, e.rect, e.pixelsBefore.get());
    else { doc.S() = e.before; doc.InvalidateSurfaces(); doc.SyncGraph(); }
}

void History::RedoOne(Document& doc, gpu::CommandContext& ctx) {
    Entry& e = entries[cursor++];
    if (e.paint) ApplyPaint(doc, ctx, e.surface, e.rect, e.pixelsAfter.get());
    else { doc.S() = e.after; doc.InvalidateSurfaces(); doc.SyncGraph(); }
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
