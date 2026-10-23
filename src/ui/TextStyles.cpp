// Character and paragraph styles panel (Styles de texte): named styles stored in
// the document, applied to the selected characters / paragraphs of the active
// text layer; editing a style updates every text that uses it.
#include "ui/App.h"
#include "ui/Filter.h"
#include "ui/Theme.h"

#include <imgui_stdlib.h>

namespace sx {

namespace {

// Paragraph indices covered by the codepoint range [a, b) (all paragraphs when empty).
std::vector<int> ParagraphsOf(const std::string& content, int a, int b) {
    std::vector<int> out;
    int para = 0, idx = 0;
    std::vector<std::pair<int, int>> spans(1, {0, 0});
    for (size_t i = 0; i < content.size(); ++i) {
        if (((unsigned char)content[i] & 0xC0) == 0x80) continue;
        if (content[i] == '\n') {
            spans.back().second = idx;
            spans.push_back({idx + 1, idx + 1});
            ++para;
        }
        ++idx;
    }
    spans.back().second = idx;
    for (int p = 0; p < (int)spans.size(); ++p) {
        bool all = a >= b;
        // b is exclusive: a selection ending right after a newline does not reach the next paragraph.
        if (all || (spans[p].second >= a && spans[p].first < b)) out.push_back(p);
    }
    return out;
}

}  // namespace

void App::DrawTextStylesPanel() {
    if (!ImGui::Begin("Styles de texte", PanelFlag("Styles de texte"))) { ImGui::End(); return; }
    ui::FilterBox();
    DocView* v = Active();
    if (!v || v->doc->Mode() != DocMode::Layers) {
        ImGui::TextDisabled(v ? "Disponible en mode calques" : "Aucun document");
        ImGui::End();
        return;
    }
    Document& d = *v->doc;
    Layer* tl = d.ActiveLayer();
    if (tl && tl->kind != LayerKind::Text) tl = nullptr;
    const bool hasSel = tl && textSelLayer == tl->id && textSelEnd > textSelStart;
    if (tl) ImGui::TextDisabled(hasSel ? "Sélection : caractères %d à %d" : "Calque de texte actif (tout le texte)", textSelStart, textSelEnd);
    else ImGui::TextDisabled("Sélectionnez un calque de texte pour appliquer un style.");

    // ---------------- character styles
    ImGui::SeparatorText("Styles de caractère");
    auto& cs = d.S().charStyles;
    if (ImGui::Button("Nouveau##c")) {
        Edit e(d, v->history, "Nouveau style de caractère");
        CharStyle c;
        c.id = d.NewId();
        c.name = "Style de caractère " + std::to_string(cs.size() + 1);
        if (tl) {
            c.font = tl->text.font;
            c.size = tl->text.size;
            c.color = tl->text.color;
            c.tracking = tl->text.tracking;
            c.faux = tl->text.faux;
        }
        cs.push_back(c);
        charStyleSel = c.id;
        e.Commit();
    }
    ImGui::SameLine();
    CharStyle* cur = nullptr;
    for (auto& c : cs) if (c.id == charStyleSel) cur = &c;
    ImGui::BeginDisabled(!cur || !tl);
    if (ImGui::Button(hasSel ? "Appliquer à la sélection" : "Appliquer au texte") && cur && tl) {
        Edit e(d, v->history, "Style de caractère");
        int a = hasSel ? textSelStart : 0, b = hasSel ? textSelEnd : Utf8Length(tl->text.content);
        tl->text.runs.push_back({a, b, cur->id});
        e.Commit();
    }
    ImGui::SameLine();
    if (ImGui::Button("Retirer") && tl) {
        // Remove styles from the selection (whole text without one): runs are cut around it.
        Edit e(d, v->history, "Retirer le style de caractère");
        int a = hasSel ? textSelStart : 0, b = hasSel ? textSelEnd : 1 << 30;
        std::vector<TextRun> out;
        for (auto& r : tl->text.runs) {
            if (r.end <= a || r.start >= b) { out.push_back(r); continue; }
            if (r.start < a) out.push_back({r.start, a, r.style});
            if (r.end > b) out.push_back({b, r.end, r.style});
        }
        tl->text.runs = out;
        e.Commit();
    }
    ImGui::EndDisabled();
    for (auto& c : cs) {
        ImGui::PushID((int)c.id);
        if (ImGui::Selectable(c.name.c_str(), c.id == charStyleSel)) charStyleSel = c.id;
        ImGui::PopID();
    }
    if (cur) {
        ImGui::PushID("cedit");
        ImGui::InputText("Nom", &cur->name);
        TrackUndo("Renommer le style");
        std::string fname = cur->font.empty() ? "(police du calque)" : Narrow(Utf8Path(cur->font).stem().wstring());
        if (ImGui::BeginCombo("Police", fname.c_str(), ImGuiComboFlags_HeightLarge)) {
            if (ImGui::Selectable("(police du calque)", cur->font.empty())) cur->font.clear();
            ImGui::InputTextWithHint("##ff", "Filtrer...", fontFilter, sizeof(fontFilter));
            for (auto& f : SystemFonts()) {
                if (fontFilter[0] && f.name.find(fontFilter) == std::string::npos) continue;
                if (ImGui::Selectable(f.name.c_str(), f.path == cur->font)) cur->font = f.path;
            }
            ImGui::EndCombo();
        }
        ImGui::SliderFloat("Taille", &cur->size, 0, 1000, cur->size <= 0 ? "(calque)" : "%.0f px", ImGuiSliderFlags_Logarithmic);
        TrackUndo("Taille du style");
        ImGui::Checkbox("##usecol", &cur->useColor);
        ImGui::SameLine();
        ImGui::ColorEdit4("Couleur", &cur->color.x, ImGuiColorEditFlags_AlphaBar);
        TrackUndo("Couleur du style");
        ImGui::SliderFloat("Approche", &cur->tracking, -200, 800, "%.0f");
        TrackUndo("Approche du style");
        ImGui::SliderFloat("Décalage de ligne de base", &cur->baseline, -200, 200, "%.0f px");
        TrackUndo("Ligne de base");
        ImGui::Checkbox("Faux gras", &cur->faux);
        ImGui::SameLine();
        ImGui::Checkbox("Souligné", &cur->underline);
        ImGui::SameLine();
        ImGui::Checkbox("Capitales", &cur->allCaps);
        TrackUndo("Attributs du style");
        if (ImGui::Button("Supprimer le style##c")) {
            Edit e(d, v->history, "Supprimer le style de caractère");
            Id id = cur->id;
            for (auto& l : d.S().layers) std::erase_if(l.text.runs, [&](const TextRun& r) { return r.style == id; });
            for (auto& p : d.S().paraStyles) if (p.charStyle == id) p.charStyle = 0;
            std::erase_if(cs, [&](const CharStyle& c) { return c.id == id; });
            charStyleSel = 0;
            e.Commit();
        }
        ImGui::PopID();
    }

    // ---------------- paragraph styles
    ImGui::SeparatorText("Styles de paragraphe");
    auto& ps = d.S().paraStyles;
    if (ImGui::Button("Nouveau##p")) {
        Edit e(d, v->history, "Nouveau style de paragraphe");
        ParaStyle p;
        p.id = d.NewId();
        p.name = "Style de paragraphe " + std::to_string(ps.size() + 1);
        if (tl) {
            p.align = tl->text.align;
            p.lineSpacing = tl->text.lineSpacing;
        }
        ps.push_back(p);
        paraStyleSel = p.id;
        e.Commit();
    }
    ImGui::SameLine();
    ParaStyle* pcur = nullptr;
    for (auto& p : ps) if (p.id == paraStyleSel) pcur = &p;
    ImGui::BeginDisabled(!pcur || !tl);
    if (ImGui::Button(hasSel ? "Appliquer aux paragraphes" : "Appliquer à tous") && pcur && tl) {
        Edit e(d, v->history, "Style de paragraphe");
        for (int p : ParagraphsOf(tl->text.content, hasSel ? textSelStart : 0, hasSel ? textSelEnd : 0)) {
            if ((int)tl->text.paraStyles.size() <= p) tl->text.paraStyles.resize(p + 1, 0);
            tl->text.paraStyles[p] = pcur->id;
        }
        e.Commit();
    }
    ImGui::SameLine();
    if (ImGui::Button("Retirer##p") && tl) {
        Edit e(d, v->history, "Retirer le style de paragraphe");
        for (int p : ParagraphsOf(tl->text.content, hasSel ? textSelStart : 0, hasSel ? textSelEnd : 0))
            if (p < (int)tl->text.paraStyles.size()) tl->text.paraStyles[p] = 0;
        e.Commit();
    }
    ImGui::EndDisabled();
    for (auto& p : ps) {
        ImGui::PushID((int)p.id);
        if (ImGui::Selectable(p.name.c_str(), p.id == paraStyleSel)) paraStyleSel = p.id;
        ImGui::PopID();
    }
    if (pcur) {
        ImGui::PushID("pedit");
        ImGui::InputText("Nom", &pcur->name);
        TrackUndo("Renommer le style");
        ImGui::Combo("Alignement", &pcur->align, "Gauche\0Centre\0Droite\0");
        TrackUndo("Alignement du style");
        ImGui::SliderFloat("Interligne", &pcur->lineSpacing, 0.5f, 3.0f, "%.2f");
        TrackUndo("Interligne du style");
        ImGui::SliderFloat("Espace avant", &pcur->spaceBefore, 0, 500, "%.0f px");
        TrackUndo("Espace avant");
        ImGui::SliderFloat("Retrait gauche", &pcur->indentLeft, 0, 1000, "%.0f px");
        TrackUndo("Retrait gauche");
        ImGui::SliderFloat("Retrait 1re ligne", &pcur->indentFirst, -500, 1000, "%.0f px");
        TrackUndo("Retrait de première ligne");
        const char* cname = "(aucun)";
        for (auto& c : cs) if (c.id == pcur->charStyle) cname = c.name.c_str();
        if (ImGui::BeginCombo("Style de caractère", cname)) {
            if (ImGui::Selectable("(aucun)", pcur->charStyle == 0)) pcur->charStyle = 0;
            for (auto& c : cs) if (ImGui::Selectable(c.name.c_str(), c.id == pcur->charStyle)) pcur->charStyle = c.id;
            ImGui::EndCombo();
        }
        if (ImGui::Button("Supprimer le style##p")) {
            Edit e(d, v->history, "Supprimer le style de paragraphe");
            Id id = pcur->id;
            for (auto& l : d.S().layers) for (auto& p : l.text.paraStyles) if (p == id) p = 0;
            std::erase_if(ps, [&](const ParaStyle& p) { return p.id == id; });
            paraStyleSel = 0;
            e.Commit();
        }
        ImGui::PopID();
    }
    ImGui::End();
}

}  // namespace sx
