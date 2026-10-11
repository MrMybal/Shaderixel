// Action editor: my actions and the built-in ones on the left, the steps of the
// chosen action in the middle (each one explained in full), the selected step's
// settings on the right. Also the batch window (chain of actions, conversion,
// output names, log).
#include "ui/App.h"
#include "ui/Theme.h"
#include "doc/Effects.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <climits>
#include <filesystem>
#include <shellapi.h>

namespace sx {

namespace {

std::string DirOfPath(const std::string& path) { return Narrow(Utf8Path(path).parent_path().wstring()); }

void Wrapped(const std::string& s, ImVec4 col) {
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(s.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

bool InputStr(const char* label, Json& p, const char* key) {
    std::string s = p[key].S();
    if (ImGui::InputText(label, &s)) { p[key] = s; return true; }
    return false;
}

}  // namespace

void App::DrawActionInspector(ActionStep& s, bool readOnly) {
    Json& p = s.params;
    if (!p.IsObject()) p = Json::Object();
    const Json& cp = p;
    bool changed = false;
    ImGui::BeginDisabled(readOnly);
    ImGui::PushItemWidth(-110 * dpi);
    const std::string& c = s.cmd;
    if (c == "image_size") {
        std::string m = cp["mode"].S("pixels");
        int mode = m == "percent" ? 1 : m == "fit" ? 2 : 0;
        if (ImGui::Combo("Mode", &mode, "Taille exacte (px)\0Pourcentage\0Tenir dans (max)\0")) { p["mode"] = mode == 1 ? "percent" : mode == 2 ? "fit" : "pixels"; changed = true; }
        if (mode == 1) {
            float pct = cp["pct"].F(100);
            if (ImGui::DragFloat("Pourcentage", &pct, 0.5f, 1, 1000, "%.1f %%")) { p["pct"] = pct; changed = true; }
        } else {
            int wh[2] = {cp["w"].I(1024), cp["h"].I(1024)};
            if (ImGui::DragInt2(mode == 2 ? "Max L x H" : "L x H (px)", wh, 1, 1, 16384)) { p["w"] = wh[0]; p["h"] = wh[1]; changed = true; }
            if (mode == 2) {
                bool e = cp["enlarge"].B();
                if (ImGui::Checkbox("Agrandir les petites images", &e)) { p["enlarge"] = e; changed = true; }
            }
        }
        int ip = cp["interp"].I(2);
        if (ImGui::Combo("Rééchantillonnage", &ip, "Au plus proche (pixel art)\0Bilinéaire\0Bicubique (lisse)\0")) { p["interp"] = ip; changed = true; }
    } else if (c == "fit_crop") {
        bool sq = cp["square"].B() || cp["w"].I() <= 0;
        if (ImGui::Checkbox("Carré", &sq)) { p["square"] = sq; if (!sq && cp["w"].I() <= 0) { p["w"] = 1024; p["h"] = 1024; } changed = true; }
        if (sq) {
            int sz = cp["size"].I(0);
            if (ImGui::DragInt("Côté (px)", &sz, 1, 0, 16384, sz <= 0 ? "côté le plus court" : "%d px")) { p["size"] = std::max(0, sz); changed = true; }
        } else {
            int wh[2] = {cp["w"].I(1024), cp["h"].I(1024)};
            if (ImGui::DragInt2("L x H (px)", wh, 1, 1, 16384)) { p["w"] = wh[0]; p["h"] = wh[1]; changed = true; }
            static const int presets[][2] = {{1920, 1080}, {1080, 1920}, {1280, 720}, {1080, 1350}, {2048, 1024}};
            for (auto& pr : presets) {
                char b[32];
                snprintf(b, sizeof(b), "%dx%d", pr[0], pr[1]);
                if (ImGui::SmallButton(b)) { p["w"] = pr[0]; p["h"] = pr[1]; changed = true; }
                ImGui::SameLine();
            }
            ImGui::NewLine();
        }
        int ip = cp["interp"].I(2);
        if (ImGui::Combo("Rééchantillonnage", &ip, "Au plus proche (pixel art)\0Bilinéaire\0Bicubique (lisse)\0")) { p["interp"] = ip; changed = true; }
    } else if (c == "canvas_size") {
        bool rel = cp["relative"].B();
        if (ImGui::Checkbox("Relatif (ajouter à la taille)", &rel)) { p["relative"] = rel; changed = true; }
        int wh[2] = {cp["w"].I(0), cp["h"].I(0)};
        if (ImGui::DragInt2(rel ? "+ L x H (px)" : "L x H (px)", wh, 1, rel ? -16384 : 1, 16384)) { p["w"] = wh[0]; p["h"] = wh[1]; changed = true; }
        ImGui::TextUnformatted("Ancrage");
        int an = cp["anchor"].I(4);
        for (int i = 0; i < 9; ++i) {
            ImGui::PushID(i);
            if (i % 3) ImGui::SameLine();
            if (ImGui::Selectable(i == an ? "#" : "·", i == an, 0, ImVec2(22 * dpi, 22 * dpi))) { p["anchor"] = i; changed = true; }
            ImGui::PopID();
        }
    } else if (c == "rotate") {
        int op = cp["op"].I(0);
        if (ImGui::Combo("Opération", &op, "Rotation 90° horaire\0Rotation 90° antihoraire\0Rotation 180°\0Symétrie horizontale\0Symétrie verticale\0")) { p["op"] = op; changed = true; }
    } else if (c == "filter") {
        auto def = Effects().Find(cp["effect"].S());
        ImGui::SetNextItemWidth(-110 * dpi);
        if (ImGui::BeginCombo("Effet", def ? def->name.c_str() : cp["effect"].S().c_str(), ImGuiComboFlags_HeightLarge)) {
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##fxf", "Rechercher...", &actionFxFilter);
            std::string q = actionFxFilter;
            for (auto& ch : q) ch = (char)tolower((unsigned char)ch);
            int shown = 0;
            for (auto& e : Effects().All()) {
                if (e->blend || e->id.empty() || (EffectIsVariant(*e) && q.empty())) continue;
                std::string hay = e->name + " " + e->id + " " + EffectFamily(*e);
                for (auto& ch : hay) ch = (char)tolower((unsigned char)ch);
                if (!q.empty() && hay.find(q) == std::string::npos) continue;
                if (++shown > 300) break;
                std::string label = e->name + "  -  " + EffectFamily(*e) + "##" + e->id;
                if (ImGui::Selectable(label.c_str(), def == e)) {
                    p["effect"] = e->id;
                    p.Erase("params");
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        bool dz = cp["destructive"].B();
        if (ImGui::Checkbox("Sur les pixels (définitif)", &dz)) { p["destructive"] = dz; changed = true; }
        ui::HelpTooltip("Coché : le filtre modifie les pixels du calque actif.\nDécoché : il est ajouté comme filtre du calque, modifiable ensuite.");
        if (def && !def->params.empty()) {
            ImGui::SeparatorText("Réglages du filtre");
            // Parameters stored as [[x,y,z,w], ...] in the effect's order; missing ones take the defaults.
            Json ps = cp["params"].IsArray() ? cp["params"] : Json::Array();
            std::vector<float4> vals;
            for (size_t i = 0; i < def->params.size(); ++i) {
                float4 v = def->params[i].def;
                if (i < ps.Size()) {
                    const Json& a = ps.At(i);
                    v = {a.Size() > 0 ? a.At(0).F(v.x) : v.x, a.Size() > 1 ? a.At(1).F() : v.y, a.Size() > 2 ? a.At(2).F() : v.z, a.Size() > 3 ? a.At(3).F() : v.w};
                }
                vals.push_back(v);
            }
            bool pc = false;
            for (size_t i = 0; i < def->params.size(); ++i) {
                const ParamDef& d = def->params[i];
                float4& v = vals[i];
                ImGui::PushID((int)i);
                const char* lab = d.label.empty() ? d.id.c_str() : d.label.c_str();
                switch (d.type) {
                    case ParamType::Int: { int x = (int)std::lround(v.x); if (ImGui::SliderInt(lab, &x, (int)d.min, (int)d.max)) { v.x = (float)x; pc = true; } break; }
                    case ParamType::Bool: { bool b = v.x > 0.5f; if (ImGui::Checkbox(lab, &b)) { v.x = b ? 1.0f : 0.0f; pc = true; } break; }
                    case ParamType::Enum: {
                        int x = (int)std::lround(v.x);
                        if (ImGui::BeginCombo(lab, x >= 0 && x < (int)d.enumItems.size() ? d.enumItems[x].c_str() : "?")) {
                            for (int k = 0; k < (int)d.enumItems.size(); ++k) if (ImGui::Selectable(d.enumItems[k].c_str(), k == x)) { v.x = (float)k; pc = true; }
                            ImGui::EndCombo();
                        }
                        break;
                    }
                    case ParamType::Color: pc |= ImGui::ColorEdit4(lab, &v.x, ImGuiColorEditFlags_Float); break;
                    case ParamType::Float2: pc |= ImGui::DragFloat2(lab, &v.x, 0.01f); break;
                    case ParamType::Float3: pc |= ImGui::DragFloat3(lab, &v.x, 0.01f); break;
                    case ParamType::Float4: pc |= ImGui::DragFloat4(lab, &v.x, 0.01f); break;
                    case ParamType::Curve:
                    case ParamType::Lut: ImGui::TextDisabled("%s : réglable seulement dans le JSON", lab); break;
                    default: pc |= ImGui::SliderFloat(lab, &v.x, d.min, d.max); break;
                }
                ImGui::PopID();
            }
            if (pc) {
                Json arr = Json::Array();
                for (auto& v : vals) {
                    Json a = Json::Array();
                    a.Push(v.x); a.Push(v.y); a.Push(v.z); a.Push(v.w);
                    arr.Push(a);
                }
                p["params"] = arr;
                changed = true;
            }
            if (p.Has("params") && ImGui::SmallButton("Valeurs par défaut")) { p.Erase("params"); changed = true; }
        }
    } else if (c == "new_layer") {
        changed |= InputStr("Nom", p, "name");
    } else if (c == "select") {
        std::string w = cp["what"].S("all");
        int k = w == "none" ? 1 : w == "invert" ? 2 : 0;
        if (ImGui::Combo("Sélection", &k, "Tout sélectionner\0Désélectionner\0Intervertir\0")) { p["what"] = k == 1 ? "none" : k == 2 ? "invert" : "all"; changed = true; }
    } else if (c == "fill") {
        bool own = cp["color"].Size() >= 3;
        if (ImGui::Checkbox("Couleur fixe", &own)) {
            if (own) { Json a = Json::Array(); a.Push(fg.x); a.Push(fg.y); a.Push(fg.z); a.Push(fg.w); p["color"] = a; }
            else p.Erase("color");
            changed = true;
        }
        ui::HelpTooltip("Décoché : la couleur de premier plan au moment de l'exécution.");
        if (own) {
            float col[4] = {cp["color"].At(0).F(), cp["color"].At(1).F(), cp["color"].At(2).F(), cp["color"].Size() > 3 ? cp["color"].At(3).F(1) : 1.0f};
            if (ImGui::ColorEdit4("Couleur", col)) { Json a = Json::Array(); for (float x : col) a.Push(x); p["color"] = a; changed = true; }
        }
    } else if (c == "layer_props") {
        bool hasO = p.Has("opacity"), hasB = p.Has("blend"), hasV = p.Has("visible");
        if (ImGui::Checkbox("##o", &hasO)) { if (hasO) p["opacity"] = 1.0f; else p.Erase("opacity"); changed = true; }
        ImGui::SameLine();
        ImGui::BeginDisabled(!hasO);
        float o = cp["opacity"].F(1) * 100;
        if (ImGui::SliderFloat("Opacité", &o, 0, 100, "%.0f %%")) { p["opacity"] = o / 100; changed = true; }
        ImGui::EndDisabled();
        if (ImGui::Checkbox("##b", &hasB)) { if (hasB) p["blend"] = 0; else p.Erase("blend"); changed = true; }
        ImGui::SameLine();
        ImGui::BeginDisabled(!hasB);
        int bm = std::clamp(cp["blend"].I(0), 0, (int)BlendMode::Count - 1);
        if (ImGui::BeginCombo("Mode", BlendModeName((BlendMode)bm))) {
            for (int k = 0; k < (int)BlendMode::Count; ++k) if (ImGui::Selectable(BlendModeName((BlendMode)k), k == bm)) { p["blend"] = k; changed = true; }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if (ImGui::Checkbox("##v", &hasV)) { if (hasV) p["visible"] = true; else p.Erase("visible"); changed = true; }
        ImGui::SameLine();
        ImGui::BeginDisabled(!hasV);
        bool vis = cp["visible"].B(true);
        if (ImGui::Checkbox("Visible", &vis)) { p["visible"] = vis; changed = true; }
        ImGui::EndDisabled();
        ImGui::TextDisabled("Les cases de gauche choisissent ce que l'étape modifie.");
    } else if (c == "export") {
        static const char* fm[] = {"png", "jpg", "tga", "bmp", "psd", "sxl", "tif"};
        int f = 0;
        for (int k = 0; k < 7; ++k) if (cp["format"].S("png") == fm[k]) f = k;
        if (ImGui::Combo("Format", &f, "PNG (sans perte, transparence)\0JPG (léger, sans transparence)\0TGA\0BMP\0PSD (calques)\0SXL (document Shaderixel)\0TIFF 16 bits (photo)\0")) { p["format"] = fm[f]; changed = true; }
        if (f == 1) {
            int q = cp["quality"].I(92);
            if (ImGui::SliderInt("Qualité", &q, 10, 100)) { p["quality"] = q; changed = true; }
        }
        std::string folder = cp["folder"].S();
        ImGui::SetNextItemWidth(-150 * dpi);
        if (ImGui::InputTextWithHint("##folder", "dossier de l'image d'origine", &folder)) { p["folder"] = folder; changed = true; }
        ImGui::SameLine();
        if (ImGui::Button("...##fold")) {
            std::string pick = platform::OpenFileDialog(hwnd, L"Un fichier du dossier\0*.*\0");
            if (!pick.empty()) { p["folder"] = DirOfPath(pick); changed = true; }
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Dossier");
        changed |= InputStr("Suffixe du nom", p, "suffix");
    } else if (c == "op") {
        const AiOperation* cur = bridge.Find(cp["name"].S());
        if (ImGui::BeginCombo("Opération", cp["name"].S().c_str(), ImGuiComboFlags_HeightLarge)) {
            for (auto& o : bridge.Operations())
                if (ImGui::Selectable(o.id.c_str(), cur == &o)) { p["name"] = o.id; changed = true; }
            ImGui::EndCombo();
        }
        if (cur) Wrapped(cur->description, ImVec4(0.65f, 0.65f, 0.7f, 1));
    } else {
        ImGui::TextDisabled("Pas de réglage pour cette étape.");
    }
    ImGui::PopItemWidth();
    // Everything else: the raw parameters.
    if (ImGui::CollapsingHeader("Paramètres avancés (JSON)")) {
        auto& buf = stepJsonEdit[&s];
        if (buf.empty() || changed) buf = p.Dump(2);
        if (ImGui::InputTextMultiline("##json", &buf, ImVec2(-1, ImGui::GetTextLineHeight() * 8))) {
            Json j;
            if (Json::Parse(buf, j) && j.IsObject()) { p = j; changed = true; }
        }
        ImGui::TextDisabled("Appliqué dès que le JSON est valide.");
    } else if (changed) {
        stepJsonEdit.erase(&s);
    }
    ImGui::EndDisabled();
    if (changed && !readOnly) SaveActions();
}

void App::DrawActionEditor() {
    if (!ImGui::FindWindowByName("Éditeur d'actions"))
        if (ImGuiWindow* c = ImGui::FindWindowByName("Canevas"); c && c->DockId) ImGui::SetNextWindowDockID(c->DockId, ImGuiCond_FirstUseEver);
    if (actionEditorFocus) {
        ImGui::SetNextWindowFocus();
        actionEditorFocus = false;
    }
    if (!ImGui::Begin("Éditeur d'actions", PanelFlag("Éditeur d'actions"))) { ImGui::End(); return; }
    PopOutCorner("Éditeur d'actions");
    DocView* v = Active();
    const auto& builtins = BuiltinActions();
    if (builtinSel >= (int)builtins.size()) builtinSel = -1;
    actionSel = std::clamp(actionSel, 0, std::max(0, (int)actions.size() - 1));
    const bool isBuiltin = builtinSel >= 0;
    Action* cur = isBuiltin ? nullptr : actions.empty() ? nullptr : &actions[actionSel];
    const Action* shown = isBuiltin ? &builtins[builtinSel] : cur;
    const std::string key = isBuiltin ? "b:" + builtins[builtinSel].name : cur ? "u:" + cur->name : "";
    const ImVec4 dim(0.62f, 0.64f, 0.70f, 1);

    // ---- left: lists
    const float leftW = 250 * dpi, rightW = std::max(300 * dpi, ImGui::GetContentRegionAvail().x * 0.32f);
    ImGui::BeginChild("##aelist", ImVec2(leftW, 0), ImGuiChildFlags_Borders);
    ImGui::SeparatorText("Mes actions");
    for (int i = 0; i < (int)actions.size(); ++i) {
        ImGui::PushID(i);
        std::string label = actions[i].name + "  (" + std::to_string(actions[i].steps.size()) + ")";
        if (actionRecording && actionRecordInto == i) label = "\xE2\x97\x8F " + label;
        if (ImGui::Selectable(label.c_str(), !isBuiltin && i == actionSel)) { actionSel = i; builtinSel = -1; actionStepSel = -1; }
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Dupliquer")) { Action a = actions[i]; a.name += " copie"; actions.insert(actions.begin() + i + 1, a); SaveActions(); }
            if (ImGui::MenuItem("Exporter...")) ExportActions(i);
            if (ImGui::MenuItem("Supprimer", nullptr, false, !actionRecording)) {
                actions.erase(actions.begin() + i);
                actionSel = std::max(0, std::min(actionSel, (int)actions.size() - 1));
                stepJsonEdit.clear();
                SaveActions();
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    if (ImGui::SmallButton("+ Nouvelle")) {
        actions.push_back({"Action " + std::to_string(actions.size() + 1), {}});
        actionSel = (int)actions.size() - 1;
        builtinSel = -1;
        actionStepSel = -1;
        SaveActions();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Importer...")) ImportActions();
    ImGui::SameLine();
    ImGui::BeginDisabled(actions.empty());
    if (ImGui::SmallButton("Exporter...")) ExportActions(-1);
    ImGui::EndDisabled();
    ImGui::SeparatorText("Actions simples");
    std::string group;
    for (int i = 0; i < (int)builtins.size(); ++i) {
        if (builtins[i].group != group) {
            group = builtins[i].group;
            ImGui::TextDisabled("%s", group.c_str());
        }
        ImGui::PushID(1000 + i);
        ImGui::Indent(8 * dpi);
        if (ImGui::Selectable(builtins[i].name.c_str(), i == builtinSel)) { builtinSel = i; actionStepSel = -1; }
        if (ImGui::IsItemHovered() && !builtins[i].steps.empty()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28);
            for (auto& s : builtins[i].steps) ImGui::BulletText("%s", ActionStepDescription(s).c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::Unindent(8 * dpi);
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- middle: the steps
    const float midW = std::max(200 * dpi, ImGui::GetContentRegionAvail().x - rightW - ImGui::GetStyle().ItemSpacing.x);
    ImGui::BeginChild("##aesteps", ImVec2(midW, 0), ImGuiChildFlags_Borders);
    if (!shown) {
        ImGui::TextDisabled("Aucune action. Créez-en une (+ Nouvelle) ou choisissez une action simple.");
        ImGui::EndChild();
        ImGui::End();
        return;
    }
    // Header: name, record, run, batch.
    if (cur) {
        std::string name = cur->name;
        ImGui::SetNextItemWidth(std::min(320 * dpi, ImGui::GetContentRegionAvail().x));
        if (ImGui::InputText("##aename", &name, ImGuiInputTextFlags_EnterReturnsTrue) || (ImGui::IsItemDeactivatedAfterEdit())) {
            if (!name.empty() && name != cur->name) {
                for (auto& k : batch.chain) if (k == "u:" + cur->name) k = "u:" + name;
                cur->name = name;
                SaveActions();
            }
        }
    } else {
        ImGui::TextUnformatted(shown->name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(action simple, lecture seule)");
    }
    const bool rec = actionRecording && cur && actionRecordInto == actionSel;
    if (cur) {
        if (rec) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(200, 50, 50, 255));
        if (ImGui::Button(rec ? "Arrêter l'enregistrement" : "Enregistrer")) {
            if (actionRecording) StopRecording();
            else {
                actionRecording = true;
                actionRecordInto = actionSel;
                Status("Enregistrement dans « " + cur->name + " » : les commandes des menus sont ajoutées à la fin");
            }
        }
        if (rec) ImGui::PopStyleColor();
        ImGui::SameLine();
    } else {
        if (ImGui::Button("Copier dans mes actions")) {
            Action a = *shown;
            a.group.clear();
            std::string base = a.name;
            for (int k = 2; FindActionKey("u:" + a.name); ++k) a.name = base + " (" + std::to_string(k) + ")";
            actions.push_back(a);
            actionSel = (int)actions.size() - 1;
            builtinSel = -1;
            SaveActions();
            ImGui::EndChild();
            ImGui::End();
            return;
        }
        ui::HelpTooltip("Une copie modifiable (étapes, réglages, ajout d'étapes).");
        ImGui::SameLine();
    }
    ImGui::BeginDisabled(!v || actionRun.active || shown->steps.empty());
    if (ImGui::Button("Exécuter sur l'image") && v) StartActionSteps(*v, shown->name, shown->steps, 0, INT_MAX, key);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Traitement par lots...")) {
        batch.open = true;
        if (std::find(batch.chain.begin(), batch.chain.end(), key) == batch.chain.end()) batch.chain = {key};
    }
    if (actionRun.active)
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1, 1), "Exécution : %s (%zu/%zu)", actionRun.name.c_str(), std::min(actionRun.next + 1, actionRun.steps.size()), actionRun.steps.size());
    ImGui::Separator();

    // The list.
    std::vector<ActionStep> dummy;
    std::vector<ActionStep>& steps = cur ? cur->steps : dummy;
    const std::vector<ActionStep>& list = cur ? cur->steps : shown->steps;
    if (actionStepSel >= (int)list.size()) actionStepSel = (int)list.size() - 1;
    int moveFrom = -1, moveTo = -1, remove = -1, dup = -1, runFirst = -1, runLast = -1;
    ImGui::BeginChild("##aeliststeps", ImVec2(0, cur ? -ImGui::GetFrameHeightWithSpacing() * 1.3f : 0));
    if (list.empty()) Wrapped("Aucune étape. Cliquez sur « Enregistrer » puis utilisez les menus (Image, Filtre, Calque, Sélection...), "
                              "ou ajoutez des étapes avec « + Ajouter une étape ».", dim);
    for (int k = 0; k < (int)list.size(); ++k) {
        const ActionStep& s = list[k];
        ImGui::PushID(k);
        const bool running = actionRun.active && actionRun.key == key && actionRun.next < actionRun.srcIndex.size() && actionRun.srcIndex[actionRun.next] == k;
        ImGui::BeginGroup();
        if (cur) {
            bool en = steps[k].enabled;
            if (ImGui::Checkbox("##en", &en)) { steps[k].enabled = en; SaveActions(); }
            ui::HelpTooltip("Décochée : l'étape est ignorée à l'exécution.");
            ImGui::SameLine();
        }
        const float x0 = ImGui::GetCursorPosX();
        std::string head = std::to_string(k + 1) + ".  " + ActionStepSummary(s);
        if (running) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.85f, 1, 1));
        else if (!s.enabled) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (ImGui::Selectable(head.c_str(), k == actionStepSel, ImGuiSelectableFlags_AllowOverlap)) actionStepSel = k;
        if (running || !s.enabled) ImGui::PopStyleColor();
        // Drag to reorder.
        if (cur && ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("SX_ACTSTEP", &k, sizeof(int));
            ImGui::TextUnformatted(head.c_str());
            ImGui::EndDragDropSource();
        }
        if (cur && ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("SX_ACTSTEP")) { moveFrom = *(const int*)pl->Data; moveTo = k; }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem()) {
            actionStepSel = k;
            if (ImGui::MenuItem("Exécuter cette étape", nullptr, false, v && !actionRun.active)) { runFirst = runLast = k; }
            if (ImGui::MenuItem("Exécuter jusqu'ici", nullptr, false, v && !actionRun.active)) { runFirst = 0; runLast = k; }
            if (ImGui::MenuItem("Exécuter à partir d'ici", nullptr, false, v && !actionRun.active)) { runFirst = k; runLast = INT_MAX; }
            if (cur) {
                ImGui::Separator();
                if (ImGui::MenuItem("Dupliquer")) dup = k;
                if (ImGui::MenuItem("Monter", nullptr, false, k > 0)) { moveFrom = k; moveTo = k - 1; }
                if (ImGui::MenuItem("Descendre", nullptr, false, k + 1 < (int)list.size())) { moveFrom = k; moveTo = k + 1; }
                if (ImGui::MenuItem("Supprimer")) remove = k;
            }
            ImGui::EndPopup();
        }
        // What it does, in full.
        ImGui::SetCursorPosX(x0 + 18 * dpi);
        ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x);
        ImGui::PushStyleColor(ImGuiCol_Text, s.enabled ? dim : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted(ActionStepDescription(s).c_str());
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
        ImGui::EndGroup();
        ImGui::Spacing();
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (cur) {
        if (ImGui::Button("+ Ajouter une étape")) ImGui::OpenPopup("##aeadd");
        if (ImGui::BeginPopup("##aeadd")) {
            for (int i = 0; i < ActionCmdCount(); ++i)
                if (ImGui::MenuItem(ActionCmdLabel(ActionCmdId(i)))) {
                    // Inserted after the selected step (at the end otherwise).
                    int at = actionStepSel >= 0 ? actionStepSel + 1 : (int)steps.size();
                    steps.insert(steps.begin() + at, {ActionCmdId(i), ActionDefaultParams(ActionCmdId(i)), true});
                    actionStepSel = at;
                    stepJsonEdit.clear();
                    SaveActions();
                }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Glisser une étape pour la déplacer, clic droit pour plus d'options.");
    }
    if (cur) {
        if (dup >= 0) { steps.insert(steps.begin() + dup + 1, steps[dup]); actionStepSel = dup + 1; }
        if (remove >= 0) { steps.erase(steps.begin() + remove); actionStepSel = std::min(actionStepSel, (int)steps.size() - 1); }
        if (moveFrom >= 0 && moveTo >= 0 && moveFrom != moveTo) {
            ActionStep s = steps[moveFrom];
            steps.erase(steps.begin() + moveFrom);
            steps.insert(steps.begin() + moveTo, s);
            actionStepSel = moveTo;
        }
        if (dup >= 0 || remove >= 0 || moveFrom >= 0) { stepJsonEdit.clear(); SaveActions(); }
    }
    if (runFirst >= 0 && v) StartActionSteps(*v, shown->name, shown->steps, runFirst, runLast, key);
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- right: the selected step
    ImGui::BeginChild("##aeinsp", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (actionStepSel < 0 || actionStepSel >= (int)list.size()) {
        ImGui::TextDisabled("Choisissez une étape pour la modifier.");
    } else {
        ActionStep tmp = list[actionStepSel];
        ActionStep& s = cur ? steps[actionStepSel] : tmp;
        ImGui::Text("Étape %d : %s", actionStepSel + 1, ActionCmdLabel(s.cmd));
        Wrapped(ActionStepDescription(s), dim);
        ImGui::Separator();
        DrawActionInspector(s, !cur);
    }
    ImGui::EndChild();
    ImGui::End();
}

// ------------------------------------------------------------------ batch window

void App::DrawBatchWindow() {
    auto& B = batch;
    if (!B.open) return;
    ImGui::SetNextWindowSize(ImVec2(640 * dpi, 640 * dpi), ImGuiCond_Appearing);
    if (!ImGui::Begin("Traitement par lots", &B.open, ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }
    const ImVec4 dim(0.62f, 0.64f, 0.70f, 1);
    ImGui::BeginDisabled(B.running);
    auto folderField = [&](const char* label, std::string& s, const char* hint) {
        ImGui::PushID(label);
        ImGui::SetNextItemWidth(-150 * dpi);
        ImGui::InputTextWithHint("##f", hint, &s);
        ImGui::SameLine();
        if (ImGui::Button("...")) {
            std::string f = platform::OpenFileDialog(hwnd, L"Un fichier du dossier\0*.*\0");
            if (!f.empty()) s = DirOfPath(f);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(label);
        ImGui::PopID();
    };

    // 1. What is done.
    ImGui::SeparatorText("1. Traitement (actions exécutées dans l'ordre)");
    int removeAt = -1, upAt = -1;
    for (int i = 0; i < (int)B.chain.size(); ++i) {
        ImGui::PushID(i);
        const Action* a = FindActionKey(B.chain[i]);
        ImGui::Text("%d.", i + 1);
        ImGui::SameLine();
        if (a) ImGui::TextUnformatted(a->name.c_str());
        else ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s (introuvable)", B.chain[i].substr(2).c_str());
        if (a && ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28);
            for (auto& s : a->steps) if (s.enabled) ImGui::BulletText("%s", ActionStepDescription(s).c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::SameLine();
        ImGui::TextDisabled(B.chain[i][0] == 'b' ? "(simple)" : "(mes actions)");
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 70 * dpi);
        if (ImGui::SmallButton("^") && i > 0) upAt = i;
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) removeAt = i;
        ImGui::PopID();
    }
    if (upAt > 0) std::swap(B.chain[upAt], B.chain[upAt - 1]);
    if (removeAt >= 0) B.chain.erase(B.chain.begin() + removeAt);
    if (ImGui::Button("+ Ajouter une action")) ImGui::OpenPopup("##badd");
    if (ImGui::BeginPopup("##badd")) {
        ImGui::TextDisabled("Mes actions");
        for (auto& a : actions) if (ImGui::MenuItem(a.name.c_str())) B.chain.push_back("u:" + a.name);
        std::string group;
        for (auto& a : BuiltinActions()) {
            if (a.group != group) { group = a.group; ImGui::Separator(); ImGui::TextDisabled("%s", group.c_str()); }
            if (ImGui::MenuItem(a.name.c_str())) B.chain.push_back("b:" + a.name);
        }
        ImGui::EndPopup();
    }
    if (B.chain.empty()) { ImGui::SameLine(); ImGui::TextDisabled("aucune : simple conversion vers le format choisi plus bas"); }

    // 2. Source.
    ImGui::SeparatorText("2. Images à traiter");
    ImGui::RadioButton("Dossier", &B.sourceKind, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Documents ouverts", &B.sourceKind, 1);
    if (B.sourceKind == 0) {
        folderField("Dossier source", B.source, "dossier des images");
        ImGui::Checkbox("Inclure les sous-dossiers", &B.subfolders);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160 * dpi);
        ImGui::InputTextWithHint("Extensions", "toutes (ex. png jpg)", &B.extFilter);
    } else {
        Wrapped(std::to_string(docs.size()) + " document(s) ouvert(s). Ils sont modifiés par les actions (annulable avec l'historique) et restent ouverts.", dim);
    }

    // 3. Output.
    ImGui::SeparatorText("3. Résultat");
    ImGui::SetNextItemWidth(260 * dpi);
    ImGui::Combo("Format", &B.format, "Aucun (seulement les étapes Exporter)\0PNG\0JPG\0TGA\0BMP\0PSD\0SXL (document)\0Même format que l'original\0TIFF 16 bits\0");
    if (B.format == 2) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120 * dpi);
        ImGui::SliderInt("Qualité", &B.quality, 10, 100);
    }
    folderField("Dossier de sortie", B.dest, B.sourceKind == 0 ? "à côté des originaux" : "à côté des documents");
    if (B.sourceKind == 0) {
        ImGui::BeginDisabled(!B.subfolders);
        ImGui::Checkbox("Recréer les sous-dossiers", &B.keepTree);
        ImGui::EndDisabled();
    }
    ImGui::SetNextItemWidth(260 * dpi);
    ImGui::InputText("Nom des fichiers", &B.pattern);
    ui::HelpTooltip("{nom} nom d'origine, {n} numéro, {n3} numéro sur 3 chiffres (001),\n{l} {h} largeur / hauteur finales, {dossier} dossier d'origine,\n{ext} extension d'origine, {date} date du jour (AAAAMMJJ).");
    ImGui::SameLine();
    if (ImGui::SmallButton("{nom}_{n3}")) B.pattern = "{nom}_{n3}";
    ImGui::SameLine();
    if (ImGui::SmallButton("{nom}_{l}x{h}")) B.pattern = "{nom}_{l}x{h}";
    ImGui::SetNextItemWidth(260 * dpi);
    ImGui::Combo("Si le fichier existe", &B.conflict, "Remplacer\0Ignorer l'image\0Ajouter un numéro\0");
    ImGui::EndDisabled();

    // Preview of the first names (sizes are those of the originals until processed).
    if (!B.running) {
        static std::vector<std::string> preview;
        static double previewTime = -10;
        if (ImGui::GetTime() - previewTime > 1.0) {
            previewTime = ImGui::GetTime();
            preview.clear();
            if (B.sourceKind == 0) {
                auto saved = B.files;
                B.files = BatchScan();
                const size_t n = B.files.size();
                static const char* kExt[] = {"", ".png", ".jpg", ".tga", ".bmp", ".psd", ".sxl", "", ".tif"};
                for (size_t i = 0; i < n && i < 4; ++i) {
                    std::string ext = B.format == 7 ? Narrow(Utf8Path(B.files[i]).extension().wstring()) : kExt[std::clamp(B.format, 0, 8)];
                    preview.push_back(Narrow(Utf8Path(B.files[i]).filename().wstring()) + "  ->  " + BatchFolder(i, nullptr) + "/" + BatchName(i, nullptr) + ext);
                }
                if (n > 4) preview.push_back("... " + std::to_string(n) + " image(s) en tout");
                if (n == 0) preview.push_back(B.source.empty() ? "Choisissez un dossier source." : "Aucune image trouvée.");
                B.files = saved;
            }
        }
        if (B.sourceKind == 0 && !preview.empty()) {
            ImGui::SeparatorText("Aperçu");
            for (auto& l : preview) Wrapped(l, dim);
        }
    }

    ImGui::Separator();
    if (B.running) {
        float f = B.files.empty() ? 0 : (float)B.index / B.files.size();
        char ov[64];
        snprintf(ov, sizeof(ov), "%zu / %zu", std::min(B.index + 1, B.files.size()), B.files.size());
        ImGui::ProgressBar(f, ImVec2(-1, 0), ov);
        if (B.index < B.files.size()) ImGui::TextDisabled("%s", B.files[B.index].c_str());
        if (ImGui::Button("Annuler")) B.cancel = true;
    } else {
        const bool ready = (B.sourceKind == 1 ? !docs.empty() : !B.source.empty()) && (!B.chain.empty() || B.format != 0);
        ImGui::BeginDisabled(!ready || actionRun.active);
        if (ImGui::Button("Démarrer", ImVec2(140 * dpi, 0))) StartBatch();
        ImGui::EndDisabled();
        if (!B.log.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("Dernier lot : %zu traitée(s), %zu ignorée(s), %zu erreur(s)", B.done, B.skipped, B.errors.size());
        }
    }
    // Log.
    if (!B.log.empty()) {
        ImGui::SeparatorText("Journal");
        if (ImGui::BeginTable("##blog", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV,
                              ImVec2(0, std::max(120 * dpi, ImGui::GetContentRegionAvail().y)))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Image");
            ImGui::TableSetupColumn("Résultat");
            ImGui::TableSetupColumn("Fichier écrit");
            ImGui::TableHeadersRow();
            for (size_t i = 0; i < B.log.size(); ++i) {
                auto& l = B.log[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(Narrow(Utf8Path(l.file).filename().wstring()).c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(l.state == 0 ? ImVec4(0.5f, 0.85f, 0.5f, 1) : l.state == 1 ? ImVec4(0.9f, 0.75f, 0.4f, 1) : ImVec4(1, 0.45f, 0.45f, 1), "%s", l.msg.c_str());
                ImGui::TableNextColumn();
                ImGui::PushID((int)i);
                if (!l.out.empty() && ImGui::Selectable(l.out.c_str()))
                    ShellExecuteW(nullptr, L"open", L"explorer.exe", (L"/select,\"" + Widen(l.out) + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

}  // namespace sx
