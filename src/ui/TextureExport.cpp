// Texture export of a Paint 3D project (Substance-like export presets): each preset
// lists its maps and what goes in each of their R, G, B, A (channels, packed masks,
// smoothness, constants). Options: PNG 8 / 16 bits or TGA, size, normal convention
// (DirectX / OpenGL), padding past the UV islands, file name pattern. The channels
// are read back from the GPU, then assembled and written on a worker thread.
#include "ui/App.h"
#include "ui/Filter.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"
#include "io/WicLoad.h"

#include <imgui_stdlib.h>
#include <filesystem>
#include <stb_image_write.h>

namespace sx {

namespace {

struct MapSpec {
    std::string suffix;
    std::string src[4];  // C.r C.g C.b C.a R S(1-R) M AO H N.r N.g N.b E.r E.g E.b 1 0
};
struct ExportPresetDef {
    const char* name;
    const char* desc;
    bool opengl;  // normal map Y convention
    std::vector<MapSpec> maps;
};

const std::vector<ExportPresetDef>& Presets() {
    static const std::vector<ExportPresetDef> p = {
        {"Unreal Engine (ORM)", "BaseColor, Normal (DirectX), ORM (R occlusion, G rugosité, B métal), Emissive.", false,
         {{"BaseColor", {"C.r", "C.g", "C.b", "1"}}, {"Normal", {"N.r", "N.g", "N.b", "1"}}, {"ORM", {"AO", "R", "M", "1"}}, {"Emissive", {"E.r", "E.g", "E.b", "1"}}}},
        {"Unity HDRP", "BaseMap, Normal (OpenGL), MaskMap (R métal, G occlusion, B détail, A lissé).", true,
         {{"BaseMap", {"C.r", "C.g", "C.b", "1"}}, {"Normal", {"N.r", "N.g", "N.b", "1"}}, {"MaskMap", {"M", "AO", "0", "S"}}, {"Emissive", {"E.r", "E.g", "E.b", "1"}}}},
        {"Unity URP / Built-in (métal)", "BaseMap, Normal (OpenGL), MetallicSmoothness (R métal, A lissé), Occlusion.", true,
         {{"BaseMap", {"C.r", "C.g", "C.b", "1"}}, {"Normal", {"N.r", "N.g", "N.b", "1"}}, {"MetallicSmoothness", {"M", "M", "M", "S"}},
          {"Occlusion", {"AO", "AO", "AO", "1"}}, {"Emission", {"E.r", "E.g", "E.b", "1"}}}},
        {"glTF / Blender (ORM)", "BaseColor, Normal (OpenGL), OcclusionRoughnessMetallic (R occlusion, G rugosité, B métal), Emissive.", true,
         {{"BaseColor", {"C.r", "C.g", "C.b", "1"}}, {"Normal", {"N.r", "N.g", "N.b", "1"}}, {"OcclusionRoughnessMetallic", {"AO", "R", "M", "1"}},
          {"Emissive", {"E.r", "E.g", "E.b", "1"}}}},
        {"Godot (ORM)", "Albedo, Normal (OpenGL), ORM (R occlusion, G rugosité, B métal), Emission.", true,
         {{"Albedo", {"C.r", "C.g", "C.b", "1"}}, {"Normal", {"N.r", "N.g", "N.b", "1"}}, {"ORM", {"AO", "R", "M", "1"}}, {"Emission", {"E.r", "E.g", "E.b", "1"}}}},
        {"Textures séparées", "Une image par canal : couleur, rugosité, métal, normale (DirectX), émissif, occlusion, hauteur.", false,
         {{"BaseColor", {"C.r", "C.g", "C.b", "1"}}, {"Roughness", {"R", "R", "R", "1"}}, {"Metallic", {"M", "M", "M", "1"}}, {"Normal", {"N.r", "N.g", "N.b", "1"}},
          {"Emissive", {"E.r", "E.g", "E.b", "1"}}, {"AO", {"AO", "AO", "AO", "1"}}, {"Height", {"H", "H", "H", "1"}}}},
    };
    return p;
}

std::vector<float> ToFloats(const gpu::Readback& rb) {
    const size_t n = (size_t)rb.width * rb.height;
    std::vector<float> out(n * 4, 0.0f);
    const size_t bpp = n ? rb.data.size() / n : 0;
    if (bpp >= 16) memcpy(out.data(), rb.data.data(), n * 16);
    else if (bpp >= 8) {
        const u16* s = (const u16*)rb.data.data();
        for (size_t i = 0; i < n * 4; ++i) out[i] = HalfToFloat(s[i]);
    } else if (bpp >= 4) {
        for (size_t i = 0; i < n * 4; ++i) out[i] = rb.data[i] / 255.0f;
    }
    return out;
}

float Encode(float linear) { return std::clamp(LinearToSrgb(std::max(linear, 0.0f)), 0.0f, 1.0f); }

}  // namespace

int TextureExportPresetCount() { return (int)Presets().size(); }

void App::StartTextureExport(DocView& v) {
    auto& E = texExport;
    Document& d = *v.doc;
    if (!v.mesh || E.folder.empty()) { Status("Export des textures : choisissez un dossier"); return; }
    v.renderer.EvaluateComplete(d, *ctx);
    TexExportJob job;
    job.view = &v;
    job.opt = E;
    for (int c = 0; c < (int)PbrChannel::Count; ++c)
        if (gpu::Texture* t = ChannelTexture(v, (PbrChannel)c)) {
            job.has[c] = true;
            job.ch[c] = ctx->ReadbackTexture(t);
        }
    // Normal with the height baked in (like the other exports).
    gpu::Texture* n = ChannelTexture(v, PbrChannel::Normal);
    gpu::Texture* h = ChannelTexture(v, PbrChannel::Height);
    if (h) {
        job.bakedNormal = v.renderer.BakeNormal(*ctx, n, h, d.Width(), d.Height());
        if (job.bakedNormal) {
            job.ch[(int)PbrChannel::Normal] = ctx->ReadbackTexture(job.bakedNormal.get());
            job.has[(int)PbrChannel::Normal] = true;
        }
    }
    if (E.padding > 0 && v.mesh->posMap) job.pos = ctx->ReadbackTexture(v.mesh->posMap.get());
    job.width = d.Width();
    job.height = d.Height();
    job.name = E.name.empty() ? (d.path.empty() ? std::string("Texture") : Narrow(Utf8Path(d.path).stem().wstring())) : E.name;
    v.renderer.DropNodeCaches(d);
    texExportJobs.push_back(std::move(job));
    Status("Export des textures...");
}

void App::PollTextureExports() {
    for (size_t i = 0; i < texExportJobs.size();) {
        TexExportJob& J = texExportJobs[i];
        if (J.result.valid()) {
            if (J.result.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                Status(J.result.get());
                texExportJobs.erase(texExportJobs.begin() + i);
                continue;
            }
            ++i;
            continue;
        }
        bool ready = !J.pos || J.pos->ready;
        for (auto& rb : J.ch) ready &= !rb || rb->ready;
        if (!ready) { ++i; continue; }
        // Everything on the CPU now: assemble on a worker thread.
        struct Data {
            std::vector<float> ch[(int)PbrChannel::Count];
            bool has[(int)PbrChannel::Count] = {};
            std::vector<u8> covered;
        };
        auto data = std::make_shared<Data>();
        for (int c = 0; c < (int)PbrChannel::Count; ++c)
            if (J.ch[c]) {
                data->ch[c] = ToFloats(*J.ch[c]);
                data->has[c] = J.has[c];
            }
        if (J.pos) {
            std::vector<float> pos = ToFloats(*J.pos);
            data->covered.resize(pos.size() / 4);
            for (size_t k = 0; k < data->covered.size(); ++k) data->covered[k] = pos[k * 4 + 3] > 0 ? 1 : 0;
        }
        J.ch = {};
        J.pos.reset();
        J.bakedNormal.reset();
        const TexExportOptions o = J.opt;
        const int W = (int)J.width, H = (int)J.height;
        const std::string name = J.name;
        J.result = std::async(std::launch::async, [data, o, W, H, name]() -> std::string {
            const ExportPresetDef& P = Presets()[std::clamp(o.preset, 0, (int)Presets().size() - 1)];
            const bool opengl = o.normal < 0 ? P.opengl : o.normal == 1;
            const size_t N = (size_t)W * H;
            auto value = [&](const std::string& s, size_t px) -> float {
                auto chan = [&](PbrChannel c, int k, float def) {
                    const auto& v = data->ch[(int)c];
                    if (!data->has[(int)c] || v.size() < N * 4) return def;
                    const float a = v[px * 4 + 3];
                    const float x = a > 1e-4f ? v[px * 4 + k] / std::min(a, 1.0f) : 0.0f;
                    return a > 1e-4f ? Encode(x) : def;
                };
                if (s == "1") return 1.0f;
                if (s == "0") return 0.0f;
                if (s == "C.r") return chan(PbrChannel::BaseColor, 0, 0.5f);
                if (s == "C.g") return chan(PbrChannel::BaseColor, 1, 0.5f);
                if (s == "C.b") return chan(PbrChannel::BaseColor, 2, 0.5f);
                if (s == "C.a") return data->has[0] && data->ch[0].size() >= N * 4 ? std::clamp(data->ch[0][px * 4 + 3], 0.0f, 1.0f) : 1.0f;
                if (s == "R") return chan(PbrChannel::Roughness, 0, 0.5f);
                if (s == "S") return 1.0f - chan(PbrChannel::Roughness, 0, 0.5f);
                if (s == "M") return chan(PbrChannel::Metallic, 0, 0.0f);
                if (s == "AO") return chan(PbrChannel::Occlusion, 0, 1.0f);
                if (s == "H") return chan(PbrChannel::Height, 0, 0.5f);
                if (s == "N.r") return chan(PbrChannel::Normal, 0, 0.5f);
                if (s == "N.g") { float g = chan(PbrChannel::Normal, 1, 0.5f); return opengl ? 1.0f - g : g; }
                if (s == "N.b") return chan(PbrChannel::Normal, 2, 1.0f);
                if (s == "E.r") return chan(PbrChannel::Emissive, 0, 0.0f);
                if (s == "E.g") return chan(PbrChannel::Emissive, 1, 0.0f);
                if (s == "E.b") return chan(PbrChannel::Emissive, 2, 0.0f);
                return 0.0f;
            };
            static const int sizes[] = {0, 0, 0, 512, 1024, 2048, 4096};
            const int outW = o.size == 0 ? W : o.size == 1 ? W / 2 : o.size == 2 ? W / 4 : sizes[std::clamp(o.size, 3, 6)];
            const int outH = o.size <= 2 ? (o.size == 0 ? H : o.size == 1 ? H / 2 : H / 4) : outW * H / std::max(W, 1);
            std::error_code ec;
            std::filesystem::create_directories(Utf8Path(o.folder), ec);
            int written = 0;
            std::string errors;
            for (const MapSpec& m : P.maps) {
                bool uses = false;
                bool needed = true;
                for (auto& s : m.src) {
                    if (s.rfind("E.", 0) == 0) needed &= data->has[(int)PbrChannel::Emissive];
                    uses |= s != "0" && s != "1";
                }
                // Separate maps: only the channels the project has.
                if (P.maps.size() == 7) {
                    const std::string& s0 = m.src[0];
                    PbrChannel c = s0 == "R" ? PbrChannel::Roughness : s0 == "M" ? PbrChannel::Metallic : s0 == "AO" ? PbrChannel::Occlusion : s0 == "H" ? PbrChannel::Height
                                   : s0[0] == 'N' ? PbrChannel::Normal : s0[0] == 'E' ? PbrChannel::Emissive : PbrChannel::BaseColor;
                    needed &= data->has[(int)c];
                }
                if (!uses || !needed) continue;
                std::vector<float> img(N * 4);
                for (size_t px = 0; px < N; ++px)
                    for (int k = 0; k < 4; ++k) img[px * 4 + k] = value(m.src[k], px);
                // Padding: the colors of the islands pushed outwards.
                if (o.padding > 0 && data->covered.size() == N) {
                    std::vector<u8> filled = data->covered;
                    for (int pass = 0; pass < o.padding; ++pass) {
                        std::vector<u8> nf = filled;
                        bool any = false;
                        for (int y = 0; y < H; ++y)
                            for (int x = 0; x < W; ++x) {
                                const size_t idx = (size_t)y * W + x;
                                if (filled[idx]) continue;
                                float acc[4] = {};
                                int c = 0;
                                for (int dy = -1; dy <= 1; ++dy)
                                    for (int dx = -1; dx <= 1; ++dx) {
                                        const int xx = x + dx, yy = y + dy;
                                        if (xx < 0 || yy < 0 || xx >= W || yy >= H || !filled[(size_t)yy * W + xx]) continue;
                                        for (int k = 0; k < 4; ++k) acc[k] += img[((size_t)yy * W + xx) * 4 + k];
                                        ++c;
                                    }
                                if (!c) continue;
                                for (int k = 0; k < 4; ++k) img[idx * 4 + k] = acc[k] / c;
                                nf[idx] = 1;
                                any = true;
                            }
                        filled.swap(nf);
                        if (!any) break;
                    }
                }
                // Size: area average down, bilinear up.
                std::vector<float> out = img;
                if (outW != W || outH != H) {
                    out.assign((size_t)outW * outH * 4, 0.0f);
                    for (int y = 0; y < outH; ++y)
                        for (int x = 0; x < outW; ++x) {
                            const float x0 = (float)x * W / outW, x1 = (float)(x + 1) * W / outW, y0 = (float)y * H / outH, y1 = (float)(y + 1) * H / outH;
                            float acc[4] = {};
                            int c = 0;
                            for (int sy = (int)y0; sy < std::max((int)std::ceil(y1), (int)y0 + 1) && sy < H; ++sy)
                                for (int sx = (int)x0; sx < std::max((int)std::ceil(x1), (int)x0 + 1) && sx < W; ++sx) {
                                    for (int k = 0; k < 4; ++k) acc[k] += img[((size_t)sy * W + sx) * 4 + k];
                                    ++c;
                                }
                            for (int k = 0; k < 4; ++k) out[((size_t)y * outW + x) * 4 + k] = c ? acc[k] / c : 0;
                        }
                }
                std::string file = o.pattern.empty() ? "{nom}_{carte}" : o.pattern;
                for (size_t at; (at = file.find("{nom}")) != std::string::npos;) file.replace(at, 5, name);
                for (size_t at; (at = file.find("{carte}")) != std::string::npos;) file.replace(at, 7, m.suffix);
                const std::string path = o.folder + "/" + file + (o.depth == 2 ? ".tga" : ".png");
                bool ok = false;
                if (o.depth == 1) {
                    std::vector<u16> px(out.size());
                    for (size_t k = 0; k < out.size(); ++k) px[k] = (u16)std::lround(std::clamp(out[k], 0.0f, 1.0f) * 65535.0f);
                    std::string err;
                    ok = SavePng16(path, px, outW, outH, &err);
                    if (!ok) errors += " " + err;
                } else {
                    std::vector<u8> px(out.size());
                    for (size_t k = 0; k < out.size(); ++k) px[k] = (u8)std::lround(std::clamp(out[k], 0.0f, 1.0f) * 255.0f);
                    ok = o.depth == 2 ? stbi_write_tga(path.c_str(), outW, outH, 4, px.data()) != 0
                                      : stbi_write_png(path.c_str(), outW, outH, 4, px.data(), outW * 4) != 0;
                }
                written += ok ? 1 : 0;
            }
            return "Export des textures (" + std::string(P.name) + ") : " + std::to_string(written) + " image(s) dans " + o.folder + errors;
        });
        ++i;
    }
}

void App::DrawTextureExportDialog() {
    auto& E = texExport;
    if (!E.open) return;
    DocView* v = Active();
    if (!v || v->doc->kind != 1) { E.open = false; return; }
    ImGui::SetNextWindowSize(ImVec2(560 * dpi, 0), ImGuiCond_Appearing);
    if (!ImGui::Begin("Exporter les textures", &E.open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::End(); return; }
    ui::FilterBox("Filtrer (Ctrl+F)...", 320.0f * dpi);
    const auto& P = Presets();
    ImGui::PushItemWidth(320 * dpi);
    if (ImGui::BeginCombo("Modèle", P[std::clamp(E.preset, 0, (int)P.size() - 1)].name)) {
        for (int i = 0; i < (int)P.size(); ++i)
            if (ImGui::Selectable(P[i].name, i == E.preset)) E.preset = i;
        ImGui::EndCombo();
    }
    const ExportPresetDef& pr = P[std::clamp(E.preset, 0, (int)P.size() - 1)];
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 520 * dpi);
    ImGui::TextDisabled("%s", pr.desc);
    ImGui::PopTextWrapPos();
    if (ImGui::BeginTable("##maps", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV, ImVec2(520 * dpi, 0))) {
        static const std::pair<const char*, const char*> names[] = {{"C.r", "couleur R"}, {"C.g", "couleur V"}, {"C.b", "couleur B"}, {"C.a", "couverture"},
                                                                    {"R", "rugosité"}, {"S", "lissé (1 - rugosité)"}, {"M", "métal"}, {"AO", "occlusion"},
                                                                    {"H", "hauteur"}, {"N.r", "normale X"}, {"N.g", "normale Y"}, {"N.b", "normale Z"},
                                                                    {"E.r", "émissif R"}, {"E.g", "émissif V"}, {"E.b", "émissif B"}, {"1", "1"}, {"0", "0"}};
        auto label = [&](const std::string& s) {
            for (auto& [k, n] : names) if (s == k) return n;
            return s.c_str();
        };
        for (const MapSpec& m : pr.maps) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(m.suffix.c_str());
            ImGui::TableNextColumn();
            ImGui::TextDisabled("R %s  G %s  B %s  A %s", label(m.src[0]), label(m.src[1]), label(m.src[2]), label(m.src[3]));
        }
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Options");
    ImGui::Combo("Format", &E.depth, "PNG 8 bits\0PNG 16 bits\0TGA 8 bits\0");
    ImGui::Combo("Taille", &E.size, "Taille du document\0Moitié\0Quart\0" "512\0" "1024\0" "2048\0" "4096\0");
    int nrm = E.normal + 1;
    if (ImGui::Combo("Normale", &nrm, "Selon le modèle\0DirectX (Y-, Unreal)\0OpenGL (Y+, Unity, Blender, Godot)\0")) E.normal = nrm - 1;
    ImGui::SliderInt("Marge (texels)", &E.padding, 0, 64);
    ui::HelpTooltip("Les couleurs des îles UV débordent de ce nombre de texels (pas de joints visibles de loin).");
    ImGui::InputText("Nom des fichiers", &E.pattern);
    ui::HelpTooltip("{nom} : nom de base, {carte} : nom de la carte (BaseColor, ORM...).");
    ImGui::InputTextWithHint("Nom de base", "nom du projet", &E.name);
    ImGui::SetNextItemWidth(260 * dpi);
    ImGui::InputTextWithHint("##folder", "dossier", &E.folder);
    ImGui::SameLine();
    if (ImGui::Button("...")) {
        std::string f = platform::SaveFileDialog(hwnd, L"Dossier des textures (nom au choix)\0*.png\0", L"png");
        if (!f.empty()) E.folder = Narrow(Utf8Path(f).parent_path().wstring());
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Dossier");
    ImGui::PopItemWidth();
    ImGui::Separator();
    ImGui::BeginDisabled(E.folder.empty() || !texExportJobs.empty());
    if (ImGui::Button(texExportJobs.empty() ? "Exporter" : "Export en cours...", ImVec2(160 * dpi, 0))) StartTextureExport(*v);
    ImGui::EndDisabled();
    ImGui::End();
}

}  // namespace sx
