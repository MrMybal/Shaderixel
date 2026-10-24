// Document templates: shipped ones (templates/ next to the exe, library/templates in the
// repository) and the user's (%LOCALAPPDATA%\Shaderixel\Templates). A template is a document
// (.sxl, or .sx3d for Paint 3D) with a .json (name, category, description) and a .png thumbnail.
// "Nouveau document" shows them by category; "Enregistrer comme modèle" adds the current document.
#include "ui/App.h"
#include "ui/Theme.h"

#include <ShlObj.h>
#include <shellapi.h>
#include <imgui_stdlib.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <filesystem>
#include <fstream>

namespace sx {

namespace fs = std::filesystem;

namespace {

std::string UserTemplateDir() {
    std::string out;
    PWSTR p = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) out = Narrow(p) + "\\Shaderixel\\Templates";
    if (p) CoTaskMemFree(p);
    return out;
}

std::vector<std::string> TemplateDirs() {
    return {UserTemplateDir(), ExeDir() + "\\templates", Narrow((fs::path(Utf8Path(ExeDir())).parent_path().parent_path() / L"library" / L"templates").wstring())};
}

std::string SafeName(std::string s) {
    for (auto& c : s)
        if (strchr("\\/:*?\"<>|", c) || (unsigned char)c < 32) c = '_';
    while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
    return s.empty() ? "Modèle" : s;
}

}  // namespace

void App::ScanTemplates() {
    templates.clear();
    const std::string user = UserTemplateDir();
    for (const std::string& dir : TemplateDirs()) {
        std::error_code ec;
        if (dir.empty() || !fs::is_directory(Utf8Path(dir), ec)) continue;
        for (auto& e : fs::directory_iterator(Utf8Path(dir), ec)) {
            const auto ext = e.path().extension();
            if (ext != L".sxl" && ext != L".sx3d") continue;
            TemplateEntry t;
            t.path = Narrow(e.path().wstring());
            t.name = Narrow(e.path().stem().wstring());
            t.user = dir == user;
            t.category = t.user ? "Mes modèles" : "Divers";
            std::string txt;
            Json j;
            if (ReadTextFile(Narrow(fs::path(e.path()).replace_extension(L".json").wstring()), txt) && Json::Parse(txt, j)) {
                t.name = j["name"].S(t.name);
                t.category = j["category"].S(t.category);
                t.desc = j["description"].S("");
                t.w = j["width"].U(0);
                t.h = j["height"].U(0);
            }
            const fs::path thumb = fs::path(e.path()).replace_extension(L".png");
            if (fs::exists(thumb, ec)) t.thumb = Narrow(thumb.wstring());
            bool dup = false;
            for (auto& o : templates) dup |= o.name == t.name && o.category == t.category;
            if (!dup) templates.push_back(std::move(t));
        }
    }
    std::stable_sort(templates.begin(), templates.end(), [](const TemplateEntry& a, const TemplateEntry& b) {
        return a.category != b.category ? a.category < b.category : a.name < b.name;
    });
}

bool App::NewFromTemplate(const TemplateEntry& t) {
    const size_t before = docs.size();
    if (!LoadSxl(t.path) || docs.size() != before + 1) return false;
    // A new, untitled document: saving asks for a place (the template stays as it is).
    Document& d = *docs.back()->doc;
    d.path.clear();
    d.SetName(t.name);
    d.dirty = false;
    Status("Nouveau document depuis le modèle « " + t.name + " »");
    return true;
}

void App::SaveAsTemplate(DocView& v, const std::string& name, const std::string& category, const std::string& desc, const std::string& dirOverride) {
    const std::string dir = dirOverride.empty() ? UserTemplateDir() : dirOverride;
    std::error_code ec;
    fs::create_directories(Utf8Path(dir), ec);
    std::string base = dir + "\\" + SafeName(name), shownName = name;
    if (dirOverride.empty()) {
        // The user's templates are never overwritten: an existing name gets a number ("Nom 2", "Nom 3"...).
        auto used = [&](const std::string& b) {
            for (const char* ext : {".sxl", ".sx3d", ".json", ".png"})
                if (fs::exists(Utf8Path(b + ext), ec)) return true;
            return false;
        };
        for (int k = 2; used(base) && k < 100000; ++k) {
            base = dir + "\\" + SafeName(name) + " " + std::to_string(k);
            shownName = name + " " + std::to_string(k);
        }
    }
    Document& d = *v.doc;
    SaveDocument(v, base + (d.kind == 1 ? ".sx3d" : ".sxl"), true);
    Json j = Json::Object();
    j["name"] = shownName;
    j["category"] = category.empty() ? "Mes modèles" : category;
    j["description"] = desc;
    j["width"] = d.Width();
    j["height"] = d.Height();
    std::ofstream(Utf8Path(base + ".json"), std::ios::binary) << j.Dump(1);
    if (v.lastImage) {
        ThumbJob job{v.renderer.ExportRGBA8(*ctx, v.lastImage), base + ".png", {}};
        for (auto& t : d.S().trimSheet.trims) job.trims.push_back(t.r);
        thumbJobs.push_back(std::move(job));
    }
    templateThumbs.erase(base + ".png");
    Status("Modèle enregistré : " + shownName + " (" + base + ")");
}

void App::UpdateTemplateJobs() {
    for (size_t i = 0; i < thumbJobs.size();) {
        auto& rb = thumbJobs[i].image;
        const std::string path = thumbJobs[i].path;
        if (!rb || !rb->ready) { ++i; continue; }
        // Thumbnail: fitted in 320 px, box-filtered.
        const int W = (int)rb->width, H = (int)rb->height;
        const float k = std::min(1.0f, 320.0f / std::max(W, H));
        const int w = std::max(1, (int)(W * k)), h = std::max(1, (int)(H * k));
        std::vector<u8> out((size_t)w * h * 4);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const int x0 = x * W / w, x1 = std::max(x0 + 1, (x + 1) * W / w), y0 = y * H / h, y1 = std::max(y0 + 1, (y + 1) * H / h);
                u32 acc[4] = {}, n = 0;
                for (int yy = y0; yy < y1; yy += std::max(1, (y1 - y0) / 4))
                    for (int xx = x0; xx < x1; xx += std::max(1, (x1 - x0) / 4)) {
                        const u8* s = &rb->data[((size_t)yy * W + xx) * 4];
                        for (int c = 0; c < 4; ++c) acc[c] += s[c];
                        ++n;
                    }
                for (int c = 0; c < 4; ++c) out[((size_t)y * w + x) * 4 + c] = (u8)(acc[c] / std::max(n, 1u));
            }
        // Trims outlined (cyan strips, orange tiles).
        for (const Rect& r : thumbJobs[i].trims) {
            const int x0 = std::clamp(r.x0 * w / W, 0, w - 1), x1 = std::clamp((r.x1 * w + W - 1) / W - 1, 0, w - 1);
            const int y0 = std::clamp(r.y0 * h / H, 0, h - 1), y1 = std::clamp((r.y1 * h + H - 1) / H - 1, 0, h - 1);
            auto dot = [&](int x, int y) {
                u8* o = &out[((size_t)y * w + x) * 4];
                o[0] = 60; o[1] = 210; o[2] = 240; o[3] = 255;
            };
            for (int x = x0; x <= x1; ++x) { dot(x, y0); dot(x, y1); }
            for (int y = y0; y <= y1; ++y) { dot(x0, y); dot(x1, y); }
        }
        stbi_write_png(path.c_str(), w, h, 4, out.data(), w * 4);
        templateThumbs.erase(path);
        thumbJobs.erase(thumbJobs.begin() + i);
        ScanTemplates();
    }
}

gpu::Texture* App::TemplateThumb(const std::string& path) {
    if (path.empty()) return nullptr;
    auto it = templateThumbs.find(path);
    if (it != templateThumbs.end()) return it->second.get();
    int w = 0, h = 0, comp = 0;
    gpu::TexturePtr tex;
    FILE* f = nullptr;
    if (_wfopen_s(&f, Widen(path).c_str(), L"rb") == 0 && f) {
        if (stbi_uc* px = stbi_load_from_file(f, &w, &h, &comp, 4)) {
            tex = gpu::Dev().CreateTexture((u32)w, (u32)h, gpu::Format::RGBA8, "TemplateThumb");
            ctx->Upload(tex.get(), px, (u32)w * 4);
            stbi_image_free(px);
        }
        fclose(f);
    }
    templateThumbs[path] = tex;
    return tex.get();
}

// ------------------------------------------------------------------ dialogs

void App::DrawNewDocDialog() {
    if (showNewDoc) {
        ImGui::OpenPopup("Nouveau document");
        showNewDoc = false;
        ScanTemplates();
        tplUi.confirmDelete = false;
    }
    ImGui::SetNextWindowSize(ImVec2(880 * dpi, 600 * dpi), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Nouveau document", nullptr, 0)) return;
    if (ImGui::BeginTabBar("##newdoc")) {
        if (ImGui::BeginTabItem("Modèles", nullptr, tplUi.tabRequest == 0 ? ImGuiTabItemFlags_SetSelected : 0)) {
            // Categories + search on the left, thumbnails on the right.
            std::vector<std::string> cats;
            for (auto& t : templates) if (std::find(cats.begin(), cats.end(), t.category) == cats.end()) cats.push_back(t.category);
            const float footer = ImGui::GetFrameHeightWithSpacing() * 2.6f;
            ImGui::BeginChild("##tplcats", ImVec2(190 * dpi, -footer), ImGuiChildFlags_Borders);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##tplsearch", "Rechercher...", &tplUi.search);
            if (ImGui::Selectable("Tous", tplUi.category.empty())) tplUi.category.clear();
            for (auto& c : cats)
                if (ImGui::Selectable(c.c_str(), tplUi.category == c)) tplUi.category = c;
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("##tplgrid", ImVec2(0, -footer), ImGuiChildFlags_Borders);
            const float cw = 170 * dpi, ch = 120 * dpi;
            const int cols = std::max(1, (int)((ImGui::GetContentRegionAvail().x + 8) / (cw + 8)));
            int col = 0;
            auto lower = [](std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; };
            const std::string q = lower(tplUi.search);
            bool create = false;
            for (int i = 0; i < (int)templates.size(); ++i) {
                const TemplateEntry& t = templates[i];
                if (!tplUi.category.empty() && t.category != tplUi.category) continue;
                if (!q.empty() && lower(t.name + " " + t.category + " " + t.desc).find(q) == std::string::npos) continue;
                if (col) ImGui::SameLine(0, 8);
                ImGui::BeginGroup();
                ImGui::PushID(i);
                const ImVec2 p = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##card", tplUi.selected == i, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(cw, ch + ImGui::GetTextLineHeight() * 2.2f))) {
                    tplUi.selected = i;
                    tplUi.confirmDelete = false;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) create = true;
                }
                if (ImGui::IsItemHovered() && !t.desc.empty()) ImGui::SetTooltip("%s", t.desc.c_str());
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(ImVec2(p.x + 4, p.y + 4), ImVec2(p.x + cw - 4, p.y + ch), IM_COL32(28, 28, 34, 255), 4);
                if (gpu::Texture* tex = TemplateThumb(t.thumb)) {
                    const float k = std::min((cw - 12) / tex->Width(), (ch - 12) / tex->Height());
                    const float w = tex->Width() * k, h = tex->Height() * k;
                    const ImVec2 a(p.x + (cw - w) * 0.5f, p.y + 4 + (ch - 4 - h) * 0.5f);
                    // Checkerboard behind (transparent templates).
                    const float cs = 8 * dpi;
                    dl->PushClipRect(a, ImVec2(a.x + w, a.y + h), true);
                    for (float yy = a.y; yy < a.y + h; yy += cs)
                        for (float xx = a.x; xx < a.x + w; xx += cs)
                            dl->AddRectFilled(ImVec2(xx, yy), ImVec2(xx + cs, yy + cs),
                                              ((int)((xx - a.x) / cs) + (int)((yy - a.y) / cs)) % 2 ? IM_COL32(70, 70, 76, 255) : IM_COL32(52, 52, 58, 255));
                    dl->PopClipRect();
                    dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(tex), a, ImVec2(a.x + w, a.y + h));
                } else {
                    dl->AddText(ImVec2(p.x + 12, p.y + ch * 0.45f), ui::col::TextDim, t.w ? (std::to_string(t.w) + " x " + std::to_string(t.h)).c_str() : "?");
                }
                dl->PushClipRect(ImVec2(p.x, p.y), ImVec2(p.x + cw, p.y + ch + ImGui::GetTextLineHeight() * 2.4f), true);
                dl->AddText(ImVec2(p.x + 6, p.y + ch + 2), IM_COL32(230, 230, 236, 255), t.name.c_str());
                std::string sub = t.category + (t.w ? "  " + std::to_string(t.w) + " x " + std::to_string(t.h) : "");
                dl->AddText(ImVec2(p.x + 6, p.y + ch + 2 + ImGui::GetTextLineHeight()), t.user ? ui::col::Accent : ui::col::TextDim, sub.c_str());
                dl->PopClipRect();
                ImGui::PopID();
                ImGui::EndGroup();
                col = (col + 1) % cols;
            }
            if (templates.empty()) ImGui::TextDisabled("Aucun modèle. Fichier > Enregistrer comme modèle pour en ajouter.");
            ImGui::EndChild();
            // Selection: details and actions.
            const TemplateEntry* sel = tplUi.selected >= 0 && tplUi.selected < (int)templates.size() ? &templates[tplUi.selected] : nullptr;
            if (sel) {
                ImGui::Text("%s", sel->name.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("%s%s", sel->category.c_str(), sel->user ? "  (modèle personnel)" : "");
                if (!sel->desc.empty()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("- %s", sel->desc.c_str());
                }
            } else {
                ImGui::TextDisabled("Choisissez un modèle (double-clic pour créer).");
            }
            ImGui::BeginDisabled(!sel);
            if (ImGui::Button("Créer", ImVec2(120 * dpi, 0)) || (create && sel)) {
                if (NewFromTemplate(*sel)) ImGui::CloseCurrentPopup();
                else Status("Modèle illisible : " + sel->path);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!sel || !sel->user);
            if (!tplUi.confirmDelete) {
                if (ImGui::Button("Supprimer le modèle")) tplUi.confirmDelete = true;
            } else if (ImGui::Button("Confirmer la suppression")) {
                // The user's own template: its document, and its description and thumbnail unless another
                // document of the same base name (.sxl beside a .sx3d) still uses them.
                std::error_code ec;
                const fs::path doc = Utf8Path(sel->path);
                fs::remove(doc, ec);
                const bool sibling = fs::exists(fs::path(doc).replace_extension(doc.extension() == L".sxl" ? L".sx3d" : L".sxl"), ec);
                if (!sibling)
                    for (const wchar_t* ext : {L".json", L".png"}) fs::remove(fs::path(doc).replace_extension(ext), ec);
                templateThumbs.erase(sel->thumb);
                tplUi.selected = -1;
                tplUi.confirmDelete = false;
                ScanTemplates();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Ouvrir le dossier de mes modèles")) {
                std::error_code ec;
                fs::create_directories(Utf8Path(UserTemplateDir()), ec);
                ShellExecuteW(nullptr, L"open", Widen(UserTemplateDir()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            ImGui::SameLine();
            if (ImGui::Button("Annuler")) ImGui::CloseCurrentPopup();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Document vide", nullptr, tplUi.tabRequest == 1 ? ImGuiTabItemFlags_SetSelected : 0)) {
            struct Preset { const char* name; int w, h; };
            static const Preset presets[] = {{"HD 1920 x 1080", 1920, 1080}, {"4K UHD 3840 x 2160", 3840, 2160}, {"Carré 2048", 2048, 2048},
                                             {"Texture 1024", 1024, 1024}, {"Texture 4096", 4096, 4096}, {"Texture 8192", 8192, 8192},
                                             {"Texture 16384", 16384, 16384}, {"A4 300 dpi", 2480, 3508}, {"Instagram 1080", 1080, 1080}};
            ImGui::PushItemWidth(320 * dpi);
            ImGui::InputText("Nom", newName, sizeof(newName));
            if (ImGui::BeginCombo("Préréglage", "Choisir...")) {
                for (auto& p : presets)
                    if (ImGui::Selectable(p.name)) { newW = p.w; newH = p.h; }
                ImGui::EndCombo();
            }
            ImGui::InputInt("Largeur", &newW);
            ImGui::InputInt("Hauteur", &newH);
            newW = std::clamp(newW, 1, 16384);
            newH = std::clamp(newH, 1, 16384);
            ImGui::Combo("Précision", &newPrecision, "16 bits flottant\0" "32 bits flottant\0");
            ImGui::Combo("Mode", &newMode, "Calques (classique)\0" "Nodal (avancé)\0");
            ImGui::Combo("Arrière-plan", &newBackground, "Blanc\0" "Transparent\0" "Noir\0" "Aucun\0");
            ImGui::PopItemWidth();
            double mb = (double)newW * newH * (newPrecision ? 16 : 8) / (1024.0 * 1024.0);
            ImGui::TextDisabled("Mémoire par calque : %.0f Mo", mb);
            ImGui::Spacing();
            if (ImGui::Button("Créer", ImVec2(120 * dpi, 0))) {
                NewDocument((u32)newW, (u32)newH, newPrecision ? Precision::Float32 : Precision::Half16, newMode ? DocMode::Nodes : DocMode::Layers, newBackground, newName);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Annuler", ImVec2(120 * dpi, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndTabItem();
        }
        tplUi.tabRequest = -1;
        ImGui::EndTabBar();
    }
    ImGui::EndPopup();
}

void App::DrawSaveTemplateDialog() {
    if (tplUi.saveRequest) {
        ImGui::OpenPopup("Enregistrer comme modèle");
        tplUi.saveRequest = false;
        if (DocView* v = Active()) {
            std::string n = v->doc->Name();
            const size_t dot = n.rfind('.');
            if (dot != std::string::npos) n = n.substr(0, dot);
            tplUi.saveName = n;
        }
        ScanTemplates();
    }
    ImGui::SetNextWindowSize(ImVec2(520 * dpi, 0));
    if (!ImGui::BeginPopupModal("Enregistrer comme modèle", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    DocView* v = Active();
    ImGui::PushItemWidth(340 * dpi);
    ImGui::InputText("Nom", &tplUi.saveName);
    ImGui::InputText("Catégorie", &tplUi.saveCategory);
    ImGui::SameLine();
    if (ImGui::BeginCombo("##cats", "", ImGuiComboFlags_NoPreview)) {
        std::vector<std::string> cats = {"Mes modèles"};
        for (auto& t : templates) if (std::find(cats.begin(), cats.end(), t.category) == cats.end()) cats.push_back(t.category);
        for (auto& c : cats)
            if (ImGui::Selectable(c.c_str())) tplUi.saveCategory = c;
        ImGui::EndCombo();
    }
    ImGui::InputTextMultiline("Description", &tplUi.saveDesc, ImVec2(340 * dpi, 60 * dpi));
    ImGui::PopItemWidth();
    ImGui::TextDisabled("Le document entier est gardé (calques, graphe, trims, repères...) ; il servira de départ, l'original n'est pas lié.");
    ImGui::BeginDisabled(!v || tplUi.saveName.empty());
    if (ImGui::Button("Enregistrer", ImVec2(140 * dpi, 0))) {
        SaveAsTemplate(*v, tplUi.saveName, tplUi.saveCategory, tplUi.saveDesc);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Annuler", ImVec2(120 * dpi, 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

}  // namespace sx
