// Custom HLSL/Slang effects: declarations editor (inputs and parameters, kept as
// //@input and //@param lines of the code), code editor, compilation and saving
// to the effect library. Shared by effect nodes, fill/adjustment layers and filters.
#include "ui/App.h"
#include "ui/Theme.h"
#include "gpu/ShaderSystem.h"

#include <imgui_stdlib.h>

#include <filesystem>
#include <fstream>

namespace sx {

namespace {
// Tokens of a metadata line, quotes kept out ("..." is one token).
std::vector<std::string> Tokens(const std::string& s) {
    std::vector<std::string> t;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && isspace((unsigned char)s[i])) ++i;
        if (i >= s.size()) break;
        if (s[i] == '"') {
            size_t e = s.find('"', i + 1);
            if (e == std::string::npos) e = s.size();
            t.push_back(s.substr(i + 1, e - i - 1));
            i = e + 1;
        } else {
            size_t e = i;
            while (e < s.size() && !isspace((unsigned char)s[e])) ++e;
            t.push_back(s.substr(i, e - i));
            i = e;
        }
    }
    return t;
}

bool IsNum(const std::string& s) {
    char* end = nullptr;
    strtof(s.c_str(), &end);
    return !s.empty() && end && *end == 0;
}

std::string Num(float v) {
    char b[32];
    snprintf(b, sizeof(b), "%g", v);
    return b;
}

// Editable form of //@input and //@param lines.
struct Decl {
    bool input = false;
    std::string id, type = "float", label;
    float v[4] = {0, 0, 0, 1};
    float mn = 0, mx = 1;
    std::string raw;        // types not edited here (enum, curve, lut) keep their line
    bool editable = true;
};

const char* kTypes[] = {"float", "int", "bool", "color", "float2"};

Decl ParseDecl(const std::string& line) {
    Decl d;
    auto t = Tokens(line.substr(3));
    if (t.empty()) return d;
    if (t[0] == "input") {
        d.input = true;
        for (size_t i = 1; i < t.size(); ++i) d.label += (i > 1 ? " " : "") + t[i];
        return d;
    }
    d.id = t.size() > 1 ? t[1] : "p";
    d.type = t.size() > 2 ? t[2] : "float";
    std::vector<float> nums;
    for (size_t i = 3; i < t.size(); ++i) {
        if (IsNum(t[i])) nums.push_back(strtof(t[i].c_str(), nullptr));
        else d.label = t[i];
    }
    auto n = [&](size_t i, float fb) { return i < nums.size() ? nums[i] : fb; };
    if (d.type == "float" || d.type == "int") { d.v[0] = n(0, 0); d.mn = n(1, 0); d.mx = n(2, d.type == "int" ? 100.0f : 1.0f); }
    else if (d.type == "bool") d.v[0] = n(0, 0);
    else if (d.type == "color") for (int k = 0; k < 4; ++k) d.v[k] = n(k, 1);
    else if (d.type == "float2") { d.v[0] = n(0, 0); d.v[1] = n(1, 0); d.mn = n(2, 0); d.mx = n(3, 1); }
    else { d.editable = false; d.raw = line; }
    return d;
}

std::string DeclLine(const Decl& d) {
    if (d.input) return "//@input " + (d.label.empty() ? std::string("Image") : d.label);
    if (!d.editable) return d.raw;
    std::string s = "//@param " + d.id + " " + d.type;
    if (d.type == "float" || d.type == "int") s += " " + Num(d.v[0]) + " " + Num(d.mn) + " " + Num(d.mx);
    else if (d.type == "bool") s += " " + Num(d.v[0] > 0.5f ? 1.0f : 0.0f);
    else if (d.type == "color") s += " " + Num(d.v[0]) + " " + Num(d.v[1]) + " " + Num(d.v[2]) + " " + Num(d.v[3]);
    else if (d.type == "float2") s += " " + Num(d.v[0]) + " " + Num(d.v[1]) + " " + Num(d.mn) + " " + Num(d.mx);
    if (!d.label.empty()) s += " \"" + d.label + "\"";
    return s;
}

// Splits code into its declarations and the rest; Join puts them back after
// the other metadata lines (//@name, //@category...).
void Split(const std::string& code, std::vector<Decl>& decls, std::vector<std::string>& other) {
    size_t pos = 0;
    while (pos <= code.size()) {
        size_t eol = code.find('\n', pos);
        if (eol == std::string::npos) eol = code.size();
        std::string line = code.substr(pos, eol - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("//@input", 0) == 0 || line.rfind("//@param", 0) == 0) decls.push_back(ParseDecl(line));
        else if (eol < code.size() || !line.empty()) other.push_back(line);
        pos = eol + 1;
    }
}

std::string Join(const std::vector<Decl>& decls, const std::vector<std::string>& other) {
    size_t at = 0;
    while (at < other.size() && other[at].rfind("//@", 0) == 0) ++at;
    std::string s;
    for (size_t i = 0; i < at; ++i) s += other[i] + "\n";
    for (auto& d : decls) if (d.input) s += DeclLine(d) + "\n";
    for (auto& d : decls) if (!d.input) s += DeclLine(d) + "\n";
    for (size_t i = at; i < other.size(); ++i) s += other[i] + (i + 1 < other.size() ? "\n" : "");
    return s;
}

std::string Slug(const std::string& name) {
    std::string s;
    for (unsigned char c : name) {
        if (isalnum(c)) s += (char)tolower(c);
        else if (!s.empty() && s.back() != '_') s += '_';
    }
    while (!s.empty() && s.back() == '_') s.pop_back();
    return s.empty() ? "effet" : s;
}
}  // namespace

bool App::CustomCodeUI(DocView& v, EffectInstance& fx, Id errorNode, const char* key) {
    Document& d = *v.doc;
    static std::string editingKey;
    static std::string buffer;
    std::string k = std::string(key) + "#" + std::to_string((uintptr_t)&fx);
    if (editingKey != k) { editingKey = k; buffer = fx.customCode; }
    bool committed = false;
    auto commit = [&](const char* label) {
        if (buffer == fx.customCode) return;
        DocState before = d.S();
        auto old = fx.Def();
        fx.customCode = buffer;
        fx.SyncParams(old);  // values of parameters that still exist are kept
        d.SyncGraph();
        v.history.PushState(label, before, d.S());
        committed = true;
    };

    // Declarations.
    std::vector<Decl> decls;
    std::vector<std::string> other;
    Split(buffer, decls, other);
    bool declChanged = false;
    ImGui::SeparatorText("Entrées et paramètres");
    ImGui::PushID(key);
    int removeAt = -1;
    const float fh = ImGui::GetFrameHeight();
    for (size_t i = 0; i < decls.size(); ++i) {
        Decl& dc = decls[i];
        ImGui::PushID((int)i);
        if (dc.input) {
            ImGui::TextColored(ImVec4(0.45f, 0.8f, 0.95f, 1), "Entrée %d", (int)std::count_if(decls.begin(), decls.begin() + i, [](const Decl& x) { return x.input; }));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - fh - 8);
            if (ImGui::InputText("##in", &dc.label, ImGuiInputTextFlags_EnterReturnsTrue)) declChanged = true;
            if (ImGui::IsItemDeactivatedAfterEdit()) declChanged = true;
        } else if (!dc.editable) {
            ImGui::TextDisabled("%s", dc.raw.c_str());
        } else {
            float w = ImGui::GetContentRegionAvail().x - fh - 8;
            ImGui::SetNextItemWidth(w * 0.3f);
            if (ImGui::InputText("##id", &dc.id, ImGuiInputTextFlags_CharsNoBlank)) {}
            if (ImGui::IsItemDeactivatedAfterEdit()) declChanged = true;
            ui::HelpTooltip("Nom utilisé dans le code (identifiant HLSL)");
            ImGui::SameLine();
            int type = 0;
            for (int t = 0; t < 5; ++t) if (dc.type == kTypes[t]) type = t;
            ImGui::SetNextItemWidth(w * 0.22f);
            if (ImGui::Combo("##type", &type, "float\0int\0bool\0color\0float2\0")) {
                dc.type = kTypes[type];
                if (dc.type == "color") for (float& c : dc.v) c = 1;
                declChanged = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(w * 0.48f - ImGui::GetStyle().ItemSpacing.x * 2);
            ImGui::InputTextWithHint("##label", "Libellé", &dc.label);
            if (ImGui::IsItemDeactivatedAfterEdit()) declChanged = true;
            // Default / range.
            ImGui::SetNextItemWidth(w);
            if (dc.type == "float" || dc.type == "int") {
                float r[3] = {dc.v[0], dc.mn, dc.mx};
                if (ImGui::InputFloat3("##def", r, "%g")) {}
                if (ImGui::IsItemDeactivatedAfterEdit()) { dc.v[0] = r[0]; dc.mn = r[1]; dc.mx = r[2]; declChanged = true; }
                ui::HelpTooltip("Défaut, minimum, maximum");
            } else if (dc.type == "bool") {
                bool b = dc.v[0] > 0.5f;
                if (ImGui::Checkbox("Activé par défaut", &b)) { dc.v[0] = b ? 1.0f : 0.0f; declChanged = true; }
            } else if (dc.type == "color") {
                if (ImGui::ColorEdit4("##col", dc.v, ImGuiColorEditFlags_NoInputs)) {}
                if (ImGui::IsItemDeactivatedAfterEdit()) declChanged = true;
                ImGui::SameLine();
                ImGui::TextDisabled("défaut");
            } else if (dc.type == "float2") {
                float r[4] = {dc.v[0], dc.v[1], dc.mn, dc.mx};
                if (ImGui::InputFloat4("##def2", r, "%g")) {}
                if (ImGui::IsItemDeactivatedAfterEdit()) { dc.v[0] = r[0]; dc.v[1] = r[1]; dc.mn = r[2]; dc.mx = r[3]; declChanged = true; }
                ui::HelpTooltip("Défaut x, y, minimum, maximum");
            }
        }
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - fh);
        if (ui::IconButton("rm", ui::IconTrash, false, fh, "Supprimer")) removeAt = (int)i;
        ImGui::PopID();
    }
    if (removeAt >= 0) { decls.erase(decls.begin() + removeAt); declChanged = true; }
    int inputs = (int)std::count_if(decls.begin(), decls.end(), [](const Decl& x) { return x.input; });
    ImGui::BeginDisabled(inputs >= 4);
    if (ImGui::Button("+ Entrée")) {
        Decl dc;
        dc.input = true;
        dc.label = inputs == 0 ? "Image" : "Entrée " + std::to_string(inputs + 1);
        decls.push_back(dc);
        declChanged = true;
    }
    ImGui::EndDisabled();
    ui::HelpTooltip("Image lue dans le code par Input(i, px). Sans entrée déclarée, l'effet a une entrée « Image ».\n"
                    "Avec plusieurs entrées, le nœud a autant de connecteurs (mode nodal).");
    ImGui::SameLine();
    if (ImGui::Button("+ Paramètre")) {
        Decl dc;
        int n = 1;
        auto taken = [&](const std::string& id) { for (auto& x : decls) if (!x.input && x.id == id) return true; return false; };
        while (taken("param" + std::to_string(n))) ++n;
        dc.id = "param" + std::to_string(n);
        dc.label = "Paramètre " + std::to_string(n);
        dc.v[0] = 0.5f;
        decls.push_back(dc);
        declChanged = true;
    }
    ui::HelpTooltip("Variable utilisable dans le code ; réglable ensuite comme les paramètres des effets de la bibliothèque.");
    if (declChanged) {
        buffer = Join(decls, other);
        commit("Modifier les paramètres");
    }

    // Code.
    ImGui::SeparatorText("Code HLSL / Slang");
    ImGui::PushFont(ui::GetFonts().mono, 0.0f);
    ImGui::InputTextMultiline("##code", &buffer, ImVec2(-1, std::max(220.0f * dpi, ImGui::GetContentRegionAvail().y - 120 * dpi)), ImGuiInputTextFlags_AllowTabInput);
    ImGui::PopFont();
    if (ImGui::Button("Compiler (Ctrl+Entrée)") || (ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::GetIO().KeyCtrl)) commit("Modifier le code");
    ImGui::SameLine();
    if (ImGui::Button("Enregistrer dans la bibliothèque...")) ImGui::OpenPopup("##savefx");
    ui::HelpTooltip("Nouvel élément de la bibliothèque (les éléments de base sont protégés ; vos éléments peuvent être verrouillés).");
    {
        auto sd = fx.Def();
        const bool blend = sd && sd->blend;
        std::string cat = sd && !sd->category.empty() && sd->category != "Personnalisé" ? sd->category : blend ? "Mes fusions" : "Utilisateur";
        if (LibrarySavePopup("##savefx", blend ? LibKind::Blend : LibKind::Effect, sd ? sd->name : "Mon effet", cat, buffer)) fxBrowser.previews.clear();
    }
    // Errors.
    std::string err;
    if (!errorNode)
        for (auto& n : d.S().graph.nodes)
            if (n.kind == NodeKind::Effect && n.fx.customCode == fx.customCode) errorNode = n.id;
    auto def = fx.Def();
    if (errorNode && v.renderer.NodeFailed(errorNode, &err)) {
        ImGui::PushFont(ui::GetFonts().mono, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.45f, 0.45f, 1));
        ImGui::TextWrapped("%s", err.c_str());
        ImGui::PopStyleColor();
        ImGui::PopFont();
    } else if (def && !def->error.empty()) {
        ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1), "%s", def->error.c_str());
    } else if (buffer != fx.customCode) {
        ImGui::TextDisabled("Modifications non compilées");
    }
    ImGui::PopID();
    return committed;
}

// Library effect -> editable code (same instance, parameters kept).
void App::ConvertToCustomCode(DocView& v, EffectInstance& fx) {
    Document& d = *v.doc;
    DocState before = d.S();
    std::string text;
    if (!fx.effectId.empty()) ReadTextFile(gpu::Shaders().ShaderDir() + "/effects/" + fx.effectId + ".slang", text);
    auto old = fx.Def();
    fx.customCode = text.empty() ? kCustomEffectTemplate : text;
    fx.effectId.clear();
    fx.graph.clear();
    fx.SyncParams(old);
    d.SyncGraph();
    v.history.PushState("Convertir en HLSL", before, d.S());
}

}  // namespace sx
