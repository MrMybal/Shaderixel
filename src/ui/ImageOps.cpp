// Image menu: image size, canvas size, rotate/flip, crop tool and crop to selection.
#include "ui/App.h"
#include "ui/Filter.h"
#include "ui/Theme.h"

#include <imgui_internal.h>
#include <cmath>

namespace sx {

void App::ApplyCanvasOp(DocView& v, u32 w, u32 h, const float m[6], int interp, const char* label) {
    if (xf.active && xf.view) TransformCommit(*xf.view);
    if (!v.renderer.ResampleCanvas(*v.doc, *ctx, v.history, w, h, m, interp, label)) {
        Status("Opération impossible (taille max 16384 px)");
        return;
    }
    v.fitPending = true;
    Status(std::string(label) + " : " + std::to_string(w) + " x " + std::to_string(h));
}

void App::RotateCanvas(DocView& v, int op) {
    {
        Json p = Json::Object();
        p["op"] = op;
        RecordAction("rotate", p);
    }
    const float W = (float)v.doc->Width(), H = (float)v.doc->Height();
    // op: 0 = 90° clockwise, 1 = 90° counter-clockwise, 2 = 180°, 3 = flip H, 4 = flip V
    switch (op) {
        case 0: { float m[6] = {0, 1, 0, -1, 0, H}; ApplyCanvasOp(v, (u32)H, (u32)W, m, 0, "Rotation 90° horaire"); break; }
        case 1: { float m[6] = {0, -1, W, 1, 0, 0}; ApplyCanvasOp(v, (u32)H, (u32)W, m, 0, "Rotation 90° antihoraire"); break; }
        case 2: { float m[6] = {-1, 0, W, 0, -1, H}; ApplyCanvasOp(v, (u32)W, (u32)H, m, 0, "Rotation 180°"); break; }
        case 3: { float m[6] = {-1, 0, W, 0, 1, 0}; ApplyCanvasOp(v, (u32)W, (u32)H, m, 0, "Symétrie horizontale"); break; }
        case 4: { float m[6] = {1, 0, 0, 0, -1, H}; ApplyCanvasOp(v, (u32)W, (u32)H, m, 0, "Symétrie verticale"); break; }
    }
}

void App::ApplyCrop(DocView& v, Rect r) {
    if (r.Empty()) return;
    float m[6] = {1, 0, (float)r.x0, 0, 1, (float)r.y0};
    v.doc->S().selection = 0;
    ApplyCanvasOp(v, (u32)r.Width(), (u32)r.Height(), m, 0, "Recadrage");
}

void App::DrawImageMenu() {
    DocView* v = Active();
    if (!ImGui::BeginMenu("Image", v != nullptr)) return;
    ui::MenuFilter();
    Document& d = *v->doc;
    DrawColorModeMenu();
    ImGui::Separator();
    if (ImGui::MenuItem("Taille de l'image...", "Ctrl+Alt+I")) {
        imageDialog = 1;
        dlgW = (int)d.Width();
        dlgH = (int)d.Height();
    }
    if (ImGui::MenuItem("Mise à l'échelle selon le contenu...", nullptr, false, !carveJob)) {
        imageDialog = 3;
        dlgW = (int)d.Width();
        dlgH = (int)d.Height();
    }
    if (ImGui::MenuItem("Taille de la zone de travail...", "Ctrl+Alt+C")) {
        imageDialog = 2;
        dlgW = (int)d.Width();
        dlgH = (int)d.Height();
        dlgAnchor = 4;
    }
    if (ImGui::BeginMenu("Rotation de l'image")) {
        if (ImGui::MenuItem("180°")) RotateCanvas(*v, 2);
        if (ImGui::MenuItem("90° horaire")) RotateCanvas(*v, 0);
        if (ImGui::MenuItem("90° antihoraire")) RotateCanvas(*v, 1);
        ImGui::Separator();
        if (ImGui::MenuItem("Symétrie horizontale de la zone de travail")) RotateCanvas(*v, 3);
        if (ImGui::MenuItem("Symétrie verticale de la zone de travail")) RotateCanvas(*v, 4);
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Recadrer selon la sélection", nullptr, false, d.S().selection != 0)) {
        RecordAction("crop_selection");
        if (Surface* s = d.Selection()) {
            cropBoundsRb = v->renderer.ComputeBounds(*ctx, s->tex.get(), nullptr, false, true);
            cropBoundsView = v;
        }
    }
    if (ImGui::MenuItem("Outil Recadrage", "C")) tool = Tool::Crop;
    ImGui::EndMenu();
}

void App::DrawImageDialogs() {
    DocView* v = Active();
    ContentScaleTick();
    AnimTick();
    DrawContentScaleProgress();
    if (cropBoundsRb && cropBoundsRb->ready) {
        auto rb = cropBoundsRb;
        cropBoundsRb.reset();
        bool alive = false;
        for (auto& dv : docs) if (dv.get() == cropBoundsView) alive = true;
        const u32* b = (const u32*)rb->data.data();
        if (alive && rb->data.size() >= 16 && b[0] < b[2] && b[1] < b[3]) ApplyCrop(*cropBoundsView, {(int)b[0], (int)b[1], (int)b[2], (int)b[3]});
    }
    if (imageDialog && !ImGui::IsPopupOpen("##imgdlg")) ImGui::OpenPopup("##imgdlg");
    if (!ImGui::BeginPopupModal("##imgdlg", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) return;
    if (!v) {
        imageDialog = 0;
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    Document& d = *v->doc;
    const int W = (int)d.Width(), H = (int)d.Height();
    ImGui::TextUnformatted(imageDialog == 1 ? "Taille de l'image" : imageDialog == 3 ? "Mise à l'échelle selon le contenu" : "Taille de la zone de travail");
    ImGui::Separator();
    ImGui::TextDisabled("Actuelle : %d x %d px", W, H);
    ImGui::SetNextItemWidth(160 * dpi);
    if (ImGui::InputInt("Largeur (px)", &dlgW) && dlgKeepRatio && imageDialog == 1) dlgH = (int)std::lround(dlgW * (double)H / W);
    ImGui::SetNextItemWidth(160 * dpi);
    if (ImGui::InputInt("Hauteur (px)", &dlgH) && dlgKeepRatio && imageDialog == 1) dlgW = (int)std::lround(dlgH * (double)W / H);
    dlgW = std::clamp(dlgW, 1, 16384);
    dlgH = std::clamp(dlgH, 1, 16384);
    if (imageDialog == 3) {
        // Seams: the least visible paths of pixels are removed (or doubled), the subjects keep their shape.
        dlgW = std::clamp(dlgW, std::min(W, 8), std::min(W * 2, 16384));
        dlgH = std::clamp(dlgH, std::min(H, 8), std::min(H * 2, 16384));
        float pw = dlgW * 100.0f / W, ph = dlgH * 100.0f / H;
        ImGui::SetNextItemWidth(160 * dpi);
        if (ImGui::SliderFloat("Largeur %", &pw, 10, 200, "%.0f")) dlgW = std::max(8, (int)std::lround(W * pw / 100.0f));
        ImGui::SetNextItemWidth(160 * dpi);
        if (ImGui::SliderFloat("Hauteur %", &ph, 10, 200, "%.0f")) dlgH = std::max(8, (int)std::lround(H * ph / 100.0f));
        ImGui::BeginDisabled(d.S().selection == 0);
        ImGui::SetNextItemWidth(160 * dpi);
        ImGui::Combo("Sélection", &dlgCarveMode, "Ignorer\0Protéger\0Supprimer\0");
        ImGui::EndDisabled();
        ImGui::TextDisabled("Sans sélection, les zones détaillées sont gardées.\nRéduction jusqu'à 10 %%, agrandissement jusqu'au double.");
    } else if (imageDialog == 1) {
        ImGui::Checkbox("Conserver les proportions", &dlgKeepRatio);
        float pct = dlgW * 100.0f / W;
        ImGui::SetNextItemWidth(160 * dpi);
        if (ImGui::DragFloat("Échelle %", &pct, 0.5f, 1, 1000, "%.1f")) {
            dlgW = std::max(1, (int)std::lround(W * pct / 100.0f));
            dlgH = std::max(1, (int)std::lround(H * pct / 100.0f));
        }
        ImGui::SetNextItemWidth(160 * dpi);
        ImGui::Combo("Rééchantillonnage", &dlgInterp, "Au plus proche\0Bilinéaire\0Bicubique\0");
    } else {
        ImGui::TextUnformatted("Ancrage");
        float s = 22 * dpi;
        for (int i = 0; i < 9; ++i) {
            if (i % 3) ImGui::SameLine();
            ImGui::PushID(i);
            bool sel = dlgAnchor == i;
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(sel ? "o" : "", ImVec2(s, s))) dlgAnchor = i;
            if (sel) ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }
    double mb = (double)dlgW * dlgH * (d.GetPrecision() == Precision::Float32 ? 16 : 8) / (1024.0 * 1024.0);
    ImGui::TextDisabled("%.0f Mo par calque", mb);
    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(100 * dpi, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        if (imageDialog == 3) {
            StartContentScale(*v, (u32)dlgW, (u32)dlgH, d.S().selection ? dlgCarveMode : 0);
        } else if (imageDialog == 1) {
            Json p = Json::Object();
            p["mode"] = "pixels"; p["w"] = dlgW; p["h"] = dlgH; p["interp"] = dlgInterp; p["pct"] = dlgW * 100.0f / W;
            RecordAction("image_size", p);
            float m[6] = {(float)W / dlgW, 0, 0, 0, (float)H / dlgH, 0};
            ApplyCanvasOp(*v, (u32)dlgW, (u32)dlgH, m, dlgInterp, "Taille de l'image");
        } else {
            Json p = Json::Object();
            p["w"] = dlgW; p["h"] = dlgH; p["anchor"] = dlgAnchor;
            RecordAction("canvas_size", p);
            int ax = dlgAnchor % 3, ay = dlgAnchor / 3;
            float ox = std::floor(ax * (dlgW - W) * 0.5f), oy = std::floor(ay * (dlgH - H) * 0.5f);
            float m[6] = {1, 0, -ox, 0, 1, -oy};
            ApplyCanvasOp(*v, (u32)dlgW, (u32)dlgH, m, 0, "Taille de la zone de travail");
        }
        imageDialog = 0;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Annuler", ImVec2(100 * dpi, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        imageDialog = 0;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// ------------------------------------------------------------------ crop tool

void App::CropInput(DocView& v, ImVec2 canvasMin, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    float2 dp = Snap(v, ScreenToDoc(v, canvasMin, io.MousePos));
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && cropActive) {
        ApplyCrop(v, CropRect());
        cropActive = false;
        return;
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        cropDragging = true;
        cropActive = true;
        cropStart = cropEnd = dp;
    }
    if (cropDragging) {
        cropEnd = dp;
        if (io.KeyShift) {
            float2 d = cropEnd - cropStart;
            float m = std::max(std::abs(d.x), std::abs(d.y));
            cropEnd = cropStart + float2{d.x < 0 ? -m : m, d.y < 0 ? -m : m};
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            cropDragging = false;
            if (Length(cropEnd - cropStart) < 2) cropActive = false;
        }
    }
}

Rect App::CropRect() const {
    return Rect{(int)std::lround(std::min(cropStart.x, cropEnd.x)), (int)std::lround(std::min(cropStart.y, cropEnd.y)),
                (int)std::lround(std::max(cropStart.x, cropEnd.x)), (int)std::lround(std::max(cropStart.y, cropEnd.y))};
}

void App::CropOverlay(DocView& v, ImVec2 canvasMin, ImVec2 canvasSize, ImDrawList* dl) {
    if (!cropActive || tool != Tool::Crop) return;
    Rect r = CropRect();
    auto S = [&](float x, float y) { return ImVec2(canvasMin.x + v.pan.x + x * v.zoom, canvasMin.y + v.pan.y + y * v.zoom); };
    ImVec2 a = S((float)r.x0, (float)r.y0), b = S((float)r.x1, (float)r.y1);
    ImVec2 c0 = canvasMin, c1(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
    ImU32 shade = IM_COL32(0, 0, 0, 140);
    dl->AddRectFilled(c0, ImVec2(c1.x, a.y), shade);
    dl->AddRectFilled(ImVec2(c0.x, b.y), c1, shade);
    dl->AddRectFilled(ImVec2(c0.x, a.y), ImVec2(a.x, b.y), shade);
    dl->AddRectFilled(ImVec2(b.x, a.y), ImVec2(c1.x, b.y), shade);
    dl->AddRect(a, b, IM_COL32_WHITE, 0, 0, 1.5f);
    for (int i = 1; i < 3; ++i) {  // rule of thirds
        float x = a.x + (b.x - a.x) * i / 3.0f, y = a.y + (b.y - a.y) * i / 3.0f;
        dl->AddLine(ImVec2(x, a.y), ImVec2(x, b.y), IM_COL32(255, 255, 255, 90));
        dl->AddLine(ImVec2(a.x, y), ImVec2(b.x, y), IM_COL32(255, 255, 255, 90));
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%d x %d  (Entrée pour valider)", r.Width(), r.Height());
    dl->AddText(ImVec2(a.x, b.y + 6), IM_COL32_WHITE, buf);
}

}  // namespace sx
