// Actions (recorded macros) and batch processing. A step is a command with JSON
// parameters, recorded from the menus / dialogs while recording, replayed on any
// document; the batch runs one action on every image of a folder and exports
// the results.
#include "ui/App.h"
#include "ui/Theme.h"
#include "doc/Effects.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace sx {

namespace {

const char* kCmds[][2] = {
    {"image_size", "Taille de l'image"},
    {"canvas_size", "Taille de la zone de travail"},
    {"rotate", "Rotation de l'image"},
    {"crop_selection", "Recadrer selon la sélection"},
    {"filter", "Filtre"},
    {"new_layer", "Nouveau calque"},
    {"duplicate_layer", "Dupliquer le calque"},
    {"merge_down", "Fusionner vers le bas"},
    {"merge_visible", "Fusionner les calques visibles"},
    {"flatten", "Aplatir l'image"},
    {"select", "Sélection"},
    {"fill", "Remplir"},
    {"erase", "Effacer"},
    {"layer_props", "Propriétés du calque"},
    {"export", "Exporter"},
    {"op", "Opération (outil IA / MCP)"},
};

const char* CmdLabel(const std::string& c) {
    for (auto& k : kCmds) if (c == k[0]) return k[1];
    return c.c_str();
}

std::string StepSummary(const ActionStep& s) {
    const Json& p = s.params;
    char b[256] = "";
    if (s.cmd == "image_size") {
        std::string m = p["mode"].S("pixels");
        if (m == "percent") snprintf(b, sizeof(b), "%.1f %%", p["pct"].F(100));
        else snprintf(b, sizeof(b), "%s%d x %d px", m == "fit" ? "adapter à " : "", p["w"].I(), p["h"].I());
    } else if (s.cmd == "canvas_size") {
        snprintf(b, sizeof(b), "%s%d x %d px", p["relative"].B() ? "+" : "", p["w"].I(), p["h"].I());
    } else if (s.cmd == "rotate") {
        static const char* ops[] = {"90° horaire", "90° antihoraire", "180°", "symétrie horizontale", "symétrie verticale"};
        snprintf(b, sizeof(b), "%s", ops[std::clamp(p["op"].I(), 0, 4)]);
    } else if (s.cmd == "filter") {
        auto def = Effects().Find(p["effect"].S());
        snprintf(b, sizeof(b), "%s%s", def ? def->name.c_str() : p["effect"].S().c_str(), p["destructive"].B() ? " (pixels)" : "");
    } else if (s.cmd == "select") {
        snprintf(b, sizeof(b), "%s", p["what"].S() == "all" ? "tout" : p["what"].S() == "none" ? "aucune" : "intervertir");
    } else if (s.cmd == "export") {
        snprintf(b, sizeof(b), "%s%s%s", p["format"].S("png").c_str(), p["folder"].S().empty() ? "" : " -> ", p["folder"].S().c_str());
    } else if (s.cmd == "op") {
        snprintf(b, sizeof(b), "%s", p["name"].S().c_str());
    } else if (s.cmd == "new_layer") {
        snprintf(b, sizeof(b), "%s", p["name"].S().c_str());
    } else if (s.cmd == "layer_props") {
        snprintf(b, sizeof(b), "opacité %.0f %%", p["opacity"].F(1) * 100);
    }
    return b[0] ? std::string(CmdLabel(s.cmd)) + " : " + b : CmdLabel(s.cmd);
}

std::string DirOf(const std::string& path) {
    auto p = Utf8Path(path).parent_path();
    return Narrow(p.wstring());
}

}  // namespace

std::string App::ActionsPath() const {
    return DirOf(settingsPath) + "/shaderixel_actions.json";
}

void App::LoadActions() {
    actions.clear();
    std::ifstream f(Utf8Path(ActionsPath()), std::ios::binary);
    Json j;
    if (f) {
        std::stringstream ss;
        ss << f.rdbuf();
        Json::Parse(ss.str(), j);
    }
    for (auto& a : j["actions"].Elements()) {
        Action act;
        act.name = a["name"].S();
        for (auto& s : a["steps"].Elements()) act.steps.push_back({s["cmd"].S(), s["params"], s["enabled"].B(true)});
        actions.push_back(std::move(act));
    }
    if (!j.Has("actions")) {
        // Starter actions.
        Action web{"Taille web (1920 px) + JPG", {}};
        Json p = Json::Object();
        p["mode"] = "fit"; p["w"] = 1920; p["h"] = 1920; p["interp"] = 2;
        web.steps.push_back({"image_size", p, true});
        web.steps.push_back({"flatten", Json::Object(), true});
        Json e = Json::Object();
        e["format"] = "jpg"; e["suffix"] = "_web"; e["folder"] = "";
        web.steps.push_back({"export", e, true});
        actions.push_back(web);
        Action rot{"Rotation 90° horaire", {}};
        Json r = Json::Object();
        r["op"] = 0;
        rot.steps.push_back({"rotate", r, true});
        actions.push_back(rot);
    }
}

void App::SaveActions() {
    Json j = Json::Object();
    Json arr = Json::Array();
    for (auto& a : actions) {
        Json o = Json::Object();
        o["name"] = a.name;
        Json st = Json::Array();
        for (auto& s : a.steps) {
            Json so = Json::Object();
            so["cmd"] = s.cmd;
            so["params"] = s.params.IsObject() ? s.params : Json::Object();
            so["enabled"] = s.enabled;
            st.Push(so);
        }
        o["steps"] = st;
        arr.Push(o);
    }
    j["actions"] = arr;
    std::ofstream f(Utf8Path(ActionsPath()), std::ios::binary);
    f << j.Dump();
}

void App::FinalizeRecordedFilter() {
    // The filter parameters are those of the layer filter once tuned (at the next step / stop).
    if (recordFilterLayer == 0 || actionRecordInto < 0 || actionRecordInto >= (int)actions.size()) return;
    auto& steps = actions[actionRecordInto].steps;
    DocView* v = Active();
    if (!steps.empty() && steps.back().cmd == "filter" && v) {
        if (Layer* l = v->doc->FindLayer(recordFilterLayer)) {
            for (auto it = l->filters.rbegin(); it != l->filters.rend(); ++it)
                if (it->effectId == steps.back().params["effect"].S()) {
                    Json ps = Json::Array();
                    for (auto& q : it->params) {
                        Json a = Json::Array();
                        a.Push(q.x); a.Push(q.y); a.Push(q.z); a.Push(q.w);
                        ps.Push(a);
                    }
                    steps.back().params["params"] = ps;
                    break;
                }
        }
    }
    recordFilterLayer = 0;
}

void App::RecordAction(const std::string& cmd, Json params) {
    if (!actionRecording || actionReplaying || actionRecordInto < 0 || actionRecordInto >= (int)actions.size()) return;
    FinalizeRecordedFilter();
    if (!params.IsObject()) params = Json::Object();
    actions[actionRecordInto].steps.push_back({cmd, params, true});
    if (cmd == "filter" && !params["destructive"].B()) {
        if (DocView* v = Active())
            if (Layer* l = v->doc->ActiveLayer()) recordFilterLayer = l->id;
    }
    SaveActions();
}

void App::StopRecording() {
    if (!actionRecording) return;
    FinalizeRecordedFilter();
    actionRecording = false;
    SaveActions();
    Status("Enregistrement de l'action terminé");
}

bool App::StartAction(DocView& v, int index, std::function<void(bool)> done) {
    if (actionRun.active) { Status("Une action est déjà en cours"); return false; }
    if (index < 0 || index >= (int)actions.size()) return false;
    if (actionRecording) StopRecording();
    actionRun = ActionRun{};
    actionRun.active = true;
    actionRun.view = &v;
    actionRun.name = actions[index].name;
    for (auto& s : actions[index].steps) if (s.enabled) actionRun.steps.push_back(s);
    actionRun.done = std::move(done);
    return true;
}

int App::ExecActionStep(DocView& v, const ActionStep& s) {
    Document& d = *v.doc;
    const Json& p = s.params;
    const int W = (int)d.Width(), H = (int)d.Height();
    auto& R = actionRun;
    if (s.cmd == "image_size") {
        std::string mode = p["mode"].S("pixels");
        int w = p["w"].I(W), h = p["h"].I(H);
        if (mode == "percent") {
            w = std::max(1, (int)std::lround(W * p["pct"].F(100) / 100.0));
            h = std::max(1, (int)std::lround(H * p["pct"].F(100) / 100.0));
        } else if (mode == "fit") {
            double k = std::min((double)p["w"].I(W) / W, (double)p["h"].I(H) / H);
            if (k >= 1.0 && !p["enlarge"].B()) return 0;
            w = std::max(1, (int)std::lround(W * k));
            h = std::max(1, (int)std::lround(H * k));
        }
        w = std::clamp(w, 1, 16384);
        h = std::clamp(h, 1, 16384);
        float m[6] = {(float)W / w, 0, 0, 0, (float)H / h, 0};
        ApplyCanvasOp(v, (u32)w, (u32)h, m, p["interp"].I(2), "Taille de l'image");
        return 0;
    }
    if (s.cmd == "canvas_size") {
        int w = p["w"].I(W), h = p["h"].I(H);
        if (p["relative"].B()) { w += W; h += H; }
        w = std::clamp(w, 1, 16384);
        h = std::clamp(h, 1, 16384);
        int anchor = p["anchor"].I(4), ax = anchor % 3, ay = anchor / 3;
        float ox = std::floor(ax * (w - W) * 0.5f), oy = std::floor(ay * (h - H) * 0.5f);
        float m[6] = {1, 0, -ox, 0, 1, -oy};
        ApplyCanvasOp(v, (u32)w, (u32)h, m, 0, "Taille de la zone de travail");
        return 0;
    }
    if (s.cmd == "rotate") { RotateCanvas(v, p["op"].I()); return 0; }
    if (s.cmd == "crop_selection") {
        if (!R.wait) {
            Surface* sel = d.Selection();
            if (!sel) return 0;  // nothing selected: skipped
            R.wait = v.renderer.ComputeBounds(*ctx, sel->tex.get(), nullptr, false, true);
            return 1;
        }
        if (!R.wait->ready) return 1;
        const u32* b = (const u32*)R.wait->data.data();
        if (R.wait->data.size() >= 16 && b[0] < b[2] && b[1] < b[3]) ApplyCrop(v, {(int)b[0], (int)b[1], (int)b[2], (int)b[3]});
        R.wait.reset();
        return 0;
    }
    if (s.cmd == "filter") {
        auto def = Effects().Find(p["effect"].S());
        if (!def) { R.error = "effet introuvable : " + p["effect"].S(); return 2; }
        EffectInstance fx;
        fx.effectId = def->id;
        fx.params = def->DefaultParams();
        const Json& ps = p["params"];
        for (size_t i = 0; i < ps.Size() && i < fx.params.size(); ++i) {
            const Json& a = ps.At(i);
            fx.params[i] = {a.At(0).F(), a.Size() > 1 ? a.At(1).F() : 0, a.Size() > 2 ? a.At(2).F() : 0, a.Size() > 3 ? a.At(3).F() : 0};
        }
        if (p["destructive"].B()) {
            Id target = d.PaintTarget();
            if (!d.GetSurface(target)) { R.error = "filtre : aucun calque de pixels"; return 2; }
            if (v.renderer.ApplyEffectToSurface(d, *ctx, v.history, target, fx, def->name)) return 0;
            return ++R.retries > 600 ? (R.error = "filtre non compilé : " + def->name, 2) : 1;  // still compiling
        }
        ApplyLibraryEffect(v, def->id, FxApply::Filter);
        if (Layer* l = d.ActiveLayer(); l && !l->filters.empty() && l->filters.back().effectId == def->id) l->filters.back().params = fx.params;
        d.SyncGraph();
        return 0;
    }
    if (s.cmd == "new_layer") {
        if (d.Mode() != DocMode::Layers) return 0;
        Edit e(d, v.history, "Nouveau calque");
        std::string n = p["name"].S();
        d.AddPixelLayer(n.empty() ? "Calque " + std::to_string(d.S().layers.size() + 1) : n);
        e.Commit();
        return 0;
    }
    if (s.cmd == "duplicate_layer") {
        if (!d.ActiveLayer()) return 0;
        Edit e(d, v.history, "Dupliquer le calque");
        d.DuplicateLayer(d.S().activeLayer);
        e.Commit();
        return 0;
    }
    if (s.cmd == "merge_down") { MergeDown(v); return 0; }
    if (s.cmd == "merge_visible") { MergeVisible(v, false); return 0; }
    if (s.cmd == "flatten") {
        if (d.Mode() == DocMode::Layers && d.S().layers.size() > 1) MergeVisible(v, true);
        return 0;
    }
    if (s.cmd == "select") {
        std::string w = p["what"].S();
        if (w == "all") v.renderer.SelectShape(d, *ctx, v.history, SelShape::All, {}, SelMode::Replace, false, 0, "Tout sélectionner");
        else if (w == "none") v.renderer.Deselect(d, v.history);
        else if (w == "invert") v.renderer.SelectionFilter(d, *ctx, v.history, 3, 0, "Intervertir");
        return 0;
    }
    if (s.cmd == "fill" || s.cmd == "erase") {
        float4 keep = fg;
        const Json& c = p["color"];
        if (c.Size() >= 3) fg = {c.At(0).F(), c.At(1).F(), c.At(2).F(), c.Size() > 3 ? c.At(3).F(1) : 1.0f};
        FillOrErase(v, s.cmd == "erase");
        fg = keep;
        return 0;
    }
    if (s.cmd == "layer_props") {
        Layer* l = d.ActiveLayer();
        if (!l) return 0;
        Edit e(d, v.history, "Propriétés du calque");
        if (p.Has("opacity")) l->opacity = std::clamp(p["opacity"].F(1), 0.0f, 1.0f);
        if (p.Has("blend")) l->blend = (BlendMode)std::clamp(p["blend"].I(), 0, (int)BlendMode::Count - 1);
        if (p.Has("visible")) l->visible = p["visible"].B(true);
        d.SyncGraph();
        e.Commit();
        return 0;
    }
    if (s.cmd == "export") {
        if (!R.exporting) {
            std::string fmt = p["format"].S("png");
            std::string folder = p["folder"].S();
            if (!R.exportFolder.empty()) folder = R.exportFolder;  // batch destination wins
            std::string stem = d.path.empty() ? d.Name() : Narrow(Utf8Path(d.path).stem().wstring());
            if (folder.empty()) folder = d.path.empty() ? DirOf(settingsPath) : DirOf(d.path);
            std::filesystem::create_directories(Utf8Path(folder));
            std::string out = folder + "/" + stem + p["suffix"].S() + "." + fmt;
            if (fmt == "sxl") SaveDocument(v, out);
            else if (fmt == "psd") ExportPsd(out, 8);
            else ExportImage(out);
            R.lastExport = out;
            R.exporting = true;
            return 1;
        }
        if (!exports.empty() || !saves.empty() || psdExport) return 1;
        R.exporting = false;
        return 0;
    }
    if (s.cmd == "op") {
        const AiOperation* op = bridge.Find(p["name"].S());
        if (!op) { R.error = "opération inconnue : " + p["name"].S(); return 2; }
        AiResult r = op->run(p["args"], DirOf(settingsPath));
        if (!r.errorCode.empty()) { R.error = r.errorMessage; return 2; }
        return 0;
    }
    R.error = "commande inconnue : " + s.cmd;
    return 2;
}

void App::UpdateActionRun() {
    auto& R = actionRun;
    if (!R.active) return;
    bool alive = false;
    for (auto& dv : docs) alive |= dv.get() == R.view;
    auto finish = [&](bool ok) {
        auto done = std::move(R.done);
        std::string name = R.name, err = R.error;
        R = ActionRun{};
        if (!ok) Status("Action « " + name + " » interrompue : " + err);
        else if (!done) Status("Action « " + name + " » terminée");
        if (done) done(ok);
    };
    if (!alive) { R.error = "document fermé"; finish(false); return; }
    actionReplaying = true;
    // Steps run back to back in the frame until one waits (readback, export, compilation).
    while (R.next < R.steps.size()) {
        int r = ExecActionStep(*R.view, R.steps[R.next]);
        if (r == 1) break;
        if (r == 2) { actionReplaying = false; finish(false); return; }
        ++R.next;
        R.retries = 0;
    }
    actionReplaying = false;
    if (R.next >= R.steps.size()) finish(true);
}

// ------------------------------------------------------------------ batch

void App::StartBatch() {
    auto& B = batch;
    B.files.clear();
    B.errors.clear();
    B.done = 0;
    std::error_code ec;
    static const char* exts[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".psd", ".hdr", ".exr", ".sxl", ".gif"};
    auto consider = [&](const std::filesystem::path& p) {
        std::string e = Narrow(p.extension().wstring());
        for (auto& ch : e) ch = (char)tolower((unsigned char)ch);
        for (auto* x : exts) if (e == x) { B.files.push_back(Narrow(p.wstring())); return; }
    };
    if (B.subfolders) {
        for (auto& e : std::filesystem::recursive_directory_iterator(Utf8Path(B.source), ec)) if (e.is_regular_file()) consider(e.path());
    } else {
        for (auto& e : std::filesystem::directory_iterator(Utf8Path(B.source), ec)) if (e.is_regular_file()) consider(e.path());
    }
    std::sort(B.files.begin(), B.files.end());
    if (B.files.empty()) { Status("Traitement par lots : aucune image dans " + B.source); return; }
    if (B.action < 0 || B.action >= (int)actions.size()) { Status("Traitement par lots : choisissez une action"); return; }
    B.running = true;
    B.index = 0;
    B.stage = 0;
    Status("Traitement par lots : " + std::to_string(B.files.size()) + " image(s)");
}

void App::UpdateBatch() {
    auto& B = batch;
    if (!B.running) return;
    auto next = [&] {
        // Close the processed document (never prompts) and move on.
        for (int i = 0; i < (int)docs.size(); ++i)
            if (docs[i].get() == B.view) { docs[i]->doc->dirty = false; CloseDocument(i, true); break; }
        B.view = nullptr;
        ++B.index;
        B.stage = 0;
    };
    if (B.cancel) {
        if (actionRun.active) return;  // let the running step finish
        if (B.view) next();
        B.running = false;
        B.cancel = false;
        Status("Traitement par lots annulé");
        return;
    }
    if (B.index >= B.files.size()) {
        B.running = false;
        Status("Traitement par lots terminé : " + std::to_string(B.done) + "/" + std::to_string(B.files.size()) +
               (B.errors.empty() ? "" : " (" + std::to_string(B.errors.size()) + " erreur(s), voir le journal)"));
        for (auto& e : B.errors) Log("Lot : %s", e.c_str());
        return;
    }
    const std::string& file = B.files[B.index];
    if (B.stage == 0) {
        size_t before = docs.size();
        OpenImage(file, false);
        if (docs.size() == before) { B.errors.push_back(file + " : ouverture impossible"); ++B.index; return; }
        B.view = Active();
        B.stage = 1;
        StartAction(*B.view, B.action, [this](bool ok) {
            if (!ok) batch.errors.push_back(batch.files[batch.index] + " : " + statusMsg);
            batch.stage = ok ? 2 : 4;
        });
        // The action's own export steps write into the batch destination.
        if (!B.dest.empty()) actionRun.exportFolder = B.dest;
        return;
    }
    if (B.stage == 1) return;  // action running
    if (B.stage == 2) {
        // Save to the destination (unless the action exported already).
        if (!B.dest.empty() && B.saveResult) {
            std::filesystem::create_directories(Utf8Path(B.dest));
            static const char* fmts[] = {"png", "jpg", "sxl", "psd"};
            std::string stem = Narrow(Utf8Path(file).stem().wstring());
            std::string out = B.dest + "/" + stem + B.suffix + "." + fmts[std::clamp(B.format, 0, 3)];
            if (B.format == 2) SaveDocument(*B.view, out);
            else if (B.format == 3) ExportPsd(out, 8);
            else ExportImage(out);
        }
        B.stage = 3;
        return;
    }
    if (B.stage == 3) {
        if (!exports.empty() || !saves.empty() || psdExport) return;
        ++B.done;
        next();
        return;
    }
    if (B.stage == 4) next();  // failed: skip
}

// ------------------------------------------------------------------ panel

void App::DrawActionsPanel() {
    if (!ImGui::Begin("Actions", PanelFlag("Actions"))) { ImGui::End(); return; }
    DocView* v = Active();
    actionSel = std::clamp(actionSel, 0, std::max(0, (int)actions.size() - 1));
    // Toolbar: record / stop, play, new, duplicate, delete.
    bool rec = actionRecording;
    if (rec) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(200, 50, 50, 255));
    if (ImGui::Button(rec ? "Arrêter" : "Enregistrer")) {
        if (rec) StopRecording();
        else if (!actions.empty()) {
            actionRecording = true;
            actionRecordInto = actionSel;
            Status("Enregistrement dans « " + actions[actionSel].name + " » : les commandes des menus sont ajoutées");
        }
    }
    if (rec) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Enregistre les commandes : taille de l'image / de la zone de travail, rotation, recadrage,\nfiltres (paramètres compris), calques, sélection, remplir / effacer, fusionner, aplatir.");
    ImGui::SameLine();
    ImGui::BeginDisabled(!v || actions.empty() || actionRun.active);
    if (ImGui::Button("Exécuter") && v) StartAction(*v, actionSel);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("+")) {
        actions.push_back({"Action " + std::to_string(actions.size() + 1), {}});
        actionSel = (int)actions.size() - 1;
        actionRename = true;
        SaveActions();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Nouvelle action");
    ImGui::SameLine();
    ImGui::BeginDisabled(actions.empty());
    if (ImGui::Button("Dupliquer")) {
        Action a = actions[actionSel];
        a.name += " copie";
        actions.insert(actions.begin() + actionSel + 1, a);
        ++actionSel;
        SaveActions();
    }
    ImGui::SameLine();
    if (ImGui::Button("Supprimer") && !actionRecording) {
        actions.erase(actions.begin() + actionSel);
        actionSel = std::max(0, actionSel - 1);
        SaveActions();
    }
    ImGui::EndDisabled();
    if (actionRun.active)
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1, 1), "Exécution : %s (%zu/%zu)", actionRun.name.c_str(), actionRun.next + 1, actionRun.steps.size());
    if (actionRecording) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Enregistrement...");
    ImGui::Separator();
    ImGui::BeginChild("##actions", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 1.2f));
    for (int i = 0; i < (int)actions.size(); ++i) {
        Action& a = actions[i];
        ImGui::PushID(i);
        bool sel = i == actionSel;
        ImGuiTreeNodeFlags fl = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | (sel ? ImGuiTreeNodeFlags_Selected : 0);
        bool open;
        if (sel && actionRename) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%s", a.name.c_str());
            ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##name", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                a.name = buf;
                actionRename = false;
                SaveActions();
            } else if (ImGui::IsItemDeactivated()) {
                a.name = buf;
                actionRename = false;
                SaveActions();
            }
            open = false;
        } else {
            open = ImGui::TreeNodeEx("##a", fl, "%s  (%zu)", a.name.c_str(), a.steps.size());
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) actionSel = i;
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { actionSel = i; actionRename = true; }
        }
        if (open) {
            int removeStep = -1, moveStep = -1, moveDir = 0;
            for (int k = 0; k < (int)a.steps.size(); ++k) {
                ActionStep& s = a.steps[k];
                ImGui::PushID(k);
                if (ImGui::Checkbox("##on", &s.enabled)) SaveActions();
                ImGui::SameLine();
                bool stepOpen = ImGui::TreeNodeEx("##s", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", StepSummary(s).c_str());
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Monter", nullptr, false, k > 0)) { moveStep = k; moveDir = -1; }
                    if (ImGui::MenuItem("Descendre", nullptr, false, k + 1 < (int)a.steps.size())) { moveStep = k; moveDir = 1; }
                    if (ImGui::MenuItem("Supprimer l'étape")) removeStep = k;
                    ImGui::EndPopup();
                }
                if (stepOpen) {
                    // Main parameters, then raw JSON for the rest.
                    bool changed = false;
                    Json& p = s.params;
                    if (s.cmd == "image_size") {
                        int mode = p["mode"].S("pixels") == "percent" ? 1 : p["mode"].S("pixels") == "fit" ? 2 : 0;
                        if (ImGui::Combo("Mode", &mode, "Pixels\0Pourcentage\0Adapter à (max)\0")) { p["mode"] = mode == 1 ? "percent" : mode == 2 ? "fit" : "pixels"; changed = true; }
                        if (mode == 1) { float pct = p["pct"].F(100); if (ImGui::DragFloat("%", &pct, 0.5f, 1, 1000)) { p["pct"] = pct; changed = true; } }
                        else {
                            int wh[2] = {p["w"].I(1024), p["h"].I(1024)};
                            if (ImGui::DragInt2("L x H", wh, 1, 1, 16384)) { p["w"] = wh[0]; p["h"] = wh[1]; changed = true; }
                        }
                    } else if (s.cmd == "export") {
                        int f = p["format"].S("png") == "jpg" ? 1 : p["format"].S("png") == "sxl" ? 2 : p["format"].S("png") == "psd" ? 3 : 0;
                        if (ImGui::Combo("Format", &f, "PNG\0JPG\0SXL\0PSD\0")) { static const char* k[] = {"png", "jpg", "sxl", "psd"}; p["format"] = k[f]; changed = true; }
                        char buf[512];
                        snprintf(buf, sizeof(buf), "%s", p["folder"].S().c_str());
                        if (ImGui::InputText("Dossier", buf, sizeof(buf))) { p["folder"] = std::string(buf); changed = true; }
                        snprintf(buf, sizeof(buf), "%s", p["suffix"].S().c_str());
                        if (ImGui::InputText("Suffixe", buf, sizeof(buf))) { p["suffix"] = std::string(buf); changed = true; }
                    } else if (s.cmd == "layer_props") {
                        float o = p["opacity"].F(1);
                        if (ImGui::SliderFloat("Opacité", &o, 0, 1)) { p["opacity"] = o; changed = true; }
                    }
                    auto& buf = stepJsonEdit[&s];
                    if (buf.empty()) buf = p.Dump(0);
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputTextMultiline("##json", &buf, ImVec2(-1, ImGui::GetTextLineHeight() * 3), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        Json j;
                        if (Json::Parse(buf, j) && j.IsObject()) { p = j; changed = true; }
                    }
                    if (changed) { buf = p.Dump(0); SaveActions(); }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            if (removeStep >= 0) { a.steps.erase(a.steps.begin() + removeStep); stepJsonEdit.clear(); SaveActions(); }
            if (moveStep >= 0) { std::swap(a.steps[moveStep], a.steps[moveStep + moveDir]); stepJsonEdit.clear(); SaveActions(); }
            if (ImGui::SmallButton("+ Étape")) ImGui::OpenPopup("##addstep");
            if (ImGui::BeginPopup("##addstep")) {
                for (auto& c : kCmds)
                    if (ImGui::MenuItem(c[1])) {
                        Json p = Json::Object();
                        if (std::string(c[0]) == "image_size") { p["mode"] = "fit"; p["w"] = 1920; p["h"] = 1920; p["interp"] = 2; }
                        if (std::string(c[0]) == "export") { p["format"] = "png"; p["folder"] = ""; p["suffix"] = ""; }
                        if (std::string(c[0]) == "select") p["what"] = "all";
                        if (std::string(c[0]) == "rotate") p["op"] = 0;
                        if (std::string(c[0]) == "op") { p["name"] = "shaderixel.layer_add"; p["args"] = Json::Object(); }
                        a.steps.push_back({c[0], p, true});
                        SaveActions();
                    }
                ImGui::EndPopup();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (ImGui::Button("Traitement par lots...", ImVec2(-1, 0))) batch.open = true;
    ImGui::End();
}

void App::DrawBatchWindow() {
    auto& B = batch;
    if (!B.open) return;
    ImGui::SetNextWindowSize(ImVec2(520 * dpi, 0), ImGuiCond_Appearing);
    if (!ImGui::Begin("Traitement par lots", &B.open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::End(); return; }
    ImGui::BeginDisabled(B.running);
    ImGui::PushItemWidth(340 * dpi);
    if (ImGui::BeginCombo("Action", B.action >= 0 && B.action < (int)actions.size() ? actions[B.action].name.c_str() : "(choisir)")) {
        for (int i = 0; i < (int)actions.size(); ++i) if (ImGui::Selectable(actions[i].name.c_str(), i == B.action)) B.action = i;
        ImGui::EndCombo();
    }
    auto folderField = [&](const char* label, std::string& s, const char* id) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s", s.c_str());
        ImGui::SetNextItemWidth(280 * dpi);
        if (ImGui::InputText(label, buf, sizeof(buf))) s = buf;
        ImGui::SameLine();
        ImGui::PushID(id);
        if (ImGui::Button("...")) {
            // Pick any file of the folder.
            std::string f = platform::OpenFileDialog(hwnd, L"Tous les fichiers\0*.*\0");
            if (!f.empty()) s = DirOf(f);
        }
        ImGui::PopID();
    };
    ImGui::SeparatorText("Source");
    folderField("Dossier source", B.source, "src");
    ImGui::Checkbox("Inclure les sous-dossiers", &B.subfolders);
    ImGui::SeparatorText("Destination");
    folderField("Dossier", B.dest, "dst");
    ImGui::Checkbox("Enregistrer le résultat", &B.saveResult);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Décochez si l'action contient déjà une étape Exporter (elle écrit alors dans ce dossier).");
    ImGui::BeginDisabled(!B.saveResult);
    ImGui::Combo("Format", &B.format, "PNG\0JPG\0SXL\0PSD\0");
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s", B.suffix.c_str());
        if (ImGui::InputText("Suffixe du nom", buf, sizeof(buf))) B.suffix = buf;
    }
    ImGui::EndDisabled();
    ImGui::PopItemWidth();
    ImGui::EndDisabled();
    ImGui::Separator();
    if (B.running) {
        float f = B.files.empty() ? 0 : (float)B.index / B.files.size();
        char ov[64];
        snprintf(ov, sizeof(ov), "%zu / %zu", std::min(B.index + 1, B.files.size()), B.files.size());
        ImGui::ProgressBar(f, ImVec2(-1, 0), ov);
        if (B.index < B.files.size()) ImGui::TextDisabled("%s", B.files[B.index].c_str());
        if (ImGui::Button("Annuler")) B.cancel = true;
    } else {
        ImGui::BeginDisabled(B.source.empty() || B.action < 0 || (B.saveResult && B.dest.empty()));
        if (ImGui::Button("Démarrer", ImVec2(120 * dpi, 0))) StartBatch();
        ImGui::EndDisabled();
        if (B.done || !B.errors.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("Dernier lot : %zu traité(s), %zu erreur(s)", B.done, B.errors.size());
        }
    }
    ImGui::End();
}

}  // namespace sx
