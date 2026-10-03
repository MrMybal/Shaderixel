#pragma once
// Programmable brushes: shaders/brushes/*.slang (see shaders/brush_common.slang
// for the Tip / Dynamics contract and the //@ brush settings).

#include "doc/Effects.h"

namespace sx {

struct BrushDef {
    std::string id;           // file stem
    std::string text;         // full source (metadata + code)
    EffectDefPtr def;         // name, category, params, body (parsed like an effect)
    float spacing = -1;       // //@spacing (fraction of the diameter), -1 = keep the user's
    float airbrush = 0;       // //@airbrush dabs per second while held
    float extent = 1.0f;      // //@extent: reach in radii (scatter, scale)
    bool selfFlow = false;    // //@selfflow
    bool dynamics = false;    // defines Dynamics()
};
using BrushDefPtr = std::shared_ptr<const BrushDef>;

// Parses a brush source (library file or edited code).
BrushDefPtr ParseBrush(const std::string& id, const std::string& text);
// Complete compute shader of a brush.
std::string BuildBrushSource(const BrushDef& b);
constexpr int kMaxBrushDabs = 960;  // per dispatch (2 float4 per dab after color, settings and parameters)

class BrushLibrary {
public:
    void Load(const std::string& dir);
    void ReloadIfChanged();
    void Reload() { Load(dir); }
    BrushDefPtr Find(const std::string& id) const;
    const std::vector<BrushDefPtr>& All() const { return brushes; }
    const std::string& Dir() const { return dir; }

private:
    std::string dir;
    std::vector<BrushDefPtr> brushes;
    u64 shaderGeneration = ~0ull;
};
BrushLibrary& Brushes();

// Template of a new brush (tip + commented dynamics examples).
extern const char* kBrushTemplate;

}  // namespace sx
