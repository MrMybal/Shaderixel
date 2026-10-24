// File > Export As (like Photoshop's): the image, the selection or every layer on its own, at several scales in
// one go (@2x, @3x...), PNG / JPEG / TIFF / TGA / BMP, transparency or a background color, transparent edges trimmed.
// The images are rendered one per frame (a few readbacks in flight), the document locked meanwhile (modal progress).
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui_stdlib.h>
#include <cmath>
#include <filesystem>
#include <set>

namespace sx {

namespace {
const char* kExportFormats[] = {".png", ".jpg", ".tif", ".tga", ".bmp"};

std::string FileSafe(std::string s) {
    for (auto& c : s)
        if (strchr("\\/:*?\"<>|", c) || (unsigned char)c < 32) c = '_';
    while (!s.empty() && (s.back() == '.' || s.back() == ' ')) s.pop_back();
    if (s.empty()) s = "_";
    // Windows device names (CON, NUL, COM1...) are not file names.
    std::string up;
    for (char c : s.substr(0, s.find('.'))) up += (char)toupper((unsigned char)c);
    static const char* reserved[] = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                                     "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
    for (const char* r : reserved)
        if (up == r) return "_" + s;
    return s;
}
}  // namespace

struct App::ExportAsJob {
    DocView* view = nullptr;
    struct Task {
        Id layer = 0;      // 0: the image as it is
        Rect area;
        float scale = 1;
        std::string path;
    };
    std::vector<Task> tasks;
    size_t next = 0;
    std::vector<std::pair<Id, bool>> visible;  // to restore after a per-layer export
    bool wasDirty = false, perLayer = false, cancel = false;
    Id shownLayer = ~0ull;
    int format = 0, quality = 90;
    bool trim = false, keepAlpha = true;
    float4 matte;
    std::string dir;
};

void App::DrawExportAsWindow() {
    auto& E = exportAs;
    ExportAsTick();
    if (!E.open) return;
    DocView* v = Active();
    if (!v) { E.open = false; return; }
    Document& d = *v->doc;
    // Selection bounds: read again when the document or its selection changed.
    if (E.selFor != v || E.selId != d.S().selection) {
        E.selFor = v;
        E.selId = d.S().selection;
        E.selRect = {};
        E.selRb.reset();
    }
    if (E.selRb && E.selRb->ready) {
        const u32* b = (const u32*)E.selRb->data.data();
        E.selRect = E.selRb->data.size() >= 16 && b[2] > b[0] && b[3] > b[1] ? Rect{(int)b[0], (int)b[1], (int)b[2], (int)b[3]} : Rect{};
        E.selRb.reset();
    }
    ImGui::SetNextWindowSize(ImVec2(440 * dpi, 0), ImGuiCond_Always);
    if (!ImGui::Begin("Exporter sous", &E.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }
    ImGui::SetNextItemWidth(200 * dpi);
    ImGui::Combo("Format", &E.format, "PNG\0JPEG\0TIFF 16 bits\0TGA\0BMP\0");
    if (E.format == 1) {
        ImGui::SetNextItemWidth(200 * dpi);
        ImGui::SliderInt("Qualité", &E.quality, 1, 100);
    }
    const bool alphaFormat = E.format == 0 || E.format == 2 || E.format == 3;
    if (alphaFormat) ImGui::Checkbox("Transparence", &E.transparent);
    if (!alphaFormat || !E.transparent) {
        ImGui::SameLine();
        ImGui::ColorEdit3("Fond", &E.matte.x, ImGuiColorEditFlags_NoInputs);
    }
    ImGui::Checkbox("Rogner les bords transparents", &E.trim);
    ImGui::SetNextItemWidth(200 * dpi);
    ImGui::Combo("Contenu", &E.area, "Image entière\0Sélection (rectangle)\0Chaque calque séparément\0");
    if (E.area == 1) {
        if (!d.S().selection) ImGui::TextDisabled("Aucune sélection : l'image entière.");
        else if (E.selRect.Empty() && !E.selRb) E.selRb = v->renderer.ComputeBounds(*ctx, d.Selection()->tex.get(), nullptr, false, true);
        else if (!E.selRect.Empty()) ImGui::TextDisabled("Sélection : %d x %d px", E.selRect.Width(), E.selRect.Height());
    } else if (E.area == 2) {
        ImGui::TextDisabled("Calques du premier niveau (un groupe : son contenu ; un plan de travail : son cadre).");
    }
    // Scales (with the suffix added to the file names).
    ImGui::SeparatorText("Tailles");
    for (size_t i = 0; i < E.scales.size(); ++i) {
        ImGui::PushID((int)i);
        ImGui::SetNextItemWidth(90 * dpi);
        ImGui::DragFloat("##s", &E.scales[i].first, 0.05f, 0.05f, 8.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120 * dpi);
        ImGui::InputText("Suffixe", &E.scales[i].second);
        ImGui::SameLine();
        const Rect base = E.area == 1 && d.S().selection && !E.selRect.Empty() ? E.selRect : Rect{0, 0, (int)d.Width(), (int)d.Height()};
        ImGui::TextDisabled("%ld x %ld", std::lround(base.Width() * E.scales[i].first), std::lround(base.Height() * E.scales[i].first));
        if (E.scales.size() > 1) {
            ImGui::SameLine();
            if (ImGui::SmallButton("-")) {
                E.scales.erase(E.scales.begin() + i);
                ImGui::PopID();
                break;
            }
        }
        ImGui::PopID();
    }
    if (ImGui::SmallButton("+ Taille")) {
        const float next = E.scales.empty() ? 1.0f : E.scales.back().first + 1.0f;
        char suf[16];
        snprintf(suf, sizeof(suf), "@%gx", next);
        E.scales.push_back({next, suf});
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("1x 2x 3x")) E.scales = {{1, ""}, {2, "@2x"}, {3, "@3x"}};
    ImGui::SameLine();
    if (ImGui::SmallButton("Moitié + entière")) E.scales = {{0.5f, "@0.5x"}, {1, ""}};
    ImGui::Separator();
    ImGui::BeginDisabled(exportAsJob != nullptr);
    if (ImGui::Button("Exporter...", ImVec2(140 * dpi, 0))) {
        const wchar_t* filters[] = {L"PNG\0*.png\0", L"JPEG\0*.jpg\0", L"TIFF\0*.tif\0", L"TGA\0*.tga\0", L"BMP\0*.bmp\0"};
        const wchar_t* exts[] = {L"png", L"jpg", L"tif", L"tga", L"bmp"};
        std::string f = platform::SaveFileDialog(hwnd, filters[E.format], exts[E.format]);
        if (!f.empty()) {
            ExportAsRun(*v, f);
            E.open = false;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Fermer", ImVec2(100 * dpi, 0))) E.open = false;
    ImGui::End();
}

void App::ExportAsRun(DocView& v, const std::string& basePath) {
    auto& E = exportAs;
    if (exportAsJob) return;
    Document& d = *v.doc;
    auto p = Utf8Path(basePath);
    auto J = std::make_shared<ExportAsJob>();
    J->view = &v;
    J->dir = Narrow(p.parent_path().wstring());
    const std::string stem = Narrow(p.stem().wstring());
    const char* ext = kExportFormats[std::clamp(E.format, 0, 4)];
    J->format = E.format;
    J->quality = E.quality;
    J->trim = E.trim;
    J->keepAlpha = E.transparent && (E.format == 0 || E.format == 2 || E.format == 3);
    J->matte = E.matte;
    J->wasDirty = d.dirty;
    const Rect whole = {0, 0, (int)d.Width(), (int)d.Height()};
    const Rect area = E.area == 1 && d.S().selection && !E.selRect.Empty() ? E.selRect : whole;
    std::set<std::string> used;  // (two layers with one name, two scales with one suffix: numbered, never overwritten)
    auto unique = [&](const std::string& name) {
        std::string path = J->dir + "/" + name + ext;
        for (int k = 2; used.count(path); ++k) path = J->dir + "/" + name + " (" + std::to_string(k) + ")" + ext;
        used.insert(path);
        return path;
    };
    if (E.area == 2 && d.Mode() == DocMode::Layers) {
        J->perLayer = true;
        for (auto& l : d.S().layers) J->visible.push_back({l.id, l.visible});
        for (auto& l : d.S().layers) {
            // Top-level layers; a clipped layer goes with its base (exported together).
            if (!l.visible || l.parent != 0 || l.clipped || l.kind == LayerKind::Adjustment) continue;
            const Rect la = l.artboard && !l.board.Empty() ? l.board.Clipped((int)d.Width(), (int)d.Height()) : whole;
            for (auto& [scale, suffix] : E.scales) J->tasks.push_back({l.id, la, scale, unique(stem + "_" + FileSafe(l.name) + suffix)});
        }
    } else {
        for (auto& [scale, suffix] : E.scales) J->tasks.push_back({0, area, scale, unique(stem + suffix)});
    }
    exportAsJob = J;
}

void App::ExportAsTick() {
    if (!exportAsJob) return;
    ExportAsJob& J = *exportAsJob;
    bool alive = false;
    for (auto& dv : docs) alive |= dv.get() == J.view;
    if (!alive) { exportAsJob.reset(); return; }
    DocView& v = *J.view;
    Document& d = *v.doc;
    DrawJobModal("Exporter sous", (float)J.next / std::max<size_t>(1, J.tasks.size()), &J.cancel);
    // One image per frame, a few written files in flight at most (memory).
    if (!J.cancel && J.next < J.tasks.size() && exports.size() < 3) {
        const auto& T = J.tasks[J.next++];
        if (J.perLayer && T.layer != J.shownLayer) {
            // This layer (and the layers clipped to it) alone.
            bool on = false;
            for (auto& l : d.S().layers) {
                if (l.parent != 0) continue;
                if (l.id == T.layer) on = true;
                else if (!l.clipped) on = false;
                bool was = false;
                for (auto& [id, vis] : J.visible) if (id == l.id) was = vis;
                l.visible = on && was;
            }
            d.SyncGraph();
            Prepare(v);
            J.shownLayer = T.layer;
        }
        const u32 w = (u32)std::max(1L, std::lround(T.area.Width() * T.scale)), h = (u32)std::max(1L, std::lround(T.area.Height() * T.scale));
        if (gpu::Texture* img = w <= 16384 && h <= 16384 ? v.renderer.EvaluateComplete(d, *ctx) : nullptr)
            if (gpu::TexturePtr scaled = v.renderer.ScaleTexture(*ctx, img, T.area, w, h)) {
                auto rb = J.format == 2 ? v.renderer.ExportRGBA16(*ctx, scaled.get()) : v.renderer.ExportRGBA8(*ctx, scaled.get());
                if (rb) {
                    PendingExport pe;
                    pe.path = T.path;
                    pe.rb = rb;
                    pe.quality = J.quality;
                    pe.trim = J.trim;
                    pe.matte = J.keepAlpha ? float4{0, 0, 0, -1} : float4{J.matte.x, J.matte.y, J.matte.z, 1};
                    exports.push_back(std::move(pe));
                }
            }
    }
    if (J.cancel || J.next >= J.tasks.size()) {
        if (J.perLayer) {
            for (auto& l : d.S().layers)
                for (auto& [id, vis] : J.visible)
                    if (id == l.id) l.visible = vis;
            d.SyncGraph();
            Prepare(v);
        }
        d.dirty = J.wasDirty;  // (showing the layers one by one is not an edit)
        Status("Exporter sous : " + std::to_string(J.next) + " image(s)" + (J.cancel ? " (interrompu)" : "") + " vers " + J.dir);
        exportAsJob.reset();
    }
}

}  // namespace sx
