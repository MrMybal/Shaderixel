#include "doc/Document.h"
#include "doc/GraphTypes.h"
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
enum Role : u32 { RoleSource = 0, RoleFilter = 1, RoleAdjust = 16, RoleBlend = 17, RoleMask = 18, RoleFilterMore = 19 };
static_assert(RoleFilterMore + (kMaxLayerFilters - 15) <= 32, "generated node roles use 5 bits");
// Filter k of a layer: roles 1-15, then 19-31.
u32 FilterRole(u32 k) { return k < 15 ? RoleFilter + k : RoleFilterMore + (k - 15); }
}  // namespace

int LayerFilterOfRole(u32 role) {
    if (role >= RoleFilter && role < RoleFilter + 15) return (int)(role - RoleFilter);
    if (role >= RoleFilterMore && role < RoleFilterMore + (u32)(kMaxLayerFilters - 15)) return (int)(role - RoleFilterMore) + 15;
    return -1;
}

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
    EffectDefPtr d = Effects().Find(effectId);
    if (d && d != libDef) {
        // The library definition changed under the same effect (replaced, hot-reloaded): the
        // values were laid out for the previous one, realign them by parameter id. A new
        // instance, or one whose effect was just switched, only records the definition.
        if (libDef && libId == effectId) const_cast<EffectInstance*>(this)->Realign(libDef, d);
        libDef = d;
        libId = effectId;
    }
    return d;
}

void EffectInstance::SyncParams(const EffectDefPtr& oldDef) {
    const bool lib = graph.empty() && customCode.empty();
    const EffectDefPtr prevLib = libDef;
    const bool prevSame = libId == effectId;
    EffectDefPtr nd = Def();
    if (!nd) return;
    // Def() already realigned the values from the definition they were laid out for.
    if (lib && prevLib && prevSame && prevLib != nd) return;
    Realign(oldDef, nd);
    if (lib) {
        libDef = nd;
        libId = effectId;
    }
}

void EffectInstance::Realign(const EffectDefPtr& oldDef, const EffectDefPtr& nd) {
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

std::vector<const Node*> FunctionPorts(const Graph& g, NodeKind k) {
    std::vector<const Node*> v;
    for (auto& n : g.nodes) if (n.kind == k) v.push_back(&n);
    std::stable_sort(v.begin(), v.end(), [](const Node* a, const Node* b) { return a->port != b->port ? a->port < b->port : a->id < b->id; });
    return v;
}

bool GraphHasFunctions(const Graph& g) {
    for (auto& n : g.nodes) if (n.kind == NodeKind::Function) return true;
    return false;
}

namespace {
Id MixId(Id a, Id b) {
    u64 h = (u64)a * 0x9E3779B97F4A7C15ull ^ ((u64)b + 0x632BE59BD9B4E019ull) * 0xBF58476D1CE4E5B9ull;
    h ^= h >> 29;
    h *= 0x94D049BB133111EBull;
    h ^= h >> 32;
    return (Id)(0x80000000u | (u32)(h & 0x7FFFFFFFu));
}

void Flatten(const Graph& in, Graph& out, Id liveOverride, int depth, std::map<std::pair<Id, int>, Id>& portOut) {
    for (const Node& n : in.nodes) {
        if (n.kind != NodeKind::Function) {
            out.nodes.push_back(n);
            continue;
        }
        if (FunctionApplied(n) && n.id != liveOverride) {
            // Applied: its stored outputs (the inner graph and the inputs are not evaluated).
            for (size_t k = 0; k < n.outSurf.size(); ++k) {
                Node s;
                s.kind = NodeKind::Surface;
                s.id = k == 0 ? n.id : MixId(n.id, 0x7FFF0000u + (u32)k);
                s.name = n.name;
                s.surface = n.outSurf[k];
                out.nodes.push_back(s);
                portOut[{n.id, (int)k}] = s.id;
            }
            continue;
        }
        Node self = n;
        self.kind = NodeKind::Split;  // pass-through of output 0 (thumbnail, plain links)
        self.sub.reset();
        self.inputs = {0};
        self.inputChannel.clear();
        if (!n.sub || depth > 8) {
            out.nodes.push_back(self);
            continue;
        }
        Graph inner;
        std::map<std::pair<Id, int>, Id> innerPorts;
        Flatten(*n.sub, inner, liveOverride, depth + 1, innerPorts);
        // Inner links to nested functions are redirected inside the inner graph first.
        for (auto& m : inner.nodes)
            for (size_t i = 0; i < m.inputs.size(); ++i) {
                const u8 c = InputChannel(m, i);
                auto it = innerPorts.find({m.inputs[i], LinkPort(c)});
                if (it == innerPorts.end()) continue;
                m.inputs[i] = it->second;
                if (i < m.inputChannel.size()) m.inputChannel[i] = LinkChannel(c);
            }
        auto ins = FunctionPorts(*n.sub, NodeKind::SubInput);
        auto outs = FunctionPorts(*n.sub, NodeKind::SubOutput);
        for (const Node& m : inner.nodes) {
            Node c = m;
            c.id = MixId(n.id, m.id);
            for (auto& x : c.inputs) if (x) x = MixId(n.id, x);
            for (auto& e : c.execLinks) e.src = MixId(n.id, e.src);
            if (m.kind == NodeKind::SubInput) {
                size_t p = 0;
                while (p < ins.size() && ins[p]->id != m.id) ++p;
                const bool linked = p < n.inputs.size() && n.inputs[p];
                c.surface = 0;
                if (!linked && PinIsValue(m.value.type)) {
                    // Unlinked value input: the value set on the function node (else the input's default).
                    c.kind = NodeKind::Value;
                    c.value = NodePinDefault(n, (int)p);
                    c.inputs.clear();
                } else {
                    c.kind = NodeKind::Split;
                    c.inputs = {linked ? n.inputs[p] : 0};
                    c.inputChannel = {linked ? InputChannel(n, p) : (u8)0};
                }
            } else if (m.kind == NodeKind::VarGet) {
                // Variable: the value set on this function node, else the graph's default.
                c.kind = NodeKind::Value;
                Value val = m.value;
                if (auto it = n.varValues.find(m.op); it != n.varValues.end()) val = it->second;
                else if (const GraphVariable* gv = n.sub->Var(m.op)) val = gv->value;
                c.value = ConvertValue(val, m.value.type, m.value.array);
            } else if (m.kind == NodeKind::Action) {
                auto it = n.execResults.find(m.id);
                c.surface = it != n.execResults.end() ? it->second : 0;
                auto is = n.execSmart.find(m.id);
                c.smart = is != n.execSmart.end() ? is->second : 0;
            } else if (m.kind == NodeKind::SubOutput || m.kind == NodeKind::Output) {
                c.kind = NodeKind::Split;
            }
            out.nodes.push_back(c);
        }
        // Output 0 through the node itself (its thumbnail and preview stay live), the others directly.
        for (size_t k = 1; k < outs.size(); ++k) portOut[{n.id, (int)k}] = MixId(n.id, outs[k]->id);
        if (!outs.empty()) self.inputs = {MixId(n.id, outs[0]->id)};
        out.nodes.push_back(self);
    }
}
}  // namespace

void FlattenGraph(const Graph& in, Graph& out, Id liveOverride) {
    out.nodes.clear();
    out.output = in.output;
    std::map<std::pair<Id, int>, Id> portOut;
    Flatten(in, out, liveOverride, 0, portOut);
    // Links from function outputs: to the node carrying that output.
    for (auto& m : out.nodes)
        for (size_t i = 0; i < m.inputs.size(); ++i) {
            const u8 c = InputChannel(m, i);
            auto it = portOut.find({m.inputs[i], LinkPort(c)});
            if (it == portOut.end()) continue;  // other multi-output nodes keep their port (values)
            m.inputs[i] = it->second;
            if (i < m.inputChannel.size()) m.inputChannel[i] = LinkChannel(c);
        }
    if (auto it = portOut.find({out.output, 0}); it != portOut.end()) out.output = it->second;
}

Id FlatChildId(Id function, Id inner) { return MixId(function, inner); }

Id FunctionOutputFlatId(const Node& f, int port) {
    if (!f.sub) return 0;
    auto outs = FunctionPorts(*f.sub, NodeKind::SubOutput);
    return port < (int)outs.size() ? MixId(f.id, outs[port]->id) : 0;
}

void CollectGraphSurfaces(const Graph& g, std::set<Id>& out) {
    for (auto& n : g.nodes) {
        if (n.surface && n.kind != NodeKind::SubInput) out.insert(n.surface);
        for (Id s : n.outSurf) if (s) out.insert(s);
        for (auto& [inner, s] : n.execResults) if (s) out.insert(s);
        if (n.sub) CollectGraphSurfaces(*n.sub, out);
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

SmartObject::SmartObject() = default;
SmartObject::~SmartObject() = default;

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
    size_t cut = std::min<size_t>(style.content.find('\n'), 32);
    if (cut < style.content.size())
        while (cut > 0 && ((unsigned char)style.content[cut] & 0xC0) == 0x80) --cut;  // not inside a UTF-8 character
    l.name = style.content.substr(0, cut);
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
    // The copy is an independent material: material layers (and 3D folders) of the block get a new
    // material id, shared by the copied layers that shared one, as are their copied masks.
    std::unordered_map<Id, Id> materials, masks, edits;
    for (auto& l : block) {
        l.id = remap[l.id];
        if (remap.count(l.parent)) l.parent = remap[l.parent];
        if (l.material) {
            auto [it, fresh] = materials.try_emplace(l.material, 0);
            if (fresh) it->second = NewId();
            l.material = it->second;
        }
        if (l.surface) {
            Id src = l.surface;
            l.surface = CreateSurface({0, 0, 0, 0});
            pendingCopies.push_back({l.surface, src});
        }
        if (l.mask) {
            Id src = l.mask;
            auto [it, fresh] = masks.try_emplace(src, 0);
            if (fresh) {
                it->second = CreateSurface({1, 1, 1, 1});
                pendingCopies.push_back({it->second, src});
            }
            l.mask = it->second;
        }
        if (l.maskEdit) {
            Id src = l.maskEdit;
            auto [it, fresh] = edits.try_emplace(src, 0);
            if (fresh) {
                it->second = CreateSurface({0, 0, 0, 0});
                pendingCopies.push_back({it->second, src});
            }
            l.maskEdit = it->second;
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
        if (l.HasCustomBlend()) {
            // Custom blend: an effect node with inputs A (below), B (layer), mask.
            Node e;
            e.id = GenId(l.id, role);
            e.kind = NodeKind::Effect;
            e.name = "Fusion " + l.name;
            e.fx = l.blendFx;
            if (auto def = e.fx.Def())
                for (size_t i = 0; i < def->params.size() && i < e.fx.params.size(); ++i)
                    if (_stricmp(def->params[i].id.c_str(), "opacity") == 0) e.fx.params[i].x = opacity;  // layer opacity drives it
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
        if (l.kind == LayerKind::Pixel || l.kind == LayerKind::Text || l.kind == LayerKind::Shape || l.kind == LayerKind::Smart) {
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
            if (fi >= (u32)kMaxLayerFilters) break;  // no node id left (see kMaxLayerFilters)
            y += 280.0f;
            Node n;
            n.id = GenId(l.id, FilterRole(fi++));
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

    bool ViewHidden(const Layer& l) const {
        return doc.viewUpTo && l.id != doc.viewUpTo && doc.LayerIndex(l.id) > doc.LayerIndex(doc.viewUpTo) && !doc.IsDescendant(doc.viewUpTo, l.id);
    }

    Id Stack(Id parent, Id base) {
        const auto& layers = doc.S().layers;
        std::vector<const Layer*> kids;
        for (auto& l : layers) if (l.parent == parent) kids.push_back(&l);
        for (size_t i = 0; i < kids.size(); ++i) {
            const Layer& l = *kids[i];
            // View "up to the layer": everything above it is left out (its groups stay).
            if (ViewHidden(l)) continue;
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
                if (ViewHidden(c)) continue;
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
// `prev`: the layers the graph was generated from (what the nodes do not carry comes from them).
static bool ExtractLayers(const Graph& g, const std::vector<Layer>* prev, std::vector<Layer>& out, std::string* why) {
    for (auto& n : g.nodes) {
        if (n.kind == NodeKind::Split) { if (why) *why = "nœuds Séparer RGBA"; return false; }
        for (u8 c : n.inputChannel)
            if (c) { if (why) *why = "liens par canal (R, V, B, A)"; return false; }
    }
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
        l.blendIf = b->blendIf;
        l.channelOff = b->channelOff;
        // A layer clips to the layer below it, not to the whole stack: no layer equivalent.
        if (b->clip) return fail("une fusion conserve l'alpha du fond (écrêtage sur toute la pile)");
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
        if (chain.size() - 1 > (size_t)kMaxLayerFilters) return fail("plus de " + std::to_string(kMaxLayerFilters) + " filtres sur un calque");
        for (size_t i = 1; i < chain.size(); ++i) {
            EffectInstance f = chain[i]->fx;
            f.enabled = !chain[i]->bypass;
            l.filters.push_back(f);
        }
        for (auto* n : chain) used.insert(n->id);
        // What the nodes do not carry, from the layer they were generated from: a disabled mask
        // (no mask node), the lock, material settings, and the text / shape / smart object
        // contents while the source is still that layer's own surface.
        if (prev && b->layer)
            for (const Layer& o : *prev) {
                if (o.id != b->layer || o.kind == LayerKind::Group) continue;
                l.locked = o.locked;
                l.material = o.material;
                l.generator = o.generator;
                l.projection = o.projection;
                if (!l.mask && o.mask && !o.maskEnabled) {
                    l.mask = o.mask;
                    l.maskEnabled = false;
                }
                if (l.mask && l.mask == o.mask) l.maskEdit = o.maskEdit;
                if (l.kind == LayerKind::Pixel && l.surface && l.surface == o.surface &&
                    (o.kind == LayerKind::Text || o.kind == LayerKind::Shape || o.kind == LayerKind::Smart)) {
                    l.kind = o.kind;
                    l.text = o.text;
                    l.shape = o.shape;
                    l.smart = o.smart;
                    for (int k = 0; k < 4; ++k) l.smartCorners[k] = o.smartCorners[k];
                    l.smartPivot = o.smartPivot;
                    l.smartInterp = o.smartInterp;
                    l.name = o.name;
                }
                break;
            }
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
    return ExtractLayers(state.graph, &state.layers, tmp, why);
}

bool Document::ConvertToLayers() {
    std::vector<Layer> layers;
    if (!ExtractLayers(state.graph, &state.layers, layers, nullptr)) return false;
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
        case NodeKind::Split:
            n.name = "Séparer RGBA";
            n.inputs = {0};
            break;
        case NodeKind::Reroute:
            // Relay point of a link (any type: taken from the first link plugged in).
            n.name = "Relais";
            n.inputs = {0};
            n.value.type = PinType::RGBA;
            n.value.array = false;
            break;
        case NodeKind::SubInput:
        case NodeKind::SubOutput: {
            int count = 0;
            for (auto& m : state.graph.nodes) if (m.kind == kind) count = std::max(count, m.port + 1);
            n.port = count;
            n.name = kind == NodeKind::SubInput ? "Entrée " + std::to_string(count + 1) : "Sortie " + std::to_string(count + 1);
            if (kind == NodeKind::SubOutput) n.inputs = {0};
            break;
        }
        default: n.name = "Nœud"; break;
    }
    state.graph.nodes.push_back(n);
    state.activeNode = n.id;
    dirty = true;
    return state.graph.nodes.back();
}

Node& Document::AddToolNode(const std::string& tool, float2 pos) {
    Node n;
    n.id = NewId();
    n.kind = NodeKind::Tool;
    n.pos = pos;
    n.tool = tool;
    n.passLive = true;  // nothing baked yet: the input passes through
    n.inputs = {0};
    if (tool == "upscale") {
        n.name = "Upscale";
        n.props["factor"] = "2";
        n.props["method"] = "bicubic";
        n.props["model"] = "4x-UltraSharp.pth";
    } else if (tool == "comfyui") {
        n.name = "ComfyUI";
        n.props["denoise"] = "0.5";
    } else {
        n.name = "Bake";
    }
    state.graph.nodes.push_back(n);
    state.activeNode = n.id;
    dirty = true;
    return state.graph.nodes.back();
}

Node& Document::AddFunctionNode(const std::string& funcKind, float2 pos) {
    auto g = std::make_shared<Graph>();
    Node in;
    in.id = NewId();
    in.kind = NodeKind::SubInput;
    in.name = "Entrée";
    in.pos = {0, 0};
    Node out;
    out.id = NewId();
    out.kind = NodeKind::SubOutput;
    out.name = "Sortie";
    out.pos = {420, 0};
    out.inputs = {in.id};
    g->nodes = {in, out};
    g->output = out.id;
    Node n;
    n.id = NewId();
    n.kind = NodeKind::Function;
    n.pos = pos;
    n.funcKind = funcKind;
    n.applyMode = funcKind == "tool";
    n.name = funcKind == "tool" ? "Outil personnalisé" : "Sous-graphe";
    n.sub = g;
    n.inputs.assign(NodeInputCount(n), 0);
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
    for (auto& n : nodes) {
        for (auto& in : n.inputs) if (in == id) in = 0;
        std::erase_if(n.execLinks, [&](const Node::ExecLink& e) { return e.src == id; });
    }
    if (state.activeNode == id) state.activeNode = 0;
    dirty = true;
}

bool Document::Link(Id from, Id to, int input, u8 channel) {
    Node* dst = state.graph.Find(to);
    const Node* srcNode = state.graph.Find(from);
    if (!dst || !srcNode || from == to) return false;
    // A relay with no link yet takes the type of the first one.
    auto freeRelay = [&](const Node& r) {
        if (r.kind != NodeKind::Reroute || (!r.inputs.empty() && r.inputs[0])) return false;
        for (auto& m : state.graph.nodes) {
            for (Id x : m.inputs) if (x == r.id) return false;
            for (auto& e : m.execLinks) if (e.src == r.id) return false;
        }
        return true;
    };
    if (freeRelay(*dst)) {
        const PinDesc o = NodeOutputPin(*srcNode, NodeOutputPortOf(*srcNode, channel));
        dst->value.type = o.type;
        dst->value.array = o.array;
    } else if (Node* sn = state.graph.Find(from); sn && freeRelay(*sn)) {
        const PinDesc i = NodeInputPin(*dst, input);
        sn->value.type = i.type;
        sn->value.array = i.array;
    }
    // Pin types must match (images with images, values with values, execution with execution).
    if (!PinsCompatible(NodeOutputPin(*srcNode, NodeOutputPortOf(*srcNode, channel)), NodeInputPin(*dst, input))) return false;
    if (NodeInputType(*dst, input) == PinType::Exec) {
        // Execution: several flows may join (no cycle check: a flow may loop back).
        if (input < (int)dst->inputs.size() && dst->inputs[input] && !(dst->inputs[input] == from && InputChannel(*dst, input) == channel)) {
            for (auto& e : dst->execLinks)
                if (e.pin == input && e.src == from && e.channel == channel) return true;
            dst->execLinks.push_back({input, from, channel});
            dirty = true;
            return true;
        }
    } else if (state.graph.WouldCycle(from, to)) {
        return false;
    }
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
    if (Node* dst = state.graph.Find(to)) {
        // An execution input with several flows: the last one added goes first.
        for (int k = (int)dst->execLinks.size() - 1; k >= 0; --k)
            if (dst->execLinks[k].pin == input) {
                dst->execLinks.erase(dst->execLinks.begin() + k);
                dirty = true;
                return;
            }
    }
    if (Node* dst = state.graph.Find(to))
        if (input < (int)dst->inputs.size()) {
            dst->inputs[input] = 0;
            if (input < (int)dst->inputChannel.size()) dst->inputChannel[input] = 0;
        }
    dirty = true;
}

bool Document::PaintsMaskEdits() const {
    if (state.mode != DocMode::Layers || !state.editMask || state.editInpaint) return false;
    for (auto& l : state.layers)
        if (l.id == state.activeLayer) return l.mask && l.maskEdit && !l.generator.empty();
    return false;
}

Id Document::PaintTarget() const {
    if (state.editInpaint && state.inpaintMask && state.surfaces.count(state.inpaintMask)) return state.inpaintMask;
    if (state.mode == DocMode::Layers) {
        for (auto& l : state.layers)
            if (l.id == state.activeLayer) {
                if (state.editMask && l.mask) return !l.generator.empty() && l.maskEdit ? l.maskEdit : l.mask;  // smart mask: its corrections
                return l.kind == LayerKind::Pixel && !l.locked ? l.surface : 0;
            }
        return 0;
    }
    const Node* n = state.graph.Find(state.activeNode);
    return n && n->kind == NodeKind::Surface ? n->surface : 0;
}

}  // namespace sx
