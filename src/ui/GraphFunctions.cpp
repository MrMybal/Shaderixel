// Functions in the graph (NodeKind::Function): sub-graphs and custom tools made of nodes, with any
// number of inputs / outputs (SubInput / SubOutput nodes inside). Two modes: Live (evaluated in
// place, flattened into the graph for rendering) or Apply (outputs stored as pixels by "Appliquer",
// the nodes before are frozen). A function is edited in its own tab (an ordinary node document
// sharing the parent's textures; every change goes back into the node), and saved to / inserted
// from a library of .sxl files (%LOCALAPPDATA%\Shaderixel\Functions, plus "functions" next to the
// executable).
#include "ui/App.h"
#include "ui/Theme.h"

#include <ShlObj.h>
#include <imgui_stdlib.h>
#include <filesystem>
#include <fstream>

namespace sx {

namespace fs = std::filesystem;

std::function<Id(Document&, float2, const std::string&)> g_insertLibraryFunction;

std::vector<FunctionLibEntry>& FunctionLibrary() {
    static std::vector<FunctionLibEntry> lib;
    return lib;
}
u64& FunctionLibraryGen() {
    static u64 gen = 1;
    return gen;
}

namespace {

std::string UserFunctionDir() {
    std::string out;
    PWSTR p = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) out = Narrow(p) + "\\Shaderixel\\Functions";
    if (p) CoTaskMemFree(p);
    return out;
}

std::string BundledFunctionDir() {
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    return Narrow((fs::path(exe).parent_path() / L"functions").wstring());
}

std::string SafeFileName(std::string s) {
    for (auto& c : s)
        if (strchr("\\/:*?\"<>|", c) || (unsigned char)c < 32) c = '_';
    while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
    return s.empty() ? "Fonction" : s;
}

// Copy of `g` with surface / smart object ids renamed (inner graphs too); SubInput previews dropped.
std::shared_ptr<Graph> RemapGraph(const Graph& g, const std::map<Id, Id>& surf, const std::map<Id, Id>& smart) {
    auto out = std::make_shared<Graph>(g);
    auto map = [](const std::map<Id, Id>& m, Id x) {
        auto it = m.find(x);
        return it == m.end() ? x : it->second;
    };
    for (auto& n : out->nodes) {
        if (n.kind == NodeKind::SubInput) n.surface = 0;
        else if (n.surface) n.surface = map(surf, n.surface);
        for (auto& s : n.outSurf) s = map(surf, s);
        if (n.smart) n.smart = map(smart, n.smart);
        if (n.sub) n.sub = RemapGraph(*n.sub, surf, smart);
    }
    return out;
}

void CollectGraphSmart(const Graph& g, std::set<Id>& out) {
    for (auto& n : g.nodes) {
        if (n.smart) out.insert(n.smart);
        if (n.sub) CollectGraphSmart(*n.sub, out);
    }
}

Id MaxGraphId(const Graph& g) {
    Id m = 0;
    for (auto& n : g.nodes) {
        m = std::max(m, n.id);
        if (n.sub) m = std::max(m, MaxGraphId(*n.sub));
    }
    return m;
}

}  // namespace

void App::ScanFunctionLibrary() {
    auto& lib = FunctionLibrary();
    lib.clear();
    for (const std::string& dir : {UserFunctionDir(), BundledFunctionDir()}) {
        std::error_code ec;
        if (dir.empty() || !fs::is_directory(Utf8Path(dir), ec)) continue;
        for (auto& e : fs::directory_iterator(Utf8Path(dir), ec)) {
            if (e.path().extension() != L".sxl") continue;
            FunctionLibEntry f;
            f.path = Narrow(e.path().wstring());
            f.name = Narrow(e.path().stem().wstring());
            f.kind = "subgraph";
            std::string txt;
            Json j;
            if (ReadTextFile(Narrow(fs::path(e.path()).replace_extension(L".json").wstring()), txt) && Json::Parse(txt, j)) {
                f.name = j["name"].S(f.name);
                f.kind = j["kind"].S("subgraph");
                f.desc = j["description"].S("");
                f.inputs = j["inputs"].I(0);
                f.outputs = j["outputs"].I(0);
            }
            bool dup = false;
            for (auto& o : lib) dup |= o.name == f.name && o.kind == f.kind;
            if (!dup) lib.push_back(std::move(f));
        }
    }
    std::sort(lib.begin(), lib.end(), [](const FunctionLibEntry& a, const FunctionLibEntry& b) { return a.name < b.name; });
    ++FunctionLibraryGen();
}

void App::InitFunctions() {
    ScanFunctionLibrary();
    g_insertLibraryFunction = [this](Document& d, float2 pos, const std::string& path) -> Id {
        for (auto& v : docs)
            if (v->doc.get() == &d) return InsertLibraryFunction(*v, path, pos);
        return 0;
    };
}

Id App::InsertLibraryFunction(DocView& v, const std::string& path, float2 pos) {
    const int prevActive = activeDoc;
    const size_t before = docs.size();
    if (!LoadSxl(path) || docs.size() != before + 1) return 0;
    std::shared_ptr<DocView> lib(std::move(docs.back()));
    docs.pop_back();
    activeDoc = focusDocRequest = prevActive;
    deferredViews.push_back({frameCounter, lib});
    Document& src = *lib->doc;
    Document& d = *v.doc;
    if (src.Width() != d.Width() || src.Height() != d.Height())
        Status("Fonction faite pour " + std::to_string(src.Width()) + " x " + std::to_string(src.Height()) + " : ses images internes sont ignorées à une autre taille");
    // Its pixels (images inside, applied tools) become surfaces of this document.
    std::set<Id> surfs, smarts;
    CollectGraphSurfaces(src.S().graph, surfs);
    CollectGraphSmart(src.S().graph, smarts);
    std::map<Id, Id> surfMap, smartMap;
    for (Id s : surfs) {
        Surface* ss = src.GetSurface(s);
        if (!ss || !ss->tex || ss->tex->Width() != d.Width() || ss->tex->Height() != d.Height()) continue;
        const Id ns = d.NewId();
        d.S().surfaces[ns] = *ss;
        surfMap[s] = ns;
    }
    for (Id s : smarts)
        if (auto it = src.smartObjects.find(s); it != src.smartObjects.end()) {
            const Id ns = d.NewId();
            auto copy = it->second;
            d.smartObjects[ns] = copy;
            smartMap[s] = ns;
        }
    std::string name = Narrow(Utf8Path(path).stem().wstring());
    for (auto& f : FunctionLibrary()) if (f.path == path) name = f.name;
    Node f;
    f.id = d.NewId();
    f.kind = NodeKind::Function;
    f.pos = pos;
    f.name = name;
    f.funcKind = src.functionKind.empty() ? "subgraph" : src.functionKind;
    f.applyMode = f.funcKind == "tool";
    auto g = RemapGraph(src.S().graph, surfMap, smartMap);
    if (!g->Find(g->output) || g->Find(g->output)->kind == NodeKind::Output) {
        auto outs = FunctionPorts(*g, NodeKind::SubOutput);
        if (!outs.empty()) g->output = outs[0]->id;
    }
    f.sub = g;
    f.inputs.assign(NodeInputCount(f), 0);
    d.S().graph.nodes.push_back(f);
    d.S().activeNode = f.id;
    d.dirty = true;
    Status("Fonction insérée : " + name + " (" + std::to_string(NodeInputCount(f)) + " entrée(s), " + std::to_string(NodeOutputCount(f)) + " sortie(s))");
    return f.id;
}

// Replaces the selected nodes by a function holding them; links crossing the selection become its pins.
void App::FunctionFromSelection(DocView& v, const std::set<Id>& selection, const std::string& funcKind) {
    Document& d = *v.doc;
    Graph& g = d.S().graph;
    std::set<Id> sel;
    for (Id id : selection)
        if (id != g.output && g.Find(id)) sel.insert(id);
    if (sel.empty()) return;
    DocState before = d.S();
    auto sub = std::make_shared<Graph>();
    Node f;
    f.id = d.NewId();
    f.kind = NodeKind::Function;
    f.funcKind = funcKind;
    f.applyMode = funcKind == "tool";
    f.name = funcKind == "tool" ? "Outil personnalisé" : "Sous-graphe";
    float2 mn{1e9f, 1e9f}, mx{-1e9f, -1e9f};
    for (auto& n : g.nodes)
        if (sel.count(n.id)) {
            sub->nodes.push_back(n);
            sub->nodes.back().layer = 0;
            mn = {std::min(mn.x, n.pos.x), std::min(mn.y, n.pos.y)};
            mx = {std::max(mx.x, n.pos.x), std::max(mx.y, n.pos.y)};
        }
    f.pos = (mn + mx) * 0.5f;
    // Incoming links -> SubInput pins.
    std::map<std::pair<Id, u8>, Id> inPins;
    int inPort = 0;
    const size_t selCount = sub->nodes.size();
    for (size_t ni = 0; ni < selCount; ++ni)
        for (size_t i = 0; i < sub->nodes[ni].inputs.size(); ++i) {
            const Node& n = sub->nodes[ni];
            const Id src = n.inputs[i];
            if (!src || sel.count(src)) continue;
            const u8 ch = InputChannel(n, i);
            auto key = std::make_pair(src, ch);
            if (!inPins.count(key)) {
                Node in;
                in.id = d.NewId();
                in.kind = NodeKind::SubInput;
                in.port = inPort++;
                const Node* s = g.Find(src);
                in.name = s ? s->name : "Entrée";
                in.pos = {mn.x - 360, mn.y + 160.0f * in.port};
                inPins[key] = in.id;
                f.inputs.push_back(src);
                f.inputChannel.push_back(ch);
                sub->nodes.push_back(in);
            }
            Node* nn = &sub->nodes[ni];  // (the vector may have grown)
            nn->inputs[i] = inPins[key];
            if (i < nn->inputChannel.size()) nn->inputChannel[i] = 0;
        }
    // Outgoing links -> SubOutput pins (the outer links now come from the function).
    std::map<std::pair<Id, u8>, int> outPins;
    int outPort = 0;
    auto outPin = [&](Id src, u8 ch) {
        auto key = std::make_pair(src, ch);
        if (auto it = outPins.find(key); it != outPins.end()) return it->second;
        Node o;
        o.id = d.NewId();
        o.kind = NodeKind::SubOutput;
        o.port = outPort;
        const Node* s = g.Find(src);
        o.name = s ? s->name : "Sortie";
        o.pos = {mx.x + 360, mn.y + 160.0f * outPort};
        o.inputs = {src};
        if (ch) o.inputChannel = {ch};
        sub->nodes.push_back(o);
        if (!sub->output) sub->output = o.id;
        outPins[key] = outPort;
        return outPort++;
    };
    for (auto& n : g.nodes) {
        if (sel.count(n.id)) continue;
        for (size_t i = 0; i < n.inputs.size(); ++i) {
            if (!sel.count(n.inputs[i])) continue;
            const int k = outPin(n.inputs[i], InputChannel(n, i));
            n.inputs[i] = f.id;
            if (n.inputChannel.size() <= i) n.inputChannel.resize(i + 1, 0);
            n.inputChannel[i] = (u8)(k << 3);
        }
    }
    if (outPins.empty()) {
        // Nothing used outside: output the last selected node.
        outPin(sub->nodes.front().id, 0);
    }
    f.sub = sub;
    g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(), [&](const Node& n) { return sel.count(n.id) > 0; }), g.nodes.end());
    g.nodes.push_back(f);
    d.S().activeNode = f.id;
    d.dirty = true;
    v.history.PushState(funcKind == "tool" ? "Créer un outil" : "Créer un sous-graphe", before, d.S());
    Status(f.name + " créé : " + std::to_string(inPort) + " entrée(s), " + std::to_string(outPort) + " sortie(s). Double-clic pour l'éditer.");
}

// Pixels of a node output as the parent renders it (function outputs through the flattened graph).
gpu::Texture* App::NodeOutputTexture(DocView& v, Id src, u8 channel) {
    Document& d = *v.doc;
    const Node* s = d.S().graph.Find(src);
    if (!s) return nullptr;
    Id id = src;
    if (s->kind == NodeKind::Function && LinkPort(channel) > 0) {
        if (FunctionApplied(*s)) id = 0;  // stored output k: surface below
        else id = FunctionOutputFlatId(*s, LinkPort(channel));
        if (!id) {
            const size_t k = LinkPort(channel);
            Surface* sf = k < s->outSurf.size() ? d.GetSurface(s->outSurf[k]) : nullptr;
            return sf ? sf->tex.get() : nullptr;
        }
    }
    return v.renderer.EvaluateNode(d, *ctx, id);
}

void App::EditFunction(DocView& parent, Id fid) {
    Document& pd = *parent.doc;
    const Node* f = pd.S().graph.Find(fid);
    if (!f || f->kind != NodeKind::Function || !f->sub) return;
    for (int i = 0; i < (int)docs.size(); ++i)
        if (docs[i]->funcLink.parent == &parent && docs[i]->funcLink.node == fid) {
            activeDoc = focusDocRequest = i;
            return;
        }
    auto doc = std::make_unique<Document>(pd.Width(), pd.Height(), pd.GetPrecision(), DocMode::Nodes, f->name + " (fonction)");
    doc->functionKind = f->funcKind;
    doc->blendInGamma = pd.blendInGamma;
    Graph g = *f->sub;
    doc->S().surfaces.clear();
    doc->S().nextId = std::max(pd.S().nextId, MaxGraphId(g) + 1) + 0x100000;
    std::set<Id> surfs, smarts;
    CollectGraphSurfaces(g, surfs);
    CollectGraphSmart(g, smarts);
    for (Id s : surfs)
        if (Surface* ps = pd.GetSurface(s)) doc->S().surfaces[s] = *ps;  // shared textures
    for (Id s : smarts)
        if (auto it = pd.smartObjects.find(s); it != pd.smartObjects.end()) doc->smartObjects[s] = it->second;
    // Inputs: what the function receives in the parent, captured for the preview.
    auto ins = FunctionPorts(g, NodeKind::SubInput);
    for (size_t p = 0; p < ins.size(); ++p) {
        Node* in = g.Find(ins[p]->id);
        in->surface = 0;
        if (p >= f->inputs.size() || !f->inputs[p]) continue;
        gpu::Texture* t = NodeOutputTexture(parent, f->inputs[p], InputChannel(*f, p));
        f = pd.S().graph.Find(fid);
        if (!t) continue;
        const Id s = doc->CreateSurfaceRaw(t->GetFormat(), "FunctionInput");
        ctx->CopyTexture(doc->GetSurface(s)->tex.get(), t);
        doc->TouchSurface(s);
        in->surface = s;
    }
    if (!g.Find(g.output)) {
        auto outs = FunctionPorts(g, NodeKind::SubOutput);
        g.output = outs.empty() ? 0 : outs[0]->id;
    }
    doc->S().graph = std::move(g);
    doc->S().activeNode = 0;
    doc->dirty = false;
    auto view = std::make_unique<DocView>();
    view->doc = std::move(doc);
    view->funcLink.parent = &parent;
    view->funcLink.node = fid;
    view->funcLink.syncKey = 0;
    docs.push_back(std::move(view));
    activeDoc = focusDocRequest = (int)docs.size() - 1;
    graphFrameRequest = true;
    Status("Édition de « " + f->name + " » : chaque modification met à jour le nœud. Nœuds Entrée / Sortie de fonction = ses broches.");
}

void App::UpdateFunctionEdits() {
    if (auto [rv, rid] = std::exchange(functionEditRequest, {nullptr, 0}); rv && ViewAlive(rv)) EditFunction(*rv, rid);
    for (auto& vp : docs) {
        DocView& v = *vp;
        if (!v.funcLink.parent) continue;
        if (!ViewAlive(v.funcLink.parent)) { v.funcLink = {}; continue; }
        Document& pd = *v.funcLink.parent->doc;
        Node* f = pd.S().graph.Find(v.funcLink.node);
        if (!f || f->kind != NodeKind::Function) { v.funcLink = {}; continue; }
        Document& cd = *v.doc;
        u64 key = ((u64)v.history.Cursor() << 32) ^ v.history.Count();
        for (auto& [sid, s] : cd.S().surfaces) key = (key ^ (s.version + sid)) * 1099511628211ull;
        if (key == v.funcLink.syncKey) continue;
        v.funcLink.syncKey = key;
        Graph g = cd.S().graph;
        for (auto& n : g.nodes) if (n.kind == NodeKind::SubInput) n.surface = 0;
        std::set<Id> surfs, smarts;
        CollectGraphSurfaces(g, surfs);
        CollectGraphSmart(g, smarts);
        for (Id s : surfs)
            if (Surface* cs = cd.GetSurface(s)) pd.S().surfaces[s] = *cs;
        for (Id s : smarts)
            if (auto it = cd.smartObjects.find(s); it != cd.smartObjects.end()) pd.smartObjects[s] = it->second;
        f->sub = std::make_shared<Graph>(std::move(g));
        f->funcKind = cd.functionKind.empty() ? f->funcKind : cd.functionKind;
        const size_t nin = (size_t)NodeInputCount(*f);
        f->inputs.resize(nin, 0);
        if (f->inputChannel.size() > nin) f->inputChannel.resize(nin);
        pd.S().nextId = std::max(pd.S().nextId, cd.S().nextId);
        pd.dirty = true;
        cd.dirty = false;
    }
}

bool App::FunctionApplyNow(DocView& v, Id fid, int tries) {
    Document& d = *v.doc;
    Node* f = d.S().graph.Find(fid);
    if (!f || f->kind != NodeKind::Function || !f->sub) return true;
    const int nout = NodeOutputCount(*f);
    if (!nout) { Status(f->name + " : la fonction n'a aucune sortie"); return true; }
    std::vector<gpu::Texture*> tex(nout, nullptr);
    for (int k = 0; k < nout; ++k) {
        const Id flat = FunctionOutputFlatId(*f, k);
        tex[k] = v.renderer.EvaluateNode(d, *ctx, flat, fid);
        f = d.S().graph.Find(fid);
        if ((!tex[k] || !v.renderer.NodeHash(flat)) && tries < 240) return false;
    }
    DocState before = d.S();
    std::vector<Id> outs;
    for (int k = 0; k < nout; ++k) {
        if (!tex[k]) { outs.push_back(0); continue; }
        const Id s = d.CreateSurfaceRaw(tex[k]->GetFormat(), "Applied");
        ctx->CopyTexture(d.GetSurface(s)->tex.get(), tex[k]);
        d.TouchSurface(s);
        outs.push_back(s);
    }
    f = d.S().graph.Find(fid);
    f->outSurf = outs;
    f->applyMode = true;
    d.dirty = true;
    v.history.PushState("Appliquer : " + f->name, before, d.S());
    Status(f->name + " appliqué (" + std::to_string(nout) + " sortie(s) en pixels) : les nœuds avant sont figés");
    return true;
}

void App::SaveFunctionToLibrary(DocView& v, Id fid) {
    Document& d = *v.doc;
    const Node* f = d.S().graph.Find(fid);
    if (!f || f->kind != NodeKind::Function || !f->sub) return;
    const std::string dir = UserFunctionDir();
    std::error_code ec;
    fs::create_directories(Utf8Path(dir), ec);
    const std::string base = dir + "\\" + SafeFileName(f->name);
    // A document made of the function graph (pixels shared with this one).
    auto doc = std::make_unique<Document>(d.Width(), d.Height(), d.GetPrecision(), DocMode::Nodes, f->name);
    doc->functionKind = f->funcKind;
    doc->blendInGamma = d.blendInGamma;
    Graph g = *f->sub;
    doc->S().surfaces.clear();
    std::set<Id> surfs, smarts;
    CollectGraphSurfaces(g, surfs);
    CollectGraphSmart(g, smarts);
    for (Id s : surfs) if (Surface* ps = d.GetSurface(s)) doc->S().surfaces[s] = *ps;
    for (Id s : smarts) if (auto it = d.smartObjects.find(s); it != d.smartObjects.end()) doc->smartObjects[s] = it->second;
    doc->S().nextId = std::max(d.S().nextId, MaxGraphId(g) + 1);
    doc->S().graph = std::move(g);
    auto keep = std::make_shared<DocView>();
    keep->doc = std::move(doc);
    SaveDocument(*keep, base + ".sxl");
    deferredViews.push_back({frameCounter + 120, keep});  // until the save has read the pixels
    Json j = Json::Object();
    j["name"] = f->name;
    j["kind"] = f->funcKind;
    j["description"] = f->sub->description;
    j["inputs"] = NodeInputCount(*f);
    j["outputs"] = NodeOutputCount(*f);
    std::ofstream(Utf8Path(base + ".json"), std::ios::binary) << j.Dump(1);
    ScanFunctionLibrary();
    Status("« " + f->name + " » enregistré dans la bibliothèque (" + base + ".sxl) : menu des nœuds > Bibliothèque");
}

void App::FunctionNodeUI(DocView& v, Node& n) {
    Document& d = *v.doc;
    const Id id = n.id;
    static DocState snap;
    ImGui::TextDisabled("%s - %d entrée(s), %d sortie(s)", n.funcKind == "tool" ? "Outil personnalisé (fonction)" : "Sous-graphe (fonction)",
                        NodeInputCount(n), NodeOutputCount(n));
    int kind = n.funcKind == "tool" ? 1 : 0;
    const char* kKinds[] = {"Sous-graphe", "Outil"};
    if (ImGui::Combo("Type", &kind, kKinds, 2)) {
        DocState before = d.S();
        n.funcKind = kind ? "tool" : "subgraph";
        v.history.PushState("Type de fonction", before, d.S());
    }
    ImGui::SeparatorText("Mode");
    int mode = n.applyMode ? 1 : 0;
    if (ImGui::RadioButton("Live", mode == 0) && n.applyMode) RequestToolAction(v, id, ToolAction::ToggleLive);
    ui::HelpTooltip("Le graphe de la fonction est calculé en direct à chaque changement.");
    ImGui::SameLine();
    if (ImGui::RadioButton("Apply", mode == 1) && !n.applyMode) RequestToolAction(v, id, ToolAction::ToggleLive);
    ui::HelpTooltip("Les sorties sont gardées en pixels quand vous cliquez « Appliquer » : les nœuds avant sont figés (plus rien à calculer).");
    if (n.applyMode) {
        if (FunctionApplied(n)) ImGui::TextColored(ImVec4(1, 0.75f, 0.3f, 1), "Appliqué : sorties en pixels, nœuds avant figés");
        else ImGui::TextColored(ImVec4(1, 0.8f, 0.4f, 1), "Pas encore appliqué (calculé en direct en attendant)");
        if (ImGui::Button(FunctionApplied(n) ? "Réappliquer" : "Appliquer")) RequestToolAction(v, id, ToolAction::Bake);
        ImGui::SameLine();
        ImGui::BeginDisabled(!FunctionApplied(n));
        if (ImGui::Button("Convertir en image")) RequestToolAction(v, id, ToolAction::ConvertToImage);
        ImGui::EndDisabled();
    }
    ImGui::SeparatorText("Fonction");
    if (ImGui::Button("Modifier (double-clic)")) EditFunction(v, id);
    ImGui::SameLine();
    if (ImGui::Button("Enregistrer dans la bibliothèque")) SaveFunctionToLibrary(v, id);
    if (n.sub) {
        std::string desc = n.sub->description;
        if (ImGui::InputTextMultiline("Description", &desc, ImVec2(0, 48 * ImGui::GetFontSize() / 13.0f))) {
            auto g = std::make_shared<Graph>(*n.sub);
            g->description = desc;
            n.sub = g;
            d.dirty = true;
        }
        if (ImGui::IsItemActivated()) snap = d.S();
        if (ImGui::IsItemDeactivatedAfterEdit()) v.history.PushState("Description", snap, d.S());
        ImGui::TextDisabled("Entrées :");
        for (const Node* p : FunctionPorts(*n.sub, NodeKind::SubInput)) { ImGui::SameLine(); ImGui::TextUnformatted(p->name.c_str()); }
        ImGui::TextDisabled("Sorties :");
        for (const Node* p : FunctionPorts(*n.sub, NodeKind::SubOutput)) { ImGui::SameLine(); ImGui::TextUnformatted(p->name.c_str()); }
    }
}

}  // namespace sx
