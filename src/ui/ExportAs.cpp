// File > Export As (like Photoshop's): the image, the selection or every layer on its own, at several scales in
// one go (@2x, @3x...), PNG / JPEG / TIFF / TGA / BMP, transparency or a background color, transparent edges trimmed.
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui_stdlib.h>
#include <cmath>
#include <filesystem>

namespace sx {

namespace {
const char* kExportFormats[] = {".png", ".jpg", ".tif", ".tga", ".bmp"};

std::string FileSafe(std::string s) {
    for (auto& c : s)
        if (strchr("\\/:*?\"<>|", c)) c = '_';
    return s;
}
}  // namespace

void App::DrawExportAsWindow() {
    auto& E = exportAs;
    if (!E.open) return;
    DocView* v = Active();
    if (!v) { E.open = false; return; }
    Document& d = *v->doc;
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
    } else {
        E.selRect = {};
    }
    // Scales (with the suffix added to the file names).
    ImGui::SeparatorText("Tailles");
    for (size_t i = 0; i < E.scales.size(); ++i) {
        ImGui::PushID((int)i);
        ImGui::SetNextItemWidth(90 * dpi);
        ImGui::DragFloat("##s", &E.scales[i].first, 0.05f, 0.05f, 8.0f, "%.2fx");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120 * dpi);
        ImGui::InputText("Suffixe", &E.scales[i].second);
        ImGui::SameLine();
        const Rect base = E.area == 1 && !E.selRect.Empty() ? E.selRect : Rect{0, 0, (int)d.Width(), (int)d.Height()};
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
    if (ImGui::Button("Exporter...", ImVec2(140 * dpi, 0))) {
        const wchar_t* filters[] = {L"PNG\0*.png\0", L"JPEG\0*.jpg\0", L"TIFF\0*.tif\0", L"TGA\0*.tga\0", L"BMP\0*.bmp\0"};
        const wchar_t* exts[] = {L"png", L"jpg", L"tif", L"tga", L"bmp"};
        std::string f = platform::SaveFileDialog(hwnd, filters[E.format], exts[E.format]);
        if (!f.empty()) {
            ExportAsRun(*v, f);
            E.open = false;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Fermer", ImVec2(100 * dpi, 0))) E.open = false;
    ImGui::End();
}

void App::ExportAsRun(DocView& v, const std::string& basePath) {
    auto& E = exportAs;
    Document& d = *v.doc;
    auto p = Utf8Path(basePath);
    const std::string dir = Narrow(p.parent_path().wstring()), stem = Narrow(p.stem().wstring());
    const char* ext = kExportFormats[std::clamp(E.format, 0, 4)];
    const bool tiff = E.format == 2;
    const bool keepAlpha = E.transparent && (E.format == 0 || E.format == 2 || E.format == 3);
    const Rect area = E.area == 1 && !E.selRect.Empty() ? E.selRect : Rect{0, 0, (int)d.Width(), (int)d.Height()};
    int files = 0;
    // One image (the composite as it is now) at every scale.
    auto exportImage = [&](const std::string& name) {
        gpu::Texture* img = v.renderer.EvaluateComplete(d, *ctx);
        if (!img) return;
        for (auto& [scale, suffix] : E.scales) {
            const u32 w = (u32)std::max(1L, std::lround(area.Width() * scale)), h = (u32)std::max(1L, std::lround(area.Height() * scale));
            if (w > 16384 || h > 16384) continue;
            gpu::TexturePtr scaled = v.renderer.ScaleTexture(*ctx, img, area, w, h);
            if (!scaled) continue;
            auto rb = tiff ? v.renderer.ExportRGBA16(*ctx, scaled.get()) : v.renderer.ExportRGBA8(*ctx, scaled.get());
            if (!rb) continue;
            PendingExport pe;
            pe.path = dir + "/" + name + suffix + ext;
            pe.rb = rb;
            pe.quality = E.quality;
            pe.trim = E.trim;
            pe.matte = keepAlpha ? float4{0, 0, 0, -1} : float4{E.matte.x, E.matte.y, E.matte.z, 1};
            exports.push_back(std::move(pe));
            ++files;
        }
    };
    if (E.area == 2 && d.Mode() == DocMode::Layers) {
        // Every visible layer alone (groups: their content together), the others hidden meanwhile.
        DocState keep = d.S();
        std::vector<Id> targets;
        for (auto& l : d.S().layers)
            if (l.visible && l.parent == 0 && l.kind != LayerKind::Adjustment) targets.push_back(l.id);
        for (Id id : targets) {
            for (auto& l : d.S().layers)
                if (l.parent == 0) l.visible = l.id == id;
            d.SyncGraph();
            Prepare(v);
            const Layer* l = d.FindLayer(id);
            exportImage(stem + "_" + FileSafe(l ? l->name : std::to_string(id)));
        }
        for (auto& l : d.S().layers)
            for (auto& k : keep.layers)
                if (k.id == l.id) l.visible = k.visible;
        d.SyncGraph();
        Prepare(v);
    } else {
        exportImage(stem);
    }
    Status("Exporter sous : " + std::to_string(files) + " fichier(s) dans " + dir);
}

}  // namespace sx
