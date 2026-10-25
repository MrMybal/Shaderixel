// File > Automate > Contact Sheet (like Photoshop's Contact Sheet II): the images of a folder laid out as
// thumbnails in a grid, on as many pages as needed (a new document per page), each on its own layer with its file
// name below as a text layer; flattened on request. One image per frame, progress and cancel in the job window.
#include "ui/App.h"
#include "render/Dispatch.h"
#include "io/WicLoad.h"

#include <algorithm>
#include <cmath>
#include <imgui_stdlib.h>

namespace sx {

void App::DrawContactSheetDialog() {
    auto& C = contactSheet;
    if (!C.open) return;
    ImGui::SetNextWindowSize(ImVec2(460 * dpi, 0), ImGuiCond_Always);
    if (!ImGui::Begin("Planche contact", &C.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }
    ImGui::SeparatorText("Images du dossier");
    ImGui::SetNextItemWidth(-90 * dpi);
    ImGui::InputTextWithHint("##folder", "dossier des images", &C.folder);
    ImGui::SameLine();
    if (ImGui::Button("Parcourir...")) {
        std::string f = platform::OpenFileDialog(hwnd, L"Un fichier du dossier\0*.*\0");
        if (!f.empty()) C.folder = Narrow(Utf8Path(f).parent_path().wstring());
    }
    ImGui::Checkbox("Inclure les sous-dossiers", &C.subfolders);
    ImGui::SeparatorText("Page");
    ImGui::SetNextItemWidth(120 * dpi);
    ImGui::InputInt("Largeur (px)", &C.width, 100);
    ImGui::SetNextItemWidth(120 * dpi);
    ImGui::InputInt("Hauteur (px)", &C.height, 100);
    if (ImGui::SmallButton("A4 300 ppp")) { C.width = 2480; C.height = 3508; }
    ImGui::SameLine();
    if (ImGui::SmallButton("A4 paysage")) { C.width = 3508; C.height = 2480; }
    ImGui::SameLine();
    if (ImGui::SmallButton("Écran 1920 x 1080")) { C.width = 1920; C.height = 1080; }
    ImGui::SetNextItemWidth(120 * dpi);
    ImGui::InputInt("Colonnes", &C.cols);
    ImGui::SetNextItemWidth(120 * dpi);
    ImGui::InputInt("Lignes", &C.rows);
    ImGui::SetNextItemWidth(120 * dpi);
    ImGui::InputInt("Marge (px)", &C.margin, 10);
    ImGui::SetNextItemWidth(120 * dpi);
    ImGui::InputInt("Espacement (px)", &C.spacing, 5);
    C.width = std::clamp(C.width, 64, 16384);
    C.height = std::clamp(C.height, 64, 16384);
    C.cols = std::clamp(C.cols, 1, 20);
    C.rows = std::clamp(C.rows, 1, 20);
    C.margin = std::clamp(C.margin, 0, 2000);
    C.spacing = std::clamp(C.spacing, 0, 2000);
    ImGui::Checkbox("Nom du fichier sous chaque image", &C.captions);
    if (C.captions) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90 * dpi);
        ImGui::InputInt("Taille", &C.fontSize, 2);
        C.fontSize = std::clamp(C.fontSize, 6, 400);
    }
    ImGui::Checkbox("Aplatir chaque page", &C.flatten);
    ImGui::BeginDisabled(C.folder.empty() || C.running);
    if (ImGui::Button("Créer", ImVec2(120 * dpi, 0))) StartContactSheet();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Fermer", ImVec2(100 * dpi, 0))) C.open = false;
    ImGui::End();
}

void App::StartContactSheet() {
    auto& C = contactSheet;
    C.files.clear();
    static const char* exts[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".psd", ".hdr", ".exr", ".gif", ".tif", ".tiff", ".webp"};
    auto consider = [&](const std::filesystem::path& p) {
        std::string e = Narrow(p.extension().wstring());
        for (auto& c : e) c = (char)tolower((unsigned char)c);
        bool image = IsRawExtension(e);
        for (auto* x : exts) image |= e == x;
        if (image) C.files.push_back(Narrow(p.wstring()));
    };
    std::error_code ec;
    if (C.subfolders) {
        for (auto& e : std::filesystem::recursive_directory_iterator(Utf8Path(C.folder), ec)) if (e.is_regular_file()) consider(e.path());
    } else {
        for (auto& e : std::filesystem::directory_iterator(Utf8Path(C.folder), ec)) if (e.is_regular_file()) consider(e.path());
    }
    std::sort(C.files.begin(), C.files.end());
    if (C.files.empty()) { Status("Planche contact : aucune image dans " + C.folder); return; }
    C.index = 0;
    C.page = nullptr;
    C.pages = 0;
    C.failed = 0;
    C.cancel = false;
    C.running = true;
    C.open = false;
}

void App::ContactSheetTick() {
    auto& C = contactSheet;
    if (!C.running) return;
    bool cancel = false;
    DrawJobModal("Planche contact", C.files.empty() ? 1.0f : (float)C.index / C.files.size(), &cancel);
    // The page in progress may have been closed meanwhile.
    bool alive = false;
    for (auto& dv : docs) alive |= dv.get() == C.page;
    if (!alive) C.page = nullptr;
    const int perPage = C.cols * C.rows;
    auto finishPage = [&]() {
        if (C.page && C.flatten) MergeVisible(*C.page, true);
        if (C.page) { C.page->history.Clear(); C.page->fitPending = true; }
    };
    if (cancel || C.cancel || C.index >= C.files.size()) {
        finishPage();
        C.running = false;
        Status("Planche contact : " + std::to_string(C.index - C.failed) + " image(s) sur " + std::to_string(C.pages) + " page(s)" +
               (C.failed ? ", " + std::to_string(C.failed) + " illisible(s)" : "") + (cancel || C.cancel ? " (arrêtée)" : ""));
        return;
    }
    const int slot = (int)(C.index % perPage);
    if (slot == 0 || !C.page) {
        finishPage();
        NewDocument((u32)C.width, (u32)C.height, Precision::Half16, DocMode::Layers, 0, "Planche contact " + std::to_string(++C.pages));
        C.page = docs.back().get();
    }
    const std::string file = C.files[C.index++];
    DocView& pv = *C.page;
    Document& pd = *pv.doc;
    u32 iw = 0, ih = 0;
    gpu::TexturePtr img = LoadImageTexture(pv, file, iw, ih);
    loadedSubDoc.reset();  // (only the image is used)
    auto* k = Kernel("resample_persp.slang");
    if (!img || !iw || !ih || !k) { ++C.failed; return; }
    // The cell: its image area above, the caption below.
    const float W = (float)C.width, H = (float)C.height, m = (float)C.margin, s = (float)C.spacing;
    const float cellW = std::max(1.0f, (W - 2 * m - (C.cols - 1) * s) / C.cols), cellH = std::max(1.0f, (H - 2 * m - (C.rows - 1) * s) / C.rows);
    const float capH = C.captions ? C.fontSize * 1.5f : 0;
    const float areaH = std::max(1.0f, cellH - capH);
    const int col = slot % C.cols, row = slot / C.cols;
    const float cx = m + col * (cellW + s), cy = m + row * (cellH + s);
    const float sc = std::min(cellW / iw, areaH / ih);
    // The image and its caption centered together in the cell.
    const float ox = std::round(cx + (cellW - iw * sc) * 0.5f), oy = std::round(cy + (cellH - (ih * sc + capH)) * 0.5f);
    const std::string stem = Narrow(Utf8Path(file).stem().wstring());
    Layer& l = pd.AddPixelLayer(stem, (int)pd.S().layers.size());
    const Id surf = l.surface;
    pd.SyncGraph();
    Prepare(pv);
    Surface* dst = pd.GetSurface(surf);
    if (!dst || !dst->tex) { ++C.failed; return; }
    // Page pixel -> image pixel: (p - offset) / scale; outside the image stays transparent (supersampled when reduced).
    struct PerspRoot { u32 dw, dh, sw, sh; float h0[3]; u32 interp; float h1[3]; u32 pad0; float h2[3]; u32 pad1; };
    gpu::DispatchDesc dd;
    dd.pipeline = k;
    dd.srv[0] = img.get();
    dd.uav[0] = dst->tex.get();
    SetConstants(dd, PerspRoot{(u32)C.width, (u32)C.height, iw, ih, {1 / sc, 0, -ox / sc}, 2, {0, 1 / sc, -oy / sc}, 0, {0, 0, 1}, 0});
    Dispatch2D(*ctx, dd, (u32)C.width, (u32)C.height);
    pd.TouchSurface(surf);
    if (C.captions) {
        // The name, shortened to fit the cell (about half an em per character).
        std::string name = stem;
        const size_t fit = (size_t)std::max(4.0f, cellW / (C.fontSize * 0.55f));
        if (name.size() > fit) name = name.substr(0, fit - 3) + "...";
        TextStyle ts;
        ts.content = name;
        ts.size = (float)C.fontSize;
        ts.color = {0.1f, 0.1f, 0.1f, 1};
        ts.align = 1;
        ts.pos = {std::round(cx + cellW * 0.5f), std::round(oy + ih * sc + C.fontSize * 0.3f)};
        pd.AddTextLayer(ts);
    }
    pd.dirty = true;
}

}  // namespace sx
