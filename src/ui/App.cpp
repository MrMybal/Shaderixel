#include "ui/App.h"
#include "ai/Comfy.h"
#include "io/UAsset.h"
#include "ui/Filter.h"
#include "ui/Lang.h"
#include "ui/Search.h"
#include "io/ColorMgmt.h"
#include "io/WicLoad.h"
#include "doc/HlslImport.h"
#include "doc/Brushes.h"
#include "ui/Theme.h"
#include "gpu/ShaderSystem.h"
#include "io/PsdImport.h"
#include "core/SysStats.h"

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
    L"Unreal Engine (*.uasset : textures, maillages)\0*.uasset\0Photos RAW\0*.dng;*.cr2;*.cr3;*.crw;*.nef;*.nrw;*.arw;*.srf;*.sr2;*.orf;*.rw2;*.raf;*.pef;*.srw;*.x3f;*.3fr;*.fff;*.erf;*.kdc;*.dcr;*.mrw;*.mos;*.mef;*.iiq;*.rwl;*.raw;*.gpr;*.cap\0Tous les fichiers\0*.*\0";
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
            demoMode = m == "nodes" ? 2 : m == "select" ? 3 : m == "transform" ? 4 : m == "transform-undo" ? 5 : m == "retouch" ? 6 : m == "groups" ? 7 : m == "save" ? 8 : m == "image" ? 9 : m == "curves" ? 10 : m == "effectgraph" ? 11 : m == "text" ? 12 : m == "shapes" ? 13 : m == "mesh" ? 14 : m == "tiles" ? 15 : m == "16k" ? 16 : m == "psd" ? 17 : m == "blendif" ? 18 : m == "liquify" ? 19 : m == "lut" ? 20 : m == "selmove" ? 21 : m == "quicksel" ? 22 : m == "pen" ? 23 : m == "merge" ? 24 : m == "guides" ? 25 : m == "stroke" ? 26 : m == "gltf" ? 27 : m == "pbr" ? 28 : m == "hdri" ? 29 : m == "genfill" ? 30 : m == "psd16" ? 31 : m == "selpath" ? 32 : m == "warp" ? 33 : m == "comfy" ? 34 : m == "sliders" ? 35 : m == "fxbrowser" ? 36 : m == "codefx" ? 37 : m == "chanblend" ? 38 : m == "chanlinks" ? 39 : m == "cygallery" ? 40 : m == "brushes" ? 41 : m == "workspace" ? 42 : m == "graphs" ? 43 : m == "dnd" ? 44 : m == "material" ? 45 : m == "custompins" ? 46 : m == "hlslimport" ? 47 : m == "tablet" ? 48 : m == "smart" ? 49 : m == "selectmask" ? 50 : m == "maglasso" ? 51 : m == "sky" ? 52 : m == "fill" ? 53 : m == "meshwarp" ? 54 : m == "actions" ? 55 : m == "artboards" ? 56 : m == "color" ? 57 : m == "photo" ? 58 : m == "textstyles" ? 59 : m == "resources" ? 60 : m == "booleans" ? 61 : m == "fxlib" ? 62 : m == "paint3d" ? 63 : m == "uv" ? 64 : m == "example3d" ? 65 : m == "render" ? 66 : m == "example3dx" ? 67 : m == "linksend" ? 68 : m == "drop" ? 69 : m == "uassetconv" ? 70 : m == "layerxf" ? 71 : m == "graphtools" ? 72 : m == "functions" ? 73 : m == "typed" ? 74 : m == "library" ? 75 : m == "trims" ? 76 : m == "nodeui" ? 77 : m == "maketemplates" ? 78 : m == "makescenes" ? 79 : m == "comments" ? 80 : m == "execjoin" ? 81 : m == "call" ? 82 : m == "inpaint" ? 83 : m == "explorer" ? 84 : m == "actioneditor" ? 85 : m == "popout" ? 86 : m == "paint3dtools" ? 87 : m == "smartmat" ? 88 : m == "bake" ? 89 : m == "simulation" ? 90 : m == "projection" ? 91 : m == "smartthumbs" ? 93 : m == "plugins" ? 94 : m == "search" ? 95 : m == "cameraraw" ? 96 : m == "view3d" ? 97 : m == "nodesel" ? 98 : m == "filter" ? 99 : m == "sidetabs" ? 100 : m == "analysis" ? 101 : m == "customshapes" ? 102 : m == "psdadjust" ? 103 : m == "undoram" ? 104 : m == "fluid" ? 105 : m == "multimat" ? 106 : m == "carve" ? 107 : m == "anim" ? 108 : m == "perspcrop" ? 109 : 1;
            if (demoMode > 1) ++i;
        }
        else if (a == "--format-library" && i + 1 < argc) {
            // Lays out the compacted HLSL of the library effects of a folder, then quits (maintenance).
            int n = FormatLibraryCode(Narrow(argv[++i]));
            Log("format-library: %d file(s) rewritten", n);
            ExitProcess(0);
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
        // Every node of the effect graphs, alone on the output (its pins with their defaults).
        int oko = 0, no = 0;
        for (auto& od : EffectGraphOps()) {
            if (onlyHlsl) break;
            const std::string op = od.op;
            if (op == "output" || op == "custom" || op == "texture" || op == "function" || !OpAllowed(od, EGKind::Effect)) continue;  // a texture only feeds HLSL nodes
            ++no;
            EffectGraph g = EffectGraph::Default();
            EGNode n;
            n.id = g.nextId++;
            n.op = op;
            n.inputs.resize(od.inputs.size());
            g.nodes.push_back(n);
            if (EGNode* o = g.Find(g.OutputNode())) {
                for (auto& l : o->inputs) l = {};
                o->inputs.resize(3);
                o->inputs[1] = {n.id, 0};
            }
            EGCompileResult r = CompileEffectGraph(g);
            std::string why = r.error;
            if (why.empty()) {
                auto def = ParseEffect("", r.code);
                auto p = gpu::Shaders().CompileNow("op " + op, BuildEffectSource(*def));
                if (p->state.load() != gpu::Program::State::Ready) why = p->errors;
            }
            if (why.empty()) {
                EGUnrealExport ue = ExportEffectGraphToUnreal(g, "M_Test", "/Game/Test");
                if (!ue.error.empty()) why = "Unreal : " + ue.error;
            }
            oko += why.empty();
            if (!why.empty()) Log("[test-ops] %-14s FAILED: %s", op.c_str(), why.c_str());
        }
        {
            // Functions: the image -> invert inside a function -> the output, collapsed then compiled and expanded.
            ++no;
            EffectGraph g = EffectGraph::Default();
            Id in = 0, outId = g.OutputNode();
            for (auto& n : g.nodes) if (n.op == "input") in = n.id;
            EGNode inv;
            inv.id = g.nextId++;
            inv.op = "oneminus";
            inv.inputs = {EGLink{in, 0}};
            g.nodes.push_back(inv);
            if (EGNode* o = g.Find(outId)) { o->inputs.resize(3); o->inputs[1] = {inv.id, 0}; }
            const std::string before = CompileEffectGraph(g).code;
            const Id fn = CollapseToFunction(g, {inv.id}, "Inverser");
            EGCompileResult r = CompileEffectGraph(g);
            const EGNode* fnode = g.Find(fn);
            std::string why = !fn ? "rien à regrouper" : !r.error.empty() ? r.error : r.code.find("(1.0 - ") == std::string::npos ? "code sans l'inversion" :
                              !fnode || fnode->pins.size() != 1 || fnode->outs.size() != 1 ? "interface de la fonction" : "";
            if (why.empty() && (!ExpandFunction(g, fn) || g.Find(fn))) why = "dégrouper";
            if (why.empty() && CompileEffectGraph(g).error.size()) why = "après dégroupage : " + CompileEffectGraph(g).error;
            if (why.empty()) ++oko;
            else Log("[test-ops] %-14s FAILED: %s", "function", why.c_str());
            (void)before;
        }
        {
            // Nested functions and pins kept by identity: F (luma, 1 - luma; two outputs) holding G (1 - x).
            ++no;
            EffectGraph g = EffectGraph::Default();
            Id in = 0, outId = g.OutputNode();
            for (auto& n : g.nodes) if (n.op == "input") in = n.id;
            EGNode lu, inv;
            lu.id = g.nextId++;
            lu.op = "luma";
            lu.inputs = {EGLink{in, 0}};
            g.nodes.push_back(lu);
            inv.id = g.nextId++;
            inv.op = "oneminus";
            inv.inputs = {EGLink{lu.id, 0}};
            g.nodes.push_back(inv);
            if (EGNode* o = g.Find(outId)) { o->inputs.resize(3); o->inputs[0] = {inv.id, 0}; o->inputs[1] = {lu.id, 0}; }
            // Follows reroutes back to the op feeding the output's input i.
            auto source = [](const EffectGraph& f, int i) -> std::string {
                const EGNode* o = f.Find(f.OutputNode());
                EGLink l = o && i < (int)o->inputs.size() ? o->inputs[i] : EGLink{};
                for (int guard = 0; guard < 16 && l.node; ++guard) {
                    const EGNode* n = f.Find(l.node);
                    if (!n) return "?";
                    if (n->op != "reroute") return n->op;
                    l = n->inputs.empty() ? EGLink{} : n->inputs[0];
                }
                return "-";
            };
            std::string why;
            const Id F = CollapseToFunction(g, {lu.id, inv.id}, "F");
            EGNode* fn = g.Find(F);
            if (!fn || fn->outs.size() != 2) why = "F : interface";
            if (why.empty()) {
                EffectGraph inner;
                EffectGraph::Deserialize(fn->text, inner);
                const EffectGraph oldInner = inner;
                Id innerInv = 0;
                for (auto& n : inner.nodes) if (n.op == "oneminus") innerInv = n.id;
                if (!CollapseToFunction(inner, {innerInv}, "G")) why = "G : rien à regrouper";
                // Its outputs swapped inside (order fields): the parent's links must follow.
                std::vector<EGNode*> outs;
                for (auto& n : inner.nodes) if (n.op == "funcout") outs.push_back(&n);
                if (outs.size() == 2) std::swap(outs[0]->index, outs[1]->index);
                fn->text = inner.Serialize();
                SyncFunctionInterface(*fn, inner, oldInner, g);
                std::string err;
                const EffectGraph flat = FlattenFunctions(g, &err);
                if (why.empty() && !err.empty()) why = err;
                if (why.empty() && (source(flat, 0) != "oneminus" || source(flat, 1) != "luma"))
                    why = "sorties mélangées : " + source(flat, 0) + " / " + source(flat, 1);
                if (why.empty() && !CompileEffectGraph(g).error.empty()) why = CompileEffectGraph(g).error;
            }
            if (why.empty()) ++oko;
            else Log("[test-ops] %-14s FAILED: %s", "nested func", why.c_str());
        }
        Log("[test-ops] %d / %d graph nodes compiled", oko, no);
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
    ui::InstallFilter();
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
    if (const char* forced = getenv("SX_LANG")) uiLanguage = forced;  // tests
    if (!SetLanguage(uiLanguage)) SetLanguage("fr");  // no dictionary found: the built-in French
    ApplyHistoryLimits();
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
    comfy::CancelAll();  // ComfyUI jobs waiting for a result stop (their futures are waited for on exit)
    if (baker.running && baker.cancel) baker.cancel->store(true);  // a long bake / unwrap stops instead of holding the exit
    if (uvJob) uvJob->cancel = true;
    carveJob.reset();  // stops and joins the seam carving thread
    animExport.reset();
    CancelUAssetConversions();  // a running Unreal conversion ends instead of holding the exit
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
        if (resetLayout || !node || !node->IsSplitNode()) {
            // Image workspace: the shipped default layout (library/layouts/image.ini) when present.
            std::string ini;
            if (layoutKind == 0)
                for (const std::string& f : {ExeDir() + "/layouts/default/image.ini", ExeDir() + "/../../library/layouts/image.ini"})
                    if (ReadTextFile(f, ini) && !ini.empty()) break;
            if (!ini.empty()) {
                BuildDockLayout(dock);  // side tab dockspaces exist, then the saved layout replaces everything
                pendingLayoutIni = ini;
                pendingLayoutName = "default";
            } else {
                BuildDockLayout(dock);
            }
        }
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
    if (frameCounter == 2) FocusPanel("Canevas");  // Canvas is the default center tab
    if (demoMode && frameCounter == 3) RunDemo();
    if (aboutAtStart && frameCounter == 3) showAbout = true;
    if (testEffects && capturePath.empty() && frameCounter == 3) autoQuit = true;  // compile test: quit once done
    if (demoMode) DemoTick();
    if (frameCounter >= 4 && frameCounter - 4 < focusAtStart.size()) FocusPanel(focusAtStart[frameCounter - 4].c_str());
    if (!focusAfterLayout.empty() && ImGui::FindWindowByName(focusAfterLayout.front().c_str())) {
        FocusPanel(focusAfterLayout.front().c_str());
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
    if (!panelOpen.count("Animation")) panelOpen["Animation"].open = false;  // opened from Fenêtre > Panneaux
    if (!ImGui::FindWindowByName("Animation"))
        if (ImGuiWindow* gw = ImGui::FindWindowByName("Graphe"); gw && gw->DockId) ImGui::SetNextWindowDockID(gw->DockId, ImGuiCond_FirstUseEver);
    if (Shown("Animation")) DrawAnimationPanel();
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
    if (Shown("Navigateur")) DrawNavigator();
    if (Shown("Analyse d'image")) {
        UpdateAnalysis();
        DrawAnalysis();
    }
    if (Shown("Effets")) DrawEffectBrowser();
    if (Shown("Fusions")) DrawBlendPanel();
    if (Shown("Matériaux")) DrawMaterialPanel();
    if (Shown("Pinceaux")) DrawBrushPanel();
    if (Shown("Aperçu du calque")) DrawLayerPreview();
    DrawLayoutSavePopup();
    DrawColorRangeWindow();
    DrawExportAsWindow();
    DrawApplyImageDialog();
    DrawCalculationsDialog();
    DrawFocusAreaWindow();
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
    for (auto& d : docs) d->history.Maintain(*ctx);  // older undo steps to system memory
    FluidTick();  // liquid paint running
    if (Active() != toolSessionsView) ResetToolSessions();
    // Finished async readbacks.
    for (size_t i = 0; i < exports.size();) {
        auto& e = exports[i];
        if (e.rb && e.rb->ready && !e.crop.Empty()) {
            // Part of the image (artboards): crop in place into a copy.
            auto c = std::make_shared<gpu::Readback>();
            c->width = (u32)e.crop.Width();
            c->height = (u32)e.crop.Height();
            c->ready = true;
            c->format = e.rb->format;
            const size_t bpp = e.rb->data.size() / std::max<size_t>(1, (size_t)e.rb->width * e.rb->height);
            c->data.resize((size_t)c->width * c->height * bpp);
            for (u32 y = 0; y < c->height; ++y)
                memcpy(&c->data[(size_t)y * c->width * bpp], &e.rb->data[(((size_t)(y + e.crop.y0)) * e.rb->width + e.crop.x0) * bpp], (size_t)c->width * bpp);
            e.rb = c;
            e.crop = {};
        }
        const bool wide = e.rb && e.rb->data.size() >= (size_t)e.rb->width * e.rb->height * 8;  // 16-bit TIFF readback
        if (e.rb && e.rb->ready && (e.trim || e.matte.w >= 0) && e.rb->data.size() >= (size_t)e.rb->width * e.rb->height * 4) {
            // Export As: transparent edges trimmed, then flattened on the background color (8 or 16 bits per channel).
            const u32 W = e.rb->width, H = e.rb->height;
            const size_t bpp = wide ? 8 : 4;
            auto alphaAt = [&](size_t i) -> u32 { return wide ? ((const u16*)e.rb->data.data())[i * 4 + 3] : e.rb->data[i * 4 + 3]; };
            u32 x0 = 0, y0 = 0, x1 = W, y1 = H;
            if (e.trim) {
                x0 = W; y0 = H; x1 = 0; y1 = 0;
                for (u32 y = 0; y < H; ++y)
                    for (u32 x = 0; x < W; ++x)
                        if (alphaAt((size_t)y * W + x)) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x + 1); y1 = std::max(y1, y + 1); }
                if (x1 <= x0) { x0 = y0 = 0; x1 = y1 = 1; }
            }
            auto c = std::make_shared<gpu::Readback>();
            c->width = x1 - x0;
            c->height = y1 - y0;
            c->ready = true;
            c->format = e.rb->format;
            c->data.resize((size_t)c->width * c->height * bpp);
            for (u32 y = 0; y < c->height; ++y)
                memcpy(&c->data[(size_t)y * c->width * bpp], &e.rb->data[(((size_t)(y + y0)) * W + x0) * bpp], (size_t)c->width * bpp);
            if (e.matte.w >= 0) {
                const float full = wide ? 65535.0f : 255.0f;
                const float m[3] = {e.matte.x * full, e.matte.y * full, e.matte.z * full};
                const size_t n = (size_t)c->width * c->height;
                for (size_t k = 0; k < n; ++k) {
                    if (wide) {
                        u16* q = (u16*)c->data.data() + k * 4;
                        const float a = q[3] / full;
                        for (int ch = 0; ch < 3; ++ch) q[ch] = (u16)std::lround(q[ch] * a + m[ch] * (1 - a));
                        q[3] = 65535;
                    } else {
                        u8* q = c->data.data() + k * 4;
                        const float a = q[3] / full;
                        for (int ch = 0; ch < 3; ++ch) q[ch] = (u8)std::lround(q[ch] * a + m[ch] * (1 - a));
                        q[3] = 255;
                    }
                }
            }
            e.rb = c;
            e.trim = false;
            e.matte.w = -1;
        }
        if (e.rb && e.rb->ready) {
            std::string ext = Narrow(Utf8Path(e.path).extension().wstring());
            for (auto& ch : ext) ch = (char)tolower(ch);
            int ok = 0;
            std::string native = e.path;
            if (ext == ".tif" || ext == ".tiff") {
                const u16* px = (const u16*)e.rb->data.data();
                bool alpha = false;
                if (e.rb->data.size() >= (size_t)e.rb->width * e.rb->height * 8)
                    for (size_t k = 3, n = (size_t)e.rb->width * e.rb->height * 4; k < n && !alpha; k += 4) alpha = px[k] != 65535;
                ok = e.rb->data.size() >= (size_t)e.rb->width * e.rb->height * 8 && SaveTiff16(native, px, (int)e.rb->width, (int)e.rb->height, alpha);
            } else if (ext == ".jpg" || ext == ".jpeg") ok = stbi_write_jpg(native.c_str(), e.rb->width, e.rb->height, 4, e.rb->data.data(), std::clamp(e.quality, 1, 100));
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
        const bool huge = (u64)w * h > 6000ull * 6000ull;  // tiled documents (sparse layers)
        if (mode == DocMode::Layers && huge && background != 1) {
            // A plain background of a very large document takes no memory: a color fill, and an empty pixel
            // layer above it to paint on (its memory is only used where it is painted).
            Layer& bgl = d.AddEffectLayer(LayerKind::Fill, "solid_color");
            bgl.name = "Arrière-plan";
            if (!bgl.effect.params.empty()) bgl.effect.params[0] = clear;
            Layer& paint = d.AddPixelLayer("Calque 1");
            d.S().activeLayer = paint.id;
            d.SyncGraph();
        } else if (mode == DocMode::Layers) {
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
    if (toolSessionsView == docs[index].get()) ResetToolSessions();  // previews owned by its renderer
    if (abCompare.view == docs[index].get()) {  // its before / after snapshot goes with it
        abCompare.snap.reset();
        abCompare.reduced.reset();
        abCompare.view = nullptr;
        abCompare.on = false;
    }
    if (fluidRun.view == docs[index].get()) fluidRun.active = false;  // the wet paint goes with its renderer
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

void App::ExportPsd(const std::string& path, int depth, DocView* target) {
    DocView* v = target ? target : Active();
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

void App::ExportImage(const std::string& path, DocView* target) {
    DocView* v = target ? target : Active();
    if (!v || path.empty()) return;
    gpu::Texture* img = v->renderer.EvaluateComplete(*v->doc, *ctx);
    if (!img) return;
    std::string ext = Narrow(Utf8Path(path).extension().wstring());
    for (auto& ch : ext) ch = (char)tolower((unsigned char)ch);
    // TIFF: 16 bits per channel (photo work); the other formats 8 bits.
    auto rb = ext == ".tif" || ext == ".tiff" ? v->renderer.ExportRGBA16(*ctx, img) : v->renderer.ExportRGBA8(*ctx, img);
    if (rb) exports.push_back({path, rb, {}, exportQuality});
}

void App::Status(const std::string& msg) {
    statusMsg = msg;
    statusTime = Now();
    Log("%s", msg.c_str());
}

void App::ResetToolSessions() {
    polyActive = false;
    lassoPts.clear();
    magLasso.rb.reset();
    magLasso.cost.clear();
    magLasso.live.clear();
    magLasso.anchors.clear();
    pen.drawing = pen.dragging = false;
    pen.before.reset();
    selectMask.mask = selectMask.preview = nullptr;  // textures of the other renderer
    selectMask.hash = 0;
    colorRange.mask = nullptr;
    colorRange.hash = 0;
    qsel.rb.reset();
    qsel.view = nullptr;
    qsel.pending = qsel.painting = false;
    qsel.pts.clear();
    cropActive = false;  // a crop frame drawn on another document
    toolSessionsView = Active();
}

namespace {
// After undo / redo: the layers chosen together stay chosen (those still there), unless the step restored a choice
// of its own (grouping clears it).
void KeepLayerChoice(Document& d, const std::vector<Id>& chosen) {
    if (chosen.empty() || !d.S().layerSel.empty()) return;
    std::vector<Id> keep;
    for (Id id : chosen)
        if (d.FindLayer(id)) keep.push_back(id);
    if (keep.size() > 1 && std::find(keep.begin(), keep.end(), d.S().activeLayer) != keep.end()) d.S().layerSel = keep;
}
}  // namespace

void App::Undo() {
    DocView* v = Active();
    if (!v || stroking) return;
    pen.drawing = false;  // the subpath being drawn may be the step undone
    if (fluidRun.active) FluidFinish(true);  // the wet paint becomes the last step (undone here)
    if (xf.active) { TransformCancel(*v); return; }
    u32 w = v->doc->Width(), h = v->doc->Height();
    const auto chosen = v->doc->S().layerSel;  // (choosing layers is not a step: the choice stays)
    v->history.Undo(*v->doc, *ctx);
    KeepLayerChoice(*v->doc, chosen);
    if (w != v->doc->Width() || h != v->doc->Height()) v->fitPending = true;
}

void App::Redo() {
    DocView* v = Active();
    if (!v || stroking || xf.active) return;
    pen.drawing = false;
    if (fluidRun.active) FluidFinish(true);
    u32 w = v->doc->Width(), h = v->doc->Height();
    const auto chosen = v->doc->S().layerSel;
    v->history.Redo(*v->doc, *ctx);
    KeepLayerChoice(*v->doc, chosen);
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
        ui::MenuFilter();
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
            ExportImage(platform::SaveFileDialog(hwnd, L"PNG\0*.png\0JPEG\0*.jpg\0TIFF 16 bits\0*.tif\0TGA\0*.tga\0BMP\0*.bmp\0", L"png"));
        if (ImGui::MenuItem("Exporter sous...", nullptr, false, v != nullptr)) exportAs.open = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Plusieurs tailles d'un coup (@2x, @3x), la sélection ou chaque calque à part, rognage, fond.");
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
            if (ImGui::MenuItem("Recadrer et redresser les photos", nullptr, false, v && v->doc->Mode() == DocMode::Layers)) StartCropPhotos(*v);
            if (ImGui::MenuItem("Planche contact...", nullptr, false, !contactSheet.running)) contactSheet.open = true;
            ImGui::SetItemTooltip("Plusieurs photos numérisées ensemble : chacune détectée sur le fond, redressée et ouverte dans son document");
            ImGui::Separator();
            if (ImGui::MenuItem("Actions")) { panelOpen["Actions"].open = true; ShowSideTab(1); FocusPanel("Actions"); }
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
        ui::MenuFilter();
        if (ImGui::MenuItem("Annuler", "Ctrl+Z", false, v && v->history.CanUndo())) Undo();
        if (ImGui::MenuItem("Rétablir", "Ctrl+Maj+Z", false, v && v->history.CanRedo())) Redo();
        ImGui::Separator();
        bool canFill = v && v->doc->PaintTarget();
        if (ImGui::MenuItem("Couper", "Ctrl+X", false, canFill)) CopyToClipboard(*v, true);
        if (ImGui::MenuItem("Copier", "Ctrl+C", false, canFill)) CopyToClipboard(*v, false);
        if (ImGui::MenuItem("Coller", "Ctrl+V", false, clipboard != nullptr)) PasteClipboard(*v);
        if (ImGui::MenuItem("Coller en perspective (calque actif)", nullptr, false, v && v->doc->ActiveLayer() && v->doc->ActiveLayer()->kind == LayerKind::Pixel && !persp.active))
            StartPerspectivePaste(*v);
        ImGui::Separator();
        if (ImGui::MenuItem("Remplir avec la couleur de premier plan", "Alt+Retour", false, canFill)) FillOrErase(*v, false);
        if (ImGui::MenuItem("Effacer", "Suppr", false, canFill)) FillOrErase(*v, true);
        if (ImGui::MenuItem("Contour...", nullptr, false, canFill && v->doc->Selection() != nullptr)) selDialog = 5;
        DrawColorEditMenu();
        ImGui::Separator();
        if (ImGui::MenuItem("Définir un motif", nullptr, false, v != nullptr)) DefinePattern(*v);
        if (ImGui::MenuItem("Définir une forme prédéfinie de pinceau", nullptr, false, v != nullptr)) DefineBrush(*v);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("La sélection (ou l'image) devient une pointe de pinceau : le sombre peint, le blanc et le transparent non.");
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
        ui::MenuFilter();
        LayerMenuItems();
        ImGui::Separator();
        SmartObjectsMenu(v);
        ImGui::EndMenu();
    }
    DrawImageMenu();
    DrawSelectionMenu();
    if (ImGui::BeginMenu("Filtre", v != nullptr)) {
        ui::MenuFilter();
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
        if (ImGui::MenuItem("Photo RAW...", "Ctrl+Maj+A", false, !layerMode || l)) OpenCameraRaw(*v);
        if (ImGui::MenuItem("Galerie d'effets...")) FocusPanel("Effets");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Affichage")) {
        ui::MenuFilter();
        if (ImGui::BeginMenu("Avant / après")) {
            DrawCompareMenu();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Analyse sur le canevas")) {
            static const char* modes[] = {"Normal", "Zébras", "Écrêtage", "Fausses couleurs", "Focus peaking", "Luminance"};
            for (int i = 0; i < 6; ++i)
                if (ImGui::MenuItem(modes[i], i == 0 ? nullptr : nullptr, analysis.overlay == i)) analysis.overlay = i;
            ImGui::Separator();
            if (ImGui::MenuItem("Panneau Analyse d'image...")) {
                panelOpen["Analyse d'image"].open = true;
                FocusPanel("Analyse d'image");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Langue / Language")) {
            for (auto& lang : AvailableLanguages())
                if (ImGui::MenuItem((lang.name + "##lang" + lang.code).c_str(), nullptr, CurrentLanguage() == lang.code)) {
                    uiLanguage = lang.code;
                    SetLanguage(lang.code);
                    SaveAiSettings();
                }
            ImGui::EndMenu();
        }
        ImGui::Separator();
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
            ImGui::MenuItem("Repères commentés", nullptr, &smartGuides);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("En déplaçant un calque : alignement sur les bords et les centres des autres calques et de la zone de travail (Ctrl : désactivé).");
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
        ui::MenuFilter();
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
            FocusPanel("Propriétés");
        }
        if (ImGui::MenuItem("Nouveau calque « Effet visuel » (graphe)")) {
            Edit e(d, v->history, "Effet visuel");
            Layer& l = d.AddEffectLayer(LayerKind::Adjustment, "");
            l.name = "Effet visuel";
            l.effect.graph = EffectGraph::Default().Serialize();
            d.SyncGraph();
            e.Commit();
            FocusPanel("Éditeur d'effets");
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
        if (ImGui::BeginMenu("Séparation de fréquences", l != nullptr && l->kind == LayerKind::Pixel)) {
            ImGui::SetNextItemWidth(160 * dpi);
            ImGui::SliderFloat("Rayon (px)", &freqRadius, 1, 60, "%.1f");
            ui::HelpTooltip("Taille des détails gardés dans le calque des hautes fréquences (pores, grain : 2 à 8 px).");
            if (ImGui::MenuItem("Créer les deux calques")) FrequencySeparation(*v, freqRadius);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(d.SelectedLayers().size() > 1 ? "Supprimer les calques" : "Supprimer le calque", nullptr, false, l != nullptr)) {
            const auto ids = d.SelectedLayers();
            Edit e(d, v->history, ids.size() > 1 ? "Supprimer les calques" : "Supprimer le calque");
            for (Id id : ids) d.RemoveLayer(id);
            d.S().layerSel.clear();
            e.Commit();
        }
        if (ImGui::MenuItem("Fusionner vers le bas", "Ctrl+E", false, l != nullptr)) MergeDown(*v);
        if (ImGui::MenuItem("Fusionner les calques visibles", "Ctrl+Maj+E")) MergeVisible(*v, false);
        if (ImGui::MenuItem("Aplatir l'image")) MergeVisible(*v, true);
        ImGui::Separator();
        if (ImGui::BeginMenu("Aligner", l != nullptr && d.Mode() == DocMode::Layers)) {
            const size_t chosen = d.SelectedLayers().size();
            ImGui::TextDisabled(d.S().selection ? "Sur la sélection" : chosen > 1 ? "Les calques choisis entre eux" : "Sur la zone de travail");
            const char* items[] = {"Bords gauches", "Centres horizontaux", "Bords de droite", "Bords supérieurs", "Centres verticaux", "Bords inférieurs"};
            for (int i = 0; i < 6; ++i)
                if (ImGui::MenuItem(items[i])) StartAlignLayer(*v, i);
            ImGui::Separator();
            if (ImGui::MenuItem("Répartir les centres horizontalement", nullptr, false, chosen >= 3)) StartAlignLayer(*v, 6);
            if (ImGui::MenuItem("Répartir les centres verticalement", nullptr, false, chosen >= 3)) StartAlignLayer(*v, 7);
            ImGui::SetItemTooltip("Trois calques choisis ou plus (Ctrl+clic dans le panneau Calques)");
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Grouper", "Ctrl+G", false, l != nullptr)) {
            Edit e(d, v->history, "Grouper");
            d.GroupLayers(d.SelectedLayers());
            e.Commit();
        }
        if (ImGui::MenuItem("Dissocier", "Ctrl+Maj+G", false, l && l->kind == LayerKind::Group)) {
            Edit e(d, v->history, "Dissocier");
            d.Ungroup(l->id);
            e.Commit();
        }
        if (ImGui::MenuItem("Lier les calques", nullptr, false, d.SelectedLayers().size() > 1)) {
            Edit e(d, v->history, "Lier les calques");
            d.LinkLayers(d.SelectedLayers());
            e.Commit();
        }
        ImGui::SetItemTooltip("Les calques choisis (Ctrl+clic dans le panneau Calques) se déplacent ensemble, même choisis seuls");
        if (ImGui::MenuItem("Délier les calques", nullptr, false, l && l->link)) {
            Edit e(d, v->history, "Délier les calques");
            d.UnlinkLayers(d.SelectedLayers());
            e.Commit();
        }
        if (ImGui::MenuItem("Sélectionner les calques liés", nullptr, false, l && l->link)) {
            const auto ids = d.WithLinked({l->id});
            d.S().layerSel = ids;
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

    // Sessions placing something on the image own Enter / Escape (nothing else reacts to them meanwhile).
    if (persp.active && !io.WantTextInput && !persp.boundsRb) {
        if (pressed(ImGuiKey_Enter) || pressed(ImGuiKey_KeypadEnter)) { FinishPerspectivePaste(true); return; }
        if (pressed(ImGuiKey_Escape)) { FinishPerspectivePaste(false); return; }
    }
    if (paramPick.active && pressed(ImGuiKey_Escape)) { paramPick.active = false; return; }
    // Transform / polygon session keys first.
    if (xf.active && xf.view) {
        if (pressed(ImGuiKey_Enter) || pressed(ImGuiKey_KeypadEnter)) { TransformCommit(*xf.view); return; }
        if (pressed(ImGuiKey_Escape)) { TransformCancel(*xf.view); return; }
    }
    if (cropActive && v && tool == Tool::Crop) {
        if (pressed(ImGuiKey_Enter) || pressed(ImGuiKey_KeypadEnter)) {
            if (cropPersp && cropQuadSet) ApplyPerspectiveCrop(*v);
            else ApplyCrop(*v, CropRect());
            cropActive = false;
            return;
        }
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
            if (shift) d.Ungroup(al->id); else d.GroupLayers(d.SelectedLayers());
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
    if (pressed(ImGuiKey_B)) {
        // B: brush (or the brush tool in use); Shift+B cycles brush -> mixer -> color replacement.
        const Tool cyc[] = {Tool::Brush, Tool::Mixer, Tool::ColorReplace};
        int k = -1;
        for (int i = 0; i < 3; ++i) if (tool == cyc[i]) k = i;
        setTool(k < 0 ? Tool::Brush : shift ? cyc[(k + 1) % 3] : tool);
    }
    if (pressed(ImGuiKey_S)) setTool(Tool::Clone);
    if (pressed(ImGuiKey_J)) {
        // J cycles (Maj+J): correcteur localisé -> correcteur -> pièce -> déplacement basé sur le contenu -> yeux rouges.
        Tool cyc[] = {Tool::SpotHeal, Tool::Heal, Tool::Patch, Tool::ContentMove, Tool::RedEye};
        int k = 0;
        bool inCycle = false;
        for (int i = 0; i < 5; ++i)
            if (tool == cyc[i]) { k = shift ? (i + 1) % 5 : i; inCycle = true; }
        setTool(shift ? cyc[k] : inCycle ? tool : Tool::SpotHeal);
    }
    if (pressed(ImGuiKey_Y)) setTool(Tool::HistoryBrush);
    if (shift && pressed(ImGuiKey_F5) && d.S().selection && d.PaintTarget()) StartContentAwareFill(*v);
    if (pressed(ImGuiKey_O)) setTool(shift ? (tool == Tool::Dodge ? Tool::Burn : tool == Tool::Burn ? Tool::Sponge : Tool::Dodge) : Tool::Dodge);
    if (pressed(ImGuiKey_R)) setTool(shift ? (tool == Tool::Blur ? Tool::Sharpen : tool == Tool::Sharpen ? Tool::Smudge : Tool::Blur) : Tool::Blur);
    if (pressed(ImGuiKey_E)) setTool(shift ? (tool == Tool::Eraser ? Tool::BackgroundEraser : tool == Tool::BackgroundEraser ? Tool::MagicEraser : Tool::Eraser) : Tool::Eraser);
    if (pressed(ImGuiKey_I)) setTool(shift && tool == Tool::Picker ? Tool::Ruler : shift && tool == Tool::Ruler ? Tool::Picker : tool == Tool::Ruler ? Tool::Ruler : Tool::Picker);
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
        float2 o = {0, 0};
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) o.x -= step;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) o.x += step;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) o.y -= step;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) o.y += step;
        const auto chosen = v->doc->WithLinked(v->doc->SelectedLayers());
        if ((o.x || o.y) && chosen.size() > 1 && !v->doc->S().selection && !v->doc->S().editMask && v->doc->Mode() == DocMode::Layers) {
            // The chosen layers (and the ones linked to them) together, without a pixel selection: one undo step.
            bool smart = false;
            v->history.BeginGroup();
            for (Id id : chosen) MoveLayerBy(*v, id, o, smart);
            v->history.EndGroup();
            if (smart) { Prepare(*v); SyncSmartLayers(*v); }
        } else if (o.x || o.y) {
            Nudge(*v, o.x, o.y);
        }
    }

    if (tool == Tool::SpotHeal) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) spotHeal.size = std::max(2.0f, spotHeal.size * 0.85f - 1);
        if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) spotHeal.size = std::min(400.0f, spotHeal.size * 1.18f + 1);
        return;
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

// Status bar gauge: label, small bar and value. `fill` is the filled share, `load` sets the colour
// (green, orange from 60 %, red from 85 %); a negative load draws an empty bar. True when hovered.
static bool StatusGauge(const char* label, float fill, float load, const char* value) {
    ImGui::BeginGroup();
    ImGui::TextDisabled("%s", label);
    const float mid = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;  // centred on the text
    ImGui::SameLine(0, 4);
    ImGui::Dummy(ImVec2(ImGui::GetFontSize() * 3.0f, ImGui::GetTextLineHeight()));
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    float bh = std::max(4.0f, ImGui::GetFontSize() * 0.5f), y = mid - bh * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(a.x, y), ImVec2(b.x, y + bh), ImGui::GetColorU32(ImGuiCol_FrameBg), 2.0f);
    if (load >= 0) {
        const ImVec4 green(0.30f, 0.78f, 0.40f, 1), orange(1.0f, 0.65f, 0.18f, 1), red(1.0f, 0.30f, 0.30f, 1);
        ImVec4 c = load < 0.6f ? green
                   : load < 0.85f ? ImLerp(green, orange, (load - 0.6f) / 0.25f)
                                  : ImLerp(orange, red, std::min(1.0f, (load - 0.85f) / 0.1f));
        float x = a.x + (b.x - a.x) * std::clamp(fill, 0.0f, 1.0f);
        if (x > a.x + 0.5f) dl->AddRectFilled(ImVec2(a.x, y), ImVec2(x, y + bh), ImGui::GetColorU32(c), 2.0f);
    }
    dl->AddRect(ImVec2(a.x, y), ImVec2(b.x, y + bh), ImGui::GetColorU32(ImGuiCol_Border), 2.0f);
    ImGui::SameLine(0, 4);
    ImGui::TextUnformatted(value);
    ImGui::EndGroup();
    return ImGui::IsItemHovered();
}

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
            // Machine load (Windows counters, sampled once a second): graphics card, processor, video memory.
            gpu::Device& gd = gpu::Dev();
            sys::WatchAdapter(gd.AdapterLuid());
            const sys::Load load = sys::CurrentLoad();
            const double MB = 1024.0 * 1024.0, GB = MB * 1024.0;
            const u64 vTotal = gd.VideoMemoryTotal(), vMine = gd.VideoMemoryUsage(), vBudget = gd.VideoMemoryBudget();
            u64 vUsed = load.vramKnown ? load.vramUsedAll : vMine;
            if (vTotal) vUsed = std::min(vUsed, vTotal);
            const u64 vFree = vTotal > vUsed ? vTotal - vUsed : 0;
            const float vFrac = vTotal ? (float)((double)vUsed / (double)vTotal) : -1.0f;
            char right[160];
            snprintf(right, sizeof(right), "%s   %.0f i/s", gd.AdapterName().c_str(), ImGui::GetIO().Framerate);
            // Zone toggles (Tab / Maj+Tab), left of the gauges and the GPU name.
            static float gaugesW = 0;  // measured on the previous frame
            const char* tb = hideBottom ? "Afficher le graphe" : "Masquer le graphe";
            const char* tr = hideRight ? "Afficher les panneaux" : "Masquer les panneaux";
            float bw = ImGui::CalcTextSize(tb).x + ImGui::CalcTextSize(tr).x + ImGui::GetStyle().FramePadding.x * 4 + ImGui::GetStyle().ItemSpacing.x * 2;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::CalcTextSize(right).x - 48 - bw - gaugesW);
            if (ImGui::SmallButton(tb)) hideBottom = !hideBottom;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Zone du bas (graphe, éditeur d'effets) - Tab");
            if (ImGui::SmallButton(tr)) hideRight = !hideRight;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Colonne de droite (onglets, calques) - Maj+Tab");
            ImGui::Dummy(ImVec2(8, 0));
            const float gx = ImGui::GetCursorScreenPos().x;
            char val[64];
            if (load.gpu >= 0) snprintf(val, sizeof(val), "%d %%", (int)(load.gpu * 100 + 0.5f));
            else snprintf(val, sizeof(val), "-");
            if (StatusGauge("GPU", load.gpu, load.gpu, val))
                ImGui::SetTooltip("Utilisation du GPU : moteur le plus chargé de la carte, toutes applications confondues");
            ImGui::Dummy(ImVec2(6, 0));
            if (load.cpu >= 0) snprintf(val, sizeof(val), "%d %%", (int)(load.cpu * 100 + 0.5f));
            else snprintf(val, sizeof(val), "-");
            if (StatusGauge("CPU", load.cpu, load.cpu, val))
                ImGui::SetTooltip("Utilisation du processeur : tous les cœurs, toutes applications confondues");
            ImGui::Dummy(ImVec2(6, 0));
            if (vTotal) snprintf(val, sizeof(val), "%.1f / %.0f Go", vUsed / GB, vTotal / GB);
            else snprintf(val, sizeof(val), "%.0f Mo", vMine / MB);
            if (StatusGauge("VRAM", vFrac, vFrac, val)) {
                ImGui::BeginTooltip();
                ImGui::Text("Mémoire vidéo utilisée (toutes les applications) : %.0f / %.0f Mo", vUsed / MB, vTotal / MB);
                ImGui::Text("Shaderixel : %.0f Mo", vMine / MB);
                if (u64 sb = gd.SparseBytes()) ImGui::Text("Textures creuses (grandes images) : %.0f Mo", sb / MB);
                if (vBudget) ImGui::Text("Budget accordé par le système : %.0f Mo", vBudget / MB);
                ImGui::EndTooltip();
            }
            ImGui::Dummy(ImVec2(6, 0));
            snprintf(val, sizeof(val), "%.1f Go", vFree / GB);
            if (StatusGauge("Libre", vTotal ? 1.0f - vFrac : 0.0f, vFrac, val)) {
                ImGui::BeginTooltip();
                ImGui::Text("Mémoire vidéo disponible sur la carte : %.0f Mo", vFree / MB);
                ImGui::TextDisabled("Quand la carte est pleine, les données passent en mémoire système et tout ralentit.");
                ImGui::EndTooltip();
            }
            gaugesW = ImGui::GetItemRectMax().x - gx;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::CalcTextSize(right).x - 16);
            ImGui::TextDisabled("%s", right);
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
}

// Automatic budgets: from the graphics card's memory and the system memory.
static void AutoHistoryLimits(u64& gpuTarget, u64& gpuMax, u64& ram) {
    const u64 vram = gpu::Dev().VideoMemoryTotal();
    MEMORYSTATUSEX ms = {sizeof(ms)};
    const u64 sys = GlobalMemoryStatusEx(&ms) ? ms.ullTotalPhys : (16ull << 30);
    gpuTarget = std::clamp<u64>(vram / 8, 512ull << 20, 8ull << 30);
    gpuMax = std::clamp<u64>(vram / 3, 1ull << 30, 16ull << 30);
    ram = std::clamp<u64>(sys / 4, 2ull << 30, 32ull << 30);
}

void App::ApplyHistoryLimits() {
    u64 t, m, r;
    AutoHistoryLimits(t, m, r);
    HistoryLimits& L = History::Limits();
    L.gpuTarget = histGpuMB > 0 ? (u64)histGpuMB << 20 : t;
    L.gpuMax = std::max(L.gpuTarget, histGpuMaxMB > 0 ? (u64)histGpuMaxMB << 20 : m);
    L.ramMax = histRamMB > 0 ? (u64)histRamMB << 20 : r;
    Log("History memory: %.2f GB on the GPU (max %.2f GB), %.1f GB in system memory", L.gpuTarget / 1073741824.0, L.gpuMax / 1073741824.0,
        L.ramMax / 1073741824.0);
}

void App::DrawHistoryMemorySettings() {
    // Sliders in GB; "auto" follows the graphics card (and the system memory for the compressed steps).
    u64 t, m, r;
    AutoHistoryLimits(t, m, r);
    const float vramGB = gpu::Dev().VideoMemoryTotal() / 1073741824.0f;
    bool changed = false;
    auto row = [&](const char* label, int& mb, u64 autoBytes, float maxGB, const char* tip) {
        ImGui::PushID(label);
        bool autoOn = mb <= 0;
        if (ImGui::Checkbox("Auto", &autoOn)) {
            mb = autoOn ? 0 : (int)(autoBytes >> 20);
            changed = true;
        }
        ImGui::SameLine();
        float gb = (mb > 0 ? mb : (int)(autoBytes >> 20)) / 1024.0f;
        ImGui::BeginDisabled(autoOn);
        ImGui::SetNextItemWidth(200 * dpi);
        if (ImGui::SliderFloat(label, &gb, 0.25f, maxGB, "%.2f Go")) {
            mb = std::max(256, (int)(gb * 1024));
            changed = true;
        }
        ImGui::EndDisabled();
        ui::HelpTooltip(tip);
        ImGui::PopID();
    };
    ImGui::TextDisabled("Carte graphique : %.0f Go", vramGB);
    row("VRAM pour l'annulation instantanée", histGpuMB, t, std::max(1.0f, vramGB * 0.75f),
        "Les retouches récentes restent sur la carte graphique jusqu'à ce volume ;\nau-delà, les plus anciennes passent en mémoire système (compressées).");
    row("VRAM maximale de l'historique", histGpuMaxMB, m, std::max(1.0f, vramGB * 0.9f),
        "Au-delà (si la mémoire système ne suit pas), les étapes les plus anciennes sont oubliées.");
    MEMORYSTATUSEX ms = {sizeof(ms)};
    const float sysGB = GlobalMemoryStatusEx(&ms) ? ms.ullTotalPhys / 1073741824.0f : 16.0f;
    row("Mémoire système maximale", histRamMB, r, std::max(2.0f, sysGB * 0.75f), "Retouches anciennes, compressées. Au-delà, les plus anciennes sont oubliées.");
    if (changed) {
        ApplyHistoryLimits();
        SaveAiSettings();
    }
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
        DocSnapshotsUI(*v);
        // Undo memory: recent steps on the GPU, older pixel steps compressed in system memory.
        const HistoryLimits& HL = History::Limits();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Mémoire : GPU %.0f / %.0f Mo, système %.0f Mo", h.MemoryBytes() / 1048576.0, HL.gpuTarget / 1048576.0, h.SystemBytes() / 1048576.0);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Les dernières retouches restent sur la carte graphique ; les plus anciennes sont compressées\n"
                              "en mémoire système (VRAM libérée) et rechargées quand on les annule.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Réglages mémoire...") || demoOpenHistMem) ImGui::OpenPopup("##histmem");
        demoOpenHistMem = false;
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + 4), ImGuiCond_Appearing);
        if (ImGui::BeginPopup("##histmem")) {
            ImGui::SeparatorText("Mémoire de l'historique");
            DrawHistoryMemorySettings();
            ImGui::EndPopup();
        }
        static std::string histFilter;
        ui::ListFilter(histFilter, "Filtrer l'historique (Ctrl+F)...");
        if (histFilter.empty() && ImGui::Selectable("Ouverture", h.Cursor() == 0)) {
            if (fluidRun.active) FluidFinish(true);  // the wet paint becomes a step first
            while (h.CanUndo()) h.Undo(*v->doc, *ctx);
        }
        for (size_t i = 0; i < h.Count(); ++i) {
            if (!histFilter.empty() && !ui::FilterMatch(histFilter, h.Label(i))) continue;
            bool applied = i < h.Cursor();
            ImGui::PushID((int)i);
            if (!applied) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            if (ImGui::Selectable(h.Label(i).c_str(), i + 1 == h.Cursor())) {
                if (fluidRun.active) FluidFinish(true);
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
    // One write at a time per file (two would share its temporary file).
    for (auto& s : saves)
        if (s->path == path) { Status("Enregistrement déjà en cours : " + path); return; }
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
            // Smart objects inside it: their contents are kept (an inner smart layer still shows),
            // their own layered documents are not (two levels down).
            Json inner = Json::Array();
            for (auto& [isid, iso] : child->smartObjects) {
                if (!iso || !iso->content || isid >= (1u << 22)) continue;
                Json io = Json::Object();
                io["id"] = isid;
                io["name"] = iso->name;
                if (!iso->linkedPath.empty()) io["linked"] = iso->linkedPath;
                inner.Push(io);
                SurfaceBlob b;
                b.id = base | isid;
                b.width = iso->content->Width();
                b.height = iso->content->Height();
                b.format = iso->content->GetFormat();
                job->blobs.push_back(std::move(b));
                job->readbacks.push_back(ctx->ReadbackTexture(iso->content.get()));
            }
            if (inner.Size()) cj["smartObjects"] = inner;
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
        job->doc = &d;
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
            if (!err.empty() && s.doc)
                for (auto& dv : docs)
                    if (dv->doc.get() == s.doc) dv->doc->dirty = true;  // not saved: closing still asks
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
                // Its own smart objects (contents only).
                for (auto& io : o["child"]["smartObjects"].Elements()) {
                    auto iso = std::make_shared<SmartObject>();
                    iso->id = io["id"].U();
                    iso->name = io["name"].S("Objet dynamique");
                    iso->linkedPath = io["linked"].S("");
                    for (auto& b : childBlobs)
                        if (b.id == iso->id && b.data.size() >= (size_t)b.width * b.height * gpu::BytesPerPixel(b.format)) {
                            iso->content = gpu::Dev().CreateTexture(b.width, b.height, b.format, "SmartContent");
                            ctx->Upload(iso->content.get(), b.data.data(), b.width * gpu::BytesPerPixel(b.format));
                            iso->width = b.width;
                            iso->height = b.height;
                        }
                    if (iso->content) child->smartObjects[iso->id] = iso;
                }
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
