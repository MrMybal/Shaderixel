#include "ui/App.h"
#include "ui/Search.h"
#include "io/ColorMgmt.h"
#include "io/WicLoad.h"
#include "doc/HlslImport.h"
#include "doc/Brushes.h"
#include "ui/Theme.h"
#include "gpu/ShaderSystem.h"
#include "io/PsdImport.h"

#include <shellapi.h>
#include <imgui_internal.h>
#include <imgui_impl_win32.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace sx {

std::string ExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return Narrow(fs::path(buf).parent_path().wstring());
}

namespace {
double Now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

std::string ShaderDir() {
    // Prefer shaders next to the exe (distribution), else the source tree (development).
    std::string local = ExeDir() + "/shaders";
    if (fs::exists(Utf8Path(local + "/common.slang"))) return local;
    return SX_SHADER_DIR;
}

const wchar_t* kImageFilter =
    L"Documents et images\0*.sxl;*.sx3d;*.uasset;*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.psd;*.psb;*.hdr;*.gif;*.tif;*.tiff;*.webp;*.heic;*.dng;*.cr2;*.cr3;*.nef;*.arw;*.orf;*.rw2;*.raf;*.pef;*.srw\0"
    L"Documents Shaderixel (*.sxl, *.sx3d)\0*.sxl;*.sx3d\0Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.psd;*.psb;*.hdr;*.gif;*.tif;*.tiff;*.webp;*.heic\0"
    L"Unreal Engine (*.uasset : textures, maillages)\0*.uasset\0Camera RAW\0*.dng;*.cr2;*.cr3;*.crw;*.nef;*.nrw;*.arw;*.srf;*.sr2;*.orf;*.rw2;*.raf;*.pef;*.srw;*.x3f;*.3fr;*.fff;*.erf;*.kdc;*.dcr;*.mrw;*.mos;*.mef;*.iiq;*.rwl;*.raw;*.gpr;*.cap\0Tous les fichiers\0*.*\0";
}  // namespace

// ------------------------------------------------------------------ lifecycle

int App::Run() {
    if (!Init()) return 1;
    while (true) {
        MSG msg;
        bool quit = false;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) quit = true;
        }
        if (platform::IsQuitRequested()) {
            platform::ClearQuitRequest();
            quitting = true;
        }
        if (quitting) {
            // Scripted runs never ask (a bake finishing after the capture marks the document modified again).
            if (Automated())
                for (auto& dv : docs) dv->doc->dirty = false;
            // Ask about each unsaved document, then leave once all are handled and saves finished.
            int dirtyIdx = -1;
            for (int i = 0; i < (int)docs.size(); ++i) if (docs[i]->doc->dirty) { dirtyIdx = i; break; }
            if (dirtyIdx >= 0 && closePrompt == -1) { activeDoc = dirtyIdx; focusDocRequest = dirtyIdx; closePrompt = dirtyIdx; }
            if (dirtyIdx < 0 && saves.empty()) quit = true;
        }
        if (quit) break;
        if (IsIconic(hwnd)) { Sleep(16); continue; }
        Frame();
    }
    Shutdown();
    return 0;
}

void App::ParseCommandLine() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i) {
        std::string a = Narrow(argv[i]);
        if (a == "--test-effects") testEffects = true;
        else if (a == "--ai-test") aiTest = true;
        else if (a == "--canvas") meshDebugCanvas = true;
        else if (a == "--demo") {
            std::string m = i + 1 < argc ? Narrow(argv[i + 1]) : "";
            demoMode = m == "nodes" ? 2 : m == "select" ? 3 : m == "transform" ? 4 : m == "transform-undo" ? 5 : m == "retouch" ? 6 : m == "groups" ? 7 : m == "save" ? 8 : m == "image" ? 9 : m == "curves" ? 10 : m == "effectgraph" ? 11 : m == "text" ? 12 : m == "shapes" ? 13 : m == "mesh" ? 14 : m == "tiles" ? 15 : m == "16k" ? 16 : m == "psd" ? 17 : m == "blendif" ? 18 : m == "liquify" ? 19 : m == "lut" ? 20 : m == "selmove" ? 21 : m == "quicksel" ? 22 : m == "pen" ? 23 : m == "merge" ? 24 : m == "guides" ? 25 : m == "stroke" ? 26 : m == "gltf" ? 27 : m == "pbr" ? 28 : m == "hdri" ? 29 : m == "genfill" ? 30 : m == "psd16" ? 31 : m == "selpath" ? 32 : m == "warp" ? 33 : m == "comfy" ? 34 : m == "sliders" ? 35 : m == "fxbrowser" ? 36 : m == "codefx" ? 37 : m == "chanblend" ? 38 : m == "chanlinks" ? 39 : m == "cygallery" ? 40 : m == "brushes" ? 41 : m == "workspace" ? 42 : m == "graphs" ? 43 : m == "dnd" ? 44 : m == "material" ? 45 : m == "custompins" ? 46 : m == "hlslimport" ? 47 : m == "tablet" ? 48 : m == "smart" ? 49 : m == "selectmask" ? 50 : m == "maglasso" ? 51 : m == "sky" ? 52 : m == "fill" ? 53 : m == "meshwarp" ? 54 : m == "actions" ? 55 : m == "artboards" ? 56 : m == "color" ? 57 : m == "photo" ? 58 : m == "textstyles" ? 59 : m == "resources" ? 60 : m == "booleans" ? 61 : m == "fxlib" ? 62 : m == "paint3d" ? 63 : m == "uv" ? 64 : m == "example3d" ? 65 : m == "render" ? 66 : m == "example3dx" ? 67 : m == "linksend" ? 68 : m == "drop" ? 69 : m == "uassetconv" ? 70 : m == "layerxf" ? 71 : m == "graphtools" ? 72 : m == "functions" ? 73 : m == "typed" ? 74 : m == "library" ? 75 : m == "trims" ? 76 : m == "nodeui" ? 77 : m == "maketemplates" ? 78 : m == "makescenes" ? 79 : m == "comments" ? 80 : m == "execjoin" ? 81 : m == "call" ? 82 : m == "inpaint" ? 83 : m == "explorer" ? 84 : m == "actioneditor" ? 85 : m == "popout" ? 86 : m == "paint3dtools" ? 87 : m == "smartmat" ? 88 : m == "bake" ? 89 : m == "simulation" ? 90 : m == "projection" ? 91 : m == "smartthumbs" ? 93 : m == "plugins" ? 94 : m == "search" ? 95 : m == "cameraraw" ? 96 : 1;
            if (demoMode > 1) ++i;
        }
        else if (a == "--capture" && i + 1 < argc) capturePath = Narrow(argv[++i]);
        else if (a == "--focus" && i + 1 < argc) focusAtStart.push_back(Narrow(argv[++i]));  // panel to bring forward (captures)
        else if (a == "--about") aboutAtStart = true;
        else if (a == "--multiwindow") multiWindowTest = true;  // screenshot of the About box
        else if (a == "--save" && i + 1 < argc) cliSavePath = Narrow(argv[++i]);  // save the active document at the capture frame
        else if (a == "--dump-image" && i + 1 < argc) dumpPath = Narrow(argv[++i]);  // document composite at the capture frame
        else if (a == "--sparse-threshold" && i + 1 < argc) gpu::Dev().SetSparseThreshold((u64)_wtoi64(argv[++i]));
        else if (a == "--frames" && i + 1 < argc) captureFrame = std::max(3, _wtoi(argv[++i]));
        else if (a == "--link" && i + 1 < argc) pendingLinkOpen = Narrow(argv[++i]);  // Unreal plugin session
        else if (a == "--export-unreal" && i + 1 < argc) exportUnrealDir = Narrow(argv[++i]);  // model + material for Unreal, then quit
        else if (a.rfind("--", 0) != 0) pendingDrop = a;  // open a file given on the command line
    }
    LocalFree(argv);
}

bool App::Init() {
    ParseCommandLine();
    ImGui_ImplWin32_EnableDpiAwareness();
    dpi = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));
    hwnd = platform::CreateMainWindow(L"Shaderixel", (int)(1600 * dpi), (int)(960 * dpi));
    if (!gpu::Dev().Init(hwnd)) {
        MessageBoxW(hwnd, gpu::BackendName() == std::string("Vulkan") ? L"Impossible d'initialiser Vulkan 1.3 (pilote graphique à jour ?)."
                                                                    : L"Impossible d'initialiser Direct3D 12.", L"Shaderixel", MB_ICONERROR);
        return false;
    }
    platform::SetResizeCallback([](u32 w, u32 h) { gpu::Dev().Resize(w, h); });
    platform::SetDropCallback([this](const std::string& p) {
        DropRequest r;
        r.path = p;
        platform::LastDropPos(r.x, r.y);
        pendingDrops.push_back(r);
    });

    std::string shaderDir = ShaderDir();
    gpu::Shaders().Init(shaderDir, ExeDir() + "/shadercache");
    Effects().Load(shaderDir + "/effects");
    Brushes().Load(shaderDir + "/brushes");
    // Warm up builtin kernels in the background.
    for (const char* s : {"composite.slang", "display.slang", "brush.slang", "clear.slang", "clear_r.slang", "gradient.slang"})
        gpu::Shaders().Builtin(s);
    if (testEffects) {
        // SX_TEST_ONLY=hlsl: only the HLSL import samples (quick iteration).
        const bool onlyHlsl = GetEnvironmentVariableA("SX_TEST_ONLY", nullptr, 0) > 0;
        int ok = 0;
        for (auto& e : Effects().All()) {
            if (onlyHlsl) break;
            auto p = gpu::Shaders().CompileNow(e->name, BuildEffectSource(*e));
            bool good = p->state.load() == gpu::Program::State::Ready;
            ok += good;
            Log("[test-effects] %-24s %s%s", e->id.c_str(), good ? "OK" : "FAILED: ", good ? "" : p->errors.c_str());
        }
        Log("[test-effects] %d / %zu effects compiled", ok, Effects().All().size());
        // Visual graphs carried by library effects (//@graph): compiled as a copy would be.
        int okg = 0, ng = 0;
        for (auto& e : Effects().All()) {
            if (onlyHlsl) break;
            if (e->graph.empty()) continue;
            ++ng;
            EffectGraph g;
            std::string why;
            if (!EffectGraph::Deserialize(e->graph, g)) why = "graphe illisible";
            EGCompileResult r = why.empty() ? CompileEffectGraph(g) : EGCompileResult{};
            if (why.empty() && !r.error.empty()) why = r.error;
            if (why.empty()) {
                auto def = ParseEffect("", r.code);
                auto p = gpu::Shaders().CompileNow(e->name + " (graphe)", BuildEffectSource(*def));
                if (p->state.load() != gpu::Program::State::Ready) why = p->errors;
                else if (def->params.size() != e->params.size()) why = "paramètres différents";
            }
            okg += why.empty();
            if (!why.empty()) Log("[test-graphs] %-24s FAILED: %s", e->id.c_str(), why.c_str());
        }
        Log("[test-graphs] %d / %d effect graphs compiled", okg, ng);
        // HLSL import: function with annotations, out parameters, body only, blend.
        const char* samples[] = {
            "//@name Vignette\n//@in Amount float 0.6 0 1 \"Intensité\" \"Réglages\"\n//@in Tint color 0 0 0 1 \"Couleur\" \"Couleur\"\n"
            "float4 Vignette(Texture2D Image, SamplerState S, float2 UV, float Amount, float4 Tint)\n"
            "{ float4 c = Image.Sample(S, UV); float k = smoothstep(0.8, 0.3, length(UV - 0.5)); return float4(lerp(Tint.rgb, c.rgb, lerp(1.0, k, Amount)), c.a); }\n",
            "//@name Séparer\n//@main Split\nfloat Luma(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }\n"
            "void Split(Texture2D Src, float2 uv, int Steps, bool Flip, out float4 Color, out float Gray)\n"
            "{ Color = Texture2DSample(Src, SrcSampler, uv); Gray = Luma(Color.rgb); if (Flip) Gray = 1.0 - Gray; Gray = floor(Gray * Steps) / Steps; }\n",
            "//@name Corps seul\n//@in Image texture\n//@in UV float2\n//@in Gain float 2 0 4\n//@out Result float3\n"
            "return Texture2DSample(Image, ImageSampler, UV).rgb * Gain;\n",
            "//@name Fusion douce\n//@kind blend\n//@in Opacity float 1 0 1\n"
            "float4 Mix(Texture2D A, Texture2D B, Texture2D M, float2 UV, float Opacity)\n"
            "{ float4 a = A.Sample(s, UV), b = B.Sample(s, UV); float k = b.a * Opacity * M.Sample(s, UV).r; return float4(lerp(a.rgb, a.rgb * b.rgb, k), max(a.a, k)); }\n",
        };
        int okh = 0;
        for (const char* src : samples) {
            EffectGraph g;
            std::string why;
            if (GraphFromHlsl(src, g, &why)) {
                auto def = ParseEffect("", CompileEffectGraph(g).code);
                auto p = gpu::Shaders().CompileNow("HLSL import " + g.name, BuildEffectSource(*def));
                if (p->state.load() != gpu::Program::State::Ready) why = p->errors;
                else {
                    std::string ps;
                    for (auto& pd : def->params) ps += pd.id + "[" + pd.group + "] ";
                    Log("[test-hlsl] %s OK: inputs %zu, params %s", g.name.c_str(), def->inputs.size(), ps.c_str());
                }
            }
            okh += why.empty();
            if (!why.empty()) Log("[test-hlsl] FAILED: %s", why.c_str());
        }
        Log("[test-hlsl] %d / %zu HLSL imports compiled", okh, std::size(samples));
        int okb = 0;
        for (auto& b : Brushes().All()) {
            if (onlyHlsl) break;
            auto p = gpu::Shaders().CompileNow("brush " + b->id, BuildBrushSource(*b));
            bool good = p->state.load() == gpu::Program::State::Ready;
            okb += good;
            Log("[test-brushes] %-24s %s%s", b->id.c_str(), good ? "OK" : "FAILED: ", good ? "" : p->errors.c_str());
        }
        Log("[test-brushes] %d / %zu brushes compiled", okb, Brushes().All().size());
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // no keyboard nav: keys belong to the tools
    // Panels and extra windows can leave the main window (other screens).
    if (!Automated() || multiWindowTest) io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    // Pointer samples in the coordinates ImGui uses (screen pixels with multi-viewports).
    platform::SetScreenCoordinates((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0);
    io.ConfigViewportsNoTaskBarIcon = true;
    RegisterWorkspaceSettings();
    ScanLayouts();
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    static std::string iniPath = ExeDir() + "/shaderixel_layout_v12.ini";
    LoadRecent();
    io.IniFilename = iniPath.c_str();
    ui::ApplyTheme(dpi);
    ui::LoadFonts(dpi);

    ImGui_ImplWin32_Init(hwnd);
    gpu::Dev().InitImGui();

    eraser = brush;
    eraser.hardness = 0.9f;
    settingsPath = ExeDir() + "/shaderixel_settings.json";
    LoadAiSettings();
    InitFunctions();
    LoadActions();
    monitorProfile = color::MonitorPath(hwnd);
    RegisterAiOps();
    InitPlugins();
    InitSearch();
    if (aiTest) bridge.allowWrites = true;
    InitLink();
    if (bridge.Start(ExeDir() + "/Shaderixel") && (aiSettings.autoStart || aiTest)) {
        std::string err;
        if (!bridge.LaunchCompanion(aiSettings, &err)) Log("Companion: %s", err.c_str());
    }
    if (Automated()) {
        platform::SetIgnoreUserInput(true);
        // Scripted run (demo / capture / dump): full size but behind the user's
        // windows and without taking the focus, so the PC stays usable.
        RECT wa;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        SetWindowPos(hwnd, HWND_BOTTOM, wa.left, wa.top, wa.right - wa.left, wa.bottom - wa.top, SWP_NOACTIVATE);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    } else {
        ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    }
    UpdateWindow(hwnd);
    // No document at launch: the canvas shows the welcome screen (logo, new/open, recent files).
    return true;
}

void App::Shutdown() {
    SaveSearch();
    ShutdownPlugins();
    ShutdownLink();
    bridge.Shutdown();
    for (auto& j : aiJobs) j.wait();
    gpu::Dev().WaitIdle();
    docs.clear();
    gpu::Dev().ShutdownImGui();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    gpu::Shaders().Shutdown();
    gpu::Dev().Shutdown();
}

}  // namespace sx
// Shared with the patched imgui_widgets.cpp (tools/patches/imgui_relative_drag.patch).
extern ImVec2 GSXMouseDelta;
extern bool GSXUseMouseDelta;
extern int GSXRelativeDragFrame;
namespace sx {

void App::UpdateRelativeDrag() {
    // Called before ImGui::NewFrame. While a slider/drag follows the mouse (it
    // hides the cursor), the cursor is pulled back to where the drag started
    // whenever it wanders, so the drag never hits a screen edge; the true motion
    // is measured here and handed to the widgets. On release the cursor
    // reappears where the drag started.
    GSXUseMouseDelta = false;
    if (Automated()) return;  // scripted runs: widgets use ImGui's own mouse delta, the real cursor is left alone
    static bool dragging = false;
    static POINT anchor = {}, last = {};
    bool active = GSXRelativeDragFrame == ImGui::GetFrameCount();  // previous frame's widgets
    POINT cur;
    GetCursorPos(&cur);
    if (active && !dragging) {
        dragging = true;
        ImVec2 c = ImGui::GetIO().MouseClickedPos[0];
        anchor = {(LONG)c.x, (LONG)c.y};
        // With multi-viewports ImGui positions are already in screen coordinates.
        if (!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)) ClientToScreen(hwnd, &anchor);
        last = cur;
    }
    if (!dragging) return;
    if (!active) {
        SetCursorPos(anchor.x, anchor.y);
        dragging = false;
        return;
    }
    GSXMouseDelta = ImVec2((float)(cur.x - last.x), (float)(cur.y - last.y));
    // A jump (cursor warped by the system, another screen) is not a motion.
    if (std::abs(GSXMouseDelta.x) > 400 || std::abs(GSXMouseDelta.y) > 400) GSXMouseDelta = ImVec2(0, 0);
    GSXUseMouseDelta = true;
    long dx = cur.x - anchor.x, dy = cur.y - anchor.y;
    if (dx * dx + dy * dy > 80 * 80) {
        SetCursorPos(anchor.x, anchor.y);
        last = anchor;
    } else {
        last = cur;
    }
}

void App::LoadLogo() {
    logoTried = true;
    HMODULE mod = GetModuleHandleW(nullptr);
    HRSRC res = FindResourceW(mod, MAKEINTRESOURCEW(2), MAKEINTRESOURCEW(10));  // RT_RCDATA
    HGLOBAL h = res ? LoadResource(mod, res) : nullptr;
    const void* data = h ? LockResource(h) : nullptr;
    if (!data) return;
    int w = 0, hgt = 0, comp = 0;
    stbi_uc* px = stbi_load_from_memory((const stbi_uc*)data, (int)SizeofResource(mod, res), &w, &hgt, &comp, 4);
    if (!px) return;
    logoTex = gpu::Dev().CreateTexture((u32)w, (u32)hgt, gpu::Format::RGBA8, "Logo");
    ctx->Upload(logoTex.get(), px, (u32)w * 4);
    stbi_image_free(px);
}

void App::DrawAbout() {
    if (showAbout) {
        ImGui::OpenPopup("À propos de Shaderixel");
        showAbout = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal("À propos de Shaderixel", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    float s = ImGui::GetFontSize() * 14;
    if (logoTex) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetFontSize() * 22 - s) * 0.5f);
        ImGui::Image((ImTextureID)gpu::Dev().UiTextureId(logoTex.get()), ImVec2(s, s));
    }
    ImGui::Dummy(ImVec2(ImGui::GetFontSize() * 22, 0));
    ImGui::TextUnformatted("Éditeur d'images entièrement GPU");
    ImGui::TextDisabled("Calques, mode nodal, effets HLSL/Slang, peinture 3D PBR, IA (MCP)");
    ImGui::TextDisabled("Rendu : %s    Compilation : %s", gpu::BackendName(), __DATE__);
    ImGui::Spacing();
    if (ImGui::Button("Fermer", ImVec2(-FLT_MIN, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void App::DrawWelcome() {
    // Shown in the canvas when no document is open: logo, quick actions, recent files.
    ImVec2 a = ImGui::GetContentRegionAvail();
    ImVec2 s0 = ImGui::GetCursorPos();
    float fs = ImGui::GetFontSize();
    float logo = logoTex ? std::clamp(std::min(a.x, a.y) * 0.38f, 96.0f, 360.0f) : 0.0f;
    std::vector<const std::string*> recent;  // existing files only
    for (auto& r : recentFiles) if (recent.size() < 6 && std::filesystem::exists(Utf8Path(r))) recent.push_back(&r);
    int nRecent = (int)recent.size();
    float colW = std::min(a.x - 2 * fs, fs * 26);
    float total = logo + fs * 4.5f + (nRecent ? fs * 2 + nRecent * ImGui::GetFrameHeightWithSpacing() : 0);
    float y = s0.y + std::max(fs, (a.y - total) * 0.45f);
    if (logoTex) {
        ImGui::SetCursorPos(ImVec2(s0.x + (a.x - logo) * 0.5f, y));
        ImGui::Image((ImTextureID)gpu::Dev().UiTextureId(logoTex.get()), ImVec2(logo, logo));
        y += logo + fs;
    }
    float x = s0.x + (a.x - colW) * 0.5f;
    float bw = (colW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    ImGui::SetCursorPos(ImVec2(x, y));
    if (ImGui::Button("Nouveau document   Ctrl+N", ImVec2(bw, fs * 2))) { showNewDoc = true; tplUi.tabRequest = 1; }
    ImGui::SameLine();
    if (ImGui::Button("Depuis un modèle...", ImVec2(bw, fs * 2))) { showNewDoc = true; tplUi.tabRequest = 0; }
    ImGui::SetCursorPosX(x);
    if (ImGui::Button("Nouveau projet Paint 3D", ImVec2(bw, fs * 2))) p3dNew.open = true;
    ImGui::SameLine();
    if (ImGui::Button("Ouvrir...   Ctrl+O", ImVec2(bw, fs * 2))) OpenImage(platform::OpenFileDialog(hwnd, kImageFilter), false);
    y += fs * 3;
    if (nRecent) {
        ImGui::SetCursorPos(ImVec2(x, y));
        ImGui::TextDisabled("Fichiers récents");
        std::string open;
        for (int i = 0; i < nRecent; ++i) {
            const std::string& r = *recent[i];
            ImGui::SetCursorPosX(x);
            std::string name = Narrow(Utf8Path(r).filename().wstring());
            if (ImGui::Selectable((name + "##" + r).c_str(), false, 0, ImVec2(colW, 0))) open = r;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", r.c_str());
        }
        if (!open.empty()) OpenImage(open, false);
    } else {
        ImGui::SetCursorPosX(x);
        ImGui::TextDisabled("Glissez une image, un .sxl ou un .psd sur la fenêtre pour l'ouvrir.");
    }
}

void App::Frame() {
    gpu::Shaders().Update();
    Effects().ReloadIfChanged();
    Brushes().ReloadIfChanged();
    ctx = &gpu::Dev().BeginFrame();
    if (!logoTried) LoadLogo();

    gpu::Dev().ImGuiNewFrame();
    ImGui_ImplWin32_NewFrame();
    UpdateRelativeDrag();  // before NewFrame: feeds the cursor motion of slider drags
    SwitchWorkspaceKind();
    ApplyPendingLayout();
    ImGui::NewFrame();
    GlobalSearch::Get().BeginFrame();
    frameSamples = platform::TakeSamples();

    if (DocView* v = Active()) {
        frameBefore = v->doc->S();
        frameBeforeValid = true;
    } else {
        frameBeforeValid = false;
    }

    DrawMenuBar();
    DrawAbout();
    DrawStatusBar();
    ImGuiID dock = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    if (!layoutBuilt || resetLayout) {
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock);
        if (resetLayout || !node || !node->IsSplitNode()) BuildDockLayout(dock);
        layoutBuilt = true;
        resetLayout = false;
    }
    UpdatePopOuts();
    if (paint3DLayoutRequest) {
        ApplyPaint3DLayout();
        ShowSideTab(5);
        paint3DLayoutRequest = false;
    }

    ++frameCounter;
    if (frameCounter == 2) ImGui::SetWindowFocus("Canevas");  // Canvas is the default center tab
    if (demoMode && frameCounter == 3) RunDemo();
    if (aboutAtStart && frameCounter == 3) showAbout = true;
    if (testEffects && capturePath.empty() && frameCounter == 3) autoQuit = true;  // compile test: quit once done
    if (demoMode) DemoTick();
    if (frameCounter >= 4 && frameCounter - 4 < focusAtStart.size()) ImGui::SetWindowFocus(focusAtStart[frameCounter - 4].c_str());
    if (!focusAfterLayout.empty() && ImGui::FindWindowByName(focusAfterLayout.front().c_str())) {
        ImGui::SetWindowFocus(focusAfterLayout.front().c_str());
        focusAfterLayout.erase(focusAfterLayout.begin());
    }
    if (!capturePath.empty() && frameCounter == (u64)captureFrame) captureRb = gpu::Dev().CaptureNextFrame();
    if (!cliSavePath.empty() && frameCounter == (u64)captureFrame && Active()) {
        SaveDocument(*Active(), cliSavePath);
        cliSavePath.clear();
        if (capturePath.empty()) autoQuit = true;  // --save alone: quit once written
    }
    if (!dumpPath.empty() && frameCounter == (u64)captureFrame && Active())
        dumpRb = Active()->renderer.ExportRGBA8(*ctx, Active()->renderer.EvaluateComplete(*Active()->doc, *ctx));
    if (dumpRb && dumpRb->ready) {
        stbi_write_png(dumpPath.c_str(), dumpRb->width, dumpRb->height, 4, dumpRb->data.data(), dumpRb->width * 4);
        Log("Dumped document image to %s", dumpPath.c_str());
        dumpRb.reset();
    }
    if (captureRb && captureRb->ready) {
        for (size_t i = 3; i < captureRb->data.size(); i += 4) captureRb->data[i] = 255;
        stbi_write_png(capturePath.c_str(), captureRb->width, captureRb->height, 4, captureRb->data.data(), captureRb->width * 4);
        Log("Captured %ux%u to %s", captureRb->width, captureRb->height, capturePath.c_str());
        captureRb.reset();
        capturePath.clear();
        autoQuit = true;
    }
    // Automated runs quit once pending saves / exports are written.
    if (!exportUnrealDir.empty() && frameCounter >= 12 && Active() && Active()->doc->kind == 1 && Active()->mesh) {
        // Command line (Unreal plugin importing a .sx3d): the model, its textures and the description.
        ExportForUnreal(*Active(), exportUnrealDir);
        exportUnrealDir.clear();
        autoQuit = true;
    } else if (!exportUnrealDir.empty() && frameCounter > 600) {
        Log("--export-unreal: no Paint 3D project open");
        exportUnrealDir.clear();
        autoQuit = true;
    }
    if (autoQuit && saves.empty() && !psdExport && !dumpRb && exports.empty() && packJobs.empty() && !LinkBusy()) {
        for (auto& dv : docs) dv->doc->dirty = false;  // never prompt
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        autoQuit = false;
    }

    bridge.Pump();
    UpdatePlugins();
    UpdateSearch();
    UpdateSmartChildren();
    UpdateSelectSubject();
    UpdateContentAwareFill();
    UpdateActionRun();
    UpdateColorJobs();
    UpdatePhotoJob();
    UpdateResources();
    UpdatePaint3D();
    UpdateLink();
    UpdateUAssetJobs();
    UpdateFunctionEdits();
    UpdateExec();
    UpdateGraphTools();
    UpdateBatch();
    UpdateSubjectInstall();
    if (DocView* av = Active(); av && frameCounter % 60 == 0) UpdateLinkedSmartObjects(*av, false);
    PollAiJobs();
    UpdateGenerativeFill();
    UpdateInpaint();
    UpdateSelectionToPath();
    UpdateImageGeneration();
    HandleShortcuts();
    DrawToolbar();
    DrawCanvas();
    if (Shown("Panneaux")) DrawSidePanel();
    else for (int i = 0; i < kSideTabs; ++i) ImGui::DockSpace(ImHashStr(("##sxside" + std::to_string(i)).c_str()), ImVec2(0, 0), ImGuiDockNodeFlags_KeepAliveOnly);
    MigrateLayout();
    DrawZoneToggles();
    DrawExtraWindows();
    if (Shown("Graphe")) DrawNodeEditor();  // first: tab order of the bottom zone
    if (!ImGui::FindWindowByName("Palette de nœuds")) DockNewPanel(2, false);
    if (Shown("Palette de nœuds")) DrawNodePalette();
    if (Shown("Calques")) DrawLayers();
    if (Shown("Vue 3D")) DrawView3D();
    if (layoutKind == 1) {
        // Render mode: only in the Paint 3D workspace (next to the 3D view / in the 3D side tab).
        if (!ImGui::FindWindowByName("Rendu"))
            if (ImGuiWindow* w3 = ImGui::FindWindowByName("Vue 3D"); w3 && w3->DockId) ImGui::SetNextWindowDockID(w3->DockId, ImGuiCond_FirstUseEver);
        if (Shown("Rendu")) DrawRenderView();
        if (!ImGui::FindWindowByName("Scène de rendu")) DockNewPanel(5, false);
        if (Shown("Scène de rendu")) DrawRenderScenePanel();
    }
    if (Shown("Éditeur d'effets")) DrawEffectEditor();
    if (Shown("Assistant IA")) DrawAiPanel();
    if (Shown("Génération IA")) DrawImageGenPanel();
    if (Shown("Propriétés")) DrawProperties();
    if (Shown("Couleur")) DrawColor();
    if (Shown("Options")) DrawBrush();
    if (Shown("Historique")) DrawHistory();
    if (Shown("Tracés")) DrawPathsPanel();
    if (!ImGui::FindWindowByName("Actions")) DockNewPanel(1, false);
    if (Shown("Actions")) DrawActionsPanel();
    if (!panelOpen.count("Éditeur d'actions")) panelOpen["Éditeur d'actions"].open = false;  // opened on demand
    if (Shown("Éditeur d'actions")) DrawActionEditor();
    if (!ImGui::FindWindowByName("Compositions de calques")) DockNewPanel(1, false);
    if (Shown("Compositions de calques")) DrawLayerCompsPanel();
    if (!ImGui::FindWindowByName("Styles de texte")) DockNewPanel(1, false);
    if (Shown("Styles de texte")) DrawTextStylesPanel();
    if (!ImGui::FindWindowByName("Nuancier")) DockNewPanel(4, false);
    if (Shown("Nuancier")) DrawSwatchesPanel();
    if (!ImGui::FindWindowByName("Motifs")) DockNewPanel(4, true);
    if (Shown("Motifs")) DrawPatternsPanel();
    if (!ImGui::FindWindowByName("Calques 3D")) DockNewPanel(5, false);
    if (Shown("Calques 3D")) DrawLayers3DPanel();
    if (Shown("Ressources")) DrawResourceBrowser();
    if (!ImGui::FindWindowByName("Inpainting IA")) DockNewPanel(6, false);
    if (Shown("Inpainting IA")) DrawInpaintPanel();
    if (!ImGui::FindWindowByName("Trim sheet")) DockNewPanel(1, true);
    if (Shown("Trim sheet")) DrawTrimPanel();
    if (!ImGui::FindWindowByName("Matériau du pinceau")) DockNewPanel(5, true);
    if (Shown("Matériau du pinceau")) DrawBrushMaterialPanel();
    DrawBatchWindow();
    DrawUnrealExportDialog();
    DrawBakerWindow();
    DrawTextureExportDialog();
    DrawTurntableDialog();
    DrawPluginPanels();
    DrawPluginManager();
    DrawColorDialogs();
    DrawPhotoDialog();
    DrawCameraRaw();
    DrawPaint3DNewDialog();
    DrawUvToolWindow();
    UpdateHistogram();
    if (Shown("Histogramme")) DrawHistogram();
    if (Shown("Effets")) DrawEffectBrowser();
    if (Shown("Fusions")) DrawBlendPanel();
    if (Shown("Matériaux")) DrawMaterialPanel();
    if (Shown("Pinceaux")) DrawBrushPanel();
    if (Shown("Aperçu du calque")) DrawLayerPreview();
    DrawLayoutSavePopup();
    DrawColorRangeWindow();
    DrawSelectMaskWindow();
    DrawNewDocDialog();
    DrawSaveTemplateDialog();
    DrawShaderErrors();
    DrawSelectionDialog();
    DrawImageDialogs();
    if (showDemo) ImGui::ShowDemoWindow(&showDemo);
    DrawPopOutCorners();

    DrawClosePrompt();
    UpdateSaves();
    UpdateTemplateJobs();
    UpdateTitle();
    // Dropped files, one per frame: into the window under the drop point, otherwise opened.
    if (!pendingDrops.empty() && pendingDrop.empty()) {
        DropRequest r = pendingDrops.front();
        pendingDrops.erase(pendingDrops.begin());
        if (!RouteDrop(r)) pendingDrop = r.path;
    }
    if (!pendingDrop.empty()) {
        std::string ext = Narrow(Utf8Path(pendingDrop).extension().wstring());
        for (auto& c : ext) c = (char)tolower((unsigned char)c);
        std::string src, err;
        if (ext == ".zip") {
            std::string err;
            Status(InstallPluginZip(pendingDrop, &err) ? "Plugin installé : " + Narrow(Utf8Path(pendingDrop).stem().wstring()) : "Archive : " + err);
        } else if (ext == ".abr") {
            ImportAbrFile(pendingDrop);
        } else if (ext == ".pat") {
            ImportPatFile(pendingDrop);
        } else if (ext == ".aco" || ext == ".ase" || ext == ".gpl") {
            LoadSwatchLibrary(pendingDrop);
            swatchesLoaded = true;
        } else if ((ext == ".hlsl" || ext == ".fx") && Active()) {
            // HLSL file dropped: a new effect with its inputs / outputs.
            if (ReadTextFile(pendingDrop, src) && !CreateEffectFromHlsl(*Active(), src, &err)) Status("Import HLSL : " + err);
        } else {
            OpenImage(pendingDrop, false);
        }
        pendingDrop.clear();
    }

    PollPsdExport();
    // Finished async readbacks.
    for (size_t i = 0; i < exports.size();) {
        auto& e = exports[i];
        if (e.rb && e.rb->ready && !e.crop.Empty()) {
            // Part of the image (artboards): crop in place into a copy.
            auto c = std::make_shared<gpu::Readback>();
            c->width = (u32)e.crop.Width();
            c->height = (u32)e.crop.Height();
            c->ready = true;
            c->data.resize((size_t)c->width * c->height * 4);
            for (u32 y = 0; y < c->height; ++y)
                memcpy(&c->data[(size_t)y * c->width * 4], &e.rb->data[(((size_t)(y + e.crop.y0)) * e.rb->width + e.crop.x0) * 4], (size_t)c->width * 4);
            e.rb = c;
            e.crop = {};
        }
        if (e.rb && e.rb->ready) {
            std::string ext = Narrow(Utf8Path(e.path).extension().wstring());
            for (auto& ch : ext) ch = (char)tolower(ch);
            int ok = 0;
            std::string native = e.path;
            if (ext == ".jpg" || ext == ".jpeg") ok = stbi_write_jpg(native.c_str(), e.rb->width, e.rb->height, 4, e.rb->data.data(), std::clamp(e.quality, 1, 100));
            else if (ext == ".bmp") ok = stbi_write_bmp(native.c_str(), e.rb->width, e.rb->height, 4, e.rb->data.data());
            else if (ext == ".tga") ok = stbi_write_tga(native.c_str(), e.rb->width, e.rb->height, 4, e.rb->data.data());
            else ok = stbi_write_png(native.c_str(), e.rb->width, e.rb->height, 4, e.rb->data.data(), e.rb->width * 4);
            Status(ok ? "Exporté : " + e.path : "Échec de l'export : " + e.path);
            exports.erase(exports.begin() + i);
        } else {
            ++i;
        }
    }
    if (pickReadback && pickReadback->ready) {
        auto& d = pickReadback->data;
        float4 c;
        if (pickReadback->format == gpu::Format::RGBA16F && d.size() >= 8) {
            const u16* h = (const u16*)d.data();
            c = {HalfToFloat(h[0]), HalfToFloat(h[1]), HalfToFloat(h[2]), HalfToFloat(h[3])};
        } else if (pickReadback->format == gpu::Format::RGBA32F && d.size() >= 16) {
            memcpy(&c, d.data(), 16);
        }
        if (c.w > 1e-5f) {
            float4& target = editBg ? bg : fg;
            target = {LinearToSrgb(c.x / c.w), LinearToSrgb(c.y / c.w), LinearToSrgb(c.z / c.w), 1.0f};
            for (int k = 0; k < 3; ++k) target[k] = std::clamp(target[k], 0.0f, 1.0f);
        }
        pickReadback.reset();
    }

    GlobalSearch::Get().EndFrame();
    ImGui::Render();
    gpu::Dev().EndFrame();
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        // Panels / windows outside the main window (after the main frame is submitted).
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
    ctx = nullptr;
}

// ------------------------------------------------------------------ documents

DocView* App::Active() {
    return activeDoc >= 0 && activeDoc < (int)docs.size() ? docs[activeDoc].get() : nullptr;
}

void App::NewDocument(u32 w, u32 h, Precision p, DocMode mode, int background, const std::string& name) {
    auto v = std::make_unique<DocView>();
    v->doc = std::make_unique<Document>(w, h, p, mode, name);
    Document& d = *v->doc;
    if (background != 3) {
        float4 clear = background == 0 ? float4{1, 1, 1, 1} : background == 2 ? float4{0, 0, 0, 1} : float4{0, 0, 0, 0};
        if (mode == DocMode::Layers) {
            d.AddPixelLayer("Arrière-plan");
            d.pendingClears.back().second = clear;
        } else {
            Node& n = d.AddNode(NodeKind::Surface, {60, 60});
            n.name = "Arrière-plan";
            d.pendingClears.back().second = clear;
            Id nid = n.id;
            d.Link(nid, d.S().graph.output, 0);
            if (Node* out = d.S().graph.Find(d.S().graph.output)) out->pos = {380, 60};
        }
    }
    d.dirty = false;
    docs.push_back(std::move(v));
    activeDoc = (int)docs.size() - 1;
    focusDocRequest = activeDoc;
}

void App::CloseDocument(int index, bool force) {
    if (index < 0 || index >= (int)docs.size()) return;
    if (!force && docs[index]->doc->dirty) {
        closePrompt = index;
        return;
    }
    if (xf.active && xf.view == docs[index].get()) xf = TransformState{};
    if (wand.view == docs[index].get()) wand = WandRequest{};
    gpu::Dev().WaitIdle();
    DetachSmartChild(*docs[index]);
    docs.erase(docs.begin() + index);
    activeDoc = std::min(activeDoc, (int)docs.size() - 1);
}

void App::LoadRecent() {
    recentFiles.clear();
    FILE* f = nullptr;
    if (_wfopen_s(&f, Widen(ExeDir() + "/recent.txt").c_str(), L"rb") != 0 || !f) return;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        std::string s = line;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (!s.empty()) recentFiles.push_back(s);
    }
    fclose(f);
}

void App::AddRecent(const std::string& path) {
    if (!path.empty()) {  // empty: just rewrite the list (after clearing it)
        std::erase(recentFiles, path);
        recentFiles.insert(recentFiles.begin(), path);
        if (recentFiles.size() > 12) recentFiles.resize(12);
    }
    FILE* f = nullptr;
    if (_wfopen_s(&f, Widen(ExeDir() + "/recent.txt").c_str(), L"wb") != 0 || !f) return;
    for (auto& r : recentFiles) fprintf(f, "%s\n", r.c_str());
    fclose(f);
}

void App::OpenImage(const std::string& path, bool asLayer) {
    if (path.empty() || !ctx) return;
    if (!asLayer && !demoMode) AddRecent(path);
    {
        std::string ext = Narrow(Utf8Path(path).extension().wstring());
        for (auto& ch : ext) ch = (char)tolower((unsigned char)ch);
        if (ext == ".sxl" || ext == ".sx3d") {
            LoadSxl(path);
            return;
        }
        if (ext == ".uasset" && OpenUAsset(path, asLayer)) return;  // meshes (Unreal conversion); textures continue below
        if ((ext == ".psd" || ext == ".psb") && !asLayer) {
            auto view = std::make_unique<DocView>();
            PsdImportReport rep;
            std::string err;
            view->doc = ImportPsd(path, view->renderer, *ctx, rep, &err);
            if (view->doc) {
                view->doc->path = path;
                docs.push_back(std::move(view));
                activeDoc = (int)docs.size() - 1;
                focusDocRequest = activeDoc;
                for (auto& w : rep.warnings) Log("PSD: %s", w.c_str());
                Status("PSD importé : " + std::to_string(rep.layers) + " calques, " + std::to_string(rep.groups) + " groupes, " +
                       std::to_string(rep.adjustments) + " réglages, " + std::to_string(rep.masks) + " masques" +
                       (rep.warnings.empty() ? "" : " (" + std::to_string(rep.warnings.size()) + " avertissements, voir le journal)"));
                return;
            }
            Log("PSD layered import failed (%s), falling back to the flattened image", err.c_str());
        }
    }
    {
        // Camera RAW (LibRaw, else WIC) and formats stb does not read (Windows Imaging Component).
        std::string ext = Narrow(Utf8Path(path).extension().wstring());
        for (auto& ch : ext) ch = (char)tolower((unsigned char)ch);
        if (IsRawExtension(ext) || ext == ".tif" || ext == ".tiff" || ext == ".webp" || ext == ".heic" || ext == ".heif" || ext == ".jxr") {
            OpenWithWic(path, asLayer);
            return;
        }
    }
    int w = 0, h = 0, comp = 0;
    bool isFloat = false;
    void* pixels = nullptr;
    if (Narrow(Utf8Path(path).extension().wstring()) == ".uasset" || Narrow(Utf8Path(path).extension().wstring()) == ".UASSET") {
        pixels = LoadImagePixels(path, w, h);  // Unreal texture (mip 0)
        if (!pixels) return;
    } else {
        FILE* f = nullptr;
        _wfopen_s(&f, Widen(path).c_str(), L"rb");
        if (!f) { Status("Impossible d'ouvrir " + path); return; }
        isFloat = stbi_is_hdr_from_file(f) != 0;
        pixels = isFloat ? (void*)stbi_loadf_from_file(f, &w, &h, &comp, 4) : (void*)stbi_load_from_file(f, &w, &h, &comp, 4);
        fclose(f);
    }
    if (!pixels) {
        if (!OpenWithWic(path, asLayer)) Status("Format d'image non supporté : " + path);
        return;
    }
    std::string name = Narrow(Utf8Path(path).filename().wstring());
    DocView* v = Active();
    if (!asLayer || !v) {
        NewDocument((u32)w, (u32)h, isFloat ? Precision::Float32 : Precision::Half16, DocMode::Layers, 3, name);
        v = Active();
        Layer& l = v->doc->AddPixelLayer("Arrière-plan");
        v->doc->pendingClears.clear();
        v->renderer.Import(*v->doc, *ctx, l.surface, pixels, (u32)w, (u32)h, isFloat, {0, 0});
        v->doc->path = path;
        v->doc->dirty = false;
        OpenedImageProfile(*v, path);
    } else {
        Document& d = *v->doc;
        DocState before = d.S();
        Id surface = 0;
        if (d.Mode() == DocMode::Layers) {
            surface = d.AddPixelLayer(name).surface;
        } else {
            surface = d.AddNode(NodeKind::Surface, nodeEd.contextPos).surface;
            d.S().graph.Find(d.S().activeNode)->name = name;
        }
        d.pendingClears.pop_back();
        // Clear first (outside of the image) then place the image centered.
        int2 off = {((int)d.Width() - w) / 2, ((int)d.Height() - h) / 2};
        d.pendingClears.push_back({surface, {0, 0, 0, 0}});
        v->renderer.Prepare(d, *ctx);
        v->renderer.Import(d, *ctx, surface, pixels, (u32)w, (u32)h, isFloat, off);
        v->history.PushState("Importer " + name, before, d.S());
    }
    stbi_image_free(pixels);
    Status("Ouvert : " + path);
}

void App::ExportPsd(const std::string& path, int depth) {
    DocView* v = Active();
    if (!v || path.empty()) return;
    if (psdExport) { Status("Un export PSD est déjà en cours"); return; }
    auto st = std::make_shared<PsdExportState>();
    st->job.path = path;
    st->job.depth = depth;
    {
        std::string ext = Narrow(Utf8Path(path).extension().wstring());
        for (auto& c : ext) c = (char)tolower((unsigned char)c);
        st->job.psb = ext == ".psb" || v->doc->Width() > 30000 || v->doc->Height() > 30000;
        if (st->job.psb && ext != ".psb") st->job.path = Narrow(Utf8Path(path).replace_extension(".psb").wstring());
    }
    if (!StartPsdExport(*v->doc, v->renderer, *ctx, st->job)) { Status("Échec de l'export PSD"); return; }
    psdExport = st;
    Status("Export PSD en cours...");
}

void App::PollPsdExport() {
    if (!psdExport) return;
    auto st = psdExport;
    if (!st->writing && PsdExportReady(st->job)) {
        st->writing = true;
        // Encoding big documents takes a while: keep the UI responsive.
        std::thread([st] {
            st->ok = WritePsd(st->job, &st->error);
            st->job.layers.clear();  // free the pixel copies early
            st->job.composite.reset();
            st->done = true;
        }).detach();
    }
    if (st->done) {
        Status(st->ok ? "Exporté : " + st->job.path : "Échec de l'export PSD : " + st->error);
        Log("PSD export %s: %s", st->ok ? "ok" : "failed", st->job.path.c_str());
        psdExport.reset();
    }
}

void App::ExportImage(const std::string& path) {
    DocView* v = Active();
    if (!v || path.empty()) return;
    gpu::Texture* img = v->renderer.EvaluateComplete(*v->doc, *ctx);
    if (!img) return;
    auto rb = v->renderer.ExportRGBA8(*ctx, img);
    if (rb) exports.push_back({path, rb, {}, exportQuality});
}

void App::Status(const std::string& msg) {
    statusMsg = msg;
    statusTime = Now();
    Log("%s", msg.c_str());
}

void App::Undo() {
    DocView* v = Active();
    if (!v || stroking) return;
    if (xf.active) { TransformCancel(*v); return; }
    u32 w = v->doc->Width(), h = v->doc->Height();
    v->history.Undo(*v->doc, *ctx);
    if (w != v->doc->Width() || h != v->doc->Height()) v->fitPending = true;
}

void App::Redo() {
    DocView* v = Active();
    if (!v || stroking || xf.active) return;
    u32 w = v->doc->Width(), h = v->doc->Height();
    v->history.Redo(*v->doc, *ctx);
    if (w != v->doc->Width() || h != v->doc->Height()) v->fitPending = true;
}

// ------------------------------------------------------------------ layout & menus

void App::DrawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    // Search index being built: the menus open invisibly (labels hidden meanwhile).
    const bool hideMenus = GlobalSearch::Get().MenusHidden();
    if (hideMenus) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f);
    DocView* v = Active();
    if (logoTex) {
        float h = ImGui::GetFrameHeight() - 2;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1);
        ImGui::Image((ImTextureID)gpu::Dev().UiTextureId(logoTex.get()), ImVec2(h, h));
        if (ImGui::IsItemClicked()) showAbout = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("À propos de Shaderixel");
    } else {
        ImGui::TextColored(ImVec4(0.6f, 0.5f, 1.0f, 1.0f), "Shaderixel");
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("Fichier")) {
        if (ImGui::MenuItem("Nouveau...", "Ctrl+N")) showNewDoc = true;
        if (ImGui::MenuItem("Nouveau projet Paint 3D...")) p3dNew.open = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Texturer un modèle 3D : canaux PBR, calques de peinture et de matériau,\nprojection de décalques, export Unreal / Unity (fichier .sx3d, espace de travail dédié).");
        if (ImGui::MenuItem("Ouvrir une image...", "Ctrl+O")) OpenImage(platform::OpenFileDialog(hwnd, kImageFilter), false);
        if (ImGui::MenuItem("Importer comme calque...", "Ctrl+Shift+O", false, v != nullptr)) OpenImage(platform::OpenFileDialog(hwnd, kImageFilter), true);
        if (ImGui::MenuItem("Importer (objet dynamique incorporé)...", nullptr, false, v != nullptr))
            PlaceSmartFile(*v, platform::OpenFileDialog(hwnd, L"Images et documents Shaderixel (sous-graphe)\0*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.psd;*.hdr;*.uasset;*.sxl;*.sx3d\0"), false);
        if (ImGui::MenuItem("Importer (objet dynamique lié)...", nullptr, false, v != nullptr))
            PlaceSmartFile(*v, platform::OpenFileDialog(hwnd, L"Images et documents Shaderixel (sous-graphe)\0*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.psd;*.hdr;*.uasset;*.sxl;*.sx3d\0"), true);
        if (ImGui::MenuItem("Importer un effet HLSL...", nullptr, false, v != nullptr)) {
            std::string path = platform::OpenFileDialog(hwnd, L"HLSL (*.hlsl, *.fx, *.txt)\0*.hlsl;*.fx;*.txt\0");
            std::string src, err;
            if (!path.empty() && ReadTextFile(path, src) && !CreateEffectFromHlsl(*v, src, &err)) Status("Import HLSL : " + err);
        }
        if (ImGui::BeginMenu("Ouvrir les fichiers récents", !recentFiles.empty())) {
            std::string open;
            for (auto& r : recentFiles) {
                std::string name = Narrow(Utf8Path(r).filename().wstring());
                if (ImGui::MenuItem((name + "##" + r).c_str())) open = r;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", r.c_str());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Effacer la liste")) {
                recentFiles.clear();
                AddRecent("");
            }
            ImGui::EndMenu();
            if (!open.empty()) OpenImage(open, false);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Enregistrer", "Ctrl+S", false, v != nullptr)) SaveCurrent(false);
        if (ImGui::MenuItem("Enregistrer sous...", "Ctrl+Maj+S", false, v != nullptr)) SaveCurrent(true);
        if (ImGui::MenuItem("Enregistrer comme modèle...", nullptr, false, v != nullptr)) tplUi.saveRequest = true;
        if (v && v->link) {
            if (ImGui::MenuItem("Envoyer vers Unreal", "Ctrl+S")) SendToUnreal(*v);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Écrit les textures et les réimporte dans Unreal (plugin Shaderixel Link).");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exporter...", "Ctrl+Alt+Maj+W", false, v != nullptr))
            ExportImage(platform::SaveFileDialog(hwnd, L"PNG\0*.png\0JPEG\0*.jpg\0TGA\0*.tga\0BMP\0*.bmp\0", L"png"));
        if (ImGui::BeginMenu("Importer des ressources")) {
            if (ImGui::MenuItem("Pinceaux Photoshop (.abr)...")) {
                std::string f = platform::OpenFileDialog(hwnd, L"Pinceaux Photoshop (*.abr)\0*.abr\0");
                if (!f.empty()) ImportAbrFile(f);
            }
            if (ImGui::MenuItem("Motifs Photoshop (.pat)...")) {
                std::string f = platform::OpenFileDialog(hwnd, L"Motifs Photoshop (*.pat)\0*.pat\0");
                if (!f.empty()) ImportPatFile(f);
            }
            if (ImGui::MenuItem("Nuancier (.aco, .ase, .gpl)...")) {
                std::string f = platform::OpenFileDialog(hwnd, L"Nuanciers (*.aco, *.ase, *.gpl)\0*.aco;*.ase;*.gpl\0");
                if (!f.empty()) { LoadSwatchLibrary(f); swatchesLoaded = true; }
            }
            ImGui::EndMenu();
        }
        if (v && v->doc->kind == 1 && ImGui::MenuItem("Exporter pour Unreal (modèle + matériau)...")) unrealDialog = true;
        if (v && v->doc->kind == 1 && ImGui::MenuItem("Cuisson des cartes (baker)...")) baker.open = true;
        if (v && v->doc->kind == 1 && ImGui::MenuItem("Exporter les textures du projet...")) {
            texExport.open = true;
            texExport.preset = v->doc->exportTemplate == 1 ? 0 : v->doc->exportTemplate == 2 ? 1 : 5;
        }
        if (ImGui::MenuItem("Exporter en TIFF CMJN...", nullptr, false, v != nullptr))
            ExportCmykTiff(*v, platform::SaveFileDialog(hwnd, L"TIFF CMJN (*.tif)\0*.tif;*.tiff\0", L"tif"));
        if (ImGui::MenuItem("Exporter les plans de travail...", nullptr, false, v != nullptr)) {
            std::string f = platform::SaveFileDialog(hwnd, L"PNG\0*.png\0JPEG\0*.jpg\0", L"png");
            if (!f.empty()) ExportArtboards(*v, f);
        }
        if (ImGui::MenuItem("Exporter les compositions de calques...", nullptr, false, v && !v->doc->S().comps.empty())) {
            std::string f = platform::SaveFileDialog(hwnd, L"PNG\0*.png\0JPEG\0*.jpg\0", L"png");
            if (!f.empty()) ExportComps(*v, f);
        }
        if (ImGui::BeginMenu("Automatisation")) {
            if (ImGui::MenuItem("Éditeur d'actions")) { panelOpen["Éditeur d'actions"].open = true; actionEditorFocus = true; }
            if (ImGui::MenuItem("Traitement par lots...")) batch.open = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Photomerge (panorama)...")) { photoDlg.open = true; photoDlg.kind = 1; }
            if (ImGui::MenuItem("Fusion HDR...")) { photoDlg.open = true; photoDlg.kind = 4; }
            ImGui::Separator();
            if (ImGui::MenuItem("Actions")) { panelOpen["Actions"].open = true; ShowSideTab(1); ImGui::SetWindowFocus("Actions"); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Exporter en PSD (calques)", v != nullptr)) {
            if (ImGui::MenuItem("8 bits par couche...")) ExportPsd(platform::SaveFileDialog(hwnd, L"Photoshop (*.psd)\0*.psd\0Grand format (*.psb)\0*.psb\0", L"psd"), 8);
            if (ImGui::MenuItem("16 bits par couche...")) ExportPsd(platform::SaveFileDialog(hwnd, L"Photoshop (*.psd)\0*.psd\0Grand format (*.psb)\0*.psb\0", L"psd"), 16);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Fermer", "Ctrl+W", false, v != nullptr)) CloseDocument(activeDoc);
        if (ImGui::MenuItem("Quitter", "Alt+F4")) PostMessageW(hwnd, WM_CLOSE, 0, 0);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Édition")) {
        if (ImGui::MenuItem("Annuler", "Ctrl+Z", false, v && v->history.CanUndo())) Undo();
        if (ImGui::MenuItem("Rétablir", "Ctrl+Maj+Z", false, v && v->history.CanRedo())) Redo();
        ImGui::Separator();
        bool canFill = v && v->doc->PaintTarget();
        if (ImGui::MenuItem("Couper", "Ctrl+X", false, canFill)) CopyToClipboard(*v, true);
        if (ImGui::MenuItem("Copier", "Ctrl+C", false, canFill)) CopyToClipboard(*v, false);
        if (ImGui::MenuItem("Coller", "Ctrl+V", false, clipboard != nullptr)) PasteClipboard(*v);
        ImGui::Separator();
        if (ImGui::MenuItem("Remplir avec la couleur de premier plan", "Alt+Retour", false, canFill)) FillOrErase(*v, false);
        if (ImGui::MenuItem("Effacer", "Suppr", false, canFill)) FillOrErase(*v, true);
        if (ImGui::MenuItem("Contour...", nullptr, false, canFill && v->doc->Selection() != nullptr)) selDialog = 5;
        DrawColorEditMenu();
        ImGui::Separator();
        if (ImGui::MenuItem("Définir un motif", nullptr, false, v != nullptr)) DefinePattern(*v);
        if (ImGui::MenuItem("Remplir avec un motif", nullptr, false, canFill)) FillWithPattern(*v, patternSel, patternScale);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Motif choisi dans le panneau Motifs (Peinture).");
        ImGui::Separator();
        if (ImGui::MenuItem("Alignement automatique des calques...", nullptr, false, v != nullptr)) { photoDlg.open = true; photoDlg.kind = 2; }
        if (ImGui::MenuItem("Fusion automatique des calques...", nullptr, false, v != nullptr)) { photoDlg.open = true; photoDlg.kind = 3; }
        ImGui::Separator();
        if (ImGui::MenuItem("Remplir d'après le contenu", "Maj+F5", false, canFill && v->doc->Selection() != nullptr && !fillJob.active))
            StartContentAwareFill(*v);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reconstruit la zone sélectionnée à partir du reste de l'image (PatchMatch, sans IA).");
        ImGui::Separator();
        if (ImGui::MenuItem("Transformation manuelle", "Ctrl+T", false, canFill)) TransformBegin(*v, false);
        if (ImGui::MenuItem("Déformation", nullptr, false, canFill)) WarpBegin(*v);
        if (ImGui::MenuItem("Marionnette", nullptr, false, canFill)) MeshWarpBegin(*v, 1);
        if (ImGui::MenuItem("Déformation de la perspective", nullptr, false, canFill)) MeshWarpBegin(*v, 2);
        ImGui::Separator();
        ImGui::MenuItem("Erreurs de shaders", nullptr, &showShaderErrors);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Calque", v != nullptr)) {
        LayerMenuItems();
        ImGui::Separator();
        SmartObjectsMenu(v);
        ImGui::EndMenu();
    }
    DrawImageMenu();
    DrawSelectionMenu();
    if (ImGui::BeginMenu("Filtre", v != nullptr)) {
        Document& d = *v->doc;
        bool layerMode = d.Mode() == DocMode::Layers;
        Layer* l = d.ActiveLayer();
        if (layerMode && !l) ImGui::TextDisabled("Aucun calque actif");
        EffectMenu([&](const std::string& id) {
            if (!layerMode || l) ApplyLibraryEffect(*v, id, FxApply::Filter);
        }, false, true);
        if (ImGui::BeginMenu("Appliquer aux pixels (destructif)", d.GetSurface(d.PaintTarget()) != nullptr)) {
            EffectMenu([&](const std::string& id) { ApplyLibraryEffect(*v, id, FxApply::Destructive); }, false, true);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Camera Raw...", "Ctrl+Maj+A", false, !layerMode || l)) OpenCameraRaw(*v);
        if (ImGui::MenuItem("Galerie d'effets...")) ImGui::SetWindowFocus("Effets");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Affichage")) {
        DrawPanelsMenu();
        ImGui::Separator();
        if (v) {
            if (ImGui::MenuItem("Adapter à l'écran", "Ctrl+0")) v->fitPending = true;
            if (ImGui::MenuItem("100 %", "Ctrl+1")) v->zoom = 1.0f;
            ImGui::Separator();
            if (ImGui::BeginMenu("Mode de vue")) {
                if (ImGui::MenuItem("Image finale", "Ctrl+Alt+1", v->viewMode == 0)) v->viewMode = 0;
                if (ImGui::MenuItem("Calque sélectionné seul", "Ctrl+Alt+2", v->viewMode == 1)) v->viewMode = 1;
                if (ImGui::MenuItem("Jusqu'au calque sélectionné", "Ctrl+Alt+3", v->viewMode == 2)) v->viewMode = 2;
                ImGui::EndMenu();
            }
            DrawProofMenu();
            ImGui::MenuItem("Fusion en sRGB", nullptr, &v->doc->blendInGamma);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mélange les calques en sRGB (comme Photoshop) au lieu de l'espace linéaire.");
            ImGui::Separator();
            ImGui::MenuItem("Règles", "Ctrl+R", &showRulers);
            ImGui::MenuItem("Repères", "Ctrl+;", &showGuides);
            ImGui::MenuItem("Grille", "Ctrl+'", &showGrid);
            ImGui::MenuItem("Magnétisme", "Ctrl+Maj+;", &snapEnabled);
            if (ImGui::BeginMenu("Paramètres de la grille")) {
                ImGui::SetNextItemWidth(120 * dpi);
                ImGui::DragFloat("Pas (px)", &gridStep, 1, 4, 4000, "%.0f");
                ImGui::SetNextItemWidth(120 * dpi);
                ImGui::SliderInt("Subdivisions", &gridSubdiv, 1, 10);
                ImGui::EndMenu();
            }
            bool anyGuide = !v->doc->S().guidesX.empty() || !v->doc->S().guidesY.empty();
            if (ImGui::MenuItem("Effacer les repères", nullptr, false, anyGuide)) {
                Edit e(*v->doc, v->history, "Effacer les repères");
                v->doc->S().guidesX.clear();
                v->doc->S().guidesY.clear();
                e.Commit();
            }
        }
        ImGui::EndMenu();
    }
    DrawWindowMenu();
    DrawPluginsMenu();
    if (ImGui::BeginMenu("Aide")) {
        std::wstring guide = Utf8Path(ExeDir()).wstring() + L"\\Guide.html";
        if (ImGui::MenuItem("Guide de test", nullptr, false, std::filesystem::exists(guide)))
            ShellExecuteW(nullptr, L"open", guide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        ImGui::Separator();
        if (ImGui::MenuItem("À propos de Shaderixel")) showAbout = true;
        ImGui::EndMenu();
    }
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16);
    DrawSearchBox();
    // Document tabs in the menu bar.
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16);
    if (ImGui::BeginTabBar("##docs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (int i = 0; i < (int)docs.size(); ++i) {
            bool open = true;
            ImGuiTabItemFlags flags = (focusDocRequest == i ? ImGuiTabItemFlags_SetSelected : 0) | (docs[i]->doc->dirty ? ImGuiTabItemFlags_UnsavedDocument : 0);
            ImGui::PushID(i);
            if (ImGui::BeginTabItem(docs[i]->doc->Name().c_str(), &open, flags)) {
                if (focusDocRequest < 0 || focusDocRequest == i) activeDoc = i;
                ImGui::EndTabItem();
            }
            ImGui::PopID();
            if (!open) { CloseDocument(i); break; }
        }
        focusDocRequest = -1;
        ImGui::EndTabBar();
    }
    if (hideMenus) ImGui::PopStyleVar();
    ImGui::EndMainMenuBar();
}

void App::LayerMenuItems() {
    DocView* v = Active();
    if (!v) return;
    Document& d = *v->doc;
    bool layerMode = d.Mode() == DocMode::Layers;
    if (layerMode) {
        if (ImGui::MenuItem("Nouveau plan de travail")) NewArtboard(*v);
        if (ImGui::MenuItem("Nouveau ton direct...")) spotDialog = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Le premier plan de travail reprend la zone de travail et ses calques ;\nles suivants s'ajoutent à droite (la zone de travail s'agrandit).");
        if (ImGui::MenuItem("Nouveau calque", "Ctrl+Maj+N")) {
            RecordAction("new_layer");
            Edit e(d, v->history, "Nouveau calque");
            d.AddPixelLayer("Calque " + std::to_string(d.S().layers.size() + 1));
            e.Commit();
        }
        if (ImGui::BeginMenu("Nouveau calque de remplissage")) {
            EffectMenu([&](const std::string& id) {
                Edit e(d, v->history, "Calque de remplissage");
                MaskFromSelection(*v, d.AddEffectLayer(LayerKind::Fill, id).id, false);
                e.Commit();
            }, true, false);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Nouveau calque HLSL personnalisé")) {
            Edit e(d, v->history, "Calque HLSL");
            Layer& l = d.AddEffectLayer(LayerKind::Adjustment, "");
            l.name = "HLSL personnalisé";
            l.effect.customCode = kCustomEffectTemplate;
            if (auto def = l.effect.Def()) l.effect.params = def->DefaultParams();
            d.SyncGraph();
            e.Commit();
            ImGui::SetWindowFocus("Propriétés");
        }
        if (ImGui::MenuItem("Nouveau calque « Effet visuel » (graphe)")) {
            Edit e(d, v->history, "Effet visuel");
            Layer& l = d.AddEffectLayer(LayerKind::Adjustment, "");
            l.name = "Effet visuel";
            l.effect.graph = EffectGraph::Default().Serialize();
            d.SyncGraph();
            e.Commit();
            ImGui::SetWindowFocus("Éditeur d'effets");
        }
        if (ImGui::BeginMenu("Nouveau calque de réglage")) {
            EffectMenu([&](const std::string& id) {
                Edit e(d, v->history, "Calque de réglage");
                MaskFromSelection(*v, d.AddEffectLayer(LayerKind::Adjustment, id).id, false);
                e.Commit();
            }, false, true);
            ImGui::EndMenu();
        }
        Layer* l = d.ActiveLayer();
        ImGui::Separator();
        if (ImGui::MenuItem("Dupliquer le calque", "Ctrl+J", false, l != nullptr)) {
            Edit e(d, v->history, "Dupliquer le calque");
            d.DuplicateLayer(l->id);
            e.Commit();
        }
        if (ImGui::MenuItem("Supprimer le calque", nullptr, false, l != nullptr)) {
            Edit e(d, v->history, "Supprimer le calque");
            d.RemoveLayer(l->id);
            e.Commit();
        }
        if (ImGui::MenuItem("Fusionner vers le bas", "Ctrl+E", false, l != nullptr)) MergeDown(*v);
        if (ImGui::MenuItem("Fusionner les calques visibles", "Ctrl+Maj+E")) MergeVisible(*v, false);
        if (ImGui::MenuItem("Aplatir l'image")) MergeVisible(*v, true);
        ImGui::Separator();
        if (ImGui::MenuItem("Grouper", "Ctrl+G", false, l != nullptr)) {
            Edit e(d, v->history, "Grouper");
            d.GroupLayer(l->id);
            e.Commit();
        }
        if (ImGui::MenuItem("Dissocier", "Ctrl+Maj+G", false, l && l->kind == LayerKind::Group)) {
            Edit e(d, v->history, "Dissocier");
            d.Ungroup(l->id);
            e.Commit();
        }
        if (ImGui::MenuItem("Masque d'écrêtage", "Ctrl+Alt+G", l && l->clipped, l && l->kind != LayerKind::Group)) {
            Edit e(d, v->history, "Masque d'écrêtage");
            l->clipped = !l->clipped;
            d.SyncGraph();
            e.Commit();
        }
        if (ImGui::MenuItem("Pixelliser le calque", nullptr, false, l && (l->kind == LayerKind::Text || l->kind == LayerKind::Shape))) {
            Edit e(d, v->history, "Pixelliser");
            l->kind = LayerKind::Pixel;
            d.SyncGraph();
            e.Commit();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Ajouter un masque de fusion", nullptr, false, l && !l->mask)) {
            Edit e(d, v->history, "Ajouter un masque");
            MaskFromSelection(*v, l->id, true);
            e.Commit();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Convertir en mode nodal")) {
            Edit e(d, v->history, "Convertir en nœuds");
            d.ConvertToNodes();
            e.Commit();
        }
    } else {
        std::string why;
        bool can = d.CanConvertToLayers(&why);
        if (ImGui::MenuItem("Convertir en mode calques", nullptr, false, can)) {
            Edit e(d, v->history, "Convertir en calques");
            d.ConvertToLayers();
            e.Commit();
        }
        if (!can && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Impossible : %s", why.c_str());
    }
}

void App::EffectMenu(const std::function<void(const std::string&)>& onPick, bool generators, bool filters) {
    for (const std::string& cat : Effects().Categories()) {
        std::vector<EffectDefPtr> items;
        for (auto& e : Effects().All()) {
            if (e->category != cat) continue;
            bool isGen = e->inputs.empty();
            if ((isGen && generators) || (!isGen && filters)) items.push_back(e);
        }
        if (items.empty()) continue;
        if (ImGui::BeginMenu(cat.c_str())) {
            for (auto& e : items)
                if (ImGui::MenuItem(e->name.c_str())) onPick(e->id);
            ImGui::EndMenu();
        }
    }
}

void App::HandleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    spaceHeld = !io.WantTextInput && ImGui::IsKeyDown(ImGuiKey_Space);
    if (io.WantTextInput || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) return;
    DocView* v = Active();
    bool ctrl = io.KeyCtrl, shift = io.KeyShift, alt = io.KeyAlt;
    auto pressed = [](ImGuiKey k) { return ImGui::IsKeyPressed(k, false); };
    auto setTool = [&](Tool t) {
        if (xf.active && xf.view) TransformCommit(*xf.view);
        tool = t;
    };

    // Transform / polygon session keys first.
    if (xf.active && xf.view) {
        if (pressed(ImGuiKey_Enter) || pressed(ImGuiKey_KeypadEnter)) { TransformCommit(*xf.view); return; }
        if (pressed(ImGuiKey_Escape)) { TransformCancel(*xf.view); return; }
    }
    if (cropActive && v && tool == Tool::Crop) {
        if (pressed(ImGuiKey_Enter) || pressed(ImGuiKey_KeypadEnter)) { ApplyCrop(*v, CropRect()); cropActive = false; return; }
        if (pressed(ImGuiKey_Escape)) { cropActive = false; return; }
    }
    if (polyActive && v) {
        if (pressed(ImGuiKey_Enter) || pressed(ImGuiKey_KeypadEnter)) { FinishPolygon(*v); return; }
        if (pressed(ImGuiKey_Escape)) { polyActive = false; lassoPts.clear(); return; }
        if (pressed(ImGuiKey_Backspace) && tool == Tool::MagneticLasso) { MagneticLassoUndoAnchor(); return; }
        if (pressed(ImGuiKey_Backspace) && lassoPts.size() > 1) { lassoPts.pop_back(); return; }
    }

    if (pressed(ImGuiKey_Tab) && !ctrl && !alt) { (shift ? hideRight : hideBottom) ^= true; return; }
    for (int i = 0; i < kSideTabs; ++i)
        if (alt && !ctrl && pressed((ImGuiKey)(ImGuiKey_1 + i))) { ShowSideTab(i); return; }
    if (ctrl && alt && pressed(ImGuiKey_R) && v && v->doc->Selection()) {
        selectMask.open = true;
        selectMask.hash = 0;
    }
    if (ctrl && pressed(ImGuiKey_Z)) { shift ? Redo() : Undo(); }
    if (ctrl && !alt && pressed(ImGuiKey_Y)) Redo();
    if (ctrl && !shift && pressed(ImGuiKey_N)) showNewDoc = true;
    if (ctrl && !shift && pressed(ImGuiKey_O)) OpenImage(platform::OpenFileDialog(hwnd, kImageFilter), false);
    if (ctrl && shift && pressed(ImGuiKey_O) && v) OpenImage(platform::OpenFileDialog(hwnd, kImageFilter), true);
    if (ctrl && shift && ImGui::GetIO().KeyAlt && ImGui::IsKeyPressed(ImGuiKey_W, false) && v)
        ExportImage(platform::SaveFileDialog(hwnd, L"PNG\0*.png\0JPEG\0*.jpg\0", L"png"));
    if (ctrl && shift && pressed(ImGuiKey_E) && v && v->doc->Mode() == DocMode::Layers) MergeVisible(*v, false);
    if (ctrl && !shift && pressed(ImGuiKey_E) && v && v->doc->Mode() == DocMode::Layers) MergeDown(*v);
    if (ctrl && !shift && !ImGui::GetIO().KeyAlt && pressed(ImGuiKey_W) && v) { CloseDocument(activeDoc); return; }
    if (ctrl && pressed(ImGuiKey_S) && v) { SaveCurrent(shift); return; }
    if (!v) return;
    Document& d = *v->doc;
    if (ctrl && pressed(ImGuiKey_0)) v->fitPending = true;
    if (ctrl && !shift && pressed(ImGuiKey_R)) showRulers = !showRulers;
    if (ctrl && !shift && pressed(ImGuiKey_Semicolon)) showGuides = !showGuides;
    if (ctrl && shift && pressed(ImGuiKey_Semicolon)) snapEnabled = !snapEnabled;
    if (ctrl && pressed(ImGuiKey_Apostrophe)) showGrid = !showGrid;
    if (ctrl && pressed(ImGuiKey_1)) v->zoom = 1.0f;

    // Selection
    if (ctrl && shift && !alt && pressed(ImGuiKey_A)) OpenCameraRaw(*v);
    if (ctrl && !shift && pressed(ImGuiKey_A))
        v->renderer.SelectShape(d, *ctx, v->history, SelShape::All, {}, SelMode::Replace, false, 0, "Tout sélectionner"), RecordSelect("all");
    if (ctrl && alt && !shift && v) {
        if (pressed(ImGuiKey_1)) v->viewMode = 0;
        if (pressed(ImGuiKey_2)) v->viewMode = 1;
        if (pressed(ImGuiKey_3)) v->viewMode = 2;
    }
    if (ctrl && alt && pressed(ImGuiKey_Y)) {
        if (shift) proof.gamutWarning = !proof.gamutWarning;
        else proof.on = !proof.on;
    }
    if (ctrl && !shift && pressed(ImGuiKey_D)) v->renderer.Deselect(d, v->history), RecordSelect("none");
    if (ctrl && shift && pressed(ImGuiKey_D)) v->renderer.Reselect(d, v->history);
    if (ctrl && shift && pressed(ImGuiKey_I)) v->renderer.SelectionFilter(d, *ctx, v->history, 3, 0, "Intervertir"), RecordSelect("invert");
    if (shift && pressed(ImGuiKey_F6) && d.S().selection) { selDialog = 1; selDialogValue = 8; }
    if (ctrl && !shift && pressed(ImGuiKey_T)) TransformBegin(*v, false);
    if (ctrl && alt && pressed(ImGuiKey_I)) { imageDialog = 1; dlgW = (int)d.Width(); dlgH = (int)d.Height(); }
    if (ctrl && alt && pressed(ImGuiKey_C)) { imageDialog = 2; dlgW = (int)d.Width(); dlgH = (int)d.Height(); dlgAnchor = 4; }

    // Clipboard & layer via copy
    if (ctrl && !shift && pressed(ImGuiKey_C)) CopyToClipboard(*v, false);
    if (ctrl && !shift && pressed(ImGuiKey_X)) CopyToClipboard(*v, true);
    if (ctrl && !shift && pressed(ImGuiKey_V)) PasteClipboard(*v);
    if (d.Mode() == DocMode::Layers) {
        if (ctrl && shift && pressed(ImGuiKey_N)) {
            RecordAction("new_layer");
            Edit e(d, v->history, "Nouveau calque");
            d.AddPixelLayer("Calque " + std::to_string(d.S().layers.size() + 1));
            e.Commit();
        }
        Layer* al = d.ActiveLayer();
        if (al && ctrl && !alt && pressed(ImGuiKey_G)) {
            Edit e(d, v->history, shift ? "Dissocier" : "Grouper");
            if (shift) d.Ungroup(al->id); else d.GroupLayer(al->id);
            e.Commit();
            al = nullptr;
        }
        if (al && ctrl && alt && pressed(ImGuiKey_G) && al->kind != LayerKind::Group) {
            Edit e(d, v->history, "Masque d'écrêtage");
            al->clipped = !al->clipped;
            d.SyncGraph();
            e.Commit();
            al = nullptr;
        }
        if (al && ctrl && (pressed(ImGuiKey_RightBracket) || pressed(ImGuiKey_LeftBracket))) {
            Edit e(d, v->history, "Déplacer le calque");
            d.MoveLayerSibling(al->id, pressed(ImGuiKey_RightBracket) ? 1 : -1);
            e.Commit();
        }
        if (ctrl && pressed(ImGuiKey_J) && d.ActiveLayer()) {
            if (d.S().selection || shift) LayerViaCopy(*v, shift);
            else {
                Edit e(d, v->history, "Dupliquer le calque");
                d.DuplicateLayer(d.S().activeLayer);
                e.Commit();
            }
        }
    }
    if (alt && !ctrl && (pressed(ImGuiKey_Enter) || pressed(ImGuiKey_Backspace))) FillOrErase(*v, false);
    if (ctrl || alt) return;

    if (pressed(ImGuiKey_Delete) && d.S().selection && d.PaintTarget()) FillOrErase(*v, true);
    if (pressed(ImGuiKey_Escape) && selDragging) selDragging = false;

    if (tool == Tool::LayerTransform) {
        // Transform tool: W / E / R / P pick its mode (as in Unreal), Espace: world / relative axes.
        if (pressed(ImGuiKey_W)) { lxf.mode = 0; return; }
        if (pressed(ImGuiKey_E)) { lxf.mode = 1; return; }
        if (pressed(ImGuiKey_R)) { lxf.mode = 2; return; }
        if (pressed(ImGuiKey_P)) { lxf.mode = 3; return; }
    }
    if (pressed(ImGuiKey_A)) setTool(Tool::LayerTransform);
    if (pressed(ImGuiKey_V)) setTool(Tool::Move);
    if (pressed(ImGuiKey_M)) setTool(shift ? (tool == Tool::RectSelect ? Tool::EllipseSelect : Tool::RectSelect) : Tool::RectSelect);
    if (pressed(ImGuiKey_L)) {
        // L cycles through the lassos (Shift+L: polygonal).
        if (shift) setTool(Tool::PolyLasso);
        else setTool(tool == Tool::Lasso ? Tool::MagneticLasso : Tool::Lasso);
    }
    if (pressed(ImGuiKey_W)) setTool(shift ? (tool == Tool::Wand ? Tool::QuickSelect : Tool::Wand) : Tool::QuickSelect);
    if (pressed(ImGuiKey_C) && !graphFocused) setTool(Tool::Crop);  // in the graph, C frames the selection in a comment
    if (pressed(ImGuiKey_T)) setTool(Tool::Text);
    if (pressed(ImGuiKey_U)) setTool(Tool::Shape);
    if (pressed(ImGuiKey_B)) setTool(Tool::Brush);
    if (pressed(ImGuiKey_S)) setTool(Tool::Clone);
    if (pressed(ImGuiKey_J)) {
        // J cycles: correcteur -> pièce -> déplacement basé sur le contenu -> yeux rouges.
        Tool cyc[] = {Tool::Heal, Tool::Patch, Tool::ContentMove, Tool::RedEye};
        int k = 0;
        for (int i = 0; i < 4; ++i) if (tool == cyc[i]) k = shift ? (i + 1) % 4 : i;
        setTool(shift ? cyc[k] : (tool == cyc[0] || tool == cyc[1] || tool == cyc[2] || tool == cyc[3]) ? tool : Tool::Heal);
    }
    if (pressed(ImGuiKey_Y)) setTool(Tool::HistoryBrush);
    if (shift && pressed(ImGuiKey_F5) && d.S().selection && d.PaintTarget()) StartContentAwareFill(*v);
    if (pressed(ImGuiKey_O)) setTool(shift ? (tool == Tool::Dodge ? Tool::Burn : tool == Tool::Burn ? Tool::Sponge : Tool::Dodge) : Tool::Dodge);
    if (pressed(ImGuiKey_R)) setTool(shift ? (tool == Tool::Blur ? Tool::Sharpen : tool == Tool::Sharpen ? Tool::Smudge : Tool::Blur) : Tool::Blur);
    if (pressed(ImGuiKey_E)) setTool(shift ? (tool == Tool::Eraser ? Tool::BackgroundEraser : tool == Tool::BackgroundEraser ? Tool::MagicEraser : Tool::Eraser) : Tool::Eraser);
    if (pressed(ImGuiKey_I)) setTool(Tool::Picker);
    if (pressed(ImGuiKey_G)) setTool(shift && tool == Tool::Gradient ? Tool::Bucket : Tool::Gradient);
    if (pressed(ImGuiKey_K)) setTool(Tool::Bucket);
    if (pressed(ImGuiKey_P)) setTool(Tool::Pen);
    if (tool == Tool::Pen && ctrl && ImGui::IsKeyPressed(ImGuiKey_Enter) && v) PathToSelection(*v, SelMode::Replace);
    if (pressed(ImGuiKey_H)) setTool(Tool::Hand);
    if (pressed(ImGuiKey_Z)) setTool(Tool::Zoom);
    if (pressed(ImGuiKey_Q)) quickMask = !quickMask;
    if (pressed(ImGuiKey_X)) std::swap(fg, bg);
    if (pressed(ImGuiKey_D)) { fg = {0, 0, 0, 1}; bg = {1, 1, 1, 1}; }

    bool selTool = tool == Tool::RectSelect || tool == Tool::EllipseSelect || tool == Tool::Lasso || tool == Tool::PolyLasso || tool == Tool::Wand || tool == Tool::QuickSelect ||
                   tool == Tool::MagneticLasso;
    if (selTool && v->doc->Selection() && !xf.active) {
        int step = shift ? 10 : 1;
        int2 o = {0, 0};
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) o.x -= step;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) o.x += step;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) o.y -= step;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) o.y += step;
        if (o.x || o.y) v->renderer.OffsetSelection(*v->doc, *ctx, v->history, o, "Déplacer la sélection");
    }
    if (tool == Tool::Move && !xf.active) {
        float step = shift ? 10.0f : 1.0f;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) Nudge(*v, -step, 0);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) Nudge(*v, step, 0);
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) Nudge(*v, 0, -step);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) Nudge(*v, 0, step);
    }

    if (tool == Tool::QuickSelect) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) qselSize = std::max(2.0f, qselSize * 0.85f - 1);
        if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) qselSize = std::min(500.0f, qselSize * 1.18f + 1);
        return;
    }
    BrushSettings& b = CurrentBrush();
    if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
        if (shift) b.hardness = std::max(0.0f, b.hardness - 0.1f);
        else b.size = std::max(1.0f, b.size * 0.85f - 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
        if (shift) b.hardness = std::min(1.0f, b.hardness + 0.1f);
        else b.size = std::min(5000.0f, b.size * 1.18f + 1);
    }
}

// ------------------------------------------------------------------ small panels

void App::DrawStatusBar() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float h = ImGui::GetFrameHeight();
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
    if (ImGui::BeginViewportSideBar("##status", vp, ImGuiDir_Down, h, flags)) {
        if (ImGui::BeginMenuBar()) {
            DocView* v = Active();
            if (v) {
                if (v->link) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.45f, 0.05f, 1));
                    if (ImGui::SmallButton("Envoyer vers Unreal")) SendToUnreal(*v);
                    ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Document ouvert depuis Unreal (plugin Shaderixel Link) : Ctrl+S renvoie aussi les textures.");
                    bool live = LinkLive(*v);
                    if (ImGui::Checkbox("Direct", &live)) SetLinkLive(*v, live);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Envoyer les textures vers Unreal tout seul, un instant après chaque modification\n(le matériau se met à jour dans Unreal sans réimporter à la main).");
                    ImGui::Separator();
                }
                ImGui::Text("%.0f %%", v->zoom * 100.0f);
                ImGui::Separator();
                ImGui::Text("%u x %u  %s  %s", v->doc->Width(), v->doc->Height(),
                            v->doc->GetPrecision() == Precision::Float32 ? "32 bits" : "16 bits",
                            v->doc->Mode() == DocMode::Layers ? "Calques" : "Nodal");
                ImGui::Separator();
                {
                    // View: final image / active layer only / up to the active layer.
                    static const char* modes[] = {"Vue : finale", "Vue : calque seul", "Vue : jusqu'au calque"};
                    ImGui::SetNextItemWidth(ImGui::CalcTextSize(modes[2]).x + ImGui::GetFrameHeight() * 1.6f);
                    if (ImGui::BeginCombo("##viewmode", modes[std::clamp(v->viewMode, 0, 2)], ImGuiComboFlags_HeightSmall)) {
                        for (int i = 0; i < 3; ++i)
                            if (ImGui::Selectable(modes[i], v->viewMode == i)) v->viewMode = i;
                        ImGui::EndCombo();
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ctrl+Alt+1 / 2 / 3");
                    if (v->viewMode) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1, 0.55f, 0.1f, 1), "(aperçu partiel)");
                    }
                }
                ImGui::Separator();
                if (cursorInDoc) ImGui::Text("X %d  Y %d", (int)cursorDoc.x, (int)cursorDoc.y);
                else ImGui::TextDisabled("X -  Y -");
                ImGui::Separator();
                if (int left = v->renderer.TilesPending(); left > 0 && v->doc->tiled) {
                    int total = std::max(1, v->renderer.TilesTotal());
                    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1, 1), "Rendu %d %%", (int)(100.0f * (total - left) / total));
                    ImGui::SameLine();
                    ImGui::ProgressBar((float)(total - left) / total, ImVec2(90, 0), "");
                } else if (v->renderer.AnyPending()) ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Compilation des shaders...");
            }
            if (!statusMsg.empty() && Now() - statusTime < 6.0) {
                ImGui::Separator();
                ImGui::TextUnformatted(statusMsg.c_str());
            }
            auto failedCount = gpu::Shaders().FailedPrograms().size();
            if (failedCount) {
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.35f, 0.4f, 1));
                if (ImGui::SmallButton((std::to_string(failedCount) + " erreur(s) de shader").c_str())) showShaderErrors = true;
                ImGui::PopStyleColor();
            }
            char right[200], sparse[48] = "";
            if (u64 sb = gpu::Dev().SparseBytes()) snprintf(sparse, sizeof(sparse), " (creux %.0f Mo)", sb / (1024.0 * 1024.0));
            snprintf(right, sizeof(right), "%s   VRAM %.0f Mo%s   %.0f i/s", gpu::Dev().AdapterName().c_str(),
                     gpu::Dev().VideoMemoryUsage() / (1024.0 * 1024.0), sparse, ImGui::GetIO().Framerate);
            // Zone toggles (Tab / Maj+Tab), left of the GPU info.
            const char* tb = hideBottom ? "Afficher le graphe" : "Masquer le graphe";
            const char* tr = hideRight ? "Afficher les panneaux" : "Masquer les panneaux";
            float bw = ImGui::CalcTextSize(tb).x + ImGui::CalcTextSize(tr).x + ImGui::GetStyle().FramePadding.x * 4 + ImGui::GetStyle().ItemSpacing.x * 2;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::CalcTextSize(right).x - 32 - bw);
            if (ImGui::SmallButton(tb)) hideBottom = !hideBottom;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Zone du bas (graphe, éditeur d'effets) - Tab");
            if (ImGui::SmallButton(tr)) hideRight = !hideRight;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Colonne de droite (onglets, calques) - Maj+Tab");
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::CalcTextSize(right).x - 16);
            ImGui::TextDisabled("%s", right);
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
}

void App::DrawShaderErrors() {
    if (!showShaderErrors) return;
    ImGui::SetNextWindowSize(ImVec2(700 * dpi, 400 * dpi), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Erreurs de shaders", &showShaderErrors)) {
        auto failed = gpu::Shaders().FailedPrograms();
        if (failed.empty()) ImGui::TextDisabled("Aucune erreur.");
        ImGui::PushFont(ui::GetFonts().mono, 0.0f);
        for (auto& p : failed) {
            if (ImGui::CollapsingHeader(p->name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                ImGui::TextWrapped("%s", p->errors.c_str());
        }
        ImGui::PopFont();
    }
    ImGui::End();
}

// ------------------------------------------------------------------ histogram

void App::UpdateHistogram() {
    DocView* v = Active();
    if (!v) { histo.valid = false; return; }
    if (histo.rb && histo.rb->ready) {
        const u32* p = (const u32*)histo.rb->data.data();
        for (int c = 0; c < 4; ++c) {
            u64 peak = 1;
            for (int i = 0; i < 256; ++i) {
                histo.bins[c][i] = p[c * 256 + i];
                if (i > 0 && i < 255) peak = std::max(peak, histo.bins[c][i]);
            }
            // Scale on the inner bins so pure black/white spikes don't flatten the rest.
            for (int i = 0; i < 256; ++i) histo.norm[c][i] = std::min(1.0f, (float)histo.bins[c][i] / (float)peak);
        }
        histo.valid = true;
        histo.rb.reset();
    }
    u64 frame = gpu::Dev().FrameNumber();
    bool stale = histo.view != v || histo.version != v->renderer.ImageVersion();
    if (!histo.rb && stale && v->lastImage && frame - histo.frame >= 8) {
        histo.rb = v->renderer.Histogram(*ctx, v->lastImage);
        histo.view = v;
        histo.version = v->renderer.ImageVersion();
        histo.frame = frame;
    }
}

void App::DrawHistogram() {
    if (!ImGui::Begin("Histogramme", PanelFlag("Histogramme"))) { ImGui::End(); return; }
    const char* channels[] = {"Couleurs", "Luminosité", "Rouge", "Vert", "Bleu"};
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
    ImGui::Combo("Couche", &histoChannel, channels, IM_ARRAYSIZE(channels));
    if (!histo.valid) {
        ImGui::TextDisabled("Aucune image");
        ImGui::End();
        return;
    }
    float w = ImGui::GetContentRegionAvail().x, h = std::clamp(ImGui::GetContentRegionAvail().y - 70 * dpi, 60 * dpi, 220 * dpi);
    ImVec2 p0 = ImGui::GetCursorScreenPos(), p1(p0.x + w, p0.y + h);
    ImGui::InvisibleButton("##histo", ImVec2(w, h));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 22, 255), 3);
    auto plot = [&](int c, ImU32 col) {
        for (int i = 0; i < 256; ++i) {
            float x0 = p0.x + w * i / 256.0f, x1 = p0.x + w * (i + 1) / 256.0f;
            dl->AddRectFilled(ImVec2(x0, p1.y - histo.norm[c][i] * (h - 2)), ImVec2(std::max(x1, x0 + 1), p1.y), col);
        }
    };
    int stat = 0;  // channel the statistics refer to
    if (histoChannel == 0) {
        // Additive RGB overlay (white where all three overlap).
        dl->PushClipRect(p0, p1, true);
        plot(1, IM_COL32(230, 60, 60, 110));
        plot(2, IM_COL32(60, 210, 80, 110));
        plot(3, IM_COL32(70, 110, 240, 110));
        dl->PopClipRect();
    } else {
        stat = histoChannel - 1;
        static const ImU32 cols[] = {IM_COL32(200, 200, 210, 220), IM_COL32(230, 70, 70, 220), IM_COL32(70, 210, 90, 220), IM_COL32(80, 120, 245, 220)};
        plot(stat, cols[stat]);
    }
    dl->AddRect(p0, p1, IM_COL32(70, 72, 84, 255), 3);
    // Statistics (weights are alpha x255).
    double total = 0, sum = 0, sum2 = 0;
    for (int i = 0; i < 256; ++i) {
        double n = (double)histo.bins[stat][i];
        total += n;
        sum += n * i;
        sum2 += n * i * i;
    }
    double mean = total > 0 ? sum / total : 0, sd = total > 0 ? std::sqrt(std::max(0.0, sum2 / total - mean * mean)) : 0;
    int median = 0;
    for (double acc = 0; median < 256; ++median) {
        acc += (double)histo.bins[stat][median];
        if (acc >= total * 0.5) break;
    }
    if (ImGui::IsItemHovered()) {
        int lvl = std::clamp((int)((ImGui::GetIO().MousePos.x - p0.x) / w * 256.0f), 0, 255);
        dl->AddLine(ImVec2(p0.x + w * (lvl + 0.5f) / 256.0f, p0.y), ImVec2(p0.x + w * (lvl + 0.5f) / 256.0f, p1.y), IM_COL32(255, 255, 255, 90));
        ImGui::SetTooltip("Niveau %d\nNombre %.0f\nPourcentage %.2f %%", lvl, histo.bins[stat][lvl] / 255.0,
                          total > 0 ? 100.0 * histo.bins[stat][lvl] / total : 0.0);
    }
    ImGui::Text("Moyenne %.2f   Écart type %.2f", mean, sd);
    ImGui::Text("Médiane %d   Pixels %.0f", std::min(median, 255), total / 255.0);
    if (histoChannel == 0) ImGui::TextDisabled("Statistiques : luminosité");
    ImGui::End();
}


void App::DrawHistory() {
    if (!ImGui::Begin("Historique", PanelFlag("Historique"))) { ImGui::End(); return; }
    DocView* v = Active();
    if (v) {
        History& h = v->history;
        if (Id t = v->doc->PaintTarget()) {
            if (ImGui::SmallButton("Nouvel instantané")) TakeHistorySnapshot(*v, t);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Source de la forme d'historique (Y) pour le calque actif : son état actuel.\nSans instantané, la source est l'état du calque avant sa première retouche.");
            ImGui::SameLine();
            ImGui::TextDisabled(v->snapshots.count(t) ? "(instantané défini)" : "");
            ImGui::Separator();
        }
        if (ImGui::Selectable("Ouverture", h.Cursor() == 0)) while (h.CanUndo()) h.Undo(*v->doc, *ctx);
        for (size_t i = 0; i < h.Count(); ++i) {
            bool applied = i < h.Cursor();
            ImGui::PushID((int)i);
            if (!applied) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            if (ImGui::Selectable(h.Label(i).c_str(), i + 1 == h.Cursor())) {
                while (h.Cursor() > i + 1) h.Undo(*v->doc, *ctx);
                while (h.Cursor() < i + 1) h.Redo(*v->doc, *ctx);
            }
            if (!applied) ImGui::PopStyleColor();
            ImGui::PopID();
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4) ImGui::SetScrollHereY(1.0f);
    }
    ImGui::End();
}

float4 App::LinearColor(const float4& c) const {
    return {SrgbToLinear(c.x), SrgbToLinear(c.y), SrgbToLinear(c.z), c.w};
}

// ------------------------------------------------------------------ native documents

void App::SaveCurrent(bool saveAs) {
    DocView* v = Active();
    if (!v) return;
    if (xf.active && xf.view == v) TransformCommit(*v);
    if (v->smartLink.parent) {
        // Inner document of a smart object: saving = updating the object (already live).
        v->smartLink.syncKey = 0;
        v->doc->dirty = false;
        Status("Objet dynamique mis à jour");
        return;
    }
    if (v->link && !saveAs) {
        // Opened from Unreal: saving sends the textures back (and keeps the project next to the session).
        SendToUnreal(*v);
        return;
    }
    std::string path = v->doc->path;
    std::string ext = Narrow(Utf8Path(path).extension().wstring());
    for (auto& ch : ext) ch = (char)tolower((unsigned char)ch);
    const bool p3d = v->doc->kind == 1;
    if (saveAs || path.empty() || ext != (p3d ? ".sx3d" : ".sxl")) {
        path = p3d ? platform::SaveFileDialog(hwnd, L"Projet Paint 3D Shaderixel (*.sx3d)\0*.sx3d\0", L"sx3d")
                   : platform::SaveFileDialog(hwnd, L"Document Shaderixel (*.sxl)\0*.sxl\0", L"sxl");
        if (path.empty()) return;
    }
    SaveDocument(*v, path);
}

void App::SaveDocument(DocView& v, const std::string& path, bool asCopy) {
    Document& d = *v.doc;
    if (!demoMode && !asCopy) AddRecent(path);
    auto job = std::make_unique<PendingSave>();
    job->path = path;
    job->meta = SerializeDocument(d);
    for (Id id : ReferencedSurfaces(d)) {
        Surface* s = d.GetSurface(id);
        SurfaceBlob b;
        b.id = id;
        b.width = s->tex->Width();
        b.height = s->tex->Height();
        b.format = s->tex->GetFormat();
        job->blobs.push_back(std::move(b));
        job->readbacks.push_back(ctx->ReadbackTexture(s->tex.get()));
    }
    // Smart objects used by layers: contents + inner document (its surfaces get ids above kSmartChildBlobBit).
    Json smartArr = Json::Array();
    u32 smartIndex = 0;
    for (auto& [sid, so] : d.smartObjects) {
        bool used = false;
        for (auto& l : d.S().layers) used |= l.kind == LayerKind::Smart && l.smart == sid;
        for (auto& gn : d.S().graph.nodes) {
            used |= (gn.kind == NodeKind::Tool || gn.kind == NodeKind::Action) && gn.smart == sid;  // native-size tool results
            for (auto& [inner, s] : gn.execSmart) used |= s == sid;
        }
        if (!used || !so->content) continue;
        Json o = Json::Object();
        o["id"] = sid;
        o["name"] = so->name;
        if (!so->linkedPath.empty()) o["linked"] = so->linkedPath;
        SurfaceBlob cb;
        cb.id = sid;
        cb.width = so->content->Width();
        cb.height = so->content->Height();
        cb.format = so->content->GetFormat();
        job->blobs.push_back(std::move(cb));
        job->readbacks.push_back(ctx->ReadbackTexture(so->content.get()));
        Document* child = so->child.get();
        for (auto& dv : docs)
            if (dv->smartLink.parent == &v && dv->smartLink.smart == sid) child = dv->doc.get();
        if (child && smartIndex < 512) {
            Id base = kSmartChildBlobBit | (smartIndex << 22);
            Json cj = SerializeDocument(*child);
            cj["blobBase"] = base;
            for (Id cid : ReferencedSurfaces(*child)) {
                Surface* cs = child->GetSurface(cid);
                if (!cs || cid >= (1u << 22)) continue;
                SurfaceBlob b;
                b.id = base | cid;
                b.width = cs->tex->Width();
                b.height = cs->tex->Height();
                b.format = cs->tex->GetFormat();
                job->blobs.push_back(std::move(b));
                job->readbacks.push_back(ctx->ReadbackTexture(cs->tex.get()));
            }
            o["child"] = cj;
            ++smartIndex;
        }
        smartArr.Push(o);
    }
    if (smartArr.Size()) job->meta["smartObjects"] = smartArr;
    // Paint 3D project: the mesh travels with the file.
    if (d.kind == 1 && v.mesh) {
        job->meta["mesh"] = MeshToJson(v.mesh->data);
        if (!d.unrealSettings.empty()) {
            Json u;
            if (Json::Parse(d.unrealSettings, u)) job->meta["unreal"] = u;
        }
        Json cam = Json::Array();
        for (float f : {v.mesh->yaw, v.mesh->pitch, v.mesh->distance, v.mesh->target.x, v.mesh->target.y, v.mesh->target.z}) cam.Push(f);
        job->meta["camera"] = cam;
        {
            // 3D view rendering: post-process, quality, key light.
            const MeshScene& m = *v.mesh;
            Json vj = Json::Object();
            vj["post"] = PostToJson(m.post);
            vj["quality"] = m.quality;
            vj["lightFollow"] = m.lightFollow;
            vj["lightYaw"] = m.lightYaw;
            vj["lightPitch"] = m.lightPitch;
            vj["lightIntensity"] = m.lightIntensity;
            vj["sunIntensity"] = m.sunIntensity;
            vj["shadow"] = m.shadowStrength;
            vj["ao"] = m.aoStrength;
            vj["ground"] = m.groundShadow;
            vj["ssao"] = m.ssao;
            if (m.envAtlas && !m.envPath.empty()) {
                vj["env"] = m.envPath;
                vj["envIntensity"] = m.envIntensity;
                vj["envRotation"] = m.envRotation;
                vj["envBackground"] = m.envBackground;
                vj["envBlur"] = m.envBlur;
            }
            auto farr = [](std::initializer_list<float> l) {
                Json a = Json::Array();
                for (float f : l) a.Push(f);
                return a;
            };
            Json lj = Json::Array();
            for (const MeshLight& x : m.lights) {
                Json o = Json::Object();
                o["on"] = x.on;
                o["follow"] = x.follow;
                o["yaw"] = x.yaw;
                o["pitch"] = x.pitch;
                o["color"] = farr({x.color.x, x.color.y, x.color.z});
                o["intensity"] = x.intensity;
                lj.Push(o);
            }
            vj["lights"] = lj;
            vj["dofAperture"] = m.dofAperture;
            vj["dofFocus"] = m.dofFocus;
            Json cj = Json::Array();
            for (const CameraBookmark& c : m.cameras) {
                Json o = Json::Object();
                o["name"] = c.name;
                o["view"] = farr({c.yaw, c.pitch, c.distance, c.fov, c.target.x, c.target.y, c.target.z});
                cj.Push(o);
            }
            vj["cameras"] = cj;
            job->meta["view3d"] = vj;
        }
        if (v.render) job->meta["render"] = RenderToJson(*v.render, Narrow(Utf8Path(path).parent_path().wstring()));
    }
    if (!asCopy) {  // a copy (template...) leaves the document as it is
        d.path = path;
        if (!v.link) d.SetName(Narrow(Utf8Path(path).filename().wstring()));  // linked: keeps the Unreal asset name
        d.dirty = false;
    }
    saves.push_back(std::move(job));
    Status("Enregistrement de " + path + "...");
    if (!pluginListeners.empty()) PluginEvent(SX_EVENT_DOCUMENT_SAVED, path);
}

void App::UpdateSaves() {
    for (size_t i = 0; i < saves.size();) {
        PendingSave& s = *saves[i];
        if (!s.started) {
            bool ready = std::all_of(s.readbacks.begin(), s.readbacks.end(), [](auto& r) { return r && r->ready; });
            if (ready) {
                for (size_t k = 0; k < s.blobs.size(); ++k) s.blobs[k].data = std::move(s.readbacks[k]->data);
                s.readbacks.clear();
                s.started = true;
                PendingSave* p = &s;
                s.writing = std::async(std::launch::async, [p] {
                    std::string err;
                    auto t0 = std::chrono::steady_clock::now();
                    bool ok = WriteSxl(p->path, p->meta, p->blobs, &err);
                    u64 raw = 0;
                    for (auto& b : p->blobs) raw += b.data.size();
                    Log("Saved %s: %.0f MB of pixels in %.2f s", p->path.c_str(), raw / 1048576.0,
                        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
                    return ok ? std::string() : err;
                });
            }
            ++i;
            continue;
        }
        if (s.writing.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            std::string err = s.writing.get();
            Status(err.empty() ? "Enregistré : " + s.path : "Échec de l'enregistrement : " + err);
            saves.erase(saves.begin() + i);
        } else {
            ++i;
        }
    }
}

bool App::LoadSxl(const std::string& path) {
    Json meta;
    std::vector<SurfaceBlob> blobs;
    std::string err;
    if (!ReadSxl(path, meta, blobs, &err)) {
        Status("Ouverture impossible : " + err);
        return false;
    }
    auto doc = DeserializeDocument(meta, blobs, &err);
    if (!doc) {
        Status("Document invalide : " + err);
        return false;
    }
    auto upload = [&](Document& target, const std::vector<SurfaceBlob>& list) {
        for (auto& b : list) {
            Surface* s = target.GetSurface(b.id);
            if (!s || !s->tex || s->tex->Width() != b.width || s->tex->Height() != b.height || s->tex->GetFormat() != b.format) continue;
            if (b.data.size() < (size_t)b.width * b.height * gpu::BytesPerPixel(b.format)) continue;
            ctx->Upload(s->tex.get(), b.data.data(), b.width * gpu::BytesPerPixel(b.format));
            target.TouchSurface(b.id);
        }
    };
    upload(*doc, blobs);
    // Smart objects: contents (blob with the object id) and inner documents.
    for (auto& o : meta["smartObjects"].Elements()) {
        auto so = std::make_shared<SmartObject>();
        so->id = o["id"].U();
        so->name = o["name"].S("Objet dynamique");
        so->linkedPath = o["linked"].S("");
        if (!so->linkedPath.empty()) {
            // Reloaded from the file at the next check if it changed since the save.
            std::error_code ec;
            auto t = std::filesystem::last_write_time(Utf8Path(so->linkedPath), ec);
            so->linkedTime = ec ? 0 : (long long)t.time_since_epoch().count();
        }
        for (auto& b : blobs)
            if (b.id == so->id && b.data.size() >= (size_t)b.width * b.height * gpu::BytesPerPixel(b.format)) {
                so->content = gpu::Dev().CreateTexture(b.width, b.height, b.format, "SmartContent");
                ctx->Upload(so->content.get(), b.data.data(), b.width * gpu::BytesPerPixel(b.format));
                so->width = b.width;
                so->height = b.height;
            }
        if (o.Has("child")) {
            Id base = o["child"]["blobBase"].U();
            std::vector<SurfaceBlob> childBlobs;
            for (auto& b : blobs)
                if (b.id >= kSmartChildBlobBit && (b.id & 0xFFC00000u) == base) {
                    SurfaceBlob c = b;
                    c.id = b.id & 0x3FFFFFu;
                    childBlobs.push_back(std::move(c));
                }
            std::string cerr;
            if (auto child = DeserializeDocument(o["child"], childBlobs, &cerr)) {
                upload(*child, childBlobs);
                child->dirty = false;
                so->child = std::move(child);
            }
        }
        if (so->content) doc->smartObjects[so->id] = so;
    }
    doc->path = path;
    doc->SetName(Narrow(Utf8Path(path).filename().wstring()));
    doc->dirty = false;
    auto view = std::make_unique<DocView>();
    view->doc = std::move(doc);
    if (meta.Has("mesh")) {
        MeshData m;
        if (MeshFromJson(meta["mesh"], m)) AttachMesh(*view, std::move(m));
        if (meta.Has("unreal")) view->doc->unrealSettings = meta["unreal"].Dump(0);
        if (view->mesh && meta.Has("view3d")) {
            MeshScene& m = *view->mesh;
            const Json& vj = meta["view3d"];
            m.post = PostFromJson(vj["post"]);
            m.quality = vj["quality"].I(1);
            m.lightFollow = vj["lightFollow"].B(true);
            m.lightYaw = vj["lightYaw"].F(m.lightYaw);
            m.lightPitch = vj["lightPitch"].F(m.lightPitch);
            m.lightIntensity = vj["lightIntensity"].F(m.lightIntensity);
            m.sunIntensity = vj["sunIntensity"].F(m.sunIntensity);
            m.shadowStrength = vj["shadow"].F(m.shadowStrength);
            m.aoStrength = vj["ao"].F(m.aoStrength);
            m.groundShadow = vj["ground"].B(true);
            m.ssao = vj["ssao"].F(0.8f);
            if (const std::string env = vj["env"].S(""); !env.empty()) {
                std::string err;
                if (LoadEnvironment(m, env, &err)) {
                    m.envIntensity = vj["envIntensity"].F(1);
                    m.envRotation = vj["envRotation"].F(0);
                    m.envBackground = vj["envBackground"].B(true);
                    m.envBlur = vj["envBlur"].F(0);
                } else {
                    Log("HDRI de la vue 3D : %s (%s)", err.c_str(), env.c_str());
                }
            }
            m.lights.clear();
            for (size_t i = 0; i < vj["lights"].Size() && (int)i < kMaxMeshLights; ++i) {
                const Json& o = vj["lights"].At(i);
                MeshLight x;
                x.on = o["on"].B(true);
                x.follow = o["follow"].B(true);
                x.yaw = o["yaw"].F();
                x.pitch = o["pitch"].F(0.5f);
                if (o["color"].Size() == 3) x.color = {o["color"].At(0).F(1), o["color"].At(1).F(1), o["color"].At(2).F(1)};
                x.intensity = o["intensity"].F(1);
                m.lights.push_back(x);
            }
            m.dofAperture = vj["dofAperture"].F(0);
            m.dofFocus = vj["dofFocus"].F(2.8f);
            m.cameras.clear();
            for (size_t i = 0; i < vj["cameras"].Size(); ++i) {
                const Json& o = vj["cameras"].At(i);
                const Json& a = o["view"];
                if (a.Size() != 7) continue;
                CameraBookmark c;
                c.name = o["name"].S("");
                c.yaw = a.At(0).F();
                c.pitch = a.At(1).F();
                c.distance = a.At(2).F(2.8f);
                c.fov = a.At(3).F(0.8f);
                c.target = {a.At(4).F(), a.At(5).F(), a.At(6).F()};
                m.cameras.push_back(c);
            }
        }
        const Json& cam = meta["camera"];
        if (view->mesh && cam.Size() == 6) {
            view->mesh->yaw = cam.At(0).F();
            view->mesh->pitch = cam.At(1).F();
            view->mesh->distance = cam.At(2).F(2.8f);
            view->mesh->target = {cam.At(3).F(), cam.At(4).F(), cam.At(5).F()};
        }
        if (view->mesh && meta.Has("render")) RenderFromJson(*view, meta["render"], Narrow(Utf8Path(path).parent_path().wstring()));
    }
    docs.push_back(std::move(view));
    activeDoc = (int)docs.size() - 1;
    focusDocRequest = activeDoc;
    Status("Ouvert : " + path);
    return true;
}

void App::DrawClosePrompt() {
    if (closePrompt >= (int)docs.size()) closePrompt = -1;
    if (closePrompt >= 0 && !ImGui::IsPopupOpen("##closeprompt")) ImGui::OpenPopup("##closeprompt");
    if (!ImGui::BeginPopupModal("##closeprompt", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) return;
    if (closePrompt < 0) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    DocView& v = *docs[closePrompt];
    ImGui::Text("Enregistrer les modifications de « %s » ?", v.doc->Name().c_str());
    ImGui::TextDisabled("Sinon, elles seront perdues.");
    ImGui::Spacing();
    int idx = closePrompt;
    if (ImGui::Button("Enregistrer", ImVec2(130 * dpi, 0))) {
        activeDoc = idx;
        SaveCurrent(false);
        if (!v.doc->dirty) CloseDocument(idx, true);  // dialog not cancelled
        closePrompt = -1;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Ne pas enregistrer", ImVec2(150 * dpi, 0))) {
        CloseDocument(idx, true);
        closePrompt = -1;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Annuler", ImVec2(110 * dpi, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        closePrompt = -1;
        quitting = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void App::UpdateTitle() {
    DocView* v = Active();
    std::string t = v ? v->doc->Name() + (v->doc->dirty ? " *" : "") + " - Shaderixel" : "Shaderixel";
    if (t != lastTitle) {
        platform::SetTitle(hwnd, t);
        lastTitle = t;
    }
}

}  // namespace sx
