#include "doc/Document.h"
#include "doc/EffectGraph.h"

#include <functional>
#include <set>

namespace sx {

namespace {
const char* kBlendNames[] = {
    "Normal", "Produit", "Superposition (écran)", "Incrustation", "Densité linéaire - (Ajout)", "Soustraction",
    "Obscurcir", "Éclaircir", "Différence", "Densité couleur -", "Densité couleur +", "Lumière crue",
    "Lumière tamisée", "Fondu", "Densité linéaire +", "Lumière vive", "Lumière linéaire", "Lumière ponctuelle",
    "Mélange maximal", "Exclusion", "Division", "Teinte", "Saturation", "Couleur", "Luminosité",
    "Couleur plus foncée", "Couleur plus claire"};

constexpr Id kOutputId = 1;
constexpr Id kGenBase = 0x40000000u;
Id GenId(Id layer, u32 role) { return kGenBase | (layer << 5) | role; }
enum Role : u32 { RoleSource = 0, RoleFilter = 1, RoleAdjust = 16, RoleBlend = 17, RoleMask = 18 };
}  // namespace

const int kBlendMenuOrder[] = {0, 13, -1, 6, 1, 10, 14, 25, -1, 7, 2, 9, 4, 26, -1, 3, 12, 11, 15, 16, 17, 18, -1, 8, 19, 5, 20, -1, 21, 22, 23, 24};
const int kBlendMenuCount = (int)(sizeof(kBlendMenuOrder) / sizeof(int));

const char* BlendModeName(BlendMode m) {
    int i = (int)m;
    return i >= 0 && i < (int)BlendMode::Count ? kBlendNames[i] : "?";
}

gpu::Format SurfaceFormat(Precision p) {
    return p == Precision::Float32 ? gpu::Format::RGBA32F : gpu::Format::RGBA16F;
}

EffectDefPtr EffectInstance::Def() const {
    if (!graph.empty()) {
        if (!cachedDef || cachedCode != graph) {
            EffectGraph g;
            if (EffectGraph::Deserialize(graph, g)) {
                EGCompileResult r = CompileEffectGraph(g);
                auto def = std::make_shared<EffectDef>(*ParseEffect("", r.code));
                if (!r.error.empty()) def->error = r.error;
                cachedDef = def;
            } else {
                auto def = std::make_shared<EffectDef>();
                def->error = "graphe illisible";
                cachedDef = def;
            }
            cachedCode = graph;
        }
        return cachedDef;
    }
    if (!customCode.empty()) {
        if (!cachedDef || cachedCode != customCode) {
            cachedDef = ParseEffect("", customCode);
            cachedCode = customCode;
        }
        return cachedDef;
    }
    return Effects().Find(effectId);
}

void EffectInstance::SyncParams(const EffectDefPtr& oldDef) {
    EffectDefPtr nd = Def();
    if (!nd) return;
    std::vector<float4> np = nd->DefaultParams();
    std::vector<CurvePoints> nc(nd->params.size());
    std::vector<Lut3DPtr> nl(nd->params.size());
    for (size_t i = 0; i < nd->params.size(); ++i) {
        if (!oldDef) continue;
        for (size_t k = 0; k < oldDef->params.size(); ++k) {
            if (oldDef->params[k].id != nd->params[i].id || oldDef->params[k].type != nd->params[i].type) continue;
            if (k < params.size()) np[i] = params[k];
            if (k < curves.size()) nc[i] = curves[k];
            if (k < luts.size()) nl[i] = luts[k];
        }
    }
    params = np;
    curves = nc;
    luts = nl;
}

int NodeInputCount(const Node& n) {
    switch (n.kind) {
        case NodeKind::Output: return 1;
        case NodeKind::Surface: return 0;
        case NodeKind::Blend: return 3;
        case NodeKind::Effect: {
            auto d = n.fx.Def();
            return d ? (int)d->inputs.size() : (int)n.inputs.size();
        }
    }
    return 0;
}

std::string NodeInputName(const Node& n, int i) {
    switch (n.kind) {
        case NodeKind::Output: return "Image";
        case NodeKind::Blend: return i == 0 ? "Fond" : i == 1 ? "Calque" : "Masque";
        case NodeKind::Effect: {
            auto d = n.fx.Def();
            if (d && i < (int)d->inputs.size()) return d->inputs[i];
            return "Entrée";
        }
        default: return "";
    }
}

Node* Graph::Find(Id id) {
    for (auto& n : nodes) if (n.id == id) return &n;
    return nullptr;
}
const Node* Graph::Find(Id id) const {
    for (auto& n : nodes) if (n.id == id) return &n;
    return nullptr;
}

bool Graph::WouldCycle(Id from, Id to) const {
    // Linking from -> to creates a cycle if `to` is reachable upstream of `from`.
    std::vector<Id> stack{from};
    std::set<Id> seen;
    while (!stack.empty()) {
        Id cur = stack.back();
        stack.pop_back();
        if (cur == to) return true;
        if (!seen.insert(cur).second) continue;
        if (auto* n = Find(cur))
            for (Id in : n->inputs) if (in) stack.push_back(in);
    }
    return false;
}

// ------------------------------------------------------------------ document

Document::Document(u32 w, u32 h, Precision p, DocMode mode, const std::string& name_)
    : precision(p), name(name_) {
    state.width = w;
    state.height = h;
    tiled = (u64)w * h > 6000ull * 6000ull;  // ~36 MP: switch to tiled rendering
    state.mode = mode;
    Node out;
    out.id = kOutputId;
    out.kind = NodeKind::Output;
    out.name = "Sortie";
    out.inputs = {0};
    state.graph.nodes.push_back(out);
    state.graph.output = kOutputId;
}

Id Document::CreateSurface(const float4& clear) {
    Id id = NewId();
    Surface s;
    s.tex = gpu::Dev().CreateTexture(state.width, state.height, SurfaceFormat(precision), "Surface");
    s.version = ++versionCounter;
    state.surfaces[id] = s;
    pendingClears.push_back({id, clear});
    return id;
}

Id Document::CreateSurfaceRaw(gpu::Format f, const char* tname) {
    Id id = NewId();
    Surface s;
    s.tex = gpu::Dev().CreateTexture(state.width, state.height, f, tname);
    s.version = ++versionCounter;
    state.surfaces[id] = s;
    return id;
}

void Document::CreateSurfaceWithId(Id id, gpu::Format f) {
    Surface s;
    s.tex = gpu::Dev().CreateTexture(state.width, state.height, f, "Surface");
    s.version = ++versionCounter;
    state.surfaces[id] = s;
    state.nextId = std::max(state.nextId, id + 1);
}

Surface* Document::GetSurface(Id id) {
    auto it = state.surfaces.find(id);
    return it == state.surfaces.end() ? nullptr : &it->second;
}

void Document::TouchSurface(Id id, const Rect* rect) {
    if (auto* s = GetSurface(id)) s->version = ++versionCounter;
    Rect r = rect ? rect->Clipped((int)Width(), (int)Height()) : Rect{0, 0, (int)Width(), (int)Height()};
    dirtyRects[id].Merge(r);
    dirty = true;
}

void Document::InvalidateSurfaces() {
    for (auto& [id, s] : state.surfaces) s.version = ++versionCounter;
}

Layer* Document::FindLayer(Id id) {
    for (auto& l : state.layers) if (l.id == id) return &l;
    return nullptr;
}

int Document::LayerIndex(Id id) const {
    for (size_t i = 0; i < state.layers.size(); ++i) if (state.layers[i].id == id) return (int)i;
    return -1;
}

// Layer list layout: bottom -> top. A group entry comes right AFTER (above) its
// contiguous block of descendants, like Photoshop's panel read upside down.

bool Document::IsDescendant(Id layer, Id group) const {
    const Layer* l = nullptr;
    for (auto& x : state.layers) if (x.id == layer) l = &x;
    while (l && l->parent) {
        if (l->parent == group) return true;
        const Layer* p = nullptr;
        for (auto& x : state.layers) if (x.id == l->parent) p = &x;
        l = p;
    }
    return false;
}

int Document::LayerDepth(Id id) const {
    int d = 0;
    const Layer* l = nullptr;
    for (auto& x : state.layers) if (x.id == id) l = &x;
    while (l && l->parent) {
        ++d;
        const Layer* p = nullptr;
        for (auto& x : state.layers) if (x.id == l->parent) p = &x;
        l = p;
    }
    return d;
}

int Document::BlockStart(Id id) const {
    int i = LayerIndex(id);
    if (i < 0) return -1;
    int s = i;
    if (state.layers[i].kind == LayerKind::Group)
        while (s > 0 && IsDescendant(state.layers[s - 1].id, id)) --s;
    return s;
}

static int InsertIndex(Document& d, int index, Id* parent) {
    const DocState& s = d.S();
    if (index >= 0) return std::min(index, (int)s.layers.size());
    // Default: above the active layer (or its whole group block), same parent.
    for (size_t i = 0; i < s.layers.size(); ++i)
        if (s.layers[i].id == s.activeLayer) {
            if (parent) *parent = s.layers[i].parent;
            return (int)i + 1;
        }
    return (int)s.layers.size();
}

Layer& Document::AddPixelLayer(const std::string& lname, int index) {
    Layer l;
    l.id = NewId();
    l.name = lname;
    l.kind = LayerKind::Pixel;
    l.surface = CreateSurface({0, 0, 0, 0});
    int at = InsertIndex(*this, index, &l.parent);
    state.layers.insert(state.layers.begin() + at, l);
    state.activeLayer = l.id;
    state.editMask = false;
    SyncGraph();
    return state.layers[at];
}

Layer& Document::AddTextLayer(const TextStyle& style) {
    Layer l;
    l.id = NewId();
    l.kind = LayerKind::Text;
    l.text = style;
    l.name = style.content.substr(0, std::min<size_t>(style.content.find('\n'), 32));
    if (l.name.empty()) l.name = "Texte";
    l.surface = CreateSurface({0, 0, 0, 0});
    int at = InsertIndex(*this, -1, &l.parent);
    state.layers.insert(state.layers.begin() + at, l);
    state.activeLayer = l.id;
    state.editMask = false;
    SyncGraph();
    return state.layers[at];
}

Layer& Document::AddShapeLayer(const ShapeStyle& style) {
    Layer l;
    l.id = NewId();
    l.kind = LayerKind::Shape;
    l.shape = style;
    static const char* names[] = {"Rectangle", "Ellipse", "Ligne", "Polygone", "Étoile"};
    l.name = names[std::clamp(style.type, 0, 4)];
    l.surface = CreateSurface({0, 0, 0, 0});
    int at = InsertIndex(*this, -1, &l.parent);
    state.layers.insert(state.layers.begin() + at, l);
    state.activeLayer = l.id;
    state.editMask = false;
    SyncGraph();
    return state.layers[at];
}

Layer& Document::AddEffectLayer(LayerKind kind, const std::string& effectId, int index) {
    Layer l;
    l.id = NewId();
    l.kind = kind;
    l.effect.effectId = effectId;
    auto def = Effects().Find(effectId);
    l.name = def ? def->name : effectId;
    if (def) l.effect.params = def->DefaultParams();
    int at = InsertIndex(*this, index, &l.parent);
    state.layers.insert(state.layers.begin() + at, l);
    state.activeLayer = l.id;
    state.editMask = false;
    SyncGraph();
    return state.layers[at];
}

Layer& Document::GroupLayer(Id id) {
    int i = LayerIndex(id);
    int s = BlockStart(id);
    Layer g;
    g.id = NewId();
    g.kind = LayerKind::Group;
    g.name = "Groupe " + std::to_string(std::count_if(state.layers.begin(), state.layers.end(), [](const Layer& x) { return x.kind == LayerKind::Group; }) + 1);
    g.parent = state.layers[i].parent;
    for (int k = s; k <= i; ++k)
        if (state.layers[k].parent == g.parent) state.layers[k].parent = g.id;
    state.layers[i].clipped = false;
    state.layers.insert(state.layers.begin() + i + 1, g);
    state.activeLayer = g.id;
    state.editMask = false;
    SyncGraph();
    return state.layers[i + 1];
}

void Document::Ungroup(Id groupId) {
    int i = LayerIndex(groupId);
    if (i < 0 || state.layers[i].kind != LayerKind::Group) return;
    Id parent = state.layers[i].parent;
    for (auto& l : state.layers) if (l.parent == groupId) l.parent = parent;
    state.layers.erase(state.layers.begin() + i);
    if (state.activeLayer == groupId) state.activeLayer = i > 0 ? state.layers[i - 1].id : 0;
    SyncGraph();
}

void Document::RemoveLayer(Id id) {
    int i = LayerIndex(id);
    if (i < 0) return;
    int s = BlockStart(id);
    state.layers.erase(state.layers.begin() + s, state.layers.begin() + i + 1);
    if (state.activeLayer == id || !FindLayer(state.activeLayer))
        state.activeLayer = state.layers.empty() ? 0 : state.layers[std::clamp(s - 1, 0, (int)state.layers.size() - 1)].id;
    state.editMask = false;
    SyncGraph();
}

// Move a layer (with its block) among its siblings: dir = +1 up, -1 down.
void Document::MoveLayerSibling(Id id, int dir) {
    int i = LayerIndex(id);
    if (i < 0) return;
    Id parent = state.layers[i].parent;
    int s = BlockStart(id);
    auto& L = state.layers;
    if (dir > 0) {
        int k = i + 1;
        while (k < (int)L.size() && L[k].parent != parent) {
            if (L[k].id == parent) return;  // reached our group's entry: already on top
            ++k;
        }
        if (k >= (int)L.size()) return;
        std::rotate(L.begin() + s, L.begin() + i + 1, L.begin() + k + 1);
    } else {
        if (s == 0 || L[s - 1].parent != parent) return;
        int bs = BlockStart(L[s - 1].id);
        std::rotate(L.begin() + bs, L.begin() + s, L.begin() + i + 1);
    }
    SyncGraph();
}

// Move a layer block above `target` (same parent as target), or inside it when it is a group.
void Document::MoveLayerTo(Id id, Id target, bool into) {
    if (id == target || IsDescendant(target, id)) return;
    int i = LayerIndex(id);
    int s = BlockStart(id);
    if (i < 0 || LayerIndex(target) < 0) return;
    std::vector<Layer> block(state.layers.begin() + s, state.layers.begin() + i + 1);
    state.layers.erase(state.layers.begin() + s, state.layers.begin() + i + 1);
    int t = LayerIndex(target);
    Layer& top = block.back();
    if (into && state.layers[t].kind == LayerKind::Group) {
        top.parent = target;
        state.layers.insert(state.layers.begin() + t, block.begin(), block.end());  // just below the group entry = top child
    } else {
        top.parent = state.layers[t].parent;
        state.layers.insert(state.layers.begin() + t + 1, block.begin(), block.end());
    }
    SyncGraph();
}

void Document::MoveLayer(Id id, int newIndex) {
    // Legacy flat move (root-level documents).
    int i = LayerIndex(id);
    if (i < 0 || state.layers[i].kind == LayerKind::Group) return;
    newIndex = std::clamp(newIndex, 0, (int)state.layers.size() - 1);
    Layer l = state.layers[i];
    state.layers.erase(state.layers.begin() + i);
    state.layers.insert(state.layers.begin() + newIndex, l);
    SyncGraph();
}

Layer& Document::DuplicateLayer(Id id) {
    int i = LayerIndex(id);
    int s = BlockStart(id);
    std::vector<Layer> block(state.layers.begin() + s, state.layers.begin() + i + 1);
    std::unordered_map<Id, Id> remap;
    for (auto& l : block) remap[l.id] = NewId();
    for (auto& l : block) {
        l.id = remap[l.id];
        if (remap.count(l.parent)) l.parent = remap[l.parent];
        if (l.surface) {
            Id src = l.surface;
            l.surface = CreateSurface({0, 0, 0, 0});
            pendingCopies.push_back({l.surface, src});
        }
        if (l.mask) {
            Id src = l.mask;
            l.mask = CreateSurface({1, 1, 1, 1});
            pendingCopies.push_back({l.mask, src});
        }
    }
    block.back().name += " copie";
    state.layers.insert(state.layers.begin() + i + 1, block.begin(), block.end());
    int ni = i + (int)block.size();
    state.activeLayer = state.layers[ni].id;
    SyncGraph();
    return state.layers[ni];
}

void Document::AddMask(Id layerId) {
    if (auto* l = FindLayer(layerId)) {
        if (!l->mask) l->mask = CreateSurface({1, 1, 1, 1});
        l->maskEnabled = true;
        state.editMask = true;
        SyncGraph();
    }
}

void Document::SyncGraph() {
    if (state.mode == DocMode::Layers) RebuildFromLayers();
    dirty = true;
}

namespace {
struct GraphBuilder {
    Document& doc;
    Graph g;
    float x = 0;

    Id Blend(const Layer& l, Id base, Id top, Id mask, BlendMode mode, float opacity, bool clip, bool bypass, float2 pos, u32 role) {
        if (!l.blendFx.customCode.empty()) {
            // Custom blend: an effect node with inputs A (below), B (layer), mask.
            Node e;
            e.id = GenId(l.id, role);
            e.kind = NodeKind::Effect;
            e.name = "Fusion " + l.name;
            e.fx = l.blendFx;
            if (auto def = e.fx.Def())
                for (size_t i = 0; i < def->params.size() && i < e.fx.params.size(); ++i)
                    if (def->params[i].id == "opacity") e.fx.params[i].x = opacity;  // layer opacity drives the template's parameter
            e.bypass = bypass;
            e.inputs = {base, top, mask};
            e.pos = pos;
            e.layer = l.id;
            g.nodes.push_back(e);
            return e.id;
        }
        Node b;
        b.id = GenId(l.id, role);
        b.kind = NodeKind::Blend;
        b.name = "Fusion " + l.name;
        b.blend = mode;
        b.opacity = opacity;
        b.clip = clip;
        b.bypass = bypass;
        b.blendIf = l.blendIf;
        b.channelOff = l.channelOff;
        b.inputs = {base, top, mask};
        b.pos = pos;
        b.layer = l.id;
        g.nodes.push_back(b);
        return b.id;
    }

    Id MaskNode(const Layer& l, float2 pos) {
        if (!l.mask || !l.maskEnabled) return 0;
        Node m;
        m.id = GenId(l.id, RoleMask);
        m.kind = NodeKind::Surface;
        m.name = l.name + " (masque)";
        m.surface = l.mask;
        m.pos = pos;
        m.layer = l.id;
        g.nodes.push_back(m);
        return m.id;
    }

    // Content of a non-group layer (source + filters). Adjustments read `base`.
    Id Content(const Layer& l, Id base, float& y) {
        x += 320.0f;
        y = 0;
        Id src;
        if (l.kind == LayerKind::Pixel || l.kind == LayerKind::Text || l.kind == LayerKind::Shape) {
            Node n;
            n.id = GenId(l.id, RoleSource);
            n.kind = NodeKind::Surface;
            n.name = l.name;
            n.surface = l.surface;
            n.pos = {x, y};
            n.layer = l.id;
            g.nodes.push_back(n);
            src = n.id;
        } else {
            Node n;
            n.id = GenId(l.id, l.kind == LayerKind::Adjustment ? RoleAdjust : RoleSource);
            n.kind = NodeKind::Effect;
            n.name = l.name;
            n.fx = l.effect;
            n.pos = {x, y};
            n.layer = l.id;
            int nin = NodeInputCount(n);
            n.inputs.assign(nin, 0);
            if (l.kind == LayerKind::Adjustment && nin > 0) n.inputs[0] = base;
            g.nodes.push_back(n);
            src = n.id;
        }
        u32 fi = 0;
        for (const auto& f : l.filters) {
            if (fi >= 15) break;
            y += 280.0f;
            Node n;
            n.id = GenId(l.id, RoleFilter + fi++);
            n.kind = NodeKind::Effect;
            auto def = f.Def();
            n.name = def ? def->name : "Filtre";
            n.fx = f;
            n.bypass = !f.enabled;
            n.pos = {x, y};
            n.layer = l.id;
            n.inputs.assign(std::max(NodeInputCount(n), 1), 0);
            n.inputs[0] = src;
            g.nodes.push_back(n);
            src = n.id;
        }
        return src;
    }

    Id Stack(Id parent, Id base) {
        const auto& layers = doc.S().layers;
        std::vector<const Layer*> kids;
        for (auto& l : layers) if (l.parent == parent) kids.push_back(&l);
        for (size_t i = 0; i < kids.size(); ++i) {
            const Layer& l = *kids[i];
            // Clipped layers directly above l (l itself may not be clipped).
            size_t j = i + 1;
            while (j < kids.size() && kids[j]->clipped && kids[j]->kind != LayerKind::Group) ++j;
            float y = 0;
            Id content;
            if (l.kind == LayerKind::Group) {
                if (l.passThrough && j == i + 1) {
                    Id inner = Stack(l.id, base);
                    bool needsBlend = l.opacity < 0.999f || (l.mask && l.maskEnabled) || !l.visible;
                    if (needsBlend && inner)
                        base = Blend(l, base, inner, MaskNode(l, {x, 840}), BlendMode::Normal, l.opacity, false, !l.visible, {x + 40, 840}, RoleBlend);
                    else if (inner)
                        base = inner;
                    continue;
                }
                content = Stack(l.id, 0);
                y = 560;
            } else {
                content = Content(l, base, y);
            }
            for (size_t k = i + 1; k < j; ++k) {
                const Layer& c = *kids[k];
                float cy = 0;
                Id cc = Content(c, content, cy);
                content = Blend(c, content, cc, MaskNode(c, {x - 130, cy + 280}), c.blend, c.opacity, true, !c.visible, {x + 40, cy + 560}, RoleBlend);
            }
            base = Blend(l, base, content, MaskNode(l, {x - 130, y + 280}), l.blend, l.opacity, false, !l.visible, {x + 40, y + 560}, RoleBlend);
            i = j - 1;
        }
        return base;
    }
};
}  // namespace

void Document::RebuildFromLayers() {
    GraphBuilder b{*this};
    Id result = b.Stack(0, 0);
    Node out;
    out.id = kOutputId;
    out.kind = NodeKind::Output;
    out.name = "Sortie";
    out.inputs = {result};
    out.pos = {b.x + 300.0f, 840.0f};
    b.g.nodes.push_back(out);
    b.g.output = kOutputId;
    // Positions moved by the user survive the rebuild.
    for (auto& n : b.g.nodes)
        if (auto it = state.layerNodePos.find(n.id); it != state.layerNodePos.end()) n.pos = it->second;
    state.graph = std::move(b.g);
    state.activeNode = 0;
}

void Document::ConvertToNodes() {
    if (state.mode == DocMode::Nodes) return;
    RebuildFromLayers();
    state.mode = DocMode::Nodes;
    // Select the node of the active layer.
    for (auto& n : state.graph.nodes)
        if (n.layer == state.activeLayer && (n.kind == NodeKind::Surface || n.kind == NodeKind::Effect)) { state.activeNode = n.id; break; }
    dirty = true;
}

// Try to read the graph back as a linear layer stack.
static bool ExtractLayers(const Graph& g, std::vector<Layer>& out, std::string* why) {
    for (auto& n : g.nodes)
        for (u8 c : n.inputChannel)
            if (c) { if (why) *why = "liens par canal (R, V, B, A)"; return false; }
    auto fail = [&](const std::string& msg) { if (why) *why = msg; return false; };
    // Count consumers: every node must feed at most one input.
    std::unordered_map<Id, int> uses;
    for (auto& n : g.nodes) for (Id in : n.inputs) if (in) uses[in]++;
    for (auto& [id, c] : uses) {
        const Node* n = g.Find(id);
        // A fusion may feed both the next fusion and an adjustment effect.
        if (c > 1 && !(n && n->kind == NodeKind::Blend && c == 2)) return fail("un nœud alimente plusieurs entrées (branche)");
    }

    const Node* outNode = g.Find(g.output);
    if (!outNode) return fail("pas de nœud de sortie");
    std::vector<const Node*> blends;
    Id cur = outNode->inputs.empty() ? 0 : outNode->inputs[0];
    while (cur) {
        const Node* n = g.Find(cur);
        if (!n || n->kind != NodeKind::Blend) return fail("la chaîne principale doit être composée de nœuds de fusion");
        blends.push_back(n);
        cur = n->inputs[0];
    }
    std::reverse(blends.begin(), blends.end());
    std::set<Id> used;
    Id prevBlend = 0;
    for (const Node* b : blends) {
        Layer l;
        l.id = b->layer ? b->layer : b->id;
        l.blend = b->blend;
        l.opacity = b->opacity;
        l.visible = !b->bypass;
        used.insert(b->id);
        // Mask
        if (b->inputs.size() > 2 && b->inputs[2]) {
            const Node* m = g.Find(b->inputs[2]);
            if (!m || m->kind != NodeKind::Surface) return fail("un masque doit être une surface");
            l.mask = m->surface;
            used.insert(m->id);
        }
        // Content chain: filters... -> source
        std::vector<const Node*> chain;
        Id c = b->inputs.size() > 1 ? b->inputs[1] : 0;
        if (!c) return fail("une fusion n'a pas de calque");
        bool adjustment = false;
        while (c) {
            const Node* n = g.Find(c);
            if (!n) return fail("lien invalide");
            if (n->kind == NodeKind::Surface) { chain.push_back(n); break; }
            if (n->kind != NodeKind::Effect) return fail("nœud non supporté dans un calque");
            chain.push_back(n);
            int nin = NodeInputCount(*n);
            for (int i = 1; i < (int)n->inputs.size(); ++i)
                if (n->inputs[i]) return fail("effet à plusieurs entrées");
            Id up = nin > 0 && !n->inputs.empty() ? n->inputs[0] : 0;
            if (nin > 0 && up == prevBlend) { adjustment = true; break; }
            if (nin > 0 && !up) return fail("effet sans entrée");
            c = up;
        }
        std::reverse(chain.begin(), chain.end());
        const Node* src = chain.front();
        if (src->kind == NodeKind::Surface) {
            l.kind = LayerKind::Pixel;
            l.surface = src->surface;
        } else {
            l.kind = adjustment ? LayerKind::Adjustment : LayerKind::Fill;
            l.effect = src->fx;
        }
        l.name = src->name;
        for (size_t i = 1; i < chain.size(); ++i) {
            EffectInstance f = chain[i]->fx;
            f.enabled = !chain[i]->bypass;
            l.filters.push_back(f);
        }
        for (auto* n : chain) used.insert(n->id);
        out.push_back(l);
        prevBlend = b->id;
    }
    // Unused nodes would be lost.
    for (auto& n : g.nodes)
        if (n.id != g.output && !used.count(n.id)) return fail("des nœuds ne sont pas connectés à la sortie (" + n.name + ")");
    return true;
}

bool Document::CanConvertToLayers(std::string* why) const {
    if (state.mode == DocMode::Layers) return false;
    std::vector<Layer> tmp;
    return ExtractLayers(state.graph, tmp, why);
}

bool Document::ConvertToLayers() {
    std::vector<Layer> layers;
    if (!ExtractLayers(state.graph, layers, nullptr)) return false;
    // Layer ids must be unique: re-id any collisions.
    std::set<Id> ids;
    for (auto& l : layers) if (!ids.insert(l.id).second || l.id >= kGenBase) l.id = NewId();
    state.layers = layers;
    state.mode = DocMode::Layers;
    state.activeLayer = layers.empty() ? 0 : layers.back().id;
    RebuildFromLayers();
    dirty = true;
    return true;
}

// ------------------------------------------------------------------ node mode

Node& Document::AddNode(NodeKind kind, float2 pos) {
    Node n;
    n.id = NewId();
    n.kind = kind;
    n.pos = pos;
    switch (kind) {
        case NodeKind::Surface:
            n.name = "Calque pixel";
            n.surface = CreateSurface({0, 0, 0, 0});
            break;
        case NodeKind::Blend:
            n.name = "Fusion";
            n.inputs = {0, 0, 0};
            break;
        default: n.name = "Nœud"; break;
    }
    state.graph.nodes.push_back(n);
    state.activeNode = n.id;
    dirty = true;
    return state.graph.nodes.back();
}

Node& Document::AddEffectNode(const std::string& effectId, const std::string& customCode, float2 pos) {
    Node n;
    n.id = NewId();
    n.kind = NodeKind::Effect;
    n.pos = pos;
    n.fx.effectId = effectId;
    n.fx.customCode = customCode;
    auto def = n.fx.Def();
    n.name = def ? def->name : "Effet";
    if (def) n.fx.params = def->DefaultParams();
    n.inputs.assign(NodeInputCount(n), 0);
    state.graph.nodes.push_back(n);
    state.activeNode = n.id;
    dirty = true;
    return state.graph.nodes.back();
}

void Document::RemoveNode(Id id) {
    if (id == state.graph.output) return;
    auto& nodes = state.graph.nodes;
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const Node& n) { return n.id == id; }), nodes.end());
    for (auto& n : nodes)
        for (auto& in : n.inputs) if (in == id) in = 0;
    if (state.activeNode == id) state.activeNode = 0;
    dirty = true;
}

bool Document::Link(Id from, Id to, int input, u8 channel) {
    Node* dst = state.graph.Find(to);
    if (!dst || !state.graph.Find(from) || from == to) return false;
    if (state.graph.WouldCycle(from, to)) return false;
    if ((int)dst->inputs.size() <= input) dst->inputs.resize(input + 1, 0);
    dst->inputs[input] = from;
    if (channel || (size_t)input < dst->inputChannel.size()) {
        if ((int)dst->inputChannel.size() <= input) dst->inputChannel.resize(input + 1, 0);
        dst->inputChannel[input] = channel;
    }
    dirty = true;
    return true;
}

void Document::Unlink(Id to, int input) {
    if (Node* dst = state.graph.Find(to))
        if (input < (int)dst->inputs.size()) {
            dst->inputs[input] = 0;
            if (input < (int)dst->inputChannel.size()) dst->inputChannel[input] = 0;
        }
    dirty = true;
}

Id Document::PaintTarget() const {
    if (state.mode == DocMode::Layers) {
        for (auto& l : state.layers)
            if (l.id == state.activeLayer) {
                if (state.editMask && l.mask) return l.mask;
                return l.kind == LayerKind::Pixel && !l.locked ? l.surface : 0;
            }
        return 0;
    }
    const Node* n = state.graph.Find(state.activeNode);
    return n && n->kind == NodeKind::Surface ? n->surface : 0;
}

}  // namespace sx
