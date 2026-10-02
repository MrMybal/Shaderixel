#include "ui/App.h"
#include "ui/Theme.h"
#include "gpu/ShaderSystem.h"
#include "io/PsdImport.h"

#include <shellapi.h>
#include <imgui_internal.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace sx {

namespace {
double Now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

std::string ExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return Narrow(fs::path(buf).parent_path().wstring());
}

std::string ShaderDir() {
    // Prefer shaders next to the exe (distribution), else the source tree (development).
    std::string local = ExeDir() + "/shaders";
    if (fs::exists(Utf8Path(local + "/common.slang"))) return local;
    return SX_SHADER_DIR;
}

const wchar_t* kImageFilter = L"Documents et images\0*.sxl;*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.psd;*.hdr;*.gif\0Documents Shaderixel (*.sxl)\0*.sxl\0Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.psd;*.hdr;*.gif\0Tous les fichiers\0*.*\0";
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
        else if (a == "--demo") {
            std::string m = i + 1 < argc ? Narrow(argv[i + 1]) : "";
            demoMode = m == "nodes" ? 2 : m == "select" ? 3 : m == "transform" ? 4 : m == "transform-undo" ? 5 : m == "retouch" ? 6 : m == "groups" ? 7 : m == "save" ? 8 : m == "image" ? 9 : m == "curves" ? 10 : m == "effectgraph" ? 11 : m == "text" ? 12 : 1;
            if (demoMode > 1) ++i;
        }
        else if (a == "--capture" && i + 1 < argc) capturePath = Narrow(argv[++i]);
        else if (a == "--frames" && i + 1 < argc) captureFrame = std::max(3, _wtoi(argv[++i]));
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
        MessageBoxW(hwnd, L"Impossible d'initialiser Direct3D 12.", L"Shaderixel", MB_ICONERROR);
        return false;
    }
    platform::SetResizeCallback([](u32 w, u32 h) { gpu::Dev().Resize(w, h); });
    platform::SetDropCallback([this](const std::string& p) { pendingDrop = p; });

    std::string shaderDir = ShaderDir();
    gpu::Shaders().Init(shaderDir, ExeDir() + "/shadercache");
    Effects().Load(shaderDir + "/effects");
    // Warm up builtin kernels in the background.
    for (const char* s : {"composite.slang", "display.slang", "brush.slang", "clear.slang", "clear_r.slang", "gradient.slang"})
        gpu::Shaders().Builtin(s);
    if (testEffects) {
        int ok = 0;
        for (auto& e : Effects().All()) {
            auto p = gpu::Shaders().CompileNow(e->name, BuildEffectSource(*e));
            bool good = p->state.load() == gpu::Program::State::Ready;
            ok += good;
            Log("[test-effects] %-24s %s%s", e->id.c_str(), good ? "OK" : "FAILED: ", good ? "" : p->errors.c_str());
        }
        Log("[test-effects] %d / %zu effects compiled", ok, Effects().All().size());
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // no keyboard nav: keys belong to the tools
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    static std::string iniPath = ExeDir() + "/shaderixel_layout_v3.ini";
    io.IniFilename = iniPath.c_str();
    ui::ApplyTheme(dpi);
    ui::LoadFonts(dpi);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_InitInfo init;
    init.Device = gpu::Dev().D3D();
    init.CommandQueue = gpu::Dev().Queue();
    init.NumFramesInFlight = gpu::kFramesInFlight;
    init.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init.SrvDescriptorHeap = gpu::Dev().GpuHeap();
    init.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* c, D3D12_GPU_DESCRIPTOR_HANDLE* g) { gpu::Dev().AllocPersistentSrv(c, g); };
    init.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE g) { gpu::Dev().FreePersistentSrv(g); };
    ImGui_ImplDX12_Init(&init);

    eraser = brush;
    eraser.hardness = 0.9f;
    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    UpdateWindow(hwnd);
    if (!demoMode) NewDocument(1920, 1080, Precision::Half16, DocMode::Layers, 0, "Sans titre");
    return true;
}

void App::Shutdown() {
    gpu::Dev().WaitIdle();
    docs.clear();
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    gpu::Shaders().Shutdown();
    gpu::Dev().Shutdown();
}

void App::Frame() {
    gpu::Shaders().Update();
    Effects().ReloadIfChanged();
    ctx = &gpu::Dev().BeginFrame();

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (DocView* v = Active()) {
        frameBefore = v->doc->S();
        frameBeforeValid = true;
    } else {
        frameBeforeValid = false;
    }

    DrawMenuBar();
    DrawStatusBar();
    ImGuiID dock = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    if (!layoutBuilt || resetLayout) {
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock);
        if (resetLayout || !node || !node->IsSplitNode()) BuildDockLayout(dock);
        layoutBuilt = true;
        resetLayout = false;
    }

    ++frameCounter;
    if (demoMode && frameCounter == 3) RunDemo();
    if (demoMode) DemoTick();
    if (!capturePath.empty() && frameCounter == (u64)captureFrame) captureRb = gpu::Dev().CaptureNextFrame();
    if (captureRb && captureRb->ready) {
        for (size_t i = 3; i < captureRb->data.size(); i += 4) captureRb->data[i] = 255;
        stbi_write_png(capturePath.c_str(), captureRb->width, captureRb->height, 4, captureRb->data.data(), captureRb->width * 4);
        Log("Captured %ux%u to %s", captureRb->width, captureRb->height, capturePath.c_str());
        captureRb.reset();
        capturePath.clear();
        for (auto& dv : docs) dv->doc->dirty = false;  // automated run: never prompt
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }

    HandleShortcuts();
    DrawToolbar();
    DrawCanvas();
    DrawLayers();
    DrawNodeEditor();
    DrawEffectEditor();
    DrawProperties();
    DrawColor();
    DrawBrush();
    DrawHistory();
    DrawNewDocDialog();
    DrawShaderErrors();
    DrawSelectionDialog();
    DrawImageDialogs();
    if (showDemo) ImGui::ShowDemoWindow(&showDemo);

    DrawClosePrompt();
    UpdateSaves();
    UpdateTitle();
    if (!pendingDrop.empty()) {
        OpenImage(pendingDrop, false);
        pendingDrop.clear();
    }

    // Finished async readbacks.
    for (size_t i = 0; i < exports.size();) {
        auto& e = exports[i];
        if (e.rb && e.rb->ready) {
            std::string ext = Narrow(Utf8Path(e.path).extension().wstring());
            for (auto& ch : ext) ch = (char)tolower(ch);
            int ok = 0;
            std::string native = e.path;
            if (ext == ".jpg" || ext == ".jpeg") ok = stbi_write_jpg(native.c_str(), e.rb->width, e.rb->height, 4, e.rb->data.data(), 92);
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

    ImGui::Render();
    gpu::Dev().EndFrame([](ID3D12GraphicsCommandList* list) { ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), list); });
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
    docs.erase(docs.begin() + index);
    activeDoc = std::min(activeDoc, (int)docs.size() - 1);
}

void App::OpenImage(const std::string& path, bool asLayer) {
    if (path.empty() || !ctx) return;
    {
        std::string ext = Narrow(Utf8Path(path).extension().wstring());
        for (auto& ch : ext) ch = (char)tolower((unsigned char)ch);
        if (ext == ".sxl") {
            LoadSxl(path);
            return;
        }
        if (ext == ".psd" && !asLayer) {
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
    FILE* f = nullptr;
    _wfopen_s(&f, Widen(path).c_str(), L"rb");
    if (!f) { Status("Impossible d'ouvrir " + path); return; }
    int w = 0, h = 0, comp = 0;
    bool isFloat = stbi_is_hdr_from_file(f) != 0;
    void* pixels = isFloat ? (void*)stbi_loadf_from_file(f, &w, &h, &comp, 4) : (void*)stbi_load_from_file(f, &w, &h, &comp, 4);
    fclose(f);
    if (!pixels) { Status("Format d'image non supporté : " + path); return; }
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

void App::ExportImage(const std::string& path) {
    DocView* v = Active();
    if (!v || !v->lastImage || path.empty()) return;
    auto rb = v->renderer.ExportRGBA8(*ctx, v->lastImage);
    if (rb) exports.push_back({path, rb});
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

void App::BuildDockLayout(ImGuiID dock) {
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->WorkSize);
    ImGuiID left, rest, right, center, bottom, rightTop, rightMid, rightBottom;
    ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, 0.035f, &left, &rest);
    ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.24f, &right, &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.32f, &bottom, &center);
    ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.30f, &rightTop, &rightMid);
    ImGui::DockBuilderSplitNode(rightMid, ImGuiDir_Down, 0.50f, &rightBottom, &rightMid);
    ImGui::DockBuilderDockWindow("Outils", left);
    ImGui::DockBuilderDockWindow("Canevas", center);
    ImGui::DockBuilderDockWindow("Graphe", bottom);
    ImGui::DockBuilderDockWindow("Éditeur d'effets", bottom);
    ImGui::DockBuilderDockWindow("Couleur", rightTop);
    ImGui::DockBuilderDockWindow("Options", rightTop);
    ImGui::DockBuilderDockWindow("Propriétés", rightMid);
    ImGui::DockBuilderDockWindow("Historique", rightMid);
    ImGui::DockBuilderDockWindow("Calques", rightBottom);
    if (ImGuiDockNode* n = ImGui::DockBuilderGetNode(left)) n->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoResizeX;
    if (ImGuiDockNode* n = ImGui::DockBuilderGetNode(center)) n->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
    ImGui::DockBuilderFinish(dock);
}

void App::DrawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    DocView* v = Active();
    ImGui::TextColored(ImVec4(0.6f, 0.5f, 1.0f, 1.0f), "Shaderixel");
    ImGui::Separator();
    if (ImGui::BeginMenu("Fichier")) {
        if (ImGui::MenuItem("Nouveau...", "Ctrl+N")) showNewDoc = true;
        if (ImGui::MenuItem("Ouvrir une image...", "Ctrl+O")) OpenImage(platform::OpenFileDialog(hwnd, kImageFilter), false);
        if (ImGui::MenuItem("Importer comme calque...", "Ctrl+Shift+O", false, v != nullptr)) OpenImage(platform::OpenFileDialog(hwnd, kImageFilter), true);
        ImGui::Separator();
        if (ImGui::MenuItem("Enregistrer", "Ctrl+S", false, v != nullptr)) SaveCurrent(false);
        if (ImGui::MenuItem("Enregistrer sous...", "Ctrl+Maj+S", false, v != nullptr)) SaveCurrent(true);
        ImGui::Separator();
        if (ImGui::MenuItem("Exporter...", "Ctrl+Shift+E", false, v != nullptr))
            ExportImage(platform::SaveFileDialog(hwnd, L"PNG\0*.png\0JPEG\0*.jpg\0TGA\0*.tga\0BMP\0*.bmp\0", L"png"));
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
        ImGui::Separator();
        if (ImGui::MenuItem("Transformation manuelle", "Ctrl+T", false, canFill)) TransformBegin(*v, false);
        ImGui::Separator();
        ImGui::MenuItem("Erreurs de shaders", nullptr, &showShaderErrors);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Calque", v != nullptr)) {
        LayerMenuItems();
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
            Edit e(d, v->history, "Filtre");
            if (layerMode && l) {
                EffectInstance fx;
                fx.effectId = id;
                if (auto def = Effects().Find(id)) fx.params = def->DefaultParams();
                l->filters.push_back(fx);
                d.SyncGraph();
            } else if (!layerMode) {
                // Insert after the active node, rewiring its consumers.
                Id src = d.S().activeNode;
                Node* sn = d.S().graph.Find(src);
                float2 pos = sn ? sn->pos + float2{220, 0} : nodeEd.contextPos;
                Id nid = d.AddEffectNode(id, "", pos).id;
                if (src && src != d.S().graph.output) {
                    for (auto& n : d.S().graph.nodes)
                        for (auto& in : n.inputs) if (in == src && n.id != nid) in = nid;
                    d.Link(src, nid, 0);
                }
            }
            e.Commit();
        }, false, true);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Affichage", v != nullptr)) {
        if (ImGui::MenuItem("Adapter à l'écran", "Ctrl+0")) v->fitPending = true;
        if (ImGui::MenuItem("100 %", "Ctrl+1")) v->zoom = 1.0f;
        ImGui::Separator();
        ImGui::MenuItem("Fusion en gamma (compatible Photoshop)", nullptr, &v->doc->blendInGamma);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Fenêtre")) {
        if (ImGui::MenuItem("Réinitialiser la disposition")) resetLayout = true;
        ImGui::MenuItem("Démo ImGui", nullptr, &showDemo);
        ImGui::EndMenu();
    }
    // Document tabs in the menu bar.
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 24);
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
    ImGui::EndMainMenuBar();
}

void App::LayerMenuItems() {
    DocView* v = Active();
    if (!v) return;
    Document& d = *v->doc;
    bool layerMode = d.Mode() == DocMode::Layers;
    if (layerMode) {
        if (ImGui::MenuItem("Nouveau calque", "Ctrl+Maj+N")) {
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
        if (ImGui::MenuItem("Pixelliser le calque", nullptr, false, l && l->kind == LayerKind::Text)) {
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
        if (pressed(ImGuiKey_Backspace) && lassoPts.size() > 1) { lassoPts.pop_back(); return; }
    }

    if (ctrl && pressed(ImGuiKey_Z)) { shift ? Redo() : Undo(); }
    if (ctrl && pressed(ImGuiKey_Y)) Redo();
    if (ctrl && !shift && pressed(ImGuiKey_N)) showNewDoc = true;
    if (ctrl && !shift && pressed(ImGuiKey_O)) OpenImage(platform::OpenFileDialog(hwnd, kImageFilter), false);
    if (ctrl && shift && pressed(ImGuiKey_O) && v) OpenImage(platform::OpenFileDialog(hwnd, kImageFilter), true);
    if (ctrl && shift && pressed(ImGuiKey_E) && v) ExportImage(platform::SaveFileDialog(hwnd, L"PNG\0*.png\0JPEG\0*.jpg\0", L"png"));
    if (ctrl && pressed(ImGuiKey_W) && v) { CloseDocument(activeDoc); return; }
    if (ctrl && pressed(ImGuiKey_S) && v) { SaveCurrent(shift); return; }
    if (!v) return;
    Document& d = *v->doc;
    if (ctrl && pressed(ImGuiKey_0)) v->fitPending = true;
    if (ctrl && pressed(ImGuiKey_1)) v->zoom = 1.0f;

    // Selection
    if (ctrl && !shift && pressed(ImGuiKey_A))
        v->renderer.SelectShape(d, *ctx, v->history, SelShape::All, {}, SelMode::Replace, false, 0, "Tout sélectionner");
    if (ctrl && !shift && pressed(ImGuiKey_D)) v->renderer.Deselect(d, v->history);
    if (ctrl && shift && pressed(ImGuiKey_D)) v->renderer.Reselect(d, v->history);
    if (ctrl && shift && pressed(ImGuiKey_I)) v->renderer.SelectionFilter(d, *ctx, v->history, 3, 0, "Intervertir");
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

    if (pressed(ImGuiKey_V)) setTool(Tool::Move);
    if (pressed(ImGuiKey_M)) setTool(shift ? (tool == Tool::RectSelect ? Tool::EllipseSelect : Tool::RectSelect) : Tool::RectSelect);
    if (pressed(ImGuiKey_L)) setTool(shift ? Tool::PolyLasso : Tool::Lasso);
    if (pressed(ImGuiKey_W)) setTool(Tool::Wand);
    if (pressed(ImGuiKey_C)) setTool(Tool::Crop);
    if (pressed(ImGuiKey_T)) setTool(Tool::Text);
    if (pressed(ImGuiKey_B)) setTool(Tool::Brush);
    if (pressed(ImGuiKey_S)) setTool(Tool::Clone);
    if (pressed(ImGuiKey_J)) setTool(Tool::Heal);
    if (pressed(ImGuiKey_O)) setTool(shift ? (tool == Tool::Dodge ? Tool::Burn : tool == Tool::Burn ? Tool::Sponge : Tool::Dodge) : Tool::Dodge);
    if (pressed(ImGuiKey_R)) setTool(shift ? (tool == Tool::Blur ? Tool::Sharpen : tool == Tool::Sharpen ? Tool::Smudge : Tool::Blur) : Tool::Blur);
    if (pressed(ImGuiKey_E)) setTool(Tool::Eraser);
    if (pressed(ImGuiKey_I)) setTool(Tool::Picker);
    if (pressed(ImGuiKey_G)) setTool(shift && tool == Tool::Gradient ? Tool::Bucket : Tool::Gradient);
    if (pressed(ImGuiKey_K)) setTool(Tool::Bucket);
    if (pressed(ImGuiKey_H)) setTool(Tool::Hand);
    if (pressed(ImGuiKey_Z)) setTool(Tool::Zoom);
    if (pressed(ImGuiKey_Q)) quickMask = !quickMask;
    if (pressed(ImGuiKey_X)) std::swap(fg, bg);
    if (pressed(ImGuiKey_D)) { fg = {0, 0, 0, 1}; bg = {1, 1, 1, 1}; }

    if (tool == Tool::Move && !xf.active) {
        float step = shift ? 10.0f : 1.0f;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) Nudge(*v, -step, 0);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) Nudge(*v, step, 0);
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) Nudge(*v, 0, -step);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) Nudge(*v, 0, step);
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
                ImGui::Text("%.0f %%", v->zoom * 100.0f);
                ImGui::Separator();
                ImGui::Text("%u x %u  %s  %s", v->doc->Width(), v->doc->Height(),
                            v->doc->GetPrecision() == Precision::Float32 ? "32 bits" : "16 bits",
                            v->doc->Mode() == DocMode::Layers ? "Calques" : "Nodal");
                ImGui::Separator();
                if (cursorInDoc) ImGui::Text("X %d  Y %d", (int)cursorDoc.x, (int)cursorDoc.y);
                else ImGui::TextDisabled("X -  Y -");
                ImGui::Separator();
                if (v->renderer.AnyPending()) ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Compilation des shaders...");
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
            char right[160];
            snprintf(right, sizeof(right), "%s   VRAM %.0f Mo   %.0f i/s", gpu::Dev().AdapterName().c_str(),
                     gpu::Dev().VideoMemoryUsage() / (1024.0 * 1024.0), ImGui::GetIO().Framerate);
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::CalcTextSize(right).x - 16);
            ImGui::TextDisabled("%s", right);
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
}

void App::DrawNewDocDialog() {
    if (showNewDoc) {
        ImGui::OpenPopup("Nouveau document");
        showNewDoc = false;
    }
    ImGui::SetNextWindowSize(ImVec2(440 * dpi, 0));
    if (!ImGui::BeginPopupModal("Nouveau document", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    struct Preset { const char* name; int w, h; };
    static const Preset presets[] = {{"HD 1920 x 1080", 1920, 1080}, {"4K UHD 3840 x 2160", 3840, 2160}, {"Carré 2048", 2048, 2048},
                                     {"Texture 4096", 4096, 4096}, {"Texture 8192", 8192, 8192}, {"Texture 16384", 16384, 16384},
                                     {"A4 300 dpi", 2480, 3508}, {"Instagram 1080", 1080, 1080}};
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
    double mb = (double)newW * newH * (newPrecision ? 16 : 8) / (1024.0 * 1024.0);
    ImGui::TextDisabled("Mémoire par calque : %.0f Mo", mb);
    if (newW * (double)newH > 8192.0 * 8192.0)
        ImGui::TextColored(ImVec4(1, 0.75f, 0.3f, 1), "Très grande image : le rendu par tuiles n'est pas encore implémenté.");
    ImGui::Spacing();
    if (ImGui::Button("Créer", ImVec2(120 * dpi, 0))) {
        NewDocument((u32)newW, (u32)newH, newPrecision ? Precision::Float32 : Precision::Half16, newMode ? DocMode::Nodes : DocMode::Layers, newBackground, newName);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Annuler", ImVec2(120 * dpi, 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
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

void App::DrawHistory() {
    if (!ImGui::Begin("Historique")) { ImGui::End(); return; }
    DocView* v = Active();
    if (v) {
        History& h = v->history;
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
    std::string path = v->doc->path;
    std::string ext = Narrow(Utf8Path(path).extension().wstring());
    for (auto& ch : ext) ch = (char)tolower((unsigned char)ch);
    if (saveAs || path.empty() || ext != ".sxl") {
        path = platform::SaveFileDialog(hwnd, L"Document Shaderixel (*.sxl)\0*.sxl\0", L"sxl");
        if (path.empty()) return;
    }
    SaveDocument(*v, path);
}

void App::SaveDocument(DocView& v, const std::string& path) {
    Document& d = *v.doc;
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
    d.path = path;
    d.SetName(Narrow(Utf8Path(path).filename().wstring()));
    d.dirty = false;
    saves.push_back(std::move(job));
    Status("Enregistrement de " + path + "...");
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
                    return WriteSxl(p->path, p->meta, p->blobs, &err) ? std::string() : err;
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
    for (auto& b : blobs) {
        Surface* s = doc->GetSurface(b.id);
        if (!s || !s->tex || s->tex->Width() != b.width || s->tex->Height() != b.height || s->tex->GetFormat() != b.format) continue;
        if (b.data.size() < (size_t)b.width * b.height * gpu::BytesPerPixel(b.format)) continue;
        ctx->Upload(s->tex.get(), b.data.data(), b.width * gpu::BytesPerPixel(b.format));
        doc->TouchSurface(b.id);
    }
    doc->path = path;
    doc->SetName(Narrow(Utf8Path(path).filename().wstring()));
    doc->dirty = false;
    auto view = std::make_unique<DocView>();
    view->doc = std::move(doc);
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
