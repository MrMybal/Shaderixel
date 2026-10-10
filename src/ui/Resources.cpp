// Resources: patterns (library, Define Pattern, Fill with pattern, Pattern Stamp,
// .pat import), swatches (Nuancier panel, .aco / .ase / .gpl libraries) and
// Photoshop brushes (.abr sampled tips -> textured brushes of the library).
#include "ui/App.h"
#include "ui/Theme.h"
#include "io/AdobeFormats.h"
#include "doc/Brushes.h"
#include "gpu/ShaderSystem.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <filesystem>
#include <fstream>

namespace sx {

namespace {

std::string Slugify(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (isalnum(c)) out.push_back((char)tolower(c));
        else if (!out.empty() && out.back() != '_') out.push_back('_');
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out.empty() ? "item" : out.substr(0, 40);
}

std::string DirOfFile(const std::string& p) { return Narrow(Utf8Path(p).parent_path().wstring()); }

}  // namespace

// ------------------------------------------------------------------ patterns

std::string App::PatternDir() const { return gpu::Shaders().ShaderDir() + "/patterns"; }

void App::EnsureDefaultPatterns() {
    std::error_code ec;
    std::filesystem::create_directories(Utf8Path(PatternDir()), ec);
    bool any = false;
    for (auto& e : std::filesystem::directory_iterator(Utf8Path(PatternDir()), ec)) any |= e.path().extension() == ".png";
    if (any) return;
    // A few procedural seamless patterns.
    auto save = [&](const char* name, int n, auto fn) {
        std::vector<u8> px((size_t)n * n * 4);
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                float4 c = fn(x, y);
                u8* p = &px[((size_t)y * n + x) * 4];
                p[0] = (u8)std::lround(std::clamp(c.x, 0.0f, 1.0f) * 255);
                p[1] = (u8)std::lround(std::clamp(c.y, 0.0f, 1.0f) * 255);
                p[2] = (u8)std::lround(std::clamp(c.z, 0.0f, 1.0f) * 255);
                p[3] = (u8)std::lround(std::clamp(c.w, 0.0f, 1.0f) * 255);
            }
        stbi_write_png((PatternDir() + "/" + name + ".png").c_str(), n, n, 4, px.data(), n * 4);
    };
    save("Damier", 64, [](int x, int y) { float v = ((x / 32 + y / 32) & 1) ? 0.85f : 0.35f; return float4{v, v, v, 1}; });
    save("Rayures", 32, [](int x, int y) { float v = ((x + y) % 32) < 12 ? 0.2f : 0.9f; return float4{v, v * 0.95f, v * 0.8f, 1}; });
    save("Pois", 48, [](int x, int y) {
        float dx = (x % 24) - 11.5f, dy = (y % 24) - 11.5f;
        float a = std::clamp(7.0f - std::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
        return float4{0.95f - a * 0.75f, 0.9f - a * 0.3f, 0.95f - a * 0.2f, 1};
    });
    save("Briques", 128, [](int x, int y) {
        int row = y / 32, xx = (x + (row & 1) * 32) % 64, yy = y % 32;
        bool mortar = yy < 3 || xx < 3;
        float n = (float)((x * 73856093 ^ y * 19349663) % 1000) / 1000.0f;
        return mortar ? float4{0.78f, 0.76f, 0.72f, 1} : float4{0.62f + n * 0.08f, 0.27f + n * 0.05f, 0.2f, 1};
    });
    save("Grille", 64, [](int x, int y) { bool l = x % 32 == 0 || y % 32 == 0; return l ? float4{0.3f, 0.5f, 0.9f, 1} : float4{1, 1, 1, 1}; });
    save("Bruit", 128, [](int x, int y) {
        u32 h = (u32)(x * 374761393 + y * 668265263);
        h = (h ^ (h >> 13)) * 1274126177;
        float v = 0.45f + ((h & 255) / 255.0f) * 0.2f;
        return float4{v, v, v, 1};
    });
}

void App::ScanPatterns() {
    EnsureDefaultPatterns();
    patterns.clear();
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(Utf8Path(PatternDir()), ec)) {
        if (e.path().extension() != ".png") continue;
        PatternEntry p;
        p.path = Narrow(e.path().wstring());
        p.name = Narrow(e.path().stem().wstring());
        patterns.push_back(std::move(p));
    }
    std::sort(patterns.begin(), patterns.end(), [](auto& a, auto& b) { return a.name < b.name; });
    patternsScanned = true;
    patternTiled.reset();
}

bool App::LoadPattern(PatternEntry& p) {
    if (!p.rgba.empty()) return true;
    int c = 0;
    u8* d = stbi_load(p.path.c_str(), &p.w, &p.h, &c, 4);
    if (!d) return false;
    p.rgba.assign(d, d + (size_t)p.w * p.h * 4);
    stbi_image_free(d);
    p.thumb = gpu::Dev().CreateTexture((u32)p.w, (u32)p.h, gpu::Format::RGBA8, "PatternThumb");
    ctx->Upload(p.thumb.get(), p.rgba.data(), (u32)p.w * 4);
    return true;
}

gpu::Texture* App::PatternTiled(DocView& v, int index, float scale) {
    if (!patternsScanned) ScanPatterns();
    if (index < 0 || index >= (int)patterns.size()) return nullptr;
    PatternEntry& p = patterns[index];
    if (!LoadPattern(p)) return nullptr;
    const u32 W = v.doc->Width(), H = v.doc->Height();
    u64 key = ((u64)index << 48) ^ ((u64)W << 24) ^ H ^ (u64)(scale * 1000) * 2654435761ull;
    if (patternTiled && patternTiledKey == key) return patternTiled.get();
    // Document aligned tiling (linear, premultiplied), bilinear for scaled patterns.
    std::vector<float> px((size_t)W * H * 4);
    float lut[256];
    for (int i = 0; i < 256; ++i) lut[i] = SrgbToLinear(i / 255.0f);
    const float inv = 1.0f / std::max(scale, 0.01f);
    for (u32 y = 0; y < H; ++y)
        for (u32 x = 0; x < W; ++x) {
            float sx = (x + 0.5f) * inv - 0.5f, sy = (y + 0.5f) * inv - 0.5f;
            int x0 = (int)std::floor(sx), y0 = (int)std::floor(sy);
            float ax = sx - x0, ay = sy - y0;
            float acc[4] = {0, 0, 0, 0};
            for (int k = 0; k < 4; ++k) {
                int xx = ((x0 + (k & 1)) % p.w + p.w) % p.w, yy = ((y0 + (k >> 1)) % p.h + p.h) % p.h;
                float wgt = ((k & 1) ? ax : 1 - ax) * ((k >> 1) ? ay : 1 - ay);
                const u8* s = &p.rgba[((size_t)yy * p.w + xx) * 4];
                float a = s[3] / 255.0f;
                acc[0] += lut[s[0]] * a * wgt;
                acc[1] += lut[s[1]] * a * wgt;
                acc[2] += lut[s[2]] * a * wgt;
                acc[3] += a * wgt;
            }
            memcpy(&px[((size_t)y * W + x) * 4], acc, 16);
        }
    patternTiled = gpu::Dev().CreateTexture(W, H, gpu::Format::RGBA32F, "PatternTiled");
    ctx->Upload(patternTiled.get(), px.data(), W * 16);
    patternTiledKey = key;
    return patternTiled.get();
}

void App::FillWithPattern(DocView& v, int index, float scale) {
    Document& d = *v.doc;
    Id target = d.PaintTarget();
    if (!d.GetSurface(target)) { Status("Remplir avec un motif : sélectionnez un calque de pixels"); return; }
    gpu::Texture* tiled = PatternTiled(v, index, scale);
    if (!tiled) return;
    gpu::TexturePtr full;
    gpu::Texture* mask = d.Selection() ? d.Selection()->tex.get() : nullptr;
    if (!mask) {
        std::vector<u8> ones((size_t)d.Width() * d.Height(), 255);
        full = gpu::Dev().CreateTexture(d.Width(), d.Height(), gpu::Format::R8, "FullMask");
        ctx->Upload(full.get(), ones.data(), d.Width());
        mask = full.get();
    }
    v.renderer.PasteMasked(d, *ctx, v.history, target, tiled, {0, 0}, mask, "Remplir avec un motif");
}

void App::DefinePattern(DocView& v) {
    // From the selection bounds of the composite (whole image without a selection).
    Document& d = *v.doc;
    gpu::Texture* img = v.renderer.EvaluateComplete(d, *ctx);
    if (!img) return;
    patternDefine.view = &v;
    patternDefine.image = v.renderer.ExportRGBA8(*ctx, img);
    patternDefine.bounds = d.Selection() ? v.renderer.ComputeBounds(*ctx, d.Selection()->tex.get(), nullptr, false, true) : nullptr;
    patternDefine.active = true;
}

void App::UpdateResources() {
    auto& P = patternDefine;
    if (!P.active) return;
    if ((P.image && !P.image->ready) || (P.bounds && !P.bounds->ready)) return;
    P.active = false;
    if (!P.image) return;
    Rect r{0, 0, (int)P.image->width, (int)P.image->height};
    if (P.bounds && P.bounds->data.size() >= 16) {
        const u32* b = (const u32*)P.bounds->data.data();
        if (b[0] < b[2] && b[1] < b[3]) r = {(int)b[0], (int)b[1], (int)b[2], (int)b[3]};
    }
    if (r.Width() > 2048 || r.Height() > 2048) { Status("Définir un motif : zone trop grande (2048 px maximum)"); return; }
    std::vector<u8> px((size_t)r.Width() * r.Height() * 4);
    for (int y = 0; y < r.Height(); ++y)
        memcpy(&px[(size_t)y * r.Width() * 4], &P.image->data[((size_t)(y + r.y0) * P.image->width + r.x0) * 4], (size_t)r.Width() * 4);
    std::filesystem::create_directories(Utf8Path(PatternDir()));
    std::string name = "Motif " + std::to_string(patterns.size() + 1);
    std::string path = PatternDir() + "/" + name + ".png";
    stbi_write_png(path.c_str(), r.Width(), r.Height(), 4, px.data(), r.Width() * 4);
    ScanPatterns();
    for (int i = 0; i < (int)patterns.size(); ++i) if (patterns[i].name == name) patternSel = i;
    Status("Motif défini : " + name + " (" + std::to_string(r.Width()) + " x " + std::to_string(r.Height()) + ")");
}

void App::ImportPatFile(const std::string& path) {
    std::vector<PatternImage> pats;
    std::string err;
    if (!ReadPat(path, pats, &err)) { Status("Motifs : " + err); return; }
    std::filesystem::create_directories(Utf8Path(PatternDir()));
    for (auto& p : pats) {
        std::string name = p.name;
        for (auto& c : name) if (strchr("\\/:*?\"<>|", c)) c = '_';
        stbi_write_png((PatternDir() + "/" + name + ".png").c_str(), p.w, p.h, 4, p.rgba.data(), p.w * 4);
    }
    ScanPatterns();
    Status(std::to_string(pats.size()) + " motif(s) importé(s)");
}

void App::DrawPatternsPanel() {
    if (!ImGui::Begin("Motifs", PanelFlag("Motifs"))) { ImGui::End(); return; }
    PopOutCorner("Motifs");
    if (!patternsScanned) ScanPatterns();
    DocView* v = Active();
    ImGui::SetNextItemWidth(120 * dpi);
    ImGui::SliderFloat("Échelle", &patternScale, 0.1f, 8.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::SameLine();
    ImGui::BeginDisabled(!v);
    if (ImGui::Button("Remplir") && v) FillWithPattern(*v, patternSel, patternScale);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remplit la sélection (ou le calque) avec le motif choisi.");
    ImGui::SameLine();
    if (ImGui::Button("Définir") && v) DefinePattern(*v);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Nouveau motif depuis la sélection (ou l'image entière).");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Importer .pat...")) {
        std::string f = platform::OpenFileDialog(hwnd, L"Motifs Photoshop (*.pat)\0*.pat\0");
        if (!f.empty()) ImportPatFile(f);
    }
    const float cell = 56 * dpi;
    int perRow = std::max(1, (int)(ImGui::GetContentRegionAvail().x / (cell + ImGui::GetStyle().ItemSpacing.x)));
    for (int i = 0; i < (int)patterns.size(); ++i) {
        PatternEntry& p = patterns[i];
        if (!LoadPattern(p)) continue;
        if (i % perRow) ImGui::SameLine();
        ImGui::PushID(i);
        ImVec2 a = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##pat", ImVec2(cell, cell))) {
            patternSel = i;
            if (tool != Tool::PatternStamp && ImGui::GetIO().KeyCtrl) tool = Tool::PatternStamp;
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        // Two repeats so the tiling shows.
        float rep = cell / std::max(p.w, p.h) < 0.5f ? 1.0f : 2.0f;
        dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(p.thumb.get()), a, ImVec2(a.x + cell, a.y + cell), ImVec2(0, 0), ImVec2(rep, rep));
        dl->AddRect(a, ImVec2(a.x + cell, a.y + cell), i == patternSel ? ui::col::Accent : ui::col::Border, 2, 0, i == patternSel ? 2.0f : 1.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (%d x %d)\nCtrl+clic : outil Tampon de motif", p.name.c_str(), p.w, p.h);
        ImGui::PopID();
    }
    ImGui::End();
}

// ------------------------------------------------------------------ swatches

std::string App::SwatchDir() const { return DirOfFile(settingsPath) + "/swatches"; }

void App::LoadSwatchLibrary(const std::string& path) {
    std::vector<Swatch> s;
    std::string err;
    if (!ReadSwatches(path, s, &err)) { Status("Nuancier : " + err); return; }
    swatches.clear();
    for (auto& x : s) swatches.push_back({x.name, x.srgb});
    swatchPath = path;
}

void App::SaveSwatches() {
    if (swatchPath.empty() || Utf8Path(swatchPath).extension() != ".aco") swatchPath = SwatchDir() + "/Mon nuancier.aco";
    std::filesystem::create_directories(Utf8Path(SwatchDir()));
    std::vector<Swatch> s;
    for (auto& x : swatches) s.push_back({x.name, x.srgb});
    WriteAco(swatchPath, s);
}

void App::EnsureSwatches() {
    if (swatchesLoaded) return;
    swatchesLoaded = true;
    std::error_code ec;
    std::filesystem::create_directories(Utf8Path(SwatchDir()), ec);
    std::string def = SwatchDir() + "/Nuancier par défaut.aco";
    if (!std::filesystem::exists(Utf8Path(def))) {
        std::vector<Swatch> s;
        for (int i = 0; i <= 8; ++i) { float g = i / 8.0f; s.push_back({"Gris " + std::to_string(i * 100 / 8) + " %", {g, g, g, 1}}); }
        const char* hues[] = {"Rouge", "Orange", "Jaune", "Citron", "Vert", "Émeraude", "Cyan", "Azur", "Bleu", "Violet", "Magenta", "Rose"};
        for (int k = 0; k < 3; ++k)
            for (int i = 0; i < 12; ++i) {
                float r, g, b;
                ImGui::ColorConvertHSVtoRGB(i / 12.0f, k == 0 ? 0.45f : 1.0f, k == 2 ? 0.55f : 1.0f, r, g, b);
                s.push_back({std::string(hues[i]) + (k == 0 ? " clair" : k == 2 ? " foncé" : ""), {r, g, b, 1}});
            }
        WriteAco(def, s);
    }
    std::string last = swatchPath.empty() ? def : swatchPath;
    LoadSwatchLibrary(std::filesystem::exists(Utf8Path(last)) ? last : def);
}

void App::DrawSwatchesPanel() {
    if (!ImGui::Begin("Nuancier", PanelFlag("Nuancier"))) { ImGui::End(); return; }
    EnsureSwatches();
    std::string cur = swatchPath.empty() ? "(aucun)" : Narrow(Utf8Path(swatchPath).stem().wstring());
    ImGui::SetNextItemWidth(std::max(80 * dpi, ImGui::GetContentRegionAvail().x - 130 * dpi));
    if (ImGui::BeginCombo("##lib", cur.c_str())) {
        std::error_code ec;
        for (auto& e : std::filesystem::directory_iterator(Utf8Path(SwatchDir()), ec)) {
            std::string x = Narrow(e.path().extension().wstring());
            if (x != ".aco" && x != ".ase" && x != ".gpl") continue;
            std::string p = Narrow(e.path().wstring());
            if (ImGui::Selectable(Narrow(e.path().stem().wstring()).c_str(), p == swatchPath)) LoadSwatchLibrary(p);
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("+")) {
        swatches.push_back({"", fg});
        SaveSwatches();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ajouter la couleur de premier plan");
    ImGui::SameLine();
    if (ImGui::Button("Charger...")) {
        std::string f = platform::OpenFileDialog(hwnd, L"Nuanciers (*.aco, *.ase, *.gpl)\0*.aco;*.ase;*.gpl\0");
        if (!f.empty()) {
            // Kept in the swatches folder so it stays in the list.
            std::string dst = SwatchDir() + "/" + Narrow(Utf8Path(f).filename().wstring());
            std::error_code ec;
            std::filesystem::copy_file(Utf8Path(f), Utf8Path(dst), std::filesystem::copy_options::overwrite_existing, ec);
            LoadSwatchLibrary(ec ? f : dst);
        }
    }
    const float s = 20 * dpi;
    int perRow = std::max(1, (int)(ImGui::GetContentRegionAvail().x / (s + 3)));
    int remove = -1;
    for (int i = 0; i < (int)swatches.size(); ++i) {
        auto& sw = swatches[i];
        if (i % perRow) ImGui::SameLine(0, 3);
        ImGui::PushID(i);
        ImVec2 a = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##sw", ImVec2(s, s));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(a, ImVec2(a.x + s, a.y + s), ImGui::ColorConvertFloat4ToU32(ImVec4(sw.srgb.x, sw.srgb.y, sw.srgb.z, 1)), 2);
        dl->AddRect(a, ImVec2(a.x + s, a.y + s), ImGui::IsItemHovered() ? ui::col::Accent : IM_COL32(0, 0, 0, 90), 2);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            if (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyAlt) bg = sw.srgb;
            else fg = sw.srgb;
            PushRecentColor(sw.srgb);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\n#%02X%02X%02X\nClic : premier plan, Ctrl+clic : arrière-plan", sw.name.empty() ? "Couleur" : sw.name.c_str(),
                              (int)std::lround(sw.srgb.x * 255), (int)std::lround(sw.srgb.y * 255), (int)std::lround(sw.srgb.z * 255));
        if (ImGui::BeginPopupContextItem()) {
            ImGui::SetNextItemWidth(160 * dpi);
            if (ImGui::InputText("Nom", &sw.name, ImGuiInputTextFlags_EnterReturnsTrue)) SaveSwatches();
            if (ImGui::MenuItem("Remplacer par le premier plan")) { sw.srgb = fg; SaveSwatches(); }
            if (ImGui::MenuItem("Supprimer")) remove = i;
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    if (remove >= 0) {
        swatches.erase(swatches.begin() + remove);
        SaveSwatches();
    }
    ImGui::End();
}

// ------------------------------------------------------------------ .abr

void App::ImportAbrFile(const std::string& path) {
    std::vector<AbrTip> tips;
    std::string err;
    if (!ReadAbr(path, tips, &err)) { Status("Pinceaux .abr : " + err); return; }
    std::string set = Slugify(Narrow(Utf8Path(path).stem().wstring()));
    std::string category = "ABR : " + Narrow(Utf8Path(path).stem().wstring());
    std::string texDir = Brushes().Dir() + "/textures/abr/" + set;
    std::filesystem::create_directories(Utf8Path(texDir));
    int n = 0;
    for (auto& t : tips) {
        // Square tip texture (centered), white ink with the sample as alpha.
        int side = std::max(t.w, t.h);
        std::vector<u8> px((size_t)side * side * 4, 0);
        int ox = (side - t.w) / 2, oy = (side - t.h) / 2;
        for (int y = 0; y < t.h; ++y)
            for (int x = 0; x < t.w; ++x) {
                u8* p = &px[((size_t)(y + oy) * side + x + ox) * 4];
                p[0] = p[1] = p[2] = 255;
                p[3] = t.gray[(size_t)y * t.w + x];
            }
        ++n;
        std::string texRel = "textures/abr/" + set + "/" + std::to_string(n) + ".png";
        stbi_write_png((Brushes().Dir() + "/" + texRel).c_str(), side, side, 4, px.data(), side * 4);
        char spacing[32];
        snprintf(spacing, sizeof(spacing), "%.2f", std::clamp(t.spacing, 0.01f, 10.0f));
        std::string code = "//@name " + t.name + "\n//@category " + category + "\n//@user\n//@spacing " + spacing +
                           "\n//@texture TipTexture \"Pointe\" " + texRel +
                           "\n//@param RandomAngle float 0 0 1 \"Rotation aléatoire\"\n//@param FollowStroke float 0 0 1 \"Suivre le trait\"\n"
                           "// Imported from a Photoshop brush file (" + Narrow(Utf8Path(path).filename().wstring()) + "): sampled tip.\n"
                           "float4 Tip(Brush b)\n{\n    float m = BrushTex(0, b.uv).a;\n    return float4(b.color, m * b.flow * saturate(b.pressure));\n}\n\n"
                           "void Dynamics(inout Dab d)\n{\n    d.angle = Rand(d.seed, 4.0) * 6.2831853 * RandomAngle + FollowStroke * atan2(d.direction.y, d.direction.x);\n}\n";
        std::ofstream o(Utf8Path(Brushes().Dir() + "/abr_" + set + "_" + std::to_string(n) + ".slang"), std::ios::binary);
        o << code;
    }
    Brushes().Reload();
    Status(std::to_string(n) + " pinceau(x) importé(s) dans la catégorie « " + category + " »");
}

}  // namespace sx
