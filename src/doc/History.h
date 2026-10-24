#pragma once
// Undo/redo. Two kinds of entries:
//  - State: full copy of DocState (layers, graph, surface table). Cheap because
//    pixels live in shared GPU textures, not in the state.
//  - Paint: GPU copies of the touched rectangle before and after an operation. Older paint steps beyond a
//    GPU budget are read back, compressed in the background and kept in system memory (Maintain), then
//    uploaded again when they are undone or redone.
// Entries pushed between BeginGroup/EndGroup undo and redo as one step.

#include "doc/Document.h"

#include <functional>
#include <future>

namespace sx {

// Memory budgets of the undo history (settings: History panel > Memory settings).
struct HistoryLimits {
    u64 gpuTarget = 768ull << 20;  // older paint steps beyond this move to system memory
    u64 gpuMax = 3ull << 30;       // undo data on the GPU, at most (oldest steps dropped)
    u64 ramMax = 6ull << 30;       // compressed undo pixels in system memory, at most
};

class History {
public:
    struct ColorSettings {
        std::string icc, cmyk;
        int mode = 0;
    };
    static HistoryLimits& Limits();
    void PushState(const std::string& label, const DocState& before, const DocState& after);
    // Profile / CMYK profile / color mode of the document (kept outside DocState).
    void PushColorSettings(const std::string& label, const ColorSettings& before, const ColorSettings& after);
    void PushPaint(const std::string& label, Id surface, const Rect& r, gpu::TexturePtr before, gpu::TexturePtr after);
    // State kept outside the document (e.g. a 3D project's model): apply(true) undoes it, apply(false) redoes it.
    void PushCustom(const std::string& label, std::function<void(bool undo)> apply);
    void BeginGroup() { if (groupDepth++ == 0) currentGroup = ++groupCounter; }
    void EndGroup() { if (--groupDepth == 0) currentGroup = 0; }

    bool CanUndo() const { return cursor > 0; }
    bool CanRedo() const { return cursor < entries.size(); }
    void Undo(Document& doc, gpu::CommandContext& ctx);
    void Redo(Document& doc, gpu::CommandContext& ctx);
    void Clear() { entries.clear(); cursor = 0; gpuBytes = ramBytes = 0; }
    // Drop the entries from n on (a dialog merging its edits into one step).
    void TruncateTo(size_t n) {
        while (entries.size() > n) { Account(entries.back(), -1); entries.pop_back(); }
        if (cursor > entries.size()) cursor = entries.size();
    }
    // Once a frame: moves older paint steps to system memory when the GPU budget is exceeded.
    void Maintain(gpu::CommandContext& ctx);

    size_t Count() const { return entries.size(); }
    size_t Cursor() const { return cursor; }
    const std::string& Label(size_t i) const { return entries[i].label; }
    bool Grouped(size_t i) const { return entries[i].group != 0 && i > 0 && entries[i - 1].group == entries[i].group; }
    u64 MemoryBytes() const { return gpuBytes; }   // undo data on the GPU
    u64 SystemBytes() const { return ramBytes; }   // compressed undo pixels in system memory

    // Pixels of a step kept in system memory (byte-plane shuffled, zlib).
    struct CpuPixels {
        std::vector<u8> z;
        u32 w = 0, h = 0;
        gpu::Format format = gpu::Format::RGBA16F;
        size_t rawSize = 0;
    };
    static CpuPixels Pack(const gpu::Readback& rb);
    static gpu::TexturePtr Unpack(const CpuPixels& c, gpu::CommandContext& ctx);

private:
    enum class Offload : u8 { Gpu, Reading, Packing, Cpu };
    struct Entry {
        std::string label;
        bool paint = false;
        bool color = false;  // document color settings (profile, CMYK profile, mode): not part of DocState
        std::function<void(bool)> custom;
        ColorSettings colorBefore, colorAfter;
        DocState before, after;
        Id surface = 0;
        Rect rect;
        gpu::TexturePtr pixelsBefore, pixelsAfter;
        u64 bytes = 0;  // on the GPU
        u32 group = 0;
        Offload offload = Offload::Gpu;
        gpu::ReadbackPtr rbBefore, rbAfter;
        std::future<std::pair<CpuPixels, CpuPixels>> packing;
        CpuPixels cpuBefore, cpuAfter;
    };
    void Account(const Entry& e, int sign);
    void Push(Entry&& e);
    void UndoOne(Document& doc, gpu::CommandContext& ctx);
    void RedoOne(Document& doc, gpu::CommandContext& ctx);
    std::vector<Entry> entries;
    size_t cursor = 0;
    u64 gpuBytes = 0, ramBytes = 0;
    u32 groupCounter = 0, currentGroup = 0;
    int groupDepth = 0;
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
