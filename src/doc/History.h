#pragma once
// Undo/redo. Two kinds of entries:
//  - State: full copy of DocState (layers, graph, surface table). Cheap because
//    pixels live in shared GPU textures, not in the state.
//  - Paint: GPU copies of the touched rectangle before and after a stroke.

#include "doc/Document.h"

namespace sx {

class History {
public:
    void PushState(const std::string& label, const DocState& before, const DocState& after);
    void PushPaint(const std::string& label, Id surface, const Rect& r, gpu::TexturePtr before, gpu::TexturePtr after);

    bool CanUndo() const { return cursor > 0; }
    bool CanRedo() const { return cursor < entries.size(); }
    void Undo(Document& doc, gpu::CommandContext& ctx);
    void Redo(Document& doc, gpu::CommandContext& ctx);
    void Clear() { entries.clear(); cursor = 0; }

    size_t Count() const { return entries.size(); }
    size_t Cursor() const { return cursor; }
    const std::string& Label(size_t i) const { return entries[i].label; }

private:
    struct Entry {
        std::string label;
        bool paint = false;
        DocState before, after;
        Id surface = 0;
        Rect rect;
        gpu::TexturePtr pixelsBefore, pixelsAfter;
        u64 bytes = 0;
    };
    void Push(Entry&& e);
    std::vector<Entry> entries;
    size_t cursor = 0;
    u64 paintBytes = 0;
};

// Helper for structural edits: snapshot on construction, push on Commit().
class Edit {
public:
    Edit(Document& d, History& h, std::string label) : doc(d), hist(h), name(std::move(label)), before(d.S()) {}
    void Commit() { hist.PushState(name, before, doc.S()); doc.dirty = true; }
private:
    Document& doc;
    History& hist;
    std::string name;
    DocState before;
};

}  // namespace sx
