// Visual effect editor (Unreal-material-editor style) for graph-based effects.
#include "ui/App.h"
#include "ui/Theme.h"
#include "doc/Brushes.h"
#include "doc/HlslImport.h"

#include <filesystem>
#include "doc/EffectGraph.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <fstream>
#include <set>

namespace sx {

namespace {
constexpr float kW = 168.0f;
constexpr float kHead = 24.0f;
constexpr float kRowH = 20.0f;

ImU32 CategoryColor(const char* cat) {
    std::string c = cat;
    if (c == "Entrées") return IM_COL32(36, 150, 168, 255);
    if (c == "Constantes") return IM_COL32(90, 110, 70, 255);
    if (c == "Paramètres") return IM_COL32(60, 150, 90, 255);
    if (c == "Maths") return IM_COL32(70, 84, 120, 255);
    if (c == "Vecteurs") return IM_COL32(110, 80, 150, 255);
    if (c == "Couleur") return IM_COL32(170, 90, 60, 255);
    if (c == "Procédural") return IM_COL32(170, 130, 50, 255);
    if (c == "Personnalisé") return IM_COL32(200, 92, 150, 255);
    if (c == "Fusion") return IM_COL32(190, 70, 110, 255);
    if (c == "Pinceau") return IM_COL32(60, 130, 200, 255);
    return IM_COL32(52, 160, 110, 255);  // output
}

int Rows(const EGOpDef& d) { return std::max<int>((int)std::max(d.inputs.size(), d.outputs.size()), 1); }

float NodeH(const EGNode& n, const EGOpDef& d) {
    float extra = (n.op == "const" || n.op == "const2" || n.op == "color" || n.op == "param" || n.op == "paramcolor" || n.op == "param2" ||
                   n.op == "input" || n.op == "sample" || n.op == "texture" || n.op == "mask" || n.op == "custom" || n.op == "brushtex")
                      ? kRowH
                      : 0.0f;
    return kHead + Rows(d) * kRowH + extra + 6;
}

std::string ValueText(const EGNode& n) {
    char b[96];
    if (n.op == "const") snprintf(b, sizeof(b), "%.3g", n.value.x);
    else if (n.op == "const2") snprintf(b, sizeof(b), "%.3g, %.3g", n.value.x, n.value.y);
    else if (n.op == "param" || n.op == "paramcolor" || n.op == "param2") snprintf(b, sizeof(b), "« %s »", (n.text.empty() ? n.name : n.text).c_str());
    else if (n.op == "texture" && !n.name.empty()) snprintf(b, sizeof(b), "Entrée %d (%s)", n.index + 1, n.name.c_str());
    else if (n.op == "brushtex") snprintf(b, sizeof(b), "Texture %d%s%s", n.index + 1, n.text.empty() ? "" : " : ", n.text.c_str());
    else if (n.op == "input" || n.op == "sample" || n.op == "texture") snprintf(b, sizeof(b), "Entrée %d", n.index + 1);
    else if (n.op == "mask") {
        std::string s;
        const char* c = "RGBA";
        int bits = n.index ? n.index : 7;
        for (int i = 0; i < 4; ++i) if (bits & (1 << i)) s += c[i];
        snprintf(b, sizeof(b), "%s", s.c_str());
    } else if (n.op == "custom") snprintf(b, sizeof(b), "code HLSL");
    else b[0] = 0;
    return b;
}
}  // namespace

EffectInstance* App::EditedEffect(DocView& v, std::string* where) {
    Document& d = *v.doc;
    if (d.Mode() == DocMode::Nodes) {
        Node* n = d.S().graph.Find(d.S().activeNode);
        if (n && n->kind == NodeKind::Effect) {
            if (where) *where = n->name;
            return &n->fx;
        }
        return nullptr;
    }
    Layer* l = d.ActiveLayer();
    if (!l) return nullptr;
    if ((l->kind == LayerKind::Fill || l->kind == LayerKind::Adjustment)) {
        if (where) *where = l->name;
        return &l->effect;
    }
    if (efEd.filterIndex >= 0 && efEd.filterIndex < (int)l->filters.size()) {
        if (where) *where = l->name + " (filtre " + std::to_string(efEd.filterIndex + 1) + ")";
        return &l->filters[efEd.filterIndex];
    }
    for (auto& f : l->filters)
        if (!f.graph.empty() || !f.customCode.empty()) {
            if (where) *where = l->name + " (filtre)";
            return &f;
        }
    return nullptr;
}

void App::DrawEffectEditor() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin("Éditeur d'effets", PanelFlag("Éditeur d'effets"), ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    DocView* v = Active();
    if (!open || !v) { ImGui::End(); return; }
    Document& d = *v->doc;
    std::string where;
    auto& ed = efEd;
    Layer* layer = d.Mode() == DocMode::Layers ? d.ActiveLayer() : nullptr;
    static Id lastLayer = 0;  // the filter chosen from Propriétés belongs to that layer
    if ((layer ? layer->id : 0) != lastLayer) { lastLayer = layer ? layer->id : 0; ed.filterIndex = -1; }
    BrushSettings& brushTarget = tool == Tool::Eraser ? eraser : brush;
    static EffectInstance brushProxy;  // the brush graph seen as an effect instance
    EffectInstance* fx = nullptr;
    if (ed.target == 1 && layer) {
        fx = &layer->blendFx;
        where = layer->name + " (fusion)";
    } else if (ed.target == 2) {
        if (brushProxy.graph != brushTarget.tipGraph) brushProxy.graph = brushTarget.tipGraph;
        brushProxy.params = brushTarget.tipParams;
        fx = &brushProxy;
        where = tool == Tool::Eraser ? "pinceau de la gomme" : "pinceau";
    } else {
        fx = EditedEffect(*v, &where);
    }
    const bool brushMode = ed.target == 2;

    // What is edited: effect, blend of the active layer, current brush.
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 8 * dpi, ImGui::GetCursorPosY() + 6 * dpi));
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Éditer :");
    ImGui::SameLine();
    ImGui::RadioButton("Effet", &ed.target, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Fusion du calque", &ed.target, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Pinceau", &ed.target, 2);

    // Code and library items (graphs continue below with their canvas).
    if (ed.target == 2 && brushTarget.tipGraph.empty()) {
        ImGui::BeginChild("##brushcode", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
        if (TipDef(brushTarget)) DrawBrushPad(*v, brushTarget);
        DrawBrushCodeEditor(*v, brushTarget);
        ImGui::EndChild();
        ImGui::End();
        return;
    }
    if (fx && fx->graph.empty() && (!fx->customCode.empty() || !fx->effectId.empty())) {
        ImGui::TextDisabled("  Édition : %s", where.c_str());
        ImGui::BeginChild("##fxcode", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
        if (!fx->customCode.empty()) DrawEffectCodeEditor(*v, *fx);
        else DrawLibraryItemView(*v, *fx, ed.target == 1);
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    if (!fx || fx->graph.empty()) {
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 16 * dpi, ImGui::GetCursorPosY() + 12 * dpi));
        ImGui::BeginGroup();
        if (ed.target == 1) {
            if (!layer) ImGui::TextWrapped("Sélectionnez un calque (mode calques) : sa fusion avec les calques du dessous se construit ici en graphe.");
            else {
                ImGui::TextWrapped("Fusion de « %s » : A = calques du dessous, B = ce calque, masque, opacité du calque.", layer->name.c_str());
                if (ImGui::Button("Créer une fusion en graphe")) {
                    Edit e(d, v->history, "Fusion en graphe");
                    layer->blendFx = {};
                    layer->blendFx.graph = EffectGraph::DefaultBlend().Serialize();
                    if (auto def = layer->blendFx.Def()) layer->blendFx.params = def->DefaultParams();
                    layer->passThrough = false;
                    d.SyncGraph();
                    e.Commit();
                }
                ImGui::SameLine();
                if (ImGui::Button("Créer une fusion en code")) {
                    Edit e(d, v->history, "Fusion personnalisée");
                    layer->blendFx = {};
                    layer->blendFx.customCode = kCustomBlendTemplate;
                    if (auto def = layer->blendFx.Def()) layer->blendFx.params = def->DefaultParams();
                    layer->passThrough = false;
                    d.SyncGraph();
                    e.Commit();
                }
            }
            ImGui::EndGroup();
            ImGui::End();
            return;
        }
        if (ed.target == 2) {
            ImGui::TextWrapped("Le pinceau actif n'est pas un graphe. Un graphe de pinceau dessine la pointe (nœud « Touche du pinceau » : position, pression, graine, temps...) ; le placement des touches se règle à gauche.");
            if (ImGui::Button("Créer un pinceau en graphe")) {
                brushTarget.tipId.clear();
                brushTarget.tipCode.clear();
                brushTarget.tipGraph = EffectGraph::DefaultBrush().Serialize();
                if (auto td = TipDef(brushTarget); td && td->def) brushTarget.tipParams = td->def->DefaultParams();
            }
            ImGui::EndGroup();
            ImGui::End();
            return;
        }
        ImGui::TextWrapped("Sélectionnez un effet basé sur un graphe (nœud, calque de réglage ou de remplissage).");
        ImGui::Spacing();
        if (fx && ImGui::Button("Convertir cet effet en graphe vide")) {
            DocState before = d.S();
            auto old = fx->Def();
            fx->graph = EffectGraph::Default().Serialize();
            fx->customCode.clear();
            fx->effectId.clear();
            fx->SyncParams(old);
            d.SyncGraph();
            v->history.PushState("Créer un graphe d'effet", before, d.S());
        }
        if (ImGui::Button("Nouvel effet depuis du HLSL...")) hlslImportRequest = true;
        ui::HelpTooltip("Colle ou ouvre du HLSL : entrées et sorties créées depuis les annotations //@in //@out ou la signature de la fonction.");
        DrawHlslImportPopup(nullptr, {}, nullptr);
        if (ImGui::Button("Nouveau calque de réglage « Effet visuel »")) {
            Edit e(d, v->history, "Effet visuel");
            if (d.Mode() == DocMode::Layers) {
                Layer& l = d.AddEffectLayer(LayerKind::Adjustment, "");
                l.name = "Effet visuel";
                l.effect.graph = EffectGraph::Default().Serialize();
                d.SyncGraph();
            } else {
                Node& n = d.AddEffectNode("", "", nodeEd.contextPos);
                n.fx.graph = EffectGraph::Default().Serialize();
                n.name = "Effet visuel";
                n.inputs.assign(NodeInputCount(n), 0);
            }
            e.Commit();
        }
        ImGui::EndGroup();
        ImGui::End();
        return;
    }

    // Parse (cached on the JSON text).
    if (ed.cachedJson != fx->graph) {
        if (!EffectGraph::Deserialize(fx->graph, ed.graph)) ed.graph = EffectGraph::Default();
        ed.cachedJson = fx->graph;
    }
    EffectGraph& g = ed.graph;
    bool changed = false;     // structural change -> push undo now
    bool live = false;        // continuous change (drag/slider) -> write without undo
    std::string changeLabel = "Modifier le graphe";
    ImGuiIO& io = ImGui::GetIO();

    // ---------------------------------------------------------- details panel
    const EGNode* selNode = g.Find(ed.selected);
    float panelW = (selNode && selNode->op == "custom" ? 400.0f : 270.0f) * dpi;  // custom nodes: pins + code need room
    ImGui::BeginChild("##efdetails", ImVec2(panelW, 0), ImGuiChildFlags_Borders, 0);
    ImGui::TextDisabled("Édition : %s", where.c_str());
    ImGui::SeparatorText("Effet");
    ImGui::PushItemWidth(-90 * dpi);
    if (ImGui::InputText("Nom", &g.name)) live = true;
    if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
    if (ImGui::InputText("Catégorie", &g.category)) live = true;
    if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
    auto trackLive = [&](bool edited) {
        if (edited) live = true;
        if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
    };
    if (g.kind == EGKind::Effect) {
        if (ImGui::SliderInt("Entrées", &g.inputs, 0, 4)) changed = true;
        if (ImGui::Checkbox("Prémultiplié", &g.premultiplied)) changed = true;
    } else if (g.kind == EGKind::Blend) {
        ImGui::TextDisabled("Entrée 1 = A (dessous), 2 = B (calque), 3 = masque.");
    } else {
        ImGui::SeparatorText("Placement des touches");
        trackLive(ImGui::SliderFloat("Espacement", &g.spacing, 0.01f, 2.0f, "%.2f"));
        trackLive(ImGui::SliderFloat("Aérographe", &g.airbrush, 0, 120, g.airbrush > 0 ? "%.0f /s" : "non"));
        ui::HelpTooltip("Touches par seconde tant que le bouton reste appuyé (0 : seulement en bougeant).");
        trackLive(ImGui::SliderFloat("Suivre le trait", &g.follow, 0, 1));
        trackLive(ImGui::SliderFloat("Dispersion", &g.scatter, 0, 4));
        trackLive(ImGui::SliderFloat("Taille aléatoire", &g.sizeJitter, 0, 1));
        trackLive(ImGui::SliderFloat("Angle aléatoire", &g.angleJitter, 0, 1));
        trackLive(ImGui::SliderFloat("Opacité aléatoire", &g.opacityJitter, 0, 1));
        ImGui::SeparatorText("Réaction au stylet");
        trackLive(ImGui::SliderFloat("Inclinaison -> angle", &g.tiltAngle, 0, 1));
        ui::HelpTooltip("La pointe s'oriente dans la direction où le stylet penche.");
        trackLive(ImGui::SliderFloat("Inclinaison -> taille", &g.tiltSize, 0, 2));
        ui::HelpTooltip("Stylet couché : touche plus large (comme un crayon sur la tranche).");
        trackLive(ImGui::SliderFloat("Rotation -> angle", &g.twistAngle, 0, 1));
        ui::HelpTooltip("Rotation du stylet sur lui-même (stylets type Art Pen).");
        trackLive(ImGui::SliderFloat("Vitesse -> taille", &g.speedSize, -1, 1));
        ui::HelpTooltip("Négatif : plus fin quand on va vite ; positif : plus épais.");
        ImGui::TextDisabled("Pression : Options > Tablette (taille, flux, courbe).");
        DrawBrushPad(*v, brushTarget);
    }
    EGNode* sel = g.Find(ed.selected);
    if (sel) {
        const EGOpDef* od = FindOp(sel->op);
        ImGui::SeparatorText(od ? od->label : sel->op.c_str());
        const std::string& op = sel->op;
        auto track = [&](bool edited) {
            if (edited) live = true;
            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
        };
        if (op == "const") track(ImGui::DragFloat("Valeur", &sel->value.x, 0.01f));
        else if (op == "const2") track(ImGui::DragFloat2("Valeur", &sel->value.x, 0.01f));
        else if (op == "color") track(ImGui::ColorEdit4("Couleur", &sel->value.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR));
        else if (op == "param" || op == "paramcolor" || op == "param2") {
            track(ImGui::InputText("Nom", &sel->name));
            if (op == "param2") {
                track(ImGui::DragFloat2("Défaut", &sel->value.x, 0.01f));
                track(ImGui::DragFloat("Min", &sel->min, 0.01f));
                track(ImGui::DragFloat("Max", &sel->max, 0.01f));
            } else if (op == "param") {
                track(ImGui::DragFloat("Défaut", &sel->value.x, 0.01f));
                track(ImGui::DragFloat("Min", &sel->min, 0.01f));
                track(ImGui::DragFloat("Max", &sel->max, 0.01f));
            } else {
                track(ImGui::ColorEdit4("Défaut", &sel->value.x, ImGuiColorEditFlags_Float));
            }
            ImGui::TextDisabled("Réglable dans Propriétés.");
        } else if (op == "brushtex") {
            int idx = sel->index + 1;
            if (ImGui::SliderInt("Texture", &idx, 1, 4)) { sel->index = idx - 1; changed = true; }
            track(ImGui::InputText("Libellé", &sel->name));
            track(ImGui::InputText("Image par défaut", &sel->text));
            ui::HelpTooltip("Chemin relatif au dossier des pinceaux (ex. textures/grain.png).\nL'utilisateur choisit une autre image dans les réglages du pinceau.");
            if (ImGui::SmallButton("Bibliothèque")) ImGui::OpenPopup("##btexlib");
            if (ImGui::BeginPopup("##btexlib")) {
                std::error_code ec;
                for (auto& e : std::filesystem::directory_iterator(Utf8Path(Brushes().Dir() + "/textures"), ec)) {
                    std::string rel = "textures/" + Narrow(e.path().filename().wstring());
                    if (ImGui::Selectable(rel.c_str(), rel == sel->text)) { sel->text = rel; changed = true; }
                }
                ImGui::EndPopup();
            }
        } else if (op == "input" || op == "sample" || op == "texture") {
            int idx = sel->index + 1;
            if (ImGui::SliderInt("Entrée", &idx, 1, std::max(g.kind == EGKind::Blend ? 3 : g.inputs, 1))) { sel->index = idx - 1; changed = true; }
        } else if (op == "mask") {
            int bits = sel->index ? sel->index : 7;
            const char* names[] = {"R", "G", "B", "A"};
            for (int i = 0; i < 4; ++i) {
                bool on = bits & (1 << i);
                if (i) ImGui::SameLine();
                if (ImGui::Checkbox(names[i], &on)) { bits = on ? bits | (1 << i) : bits & ~(1 << i); changed = true; }
            }
            sel->index = bits ? bits : 1;
        } else if (op == "custom") {
            track(ImGui::InputText("Titre", &sel->name));
            if (!sel->HasPins()) {
                ImGui::TextDisabled("Entrées A B C D (float4), retourner un float4.");
                if (ImGui::Button("Déclarer les broches")) {
                    for (const char* nm : {"A", "B", "C", "D"}) {
                        EGPinDecl d;
                        d.name = nm;
                        d.type = kPinFloat4;
                        sel->pins.push_back(d);
                    }
                    EGPinDecl o;
                    o.name = "Resultat";
                    o.type = kPinFloat4;
                    sel->outs = {o};
                    changed = true;
                }
                ui::HelpTooltip("Entrées et sorties nommées et typées (float à float4, texture lue à n'importe quel UV).");
            } else {
                // Declared pins: name (HLSL identifier) + type; textures are read with Texture2DSample(T, S, uv).
                // Pin types (EGPinType order): float, float2, float3, float4, Texture, int, bool, couleur.
                static const char* kPinTypes = "float\0float2\0float3\0float4\0Texture\0int\0bool\0couleur\0";
                auto pinList = [&](std::vector<EGPinDecl>& pins, const char* title, bool inputs) {
                    ImGui::SeparatorText(title);
                    int removeAt = -1, moveUp = -1;
                    for (size_t i = 0; i < pins.size(); ++i) {
                        EGPinDecl& pin = pins[i];
                        ImGui::PushID((int)i + (inputs ? 0 : 1000));
                        std::string head = pin.name + "  (" + PinTypeName(pin.type) + ")" + (pin.category.empty() ? "" : "  [" + pin.category + "]");
                        bool open = ImGui::TreeNodeEx("##pin", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", head.c_str());
                        ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight() * 2 - 8);
                        if (i > 0 && ImGui::SmallButton("^")) moveUp = (int)i;
                        ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight());
                        if (ImGui::SmallButton("x")) removeAt = (int)i;
                        if (open) {
                            track(ImGui::InputText("Nom", &pin.name, ImGuiInputTextFlags_CharsNoBlank));
                            ui::HelpTooltip("Identifiant utilisé dans le code.");
                            int t = pin.type - 1;
                            bool texOk = inputs;
                            if (ImGui::Combo("Type", &t, kPinTypes)) {
                                int nt = std::clamp(t + 1, 1, (int)kPinTypeCount);
                                if (nt == kPinTexture && !texOk) nt = kPinFloat4;
                                if (nt == kPinColor && pin.def == float4{}) pin.def = {1, 1, 1, 1};
                                if (nt == kPinInt && pin.max == 1) pin.max = 10;
                                pin.type = nt;
                                changed = true;
                            }
                            track(ImGui::InputText("Catégorie", &pin.category));
                            ui::HelpTooltip(inputs ? "Groupe sous lequel le réglage apparaît dans Propriétés." : "Groupe de la sortie.");
                            if (inputs && pin.type != kPinTexture) {
                                track(ImGui::InputText("Libellé", &pin.label));
                                ui::HelpTooltip("Nom affiché dans Propriétés (sinon le nom).");
                                // Default + range: the value used when nothing is connected (then a parameter of the effect).
                                switch (pin.type) {
                                    case kPinFloat: track(ImGui::DragFloat("Défaut", &pin.def.x, 0.01f)); break;
                                    case kPinInt: {
                                        int iv = (int)std::lround(pin.def.x);
                                        if (ImGui::DragInt("Défaut", &iv)) { pin.def.x = (float)iv; live = true; }
                                        if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
                                        break;
                                    }
                                    case kPinBool: {
                                        bool bv = pin.def.x > 0.5f;
                                        if (ImGui::Checkbox("Défaut", &bv)) { pin.def.x = bv ? 1.0f : 0.0f; changed = true; }
                                        break;
                                    }
                                    case kPinFloat2: track(ImGui::DragFloat2("Défaut", &pin.def.x, 0.01f)); break;
                                    case kPinFloat3: track(ImGui::DragFloat3("Défaut", &pin.def.x, 0.01f)); break;
                                    case kPinFloat4: track(ImGui::DragFloat4("Défaut", &pin.def.x, 0.01f)); break;
                                    case kPinColor: track(ImGui::ColorEdit4("Défaut", &pin.def.x, ImGuiColorEditFlags_Float)); break;
                                }
                                if (pin.type != kPinBool && pin.type != kPinColor) {
                                    float r[2] = {pin.min, pin.max};
                                    if (ImGui::DragFloat2("Min / max", r, 0.01f)) { pin.min = r[0]; pin.max = r[1]; live = true; }
                                    if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
                                }
                                ImGui::TextDisabled("Non reliée : réglable dans Propriétés.");
                            }
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                    if (removeAt >= 0) {
                        pins.erase(pins.begin() + removeAt);
                        if (inputs && removeAt < (int)sel->inputs.size()) sel->inputs.erase(sel->inputs.begin() + removeAt);
                        changed = true;
                    }
                    if (moveUp > 0) {
                        std::swap(pins[moveUp], pins[moveUp - 1]);
                        if (inputs && moveUp < (int)sel->inputs.size()) std::swap(sel->inputs[moveUp], sel->inputs[moveUp - 1]);
                        changed = true;
                    }
                    if (ImGui::SmallButton(inputs ? "+ Entrée" : "+ Sortie")) {
                        EGPinDecl d;
                        d.name = (inputs ? "In" : "Out") + std::to_string(pins.size() + 1);
                        d.type = inputs ? kPinFloat : kPinFloat4;
                        pins.push_back(d);
                        changed = true;
                    }
                };
                pinList(sel->pins, "Entrées", true);
                pinList(sel->outs, "Sorties (une seule : return ; plusieurs : paramètres out)", false);
                sel->inputs.resize(sel->pins.size());
                ImGui::TextDisabled("Déclarations (fonctions, constantes)");
                ImGui::PushFont(ui::GetFonts().mono, 0.0f);
                track(ImGui::InputTextMultiline("##decl", &sel->decl, ImVec2(-1, 60 * dpi), ImGuiInputTextFlags_AllowTabInput));
                ImGui::PopFont();
            }
            ImGui::TextDisabled("Code");
            ImGui::PushFont(ui::GetFonts().mono, 0.0f);
            track(ImGui::InputTextMultiline("##code", &sel->text, ImVec2(-1, 180 * dpi), ImGuiInputTextFlags_AllowTabInput));
            ImGui::PopFont();
        }
        if (op != "output" && ImGui::Button("Supprimer le nœud")) {
            Id rid = sel->id;
            g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(), [&](const EGNode& n) { return n.id == rid; }), g.nodes.end());
            for (auto& n : g.nodes) for (auto& l : n.inputs) if (l.node == rid) l = {};
            ed.selected = 0;
            sel = nullptr;
            changed = true;
            changeLabel = "Supprimer un nœud";
        }
    }
    ImGui::PopItemWidth();

    // Compile status
    ImGui::SeparatorText("Compilation");
    EGCompileResult cr = CompileEffectGraph(g);
    std::string shaderErr;
    Id renderNode = 0;
    for (auto& n : d.S().graph.nodes)
        if (n.kind == NodeKind::Effect && n.fx.graph == fx->graph) renderNode = n.id;
    bool failed = renderNode && v->renderer.NodeFailed(renderNode, &shaderErr);
    if (brushMode)
        if (auto td = TipDef(brushTarget)) failed = !v->renderer.BrushPipeline(*td, &shaderErr) && !shaderErr.empty();
    if (!cr.error.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", cr.error.c_str());
    else if (failed) {
        ImGui::PushFont(ui::GetFonts().mono, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.45f, 0.45f, 1));
        ImGui::TextWrapped("%s", shaderErr.c_str());
        ImGui::PopStyleColor();
        ImGui::PopFont();
    } else ImGui::TextColored(ImVec4(0.45f, 0.9f, 0.55f, 1), "OK");
    if (ImGui::Button("Importer du HLSL...")) hlslImportRequest = true;
    ui::HelpTooltip("Colle ou ouvre du HLSL : un nœud est créé avec ses entrées et sorties\n(annotations //@in //@out, ou signature d'une fonction).");
    DrawHlslImportPopup(&g, ed.ctxPos.x == 0 && ed.ctxPos.y == 0 ? float2{120, 120} : ed.ctxPos, &changed);
    if (ImGui::Button("Voir le HLSL généré")) ImGui::OpenPopup("##hlsl");
    if (ImGui::BeginPopup("##hlsl")) {
        ImGui::PushFont(ui::GetFonts().mono, 0.0f);
        std::string code = cr.code;
        ImGui::InputTextMultiline("##gen", &code, ImVec2(620 * dpi, 420 * dpi), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopFont();
        ImGui::EndPopup();
    }
    if (brushMode) {
        if (ImGui::Button("Convertir en HLSL éditable")) {
            brushTarget.tipCode = cr.code;
            brushTarget.tipGraph.clear();
            ImGui::EndChild();
            ImGui::End();
            return;
        }
    } else if (ImGui::Button("Convertir en HLSL éditable")) {
        DocState before = d.S();
        auto old = fx->Def();
        fx->customCode = cr.code;
        fx->graph.clear();
        fx->SyncParams(old);
        d.SyncGraph();
        v->history.PushState("Convertir en HLSL", before, d.S());
        ImGui::EndChild();
        ImGui::End();
        return;
    }
    if (g.kind != EGKind::Effect) {
        // Library: blends -> shaders/effects, brushes -> shaders/brushes (graph kept for editing).
        if (ImGui::Button("Enregistrer dans la bibliothèque...")) ImGui::OpenPopup("##savegraphlib");
        std::string savedId;
        const LibKind lk = g.kind == EGKind::Brush ? LibKind::Brush : LibKind::Blend;
        // The graph rides along (//@graph) so the library item opens again as a graph.
        if (LibrarySavePopup("##savegraphlib", lk, g.name, g.category, "//@graph " + g.ToJson().Dump(0) + "\n" + cr.code, &savedId) && lk == LibKind::Brush) {
            std::vector<float4> keep = brushTarget.tipParams;
            SelectTip(brushTarget, savedId);
            if (keep.size() == brushTarget.tipParams.size()) brushTarget.tipParams = keep;
        }
    }
    if (g.kind == EGKind::Effect && ImGui::Button("Exporter vers un matériau Unreal...")) {
        std::string path = platform::SaveFileDialog(hwnd, L"Script Python Unreal (*.py)\0*.py\0", L"py");
        if (!path.empty()) {
            std::string matName = "M_SX_";
            for (char c : g.name) matName += (isalnum((unsigned char)c) ? c : '_');
            EGUnrealExport ex = ExportEffectGraphToUnreal(g, matName, "/Game/Shaderixel");
            if (!ex.error.empty()) Status("Export impossible : " + ex.error);
            else {
                std::ofstream f(Utf8Path(path), std::ios::binary);
                f << ex.python;
                std::ofstream h(Utf8Path(path + ".hlsl"), std::ios::binary);
                h << ex.customCode;
                Status("Script Unreal exporté : " + path);
            }
        }
    }
    ui::HelpTooltip("Génère un script Python à exécuter dans l'éditeur Unreal :\ncrée un matériau (nœud Custom + paramètres + textures).");
    ImGui::EndChild();
    ImGui::SameLine(0, 0);

    // ---------------------------------------------------------- graph canvas
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    if (size.x < 20 || size.y < 20) { ImGui::End(); return; }
    ImGui::InvisibleButton("##efcanvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    bool hovered = ImGui::IsItemHovered();
    bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
    auto toS = [&](float2 p) { return ImVec2(origin.x + (p.x + ed.pan.x) * ed.zoom, origin.y + (p.y + ed.pan.y) * ed.zoom); };
    auto toG = [&](ImVec2 s) { return float2{(s.x - origin.x) / ed.zoom - ed.pan.x, (s.y - origin.y) / ed.zoom - ed.pan.y}; };

    if (hovered && io.MouseWheel != 0) {
        float2 b = toG(io.MousePos);
        ed.zoom = std::clamp(ed.zoom * std::pow(1.15f, io.MouseWheel), 0.25f, 2.5f);
        float2 a = toG(io.MousePos);
        ed.pan = ed.pan + (a - b);
    }
    if (ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0) || ImGui::IsMouseDragging(ImGuiMouseButton_Right, 4) ||
                                  (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0) && (io.KeyAlt || spaceHeld))))
        ed.pan = ed.pan + float2{io.MouseDelta.x / ed.zoom, io.MouseDelta.y / ed.zoom};

    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(17, 18, 22, 255));
    float step = 24.0f * ed.zoom;
    if (step > 6) {
        float ox = std::fmod(ed.pan.x * ed.zoom, step), oy = std::fmod(ed.pan.y * ed.zoom, step);
        for (float x = ox; x < size.x; x += step) dl->AddLine(ImVec2(origin.x + x, origin.y), ImVec2(origin.x + x, origin.y + size.y), ui::col::Grid);
        for (float y = oy; y < size.y; y += step) dl->AddLine(ImVec2(origin.x, origin.y + y), ImVec2(origin.x + size.x, origin.y + y), ui::col::Grid);
    }

    auto inPin = [&](const EGNode& n, int i) { return toS(n.pos + float2{0, kHead + kRowH * (i + 0.5f)}); };
    auto outPin = [&](const EGNode& n, int k) { return toS(n.pos + float2{kW, kHead + kRowH * (k + 0.5f)}); };
    float pr = 5.5f * ed.zoom;
    ImVec2 m = io.MousePos;
    auto hit = [&](ImVec2 a, float r) { float dx = a.x - m.x, dy = a.y - m.y; return dx * dx + dy * dy <= r * r; };

    Id hNode = 0, hOutNode = 0, hInNode = 0;
    int hOut = -1, hIn = -1;
    if (hovered) {
        for (int k = (int)g.nodes.size() - 1; k >= 0 && !hNode && !hOutNode && !hInNode; --k) {
            const EGNode& n = g.nodes[k];
            if (!FindOp(n.op)) continue;
            const EGOpDef eff = EffectiveOp(n);
            const EGOpDef* od = &eff;
            for (int o = 0; o < (int)od->outputs.size(); ++o) if (hit(outPin(n, o), pr * 2.2f)) { hOutNode = n.id; hOut = o; }
            for (int i = 0; i < (int)od->inputs.size() && !hOutNode; ++i) if (hit(inPin(n, i), pr * 2.2f)) { hInNode = n.id; hIn = i; }
            ImVec2 a = toS(n.pos), b = toS(n.pos + float2{kW, NodeH(n, *od)});
            if (!hOutNode && !hInNode && m.x >= a.x && m.x <= b.x && m.y >= a.y && m.y <= b.y) hNode = n.id;
        }
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt && !spaceHeld) {
        if (hOutNode) {
            ed.dragLink = true;
            ed.linkFrom = hOutNode;
            ed.linkFromOut = hOut;
            ed.linkTo = 0;
        } else if (hInNode) {
            EGNode* n = g.Find(hInNode);
            if (n && hIn < (int)n->inputs.size() && n->inputs[hIn].node) {
                ed.dragLink = true;
                ed.linkFrom = n->inputs[hIn].node;
                ed.linkFromOut = n->inputs[hIn].output;
                n->inputs[hIn] = {};
                changed = true;
                changeLabel = "Délier";
            } else {
                ed.dragLink = true;
                ed.linkFrom = 0;
                ed.linkTo = hInNode;
                ed.linkToIn = hIn;
            }
        } else if (hNode) {
            ed.selected = hNode;
            ed.dragNode = hNode;
            ed.moved = false;
        } else {
            ed.selected = 0;
        }
    }
    if (ed.dragNode && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
        if (EGNode* n = g.Find(ed.dragNode)) {
            n->pos = n->pos + float2{io.MouseDelta.x / ed.zoom, io.MouseDelta.y / ed.zoom};
            ed.moved = true;
            live = true;
        }
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (ed.dragLink) {
            Id from = 0, to = 0;
            int fo = 0, ti = 0;
            if (ed.linkFrom && hInNode) { from = ed.linkFrom; fo = ed.linkFromOut; to = hInNode; ti = hIn; }
            else if (ed.linkTo && hOutNode) { from = hOutNode; fo = hOut; to = ed.linkTo; ti = ed.linkToIn; }
            if (from && to && from != to && !g.WouldCycle(from, to)) {
                if (EGNode* n = g.Find(to)) {
                    if ((int)n->inputs.size() <= ti) n->inputs.resize(ti + 1);
                    n->inputs[ti] = {from, fo};
                    changed = true;
                    changeLabel = "Relier";
                }
            }
            ed.dragLink = false;
        }
        if (ed.dragNode) {
            if (ed.moved) { changed = true; changeLabel = "Déplacer un nœud"; }
            ed.dragNode = 0;
        }
    }

    // Typed pins and links: scalar gray, vector 2 green, RGB yellow, RGBA pink (Unreal colors).
    auto typeColor = [](int t, int alpha = 255) {
        switch (t) {
            case 1: return IM_COL32(170, 172, 180, alpha);
            case 2: return IM_COL32(110, 210, 120, alpha);
            case 3: return IM_COL32(235, 200, 80, alpha);
            case 4: return IM_COL32(230, 120, 210, alpha);
            case kEGTexture: return IM_COL32(90, 170, 240, alpha);  // texture (custom node pins)
            default: return IM_COL32(150, 150, 170, alpha);
        }
    };
    static const char* kTypeNames[] = {"", "x", "xy", "RGB", "RGBA"};
    // Links
    auto link = [&](ImVec2 a, ImVec2 b, ImU32 col) {
        float dx = std::max(std::abs(b.x - a.x) * 0.5f, 30.0f * ed.zoom);
        dl->AddBezierCubic(a, ImVec2(a.x + dx, a.y), ImVec2(b.x - dx, b.y), b, col, 2.0f * ed.zoom);
    };
    for (auto& n : g.nodes)
        for (int i = 0; i < (int)n.inputs.size(); ++i) {
            const EGNode* s = n.inputs[i].node ? g.Find(n.inputs[i].node) : nullptr;
            if (s) link(outPin(*s, n.inputs[i].output), inPin(n, i), (s->id == ed.selected || n.id == ed.selected) ? ui::col::Accent2 : typeColor(cr.OutType(s->id, n.inputs[i].output), 200));
        }
    if (ed.dragLink) {
        if (ed.linkFrom) { if (const EGNode* s = g.Find(ed.linkFrom)) link(outPin(*s, ed.linkFromOut), m, ui::col::Accent); }
        else if (const EGNode* t = g.Find(ed.linkTo)) link(m, inPin(*t, ed.linkToIn), ui::col::Accent);
    }

    // Nodes
    ImFont* font = ImGui::GetFont();
    float fs = ImGui::GetFontSize() * ed.zoom;
    bool text = ed.zoom > 0.45f;
    std::set<Id> errNodes(cr.errorNodes.begin(), cr.errorNodes.end());
    for (auto& n : g.nodes) {
        if (!FindOp(n.op)) continue;
        const EGOpDef eff = EffectiveOp(n);
        const EGOpDef* od = &eff;
        float h = NodeH(n, *od);
        ImVec2 a = toS(n.pos), b = toS(n.pos + float2{kW, h});
        float r = 5 * ed.zoom;
        dl->AddRectFilled(ImVec2(a.x + 3, a.y + 4), ImVec2(b.x + 3, b.y + 4), IM_COL32(0, 0, 0, 70), r);
        dl->AddRectFilled(a, b, ui::col::PanelLight, r);
        dl->AddRectFilled(a, ImVec2(b.x, a.y + kHead * ed.zoom), CategoryColor(od->category), r, ImDrawFlags_RoundCornersTop);
        bool selected = n.id == ed.selected;
        dl->AddRect(a, b, errNodes.count(n.id) ? ui::col::Error : selected ? ui::col::Accent : ui::col::Border, r, 0, selected ? 2.0f : 1.0f);
        if (text) {
            std::string title = (n.op == "param" || n.op == "paramcolor" || n.op == "param2" || n.op == "custom") && !n.name.empty()
                                    ? (n.op == "custom" ? "HLSL : " + n.name : !n.text.empty() ? n.text : n.name)
                                    : std::string(od->label);
            dl->PushClipRect(a, ImVec2(b.x - 4, a.y + kHead * ed.zoom), true);
            dl->AddText(font, fs, ImVec2(a.x + 7 * ed.zoom, a.y + (kHead * ed.zoom - fs) * 0.5f), IM_COL32_WHITE, title.c_str());
            dl->PopClipRect();
        }
        for (int i = 0; i < (int)od->inputs.size(); ++i) {
            ImVec2 p = inPin(n, i);
            bool conn = i < (int)n.inputs.size() && n.inputs[i].node;
            int t = conn ? cr.OutType(n.inputs[i].node, n.inputs[i].output) : 0;
            int declared = n.op == "custom" && i < (int)n.pins.size() ? (n.pins[i].type == kEGTexture ? kEGTexture : PinValueSize(n.pins[i].type)) : 0;
            if (conn) dl->AddCircleFilled(p, pr, typeColor(t));
            else dl->AddCircle(p, pr, declared ? typeColor(declared) : IM_COL32(120, 122, 140, 255), 0, 1.5f * ed.zoom);
            if (text) {
                std::string lbl = od->inputs[i].name;
                if (!conn && od->inputs[i].defValue >= 0) {  // negative: implicit source (UV, opacity...)
                    char b2[32];
                    snprintf(b2, sizeof(b2), " (%.3g)", od->inputs[i].defValue);
                    lbl += b2;
                }
                dl->AddText(font, fs * 0.88f, ImVec2(p.x + 9 * ed.zoom, p.y - fs * 0.45f), ui::col::TextDim, lbl.c_str());
            }
        }
        for (int o = 0; o < (int)od->outputs.size(); ++o) {
            ImVec2 p = outPin(n, o);
            int t = cr.OutType(n.id, o);
            dl->AddCircleFilled(p, pr, typeColor(t));
            // Label: the output name, else its type.
            const char* lbl = od->outputs[o][0] ? od->outputs[o] : (t >= 1 && t <= 4 ? kTypeNames[t] : t == kEGTexture ? "Texture" : "");
            if (text && lbl[0]) {
                ImVec2 ts = font->CalcTextSizeA(fs * 0.88f, FLT_MAX, 0, lbl);
                dl->AddText(font, fs * 0.88f, ImVec2(p.x - 9 * ed.zoom - ts.x, p.y - fs * 0.45f), typeColor(t, 230), lbl);
            }
        }
        std::string vt = ValueText(n);
        if (text && !vt.empty()) {
            ImVec2 tp(a.x + 8 * ed.zoom, a.y + (kHead + Rows(*od) * kRowH + 2) * ed.zoom);
            if (n.op == "color" || n.op == "paramcolor") {
                ImU32 c = ImGui::ColorConvertFloat4ToU32(ImVec4(n.value.x, n.value.y, n.value.z, 1));
                dl->AddRectFilled(tp, ImVec2(tp.x + 40 * ed.zoom, tp.y + fs), c, 3);
                tp.x += 46 * ed.zoom;
            }
            dl->AddText(font, fs * 0.9f, tp, ui::col::Text, vt.c_str());
        }
    }

    // Context menu: add node
    if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::GetIO().MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < 16.0f) {
        ed.ctxPos = toG(m);
        ed.search[0] = 0;
        ImGui::OpenPopup("##efadd");
    }
    if (ImGui::BeginPopup("##efadd")) {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##s", "Rechercher...", ed.search, sizeof(ed.search));
        std::string q = ed.search;
        for (auto& c : q) c = (char)tolower((unsigned char)c);
        auto add = [&](const EGOpDef& od) {
            EGNode n;
            n.id = g.nextId++;
            n.op = od.op;
            n.pos = ed.ctxPos;
            n.inputs.resize(od.inputs.size());
            if (n.op == "param") { n.name = "Valeur"; n.value = {0.5f, 0, 0, 0}; }
            if (n.op == "param2") { n.name = "Position"; n.value = {0.5f, 0.5f, 0, 0}; }
            if (n.op == "paramcolor") { n.name = "Couleur"; n.value = {1, 0.5f, 0.2f, 1}; }
            if (n.op == "color") n.value = {1, 1, 1, 1};
            if (n.op == "const") n.value = {1, 0, 0, 0};
            if (n.op == "custom") n.text = "    return A * B;";
            if (n.op == "mask") n.index = 7;
            g.nodes.push_back(n);
            ed.selected = n.id;
            changed = true;
            changeLabel = std::string("Ajouter ") + od.label;
            ImGui::CloseCurrentPopup();
        };
        std::string lastCat;
        for (auto& od : EffectGraphOps()) {
            if (std::string(od.op) == "output" || !OpAllowed(od, g.kind)) continue;
            if (!q.empty()) {
                std::string l = od.label;
                for (auto& c : l) c = (char)tolower((unsigned char)c);
                if (l.find(q) == std::string::npos && std::string(od.op).find(q) == std::string::npos) continue;
                if (ImGui::MenuItem(od.label)) add(od);
                continue;
            }
            if (od.category != lastCat) {
                if (!lastCat.empty()) ImGui::Separator();
                ImGui::TextDisabled("%s", od.category);
                lastCat = od.category;
            }
            if (ImGui::MenuItem(od.label)) add(od);
        }
        ImGui::EndPopup();
    }
    if (focused && !io.WantTextInput && (ImGui::IsKeyPressed(ImGuiKey_Delete)) && ed.selected) {
        EGNode* n = g.Find(ed.selected);
        if (n && n->op != "output") {
            Id rid = n->id;
            g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(), [&](const EGNode& x) { return x.id == rid; }), g.nodes.end());
            for (auto& x : g.nodes) for (auto& l : x.inputs) if (l.node == rid) l = {};
            ed.selected = 0;
            changed = true;
            changeLabel = "Supprimer un nœud";
        }
    }
    dl->AddText(ImVec2(origin.x + 10, origin.y + 8), ui::col::TextDim, "Clic droit : ajouter un nœud  |  glisser une sortie vers une entrée  |  Suppr");
    dl->PopClipRect();
    ImGui::End();

    // Write back: brush graph -> brush settings (no document change).
    if (brushMode && (changed || live)) {
        auto old = fx->Def();
        fx->graph = g.Serialize();
        ed.cachedJson = fx->graph;
        fx->SyncParams(old);
        brushTarget.tipGraph = fx->graph;
        brushTarget.tipParams = fx->params;
        if (g.spacing > 0) brushTarget.spacing = g.spacing;
        return;
    }
    // Write back to the effect instance.
    if (changed || live) {
        auto old = fx->Def();
        fx->graph = g.Serialize();
        ed.cachedJson = fx->graph;
        fx->SyncParams(old);
        // Parameter defaults edited in the graph also update the instance value.
        if (sel && (sel->op == "param" || sel->op == "paramcolor")) {
            auto nd = fx->Def();
            for (size_t i = 0; nd && i < nd->params.size() && i < fx->params.size(); ++i)
                if (nd->params[i].label == sel->name || nd->params[i].id.find(sel->name) != std::string::npos) fx->params[i] = nd->params[i].def;
        }
        if (d.Mode() == DocMode::Layers) d.SyncGraph();
        else if (Node* n = d.S().graph.Find(d.S().activeNode)) n->inputs.resize(NodeInputCount(*n), 0);
        d.dirty = true;
        if (!ed.pendingBefore) ed.pendingBefore = frameBefore;  // state before the first change of this gesture
        if (changed) {
            v->history.PushState(changeLabel, *ed.pendingBefore, d.S());
            ed.pendingBefore.reset();
        }
    }
}


// ------------------------------------------------------------------ code / library views of the editor

void App::DrawLibraryItemView(DocView& v, EffectInstance& fx, bool blend) {
    // A library effect / blend: read-only here, edited as a copy kept in the document.
    Document& d = *v.doc;
    auto def = fx.Def();
    if (!def) { ImGui::TextDisabled("Effet introuvable : %s", fx.effectId.c_str()); return; }
    const LibKind kind = blend ? LibKind::Blend : LibKind::Effect;
    ImGui::Text("%s", def->name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s  -  bibliothèque", def->category.c_str());
    bool user = IsUserLibraryItem(kind, fx.effectId), locked = LibraryItemLocked(kind, fx.effectId);
    if (!user) ImGui::TextColored(ImVec4(0.55f, 0.8f, 1, 1), "Élément de base protégé : modifier crée une copie dans ce document.");
    else {
        ImGui::TextColored(ImVec4(0.55f, 0.8f, 1, 1), "Élément de votre bibliothèque : modifier crée une copie dans ce document.");
        bool lk = locked;
        if (ImGui::Checkbox("Verrouillé", &lk)) SetLibraryLock(kind, fx.effectId, lk);
        ui::HelpTooltip("Verrouillé : l'élément ne peut plus être remplacé par un enregistrement (dans aucun projet).");
    }
    if (ImGui::Button("Modifier une copie (code)")) ConvertToCustomCode(v, fx);
    if (!def->graph.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Modifier une copie (graphe)")) {
            DocState before = d.S();
            auto old = fx.Def();
            fx.graph = def->graph;
            fx.effectId.clear();
            fx.customCode.clear();
            fx.SyncParams(old);
            d.SyncGraph();
            v.history.PushState("Modifier une copie (graphe)", before, d.S());
            efEd.cachedJson.clear();
        }
    }
    std::string text;
    ReadTextFile(gpu::Shaders().ShaderDir() + "/effects/" + fx.effectId + ".slang", text);
    ImGui::PushFont(ui::GetFonts().mono, 0.0f);
    ImGui::InputTextMultiline("##libcode", &text, ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
    ImGui::PopFont();
}

void App::DrawEffectCodeEditor(DocView& v, EffectInstance& fx) {
    Document& d = *v.doc;
    ImGui::TextDisabled("Copie modifiable dans ce document : la bibliothèque n'est pas touchée.");
    bool committed = CustomCodeUI(v, fx, 0, "efed");
    if (committed && d.Mode() == DocMode::Nodes)
        if (Node* n = d.S().graph.Find(d.S().activeNode); n && &n->fx == &fx) {
            if (auto nd = n->fx.Def()) n->name = nd->name;
            n->inputs.resize(NodeInputCount(*n), 0);
        }
}

void App::DrawBrushCodeEditor(DocView& v, BrushSettings& b) {
    auto td = TipDef(b);
    auto newButtons = [&] {
        if (ImGui::Button("Nouveau pinceau (code)")) {
            b.tipId.clear();
            b.tipGraph.clear();
            b.tipCode = kBrushTemplate;
            b.tipParams = ParseBrush("edit", b.tipCode)->def->DefaultParams();
            brushEd.bufferKey.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Nouveau pinceau (graphe)")) {
            b.tipId.clear();
            b.tipCode.clear();
            b.tipGraph = EffectGraph::DefaultBrush().Serialize();
            if (auto nd = TipDef(b); nd && nd->def) b.tipParams = nd->def->DefaultParams();
            efEd.cachedJson.clear();
        }
    };
    if (!td) {
        ImGui::TextWrapped("Pointe ronde standard (non programmable). Créez un pinceau : la pointe (Tip) dessine chaque touche, "
                           "Dynamics place les touches (suivre le trait, dispersion, aléa, temps).");
        newButtons();
        return;
    }
    std::string name = td->def ? td->def->name : "Pinceau";
    std::string category = td->def ? td->def->category : "Mes pinceaux";
    if (b.tipCode.empty()) {
        // Library brush: read-only, edited as a copy in the brush settings.
        bool user = IsUserLibraryItem(LibKind::Brush, b.tipId), locked = LibraryItemLocked(LibKind::Brush, b.tipId);
        ImGui::Text("%s", name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s  -  bibliothèque", category.c_str());
        ImGui::TextColored(ImVec4(0.55f, 0.8f, 1, 1), user ? "Pinceau de votre bibliothèque : modifier crée une copie." : "Pinceau de base protégé : modifier crée une copie.");
        if (user) {
            bool lk = locked;
            if (ImGui::Checkbox("Verrouillé", &lk)) SetLibraryLock(LibKind::Brush, b.tipId, lk);
        }
        if (ImGui::Button("Modifier une copie (code)")) {
            b.tipCode = td->text;
            brushEd.bufferKey.clear();
        }
        ImGui::SameLine();
        newButtons();
        std::string text = td->text;
        ImGui::PushFont(ui::GetFonts().mono, 0.0f);
        ImGui::InputTextMultiline("##brushlib", &text, ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopFont();
        return;
    }
    // Edited copy.
    if (brushEd.bufferKey != "edit") {
        brushEd.bufferKey = "edit";
        brushEd.buffer = b.tipCode;
    }
    ImGui::Text("%s", name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("copie modifiable (réglages du pinceau) : la bibliothèque n'est pas touchée");
    ImGui::PushFont(ui::GetFonts().mono, 0.0f);
    ImGui::InputTextMultiline("##brushcode", &brushEd.buffer, ImVec2(-1, std::max(160.0f * dpi, ImGui::GetContentRegionAvail().y - 80 * dpi)),
                              ImGuiInputTextFlags_AllowTabInput);
    ImGui::PopFont();
    bool apply = ImGui::Button("Appliquer (Ctrl+Entrée)") ||
                 (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::GetIO().KeyCtrl);
    if (apply && brushEd.buffer != b.tipCode) {
        auto old = td->def;
        b.tipCode = brushEd.buffer;
        auto nd = TipDef(b);
        // Keep the values of parameters that still exist.
        std::vector<float4> params = nd->def->DefaultParams();
        if (old)
            for (size_t i = 0; i < nd->def->params.size(); ++i)
                for (size_t k = 0; k < old->params.size() && k < b.tipParams.size(); ++k)
                    if (old->params[k].id == nd->def->params[i].id) params[i] = b.tipParams[k];
        b.tipParams = params;
        if (nd->spacing > 0) b.spacing = nd->spacing;
    }
    ImGui::SameLine();
    if (ImGui::Button("Enregistrer dans la bibliothèque...")) ImGui::OpenPopup("##savebrushlib");
    std::string savedId;
    if (LibrarySavePopup("##savebrushlib", LibKind::Brush, name, category == "Exemples" ? "Mes pinceaux" : category, brushEd.buffer, &savedId)) {
        std::vector<float4> keep = b.tipParams;
        SelectTip(b, savedId);
        if (keep.size() == b.tipParams.size()) b.tipParams = keep;
    }
    if (!b.tipId.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Revenir à l'original")) {
            b.tipCode.clear();
            brushEd.bufferKey.clear();
            if (auto lib = Brushes().Find(b.tipId); lib && lib->def) b.tipParams = lib->def->DefaultParams();
        }
    }
    std::string err;
    if (auto nt = TipDef(b)) {
        if (!v.renderer.BrushPipeline(*nt, &err) && !err.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.45f, 0.45f, 1));
            ImGui::TextWrapped("%s", err.c_str());
            ImGui::PopStyleColor();
        } else if (nt->def && !nt->def->error.empty()) {
            ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1), "%s", nt->def->error.c_str());
        } else if (brushEd.buffer != b.tipCode) {
            ImGui::TextDisabled("Modifications non appliquées");
        }
    }
}


// ------------------------------------------------------------------ HLSL import

bool App::CreateEffectFromHlsl(DocView& v, const std::string& source, std::string* error) {
    Document& d = *v.doc;
    EffectGraph g;
    if (!GraphFromHlsl(source, g, error)) return false;
    Edit e(d, v.history, "Effet HLSL importé");
    if (d.Mode() == DocMode::Layers) {
        if (g.kind == EGKind::Blend) {
            Layer* l = d.ActiveLayer();
            if (!l) { if (error) *error = "sélectionnez le calque qui recevra la fusion"; return false; }
            l->blendFx = {};
            l->blendFx.graph = g.Serialize();
            if (auto def = l->blendFx.Def()) l->blendFx.params = def->DefaultParams();
            l->passThrough = false;
            efEd.target = 1;
        } else {
            Layer& l = d.AddEffectLayer(g.inputs == 0 ? LayerKind::Fill : LayerKind::Adjustment, "");
            l.name = g.name;
            l.effect.graph = g.Serialize();
            if (auto def = l.effect.Def()) l.effect.params = def->DefaultParams();
            efEd.target = 0;
            efEd.filterIndex = -1;
        }
        d.SyncGraph();
    } else {
        Node& n = d.AddEffectNode("", "", nodeEd.contextPos);
        n.fx.graph = g.Serialize();
        n.name = g.name;
        if (auto def = n.fx.Def()) n.fx.params = def->DefaultParams();
        n.inputs.assign(NodeInputCount(n), 0);
        d.S().activeNode = n.id;
        efEd.target = 0;
    }
    e.Commit();
    efEd.cachedJson.clear();
    Status("Effet HLSL importé : " + g.name);
    return true;
}

void App::DrawHlslImportPopup(EffectGraph* g, float2 pos, bool* changed) {
    // g: add the node to this graph; null: a new effect (layer / node) in the document.
    if (hlslImportRequest) {
        ImGui::OpenPopup("##hlslimport");
        hlslImportRequest = false;
        hlslImportError.clear();
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopup("##hlslimport")) return;
    ImGui::TextUnformatted(g ? "Importer du HLSL dans ce graphe" : "Nouvel effet depuis du HLSL");
    ImGui::TextDisabled("Annotations : //@name, //@category, //@in nom type défaut min max \"Libellé\" \"Catégorie\", //@out nom type, //@main fonction");
    ImGui::TextDisabled("Types : float float2 float3 float4 int bool color texture ; textures lues par Texture2DSample(T, S, uv) ou T.Sample(S, uv).");
    if (ImGui::Button("Ouvrir un fichier...")) {
        std::string path = platform::OpenFileDialog(hwnd, L"HLSL (*.hlsl, *.fx, *.slang, *.txt)\0*.hlsl;*.fx;*.slang;*.txt\0");
        if (!path.empty()) ReadTextFile(path, hlslImportBuffer);
    }
    ImGui::SameLine();
    if (ImGui::Button("Exemple")) {
        hlslImportBuffer =
            "//@name Vignette douce\n//@category Mes effets\n"
            "//@in Amount float 0.6 0 1 \"Intensité\" \"Réglages\"\n"
            "//@in Radius float 0.75 0.1 1.5 \"Rayon\" \"Réglages\"\n"
            "//@in Tint color 0 0 0 1 \"Couleur\" \"Couleur\"\n"
            "//@out Result float4\n\n"
            "float4 Vignette(Texture2D Image, SamplerState S, float2 UV, float Amount, float Radius, float4 Tint)\n"
            "{\n"
            "    float4 c = Image.Sample(S, UV);\n"
            "    float d = length(UV - 0.5) * 1.41421;\n"
            "    float k = smoothstep(Radius, Radius - 0.5, d);\n"
            "    return float4(lerp(Tint.rgb, c.rgb, lerp(1.0, k, Amount)), c.a);\n"
            "}\n";
    }
    ImGui::PushFont(ui::GetFonts().mono, 0.0f);
    ImGui::InputTextMultiline("##hlslsrc", &hlslImportBuffer, ImVec2(760 * dpi, 420 * dpi), ImGuiInputTextFlags_AllowTabInput);
    ImGui::PopFont();
    if (ImGui::Button(g ? "Ajouter le nœud" : "Créer l'effet")) {
        hlslImportError.clear();
        if (g) {
            HlslImport imp = ParseHlslNode(hlslImportBuffer);
            if (!imp.error.empty()) hlslImportError = imp.error;
            else {
                efEd.selected = AddHlslNode(*g, imp, pos);
                if (changed) *changed = true;
                ImGui::CloseCurrentPopup();
            }
        } else if (DocView* v = Active()) {
            if (CreateEffectFromHlsl(*v, hlslImportBuffer, &hlslImportError)) ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Fermer")) ImGui::CloseCurrentPopup();
    if (!hlslImportError.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", hlslImportError.c_str());
    }
    ImGui::EndPopup();
}

// ------------------------------------------------------------------ brush test pad

void App::DrawBrushPad(DocView& v, BrushSettings& b) {
    // Paint with the brush being edited (mouse or pen: pressure, tilt, rotation, speed).
    auto td = TipDef(b);
    const float h = 150 * dpi;
    const float w = std::max(120.0f, ImGui::GetContentRegionAvail().x);
    const u32 pw = (u32)w, ph = (u32)h;
    if (!pad.ready || !v.renderer.PadTexture() || v.renderer.PadTexture()->Width() != pw || v.renderer.PadTexture()->Height() != ph) {
        v.renderer.PadClear(*ctx, pw, ph);
        pad.ready = true;
    }
    ImVec2 o = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Light paper behind (dark brushes stay visible), the pad on top.
    dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), IM_COL32(236, 234, 228, 255), 4);
    if (gpu::Texture* t = v.renderer.PadTexture()) dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(t), o, ImVec2(o.x + w, o.y + h));
    ImGui::InvisibleButton("##brushpad", ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    dl->AddRect(o, ImVec2(o.x + w, o.y + h), hovered ? ui::col::Accent : ui::col::Border, 4);
    if (!pad.stroking) dl->AddText(ImVec2(o.x + 8, o.y + 6), IM_COL32(90, 90, 100, 255), "Zone d'essai : dessinez ici (souris ou stylet)");
    if (hovered) {
        float r = std::max(b.size * 0.5f, 1.0f);
        ImVec2 m = ImGui::GetIO().MousePos;
        dl->AddCircle(m, r, IM_COL32(40, 40, 50, 200), 0, 1.0f);
    }
    if (!td) return;
    const bool erase = tool == Tool::Eraser;
    EffectInstance tip;
    tip.params = b.tipParams;
    BrushSettings bound = b;
    BindTipTextures(bound, *td);
    float4 color = erase ? float4{1, 1, 1, 1} : LinearColor(fg);
    const float tipSpacing = td->spacing > 0 ? td->spacing : b.spacing;
    auto emit = [&](const std::vector<PointerSample>& samples) {
        std::vector<Dab> dabs;
        std::vector<float4> extra;
        for (auto& s : samples) {
            float2 p{s.x - o.x, s.y - o.y};
            float pr = s.pen ? PenPressure(s.pressure) : 1.0f;
            float2 d = p - pad.last;
            float len = Length(d);
            float spacing = std::max(1.0f, b.size * std::max(tipSpacing, 0.01f) * (b.pressureSize ? std::max(pr, 0.1f) : 1.0f));
            float angle = len > 1e-3f ? std::atan2(d.y, d.x) : 0.0f;
            float t = spacing - pad.remainder;
            while (t <= len) {
                float2 q = pad.last + d * (t / std::max(len, 1e-5f));
                dabs.push_back({q.x, q.y, b.size * 0.5f * (b.pressureSize ? std::max(pr, 0.05f) : 1.0f), b.flow * (b.pressureOpacity ? pr : 1.0f)});
                extra.push_back({pr, angle, (float)(ImGui::GetTime() - pad.start), pad.dist + t});
                extra.push_back(PenExtra(s));
                t += spacing;
            }
            pad.remainder = len > 0 ? len - (t - spacing) : pad.remainder;
            pad.dist += len;
            if (len > 0) pad.last = p;
        }
        if (!dabs.empty() && v.renderer.PadStroke(*ctx, *td, tip, bound, color, dabs, extra, erase, pad.dabs)) pad.dabs += (u32)dabs.size();
    };
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        v.renderer.PadBeginStroke(*ctx);
        pad.stroking = true;
        ImVec2 m = ImGui::GetIO().MousePos;
        pad.last = {m.x - o.x, m.y - o.y};
        pad.remainder = 0;
        pad.dist = 0;
        pad.dabs = 0;
        pad.start = ImGui::GetTime();
        PointerSample first = platform::PenActive() ? platform::LastPenSample() : PointerSample{m.x, m.y, 1.0f, false, false};
        first.x = m.x;
        first.y = m.y;
        pad.remainder = b.size * std::max(tipSpacing, 0.01f);  // first dab at the click
        emit({first});
    }
    if (pad.stroking) {
        std::vector<PointerSample> samples = frameSamples;
        if (samples.empty()) {
            PointerSample s = platform::PenActive() ? platform::LastPenSample() : PointerSample{0, 0, 1.0f, false, false};
            s.x = ImGui::GetIO().MousePos.x;
            s.y = ImGui::GetIO().MousePos.y;
            samples.push_back(s);
        }
        emit(samples);
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) pad.stroking = false;
    }
    if (ImGui::SmallButton("Effacer la zone d'essai")) v.renderer.PadClear(*ctx, pw, ph);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", platform::PenActive() ? platform::TabletStatus().c_str() : "Souris : pression 1, sans inclinaison");
}

}  // namespace sx
