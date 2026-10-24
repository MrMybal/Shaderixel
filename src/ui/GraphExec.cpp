// Typed graph nodes in the UI and the execution flow.
// - Properties of values, operations, variables, events, actions, function pins; value editors
//   (scalars and arrays); variables of a graph; effect parameters shown as pins.
// - Interpreter: an Event node starts a flow; Action / VarSet nodes run along the exec links
//   (sequence, branch, loops, bake, upscale, ComfyUI, export, message). Image results are kept
//   (Action: its surface; in a function: per function node), so the nodes before are no longer
//   evaluated - bakes are handled by the flow itself.
#include "ui/App.h"
#include "ui/Theme.h"
#include "doc/GraphTypes.h"
#include "ai/Comfy.h"

#include <imgui_stdlib.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <filesystem>

namespace sx {

namespace {

gpu::Format ExecContentFormat(const Document& d) { return d.GetPrecision() == Precision::Float32 ? gpu::Format::RGBA32F : gpu::Format::RGBA16F; }

std::string EncodePngExec(const u8* rgba, int w, int h) {
    std::string s;
    stbi_write_png_to_func([](void* c, void* data, int size) { ((std::string*)c)->append((const char*)data, size); }, &s, w, h, 4, rgba, w * 4);
    return s;
}

bool ScalarEditor(const char* id, PinType t, float4& f, std::string& s) {
    switch (t) {
        case PinType::Float: return ImGui::DragFloat(id, &f.x, 0.01f);
        case PinType::Int: {
            int i = (int)std::round(f.x);
            if (ImGui::DragInt(id, &i)) { f.x = (float)i; return true; }
            return false;
        }
        case PinType::Bool: {
            bool b = f.x != 0;
            if (ImGui::Checkbox(id, &b)) { f.x = b ? 1.0f : 0.0f; return true; }
            return false;
        }
        case PinType::Vec2: return ImGui::DragFloat2(id, &f.x, 0.01f);
        case PinType::Color: return ImGui::ColorEdit4(id, &f.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaPreviewHalf);
        case PinType::String:
        case PinType::Name: return ImGui::InputText(id, &s);
        default: ImGui::TextDisabled("(image)"); return false;
    }
}

}  // namespace

// Editor of a value: one widget, or a list for arrays (add / remove items).
bool ValueEditor(const char* label, Value& v) {
    bool changed = false;
    ImGui::PushID(label);
    if (!v.array) {
        changed = ScalarEditor(label, v.type, v.f, v.s);
    } else {
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu)", v.Size());
        ImGui::Indent();
        const bool text = PinIsText(v.type);
        int remove = -1;
        for (size_t i = 0; i < v.Size(); ++i) {
            ImGui::PushID((int)i);
            float4 f = text ? float4{} : v.fa[i];
            std::string s = text ? v.sa[i] : "";
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() * 1.6f);
            if (ScalarEditor(("##" + std::to_string(i)).c_str(), v.type, f, s)) {
                if (text) v.sa[i] = s;
                else v.fa[i] = f;
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) remove = (int)i;
            ImGui::PopID();
        }
        if (remove >= 0) {
            if (text) v.sa.erase(v.sa.begin() + remove);
            else v.fa.erase(v.fa.begin() + remove);
            changed = true;
        }
        if (ImGui::SmallButton("+ élément")) {
            if (text) v.sa.push_back(v.sa.empty() ? "" : v.sa.back());
            else v.fa.push_back(v.fa.empty() ? (v.type == PinType::Color ? float4{1, 1, 1, 1} : float4{}) : v.fa.back());
            changed = true;
        }
        ImGui::Unindent();
    }
    ImGui::PopID();
    return changed;
}

namespace {
// Pin type picker (value types, image, and array flag).
bool PinTypeCombo(const char* label, PinType& t, bool& array, bool allowImage) {
    bool changed = false;
    if (ImGui::BeginCombo(label, PinTypeLabel(t))) {
        for (int k = 0; k <= (int)PinType::Name; ++k) {
            PinType c = (PinType)k;
            if (c == PinType::Channel || (!allowImage && PinIsImage(c))) continue;
            if (ImGui::Selectable(PinTypeLabel(c), c == t)) { t = c; changed = true; }
        }
        ImGui::EndCombo();
    }
    if (PinIsValue(t)) {
        ImGui::SameLine();
        if (ImGui::Checkbox("Tableau", &array)) changed = true;
    } else if (array) {
        array = false;
        changed = true;
    }
    return changed;
}
}  // namespace

// ------------------------------------------------------------------ properties

void App::ValueInputsUI(DocView& v, Node& n) {
    // Unlinked value inputs of the node: their values (shown on the node too).
    Document& d = *v.doc;
    static DocState snap;
    std::vector<PinDesc> ins;
    NodeSignature(n, &ins, nullptr);
    bool any = false;
    for (int i = 0; i < (int)ins.size(); ++i) {
        if (!PinIsValue(ins[i].type)) continue;
        if (!any) ImGui::SeparatorText("Entrées");
        any = true;
        if (i < (int)n.inputs.size() && n.inputs[i]) {
            ImGui::TextDisabled("%s : relié", ins[i].name.c_str());
            continue;
        }
        Value val = NodePinDefault(n, i);
        if (ValueEditor(ins[i].name.c_str(), val)) {
            NodeSetPinValue(n, i, val);
            d.dirty = true;
        }
        if (ImGui::IsItemActivated()) snap = d.S();
        if (ImGui::IsItemDeactivatedAfterEdit()) v.history.PushState("Valeur", snap, d.S());
    }
}

void App::GraphVariablesUI(DocView& v, Graph& g) {
    Document& d = *v.doc;
    ImGui::SeparatorText("Variables du graphe");
    ui::HelpTooltip("Lues par « Lire une variable », changées par « Définir une variable » (exécution). Dans une fonction, les variables exposées "
                    "se règlent sur chaque nœud de la fonction.");
    int remove = -1;
    for (size_t i = 0; i < g.variables.size(); ++i) {
        GraphVariable& var = g.variables[i];
        ImGui::PushID((int)i);
        std::string name = var.name;
        if (ImGui::InputText("Nom", &name, ImGuiInputTextFlags_EnterReturnsTrue) && !name.empty() && !g.Var(name)) {
            DocState before = d.S();
            for (auto& n : g.nodes)
                if ((n.kind == NodeKind::VarGet || n.kind == NodeKind::VarSet) && n.op == var.name) {
                    n.op = name;
                    n.name = (n.kind == NodeKind::VarGet ? "Lire " : "Définir ") + name;
                }
            var.name = name;
            v.history.PushState("Renommer la variable", before, d.S());
        }
        PinType t = var.value.type;
        bool arr = var.value.array;
        if (PinTypeCombo("Type", t, arr, false)) {
            DocState before = d.S();
            var.value = ConvertValue(var.value, t, arr);
            for (auto& n : g.nodes)
                if ((n.kind == NodeKind::VarGet || n.kind == NodeKind::VarSet) && n.op == var.name) n.value = MakeValue(t, arr);
            v.history.PushState("Type de variable", before, d.S());
        }
        if (ValueEditor("Valeur", var.value)) d.dirty = true;
        ImGui::Checkbox("Exposée (réglable sur le nœud de la fonction)", &var.exposed);
        if (ImGui::SmallButton("Supprimer la variable")) remove = (int)i;
        ImGui::Separator();
        ImGui::PopID();
    }
    if (remove >= 0) {
        DocState before = d.S();
        g.variables.erase(g.variables.begin() + remove);
        v.history.PushState("Supprimer la variable", before, d.S());
    }
    if (ImGui::Button("+ Variable")) {
        DocState before = d.S();
        GraphVariable var;
        int k = (int)g.variables.size() + 1;
        do var.name = "Variable " + std::to_string(k++);
        while (g.Var(var.name));
        var.value = MakeValue(PinType::Float);
        g.variables.push_back(var);
        v.history.PushState("Nouvelle variable", before, d.S());
    }
}

void App::TypedNodeUI(DocView& v, Node& n) {
    Document& d = *v.doc;
    Graph& g = d.S().graph;
    switch (n.kind) {
        case NodeKind::Value: {
            PinType t = n.value.type;
            bool arr = n.value.array;
            if (PinTypeCombo("Type", t, arr, false)) {
                DocState before = d.S();
                n.value = ConvertValue(n.value, t, arr);
                v.history.PushState("Type de valeur", before, d.S());
            }
            static DocState snap;
            if (ValueEditor("Valeur", n.value)) d.dirty = true;
            if (ImGui::IsItemActivated()) snap = d.S();
            if (ImGui::IsItemDeactivatedAfterEdit()) v.history.PushState("Valeur", snap, d.S());
            break;
        }
        case NodeKind::SubInput:
        case NodeKind::SubOutput: {
            ImGui::TextDisabled(n.kind == NodeKind::SubInput ? "Entrée de la fonction (broche du nœud)" : "Sortie de la fonction (broche du nœud)");
            PinType t = n.value.type;
            bool arr = n.value.array;
            if (PinTypeCombo("Type", t, arr, true)) {
                DocState before = d.S();
                n.value = ConvertValue(n.value, t, arr);
                if (!PinIsValue(t)) n.value = MakeValue(t);
                v.history.PushState("Type de broche", before, d.S());
            }
            if (n.kind == NodeKind::SubInput && PinIsValue(n.value.type)) {
                ImGui::TextDisabled("Valeur par défaut (si rien n'est relié sur le nœud) :");
                if (ValueEditor("Défaut", n.value)) d.dirty = true;
            }
            if (ImGui::InputInt("Ordre", &n.port)) d.dirty = true;
            break;
        }
        case NodeKind::Math:
            if (auto* m = FindMathOp(n.op)) ImGui::TextDisabled("%s - %s", m->category.c_str(), m->id.c_str());
            ValueInputsUI(v, n);
            ImGui::Text("Résultat : %s", ValueToText(v.renderer.EvalValue(d, n.id, 0), 6).c_str());
            break;
        case NodeKind::VarGet:
        case NodeKind::VarSet: {
            if (ImGui::BeginCombo("Variable", n.op.c_str())) {
                for (auto& var : g.variables)
                    if (ImGui::Selectable(var.name.c_str(), var.name == n.op)) {
                        DocState before = d.S();
                        n.op = var.name;
                        n.value = MakeValue(var.value.type, var.value.array);
                        n.name = (n.kind == NodeKind::VarGet ? "Lire " : "Définir ") + var.name;
                        v.history.PushState("Variable", before, d.S());
                    }
                ImGui::EndCombo();
            }
            if (const GraphVariable* var = g.Var(n.op)) ImGui::Text("Valeur : %s", ValueToText(var->value, 6).c_str());
            if (n.kind == NodeKind::VarSet) ValueInputsUI(v, n);
            GraphVariablesUI(v, g);
            break;
        }
        case NodeKind::Event:
            ImGui::TextDisabled("Point de départ d'une exécution : son nom est le bouton.");
            if (v.funcLink.parent) ImGui::TextWrapped("Dans une fonction : le bouton apparaît sur le nœud de la fonction (Propriétés, clic droit).");
            else if (ImGui::Button(("Exécuter : " + n.name).c_str())) StartExec(v, 0, n.id);
            break;
        case NodeKind::Action: {
            const ExecActionDef* a = FindExecAction(n.op);
            if (a) ImGui::TextWrapped("%s", a->desc.c_str());
            if (n.op == "call") {
                // Pick the function and its event among the function nodes of the document's graph.
                const std::string cur = NodePinDefault(n, 1).s;
                if (ImGui::BeginCombo("Fonction##call", cur.empty() ? "Choisir..." : cur.c_str())) {
                    for (auto& fn : d.S().graph.nodes)
                        if (fn.kind == NodeKind::Function && ImGui::Selectable(fn.name.c_str(), fn.name == cur)) {
                            DocState before = d.S();
                            Value v1 = MakeValue(PinType::Name);
                            v1.s = fn.name;
                            NodeSetPinValue(n, 1, v1);
                            v.history.PushState("Fonction appelée", before, d.S());
                        }
                    ImGui::EndCombo();
                }
                const Node* target = nullptr;
                for (auto& fn : d.S().graph.nodes) if (fn.kind == NodeKind::Function && fn.name == cur && fn.sub) target = &fn;
                const std::string ev = NodePinDefault(n, 2).s;
                if (target && ImGui::BeginCombo("Événement##call", ev.empty() ? "(le premier)" : ev.c_str())) {
                    for (auto& m : target->sub->nodes)
                        if (m.kind == NodeKind::Event && ImGui::Selectable(m.name.c_str(), m.name == ev)) {
                            DocState before = d.S();
                            Value v2 = MakeValue(PinType::Name);
                            v2.s = m.name;
                            NodeSetPinValue(n, 2, v2);
                            v.history.PushState("Événement appelé", before, d.S());
                        }
                    ImGui::EndCombo();
                }
            }
            ValueInputsUI(v, n);
            if (n.surface) {
                ImGui::TextColored(ImVec4(1, 0.75f, 0.3f, 1), "Résultat gardé : les nœuds avant ne sont plus calculés");
                if (ImGui::Button("Effacer le résultat")) {
                    DocState before = d.S();
                    n.surface = 0;
                    n.smart = 0;
                    v.history.PushState("Effacer le résultat", before, d.S());
                }
            }
            break;
        }
        default: break;
    }
}

// Function node: its value inputs, exposed variables and execution buttons.
void App::FunctionSettingsUI(DocView& v, Node& f) {
    Document& d = *v.doc;
    ValueInputsUI(v, f);
    if (!f.sub) return;
    bool header = false;
    for (auto& var : f.sub->variables) {
        if (!var.exposed) continue;
        if (!header) ImGui::SeparatorText("Variables");
        header = true;
        Value val = var.value;
        auto it = f.varValues.find(var.name);
        if (it != f.varValues.end()) val = ConvertValue(it->second, var.value.type, var.value.array);
        static DocState snap;
        if (ValueEditor(var.name.c_str(), val)) {
            f.varValues[var.name] = val;
            d.dirty = true;
        }
        if (ImGui::IsItemActivated()) snap = d.S();
        if (ImGui::IsItemDeactivatedAfterEdit()) v.history.PushState("Variable", snap, d.S());
        if (it != f.varValues.end()) {
            ImGui::SameLine();
            if (ImGui::SmallButton(("Défaut##" + var.name).c_str())) {
                DocState before = d.S();
                f.varValues.erase(var.name);
                v.history.PushState("Variable par défaut", before, d.S());
            }
        }
    }
    std::vector<const Node*> events;
    for (auto& m : f.sub->nodes) if (m.kind == NodeKind::Event) events.push_back(&m);
    if (!events.empty()) {
        ImGui::SeparatorText("Exécution");
        const bool running = execRun && execRun->rootOwner == f.id;
        ImGui::BeginDisabled(running);
        for (const Node* e : events) {
            if (ImGui::Button(e->name.c_str())) StartExec(v, f.id, e->id);
            ImGui::SameLine();
        }
        ImGui::EndDisabled();
        ImGui::NewLine();
        if (running) ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Exécution en cours...");
        if (!f.execResults.empty() && ImGui::Button("Effacer les résultats")) {
            DocState before = d.S();
            f.execResults.clear();
            f.execSmart.clear();
            v.history.PushState("Effacer les résultats", before, d.S());
        }
    }
}

// Effect node: which parameters are pins (linked to values).
void App::EffectPinsUI(DocView& v, Node& n) {
    Document& d = *v.doc;
    auto def = n.fx.Def();
    if (!def || def->params.empty()) return;
    AlignParamPins(n);  // pins follow their parameters if the definition changed
    if (!ImGui::CollapsingHeader("Paramètres en broches")) return;
    ui::HelpTooltip("Cochez un paramètre pour le relier à une valeur du graphe (Float, Couleur, variable, entrée de fonction...).");
    for (int k = 0; k < (int)def->params.size(); ++k) {
        const ParamType t = def->params[k].type;
        if (t == ParamType::Curve || t == ParamType::Lut) continue;
        bool on = std::find(n.paramPins.begin(), n.paramPins.end(), k) != n.paramPins.end();
        if (ImGui::Checkbox((def->params[k].label + "##pp" + std::to_string(k)).c_str(), &on)) {
            DocState before = d.S();
            const int nimg = (int)def->inputs.size();
            // Keep the links of the other pins in place.
            std::map<int, std::pair<Id, u8>> links;
            for (size_t j = 0; j < n.paramPins.size(); ++j) {
                const size_t pin = nimg + j;
                if (pin < n.inputs.size()) links[n.paramPins[j]] = {n.inputs[pin], InputChannel(n, pin)};
            }
            if (on) n.paramPins.push_back(k);
            else n.paramPins.erase(std::remove(n.paramPins.begin(), n.paramPins.end(), k), n.paramPins.end());
            n.inputs.resize(nimg);
            if (n.inputChannel.size() > (size_t)nimg) n.inputChannel.resize(nimg);
            for (int pk : n.paramPins) {
                auto it = links.find(pk);
                n.inputs.push_back(it != links.end() ? it->second.first : 0);
                if (it != links.end() && it->second.second) {
                    n.inputChannel.resize(n.inputs.size(), 0);
                    n.inputChannel.back() = it->second.second;
                }
            }
            d.dirty = true;
            v.history.PushState("Paramètre en broche", before, d.S());
        }
    }
}

// ------------------------------------------------------------------ interpreter

void App::StartExec(DocView& v, Id owner, Id event) {
    if (execRun) { Status("Une exécution est déjà en cours"); return; }
    auto r = std::make_unique<ExecRun>();
    r->view = &v;
    r->owner = owner;
    r->event = event;
    r->before = v.doc->S();
    r->items.push_back({ExecItem::Follow, event, 0, 0, owner});
    r->rootOwner = owner;
    v.renderer.runtimeValues.clear();
    const Graph* g = ExecGraph(*r);
    const Node* e = g ? g->Find(event) : nullptr;
    r->label = e ? e->name : "Exécuter";
    static u64 runSerial = 0;
    r->serial = ++runSerial;
    execRun = std::move(r);
    Status("Exécution : " + execRun->label);
}

const Graph* App::ExecGraph(ExecRun& r) {
    Document& d = *r.view->doc;
    if (!r.owner) return &d.S().graph;
    const Node* f = d.S().graph.Find(r.owner);
    return f && f->sub ? f->sub.get() : nullptr;
}

void App::FinishExec(const std::string& msg) {
    if (!execRun) return;
    ExecRun& r = *execRun;
    if (ViewAlive(r.view)) {
        r.view->doc->dirty = true;
        r.view->history.PushState("Exécution : " + r.label, r.before, r.view->doc->S());
    }
    Status(msg);
    execRun.reset();
}

// Stores an image result of an action (its surface; inside a function, on the function node).
void App::ExecStoreResult(ExecRun& r, Id node, gpu::Texture* tex, gpu::TexturePtr full) {
    Document& d = *r.view->doc;
    const Id s = d.CreateSurfaceRaw(SurfaceFormat(d.GetPrecision()), "ExecResult");
    if (tex->Width() == d.Width() && tex->Height() == d.Height() && tex->GetFormat() == SurfaceFormat(d.GetPrecision())) {
        ctx->CopyTexture(d.GetSurface(s)->tex.get(), tex);
        d.TouchSurface(s);
    } else {
        const float W = (float)d.Width(), H = (float)d.Height();
        const float2 c[4] = {{0, 0}, {W, 0}, {W, H}, {0, H}};
        r.view->renderer.PlaceSmart(d, *ctx, s, tex, c, 2);
    }
    Id smart = 0;
    if (full) smart = NewSmartObject(d, "Résultat", full)->id;
    if (r.owner) {
        if (Node* f = d.S().graph.Find(r.owner)) {
            f->execResults[node] = s;
            if (smart) f->execSmart[node] = smart;
            else f->execSmart.erase(node);
        }
    } else if (Node* n = d.S().graph.Find(node)) {
        n->surface = s;
        n->smart = smart;
    }
    d.dirty = true;
}

void App::UpdateExec() {
    if (!execRun) return;
    ExecRun& r = *execRun;
    if (!ViewAlive(r.view)) { execRun.reset(); return; }
    if (r.waiting) return;
    Document& d = *r.view->doc;
    Renderer& rd = r.view->renderer;
    auto flat = [&](Id inner) { return r.owner ? FlatChildId(r.owner, inner) : inner; };
    // Input i of an inner node: its value (through the flattened graph).
    auto inValue = [&](Id inner, int i) -> Value {
        rd.BeginGraph(d, r.owner);
        const Node* fn = rd.EvalGraph(d).Find(flat(inner));
        return fn ? rd.InputValue(d, *fn, i) : Value{};
    };
    auto inImage = [&](Id inner, int i) -> gpu::Texture* {
        rd.BeginGraph(d, r.owner);
        const Node* fn = rd.EvalGraph(d).Find(flat(inner));
        if (!fn || i >= (int)fn->inputs.size() || !fn->inputs[i]) return nullptr;
        const Id src = fn->inputs[i];
        rd.NewEvalPass();  // variables / loop values may have changed since the last evaluation
        return rd.EvaluateNode(d, *ctx, src, r.owner);
    };
    // Full-size result of the node feeding input i (an upscale before an export...).
    auto inFull = [&](Id inner, int i) -> gpu::Texture* {
        const Graph* g = ExecGraph(r);
        const Node* n = g ? g->Find(inner) : nullptr;
        if (!n || i >= (int)n->inputs.size()) return nullptr;
        Id src = n->inputs[i];
        Id smart = 0;
        if (r.owner) {
            if (const Node* f = d.S().graph.Find(r.owner))
                if (auto it = f->execSmart.find(src); it != f->execSmart.end()) smart = it->second;
        } else if (const Node* s = d.S().graph.Find(src)) {
            smart = s->kind == NodeKind::Action ? s->smart : 0;
        }
        SmartObject* so = smart ? d.GetSmart(smart) : nullptr;
        return so ? so->content.get() : nullptr;
    };
    for (int budget = 0; budget < 256 && !r.items.empty() && !r.waiting; ++budget) {
        if (++r.steps > 200000) { FinishExec(r.label + " : arrêtée (trop d'étapes, boucle infinie ?)"); return; }
        ExecItem it = r.items.back();
        r.items.pop_back();
        r.owner = it.owner;  // the graph this step belongs to (a call switches functions)
        const Graph* g = ExecGraph(r);
        if (!g) { FinishExec(r.label + " : graphe introuvable"); return; }
        if (it.kind == ExecItem::Follow) {
            // Nodes whose execution input comes from (node, port), run in graph order.
            std::vector<Id> next;
            const Node* from = g->Find(it.node);
            for (auto& n : g->nodes) {
                bool hit = false;
                for (auto& e : n.execLinks)  // flows joining this input
                    hit |= e.src == it.node && from && NodeOutputPortOf(*from, e.channel) == it.port;
                if (hit) { next.push_back(n.id); continue; }
                for (size_t i = 0; i < n.inputs.size(); ++i) {
                    if (n.inputs[i] != it.node || NodeInputType(n, (int)i) != PinType::Exec) continue;
                    const Node* src = g->Find(it.node);
                    if (src && NodeOutputPortOf(*src, InputChannel(n, i)) == it.port) { next.push_back(n.id); break; }
                }
            }
            for (auto k = next.rbegin(); k != next.rend(); ++k) r.items.push_back({ExecItem::Run, *k, 0, 0, r.owner});
            continue;
        }
        const Node* n = g->Find(it.node);
        if (!n) continue;
        const Id id = n->id;
        if (n->kind == NodeKind::VarSet) {
            const Value val = inValue(id, 1);
            const std::string name = n->op;
            if (r.owner) {
                if (Node* f = d.S().graph.Find(r.owner)) f->varValues[name] = val;
            } else if (GraphVariable* gv = d.S().graph.Var(name)) {
                gv->value = ConvertValue(val, gv->value.type, gv->value.array);
            }
            r.items.push_back({ExecItem::Follow, id, 0, 0, r.owner});
            continue;
        }
        if (n->kind == NodeKind::Reroute) {
            r.items.push_back({ExecItem::Follow, id, 0, 0, r.owner});  // relay of an execution link
            continue;
        }
        if (n->kind != NodeKind::Action) continue;
        const std::string op = n->op;
        auto follow = [&](int port) { r.items.push_back({ExecItem::Follow, id, port, 0, r.owner}); };
        if (op == "sequence") {
            follow(2);
            follow(1);
            follow(0);
        } else if (op == "branch") {
            follow(inValue(id, 1).f.x != 0 ? 0 : 1);
        } else if (op == "for_range" || op == "foreach" || op == "foreach_text") {
            // Loop: the item comes back with the next index after the body.
            const int i = it.kind == ExecItem::Loop ? it.index : (op == "for_range" ? (int)inValue(id, 1).f.x : 0);
            bool more;
            const u64 key = (u64)flat(id) << 8;
            if (op == "for_range") {
                more = i <= (int)inValue(id, 2).f.x;
                if (more) { Value iv = MakeValue(PinType::Int); iv.f.x = (float)i; rd.runtimeValues[key | 1] = iv; }
            } else {
                Value arr = inValue(id, 1);
                more = i < (int)arr.Size();
                if (more) {
                    Value e = MakeValue(arr.type);
                    if (PinIsText(arr.type)) e.s = arr.sa[i];
                    else e.f = arr.fa[i];
                    Value iv = MakeValue(PinType::Int);
                    iv.f.x = (float)i;
                    rd.runtimeValues[key | 1] = e;
                    rd.runtimeValues[key | 2] = iv;
                }
            }
            if (more) {
                r.items.push_back({ExecItem::Loop, id, 0, i + 1, r.owner});
                follow(0);
            } else {
                follow(op == "for_range" ? 2 : 3);
            }
        } else if (op == "call") {
            // Another function's event (top-level function node, by name): it runs, then this flow goes on.
            const std::string fname = inValue(id, 1).s, ename = inValue(id, 2).s;
            const Node* target = nullptr;
            for (auto& fn : d.S().graph.nodes)
                if (fn.kind == NodeKind::Function && fn.name == fname && fn.sub) target = &fn;
            const Node* evn = nullptr;
            if (target)
                for (auto& m : target->sub->nodes)
                    if (m.kind == NodeKind::Event && (ename.empty() || m.name == ename) && !evn) evn = &m;
            follow(0);  // continuation, below the call on the stack
            if (target && evn) r.items.push_back({ExecItem::Follow, evn->id, 0, 0, target->id});
            else Status(n->name + " : fonction « " + fname + " » ou événement « " + ename + " » introuvable");
        } else if (op == "print") {
            Status(inValue(id, 1).s);
            follow(0);
        } else if (op == "bake") {
            gpu::Texture* tex = inImage(id, 1);
            if (tex) ExecStoreResult(r, id, tex, nullptr);
            else Status(n->name + " : pas d'image en entrée");
            follow(0);
        } else if (op == "upscale" || op == "comfyui") {
            gpu::Texture* tex = inImage(id, 1);
            if (!tex) { Status(n->name + " : pas d'image en entrée"); follow(0); continue; }
            const int factor = std::clamp((int)inValue(id, 2).f.x, 1, 8);
            const bool ai = op == "comfyui" || inValue(id, 3).f.x != 0;
            if (!ai) {
                const u32 uw = std::min<u32>(tex->Width() * factor, 16384), uh = std::min<u32>(tex->Height() * factor, 16384);
                auto full = gpu::Dev().CreateTexture(uw, uh, ExecContentFormat(d), "Upscale");
                if (!full) { Status(n->name + " : image agrandie trop grande pour la carte graphique"); follow(0); continue; }
                const float2 c[4] = {{0, 0}, {(float)uw, 0}, {(float)uw, (float)uh}, {0, (float)uh}};
                rd.PlaceTexture(*ctx, full.get(), tex, c, 2);
                ExecStoreResult(r, id, tex, full);
                follow(0);
                continue;
            }
            // ComfyUI: the image goes out as a PNG, the flow waits for the result.
            comfy::Job job;
            job.endpoint = aiSettings.comfyEndpoint;
            job.checkpoint = aiSettings.comfyCheckpoint;
            job.negative = aiSettings.comfyNegative;
            job.steps = aiSettings.comfySteps;
            u32 outW = tex->Width(), outH = tex->Height();
            if (op == "upscale") {
                outW = std::min<u32>(outW * factor, 16384);
                outH = std::min<u32>(outH * factor, 16384);
                Json wf = Json::Object();
                auto node = [&](const char* nid, const char* cls, Json in) { Json o = Json::Object(); o["class_type"] = cls; o["inputs"] = std::move(in); wf[nid] = std::move(o); };
                auto link = [](const char* nid, int k) { Json a = Json::Array(); a.Push(nid); a.Push(k); return a; };
                Json i1 = Json::Object(); i1["image"] = "input.png"; node("1", "LoadImage", i1);
                Json i2 = Json::Object(); i2["model_name"] = inValue(id, 4).s; node("2", "UpscaleModelLoader", i2);
                Json i3 = Json::Object(); i3["upscale_model"] = link("2", 0); i3["image"] = link("1", 0); node("3", "ImageUpscaleWithModel", i3);
                Json i4 = Json::Object(); i4["image"] = link("3", 0); i4["upscale_method"] = "lanczos"; i4["width"] = (int)outW; i4["height"] = (int)outH; i4["crop"] = "disabled";
                node("4", "ImageScale", i4);
                Json i5 = Json::Object(); i5["filename_prefix"] = "shaderixel_upscale"; i5["images"] = link("4", 0); node("5", "SaveImage", i5);
                job.workflow = wf.Dump(1);
            } else {
                job.prompt = inValue(id, 2).s;
                job.denoise = inValue(id, 3).f.x;
                const std::string wfPath = inValue(id, 4).s;
                if (!wfPath.empty() && !ReadTextFile(wfPath, job.workflow)) { FinishExec(n->name + " : workflow introuvable (" + wfPath + ")"); return; }
            }
            job.width = (int)tex->Width();
            job.height = (int)tex->Height();
            r.waiting = true;
            // The user may edit while ComfyUI works (minutes): the steps done so far become their own undo step,
            // and the result will be recorded from the state it lands on (undoing it keeps those edits).
            {
                Document& dd = *r.view->doc;
                r.view->history.PushState("Exécution : " + r.label, r.before, dd.S());
                r.before = dd.S();
            }
            r.pendingNode = id;
            r.pendingOwner = r.owner;
            r.pendingJob = job;
            r.pendingImage = rd.ExportRGBA8(*ctx, tex);
            Status(r.label + " : " + n->name + " (ComfyUI)...");
        } else if (op == "export_png") {
            gpu::Texture* tex = inFull(id, 1);
            if (!tex) tex = inImage(id, 1);
            std::string path = inValue(id, 2).s;
            if (getenv("SX_EXECDEBUG")) {
                std::string keys;
                for (auto& [k, val] : rd.runtimeValues) keys += std::to_string(k >> 8) + ":" + std::to_string(k & 0xFF) + "=" + ValueToText(val) + " ";
                Log("[exec] export %s runtime %s", path.c_str(), keys.c_str());
            }
            if (!tex || path.empty()) { Status(n->name + " : image ou fichier manquant"); follow(0); continue; }
            std::filesystem::path p = Utf8Path(path);
            if (p.is_relative()) {
                std::filesystem::path base = d.path.empty() ? Utf8Path(ExeDir()) / L"exports" : Utf8Path(d.path).parent_path();
                p = base / p;
            }
            if (p.extension().empty()) p += L".png";
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
            toolExports.push_back({rd.ExportRGBA8(*ctx, tex), Narrow(p.wstring())});
            follow(0);
        } else {
            follow(0);
        }
    }
    // A ComfyUI step: once the image is read back, the request goes to a worker.
    if (r.waiting && r.pendingImage && r.pendingImage->ready && !r.pendingSent) {
        r.pendingSent = true;
        const auto& img = *r.pendingImage;
        std::vector<u8> opaque(img.data.begin(), img.data.end());
        for (size_t i = 3; i < opaque.size(); i += 4) opaque[i] = 255;
        std::string png = EncodePngExec(opaque.data(), (int)img.width, (int)img.height);
        comfy::Job job = r.pendingJob;
        job.inputPng.assign(png.begin(), png.end());
        ExecRun* run = &r;
        const u64 serial = r.serial;
        AiAsync([this, job, run, serial]() -> std::function<void()> {
            std::vector<u8> out;
            std::string err;
            bool ok = comfy::Run(job, out, err);
            return [this, ok, out = std::move(out), err, run, serial]() {
                if (!execRun || execRun.get() != run || execRun->serial != serial || !ViewAlive(run->view)) return;
                if (!ok || out.empty()) { FinishExec(run->label + " : " + (err.empty() ? std::string("ComfyUI n'a renvoyé aucune image") : err)); return; }
                run->before = run->view->doc->S();  // edits made meanwhile stay out of this step
                int w = 0, h = 0, comp = 0;
                stbi_uc* px = stbi_load_from_memory(out.data(), (int)out.size(), &w, &h, &comp, 4);
                if (!px) { FinishExec(run->label + " : image ComfyUI illisible"); return; }
                Document& d = *run->view->doc;
                auto full = run->view->renderer.ImportTexture(*ctx, px, (u32)w, (u32)h, false, ExecContentFormat(d));
                stbi_image_free(px);
                if (!full) { FinishExec(run->label + " : image ComfyUI trop grande pour la carte graphique"); return; }
                run->owner = run->pendingOwner;
                ExecStoreResult(*run, run->pendingNode, full.get(), (u32)w != d.Width() || (u32)h != d.Height() ? full : nullptr);
                run->waiting = false;
                run->pendingSent = false;
                run->pendingImage.reset();
                run->items.push_back({ExecItem::Follow, run->pendingNode, 0, 0, run->pendingOwner});
            };
        });
    }
    if (execRun && !execRun->waiting && execRun->items.empty()) FinishExec(execRun->label + " : terminé (" + std::to_string(execRun->steps) + " étapes)");
}

}  // namespace sx
