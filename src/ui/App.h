#pragma once
// Application shell: window, frame loop, documents, panels.

#include "doc/Document.h"
#include "doc/History.h"
#include "render/Renderer.h"
#include "platform/Platform.h"

#include <imgui.h>
#include <memory>
#include <optional>

namespace sx {

enum class Tool { Brush, Eraser, Picker, Gradient, Bucket, Hand, Zoom, Count };

struct DocView {
    std::unique_ptr<Document> doc;
    History history;
    Renderer renderer;
    float zoom = 1.0f;
    float2 pan;               // screen offset (canvas-local) of document origin
    bool fitPending = true;
    gpu::TexturePtr viewTex;
    gpu::Texture* lastImage = nullptr;
};

struct NodeEditorState {
    float2 pan = {40, 40};
    float zoom = 1.0f;
    Id dragNode = 0;
    bool draggingLink = false;
    Id linkFrom = 0;          // output side
    Id linkTo = 0;            // input side when dragging from an input
    int linkInput = -1;
    float2 contextPos;        // graph position of right click
    std::optional<DocState> moveBefore;
    char search[64] = {};
};

class App {
public:
    int Run();

private:
    bool Init();
    void Shutdown();
    void Frame();

    // Documents
    DocView* Active();
    void NewDocument(u32 w, u32 h, Precision p, DocMode mode, int background, const std::string& name);
    void OpenImage(const std::string& path, bool asLayer);
    void ExportImage(const std::string& path);
    void CloseDocument(int index);
    void FitView(DocView& v, ImVec2 avail);

    // UI
    void BuildDockLayout(ImGuiID dockspace);
    void DrawMenuBar();
    void DrawToolbar();
    void DrawCanvas();
    void DrawLayers();
    void DrawNodeEditor();
    void DrawProperties();
    void DrawColor();
    void DrawBrush();
    void DrawHistory();
    void DrawStatusBar();
    void DrawNewDocDialog();
    void DrawShaderErrors();
    void HandleShortcuts();

    // Property editing helpers (undo-aware)
    bool EffectParamsUI(EffectInstance& fx, const char* label);
    void TrackUndo(const char* label);
    void LayerMenuItems();
    void EffectMenu(const std::function<void(const std::string&)>& onPick, bool generators, bool filters);

    // Canvas interaction
    void CanvasInput(DocView& v, ImVec2 canvasMin, ImVec2 canvasSize, bool hovered);
    void PaintSamples(DocView& v, ImVec2 canvasMin, const std::vector<PointerSample>& samples, bool release);
    float2 ScreenToDoc(const DocView& v, ImVec2 canvasMin, ImVec2 p) const;
    float4 LinearColor(const float4& srgb) const;
    void Undo();
    void Redo();
    void Status(const std::string& msg);

    // Automation (command line): --test-effects, --demo [nodes], --capture <png> [--frames N]
    void ParseCommandLine();
    void RunDemo();
    bool testEffects = false;
    int demoMode = 0;              // 0 none, 1 layers, 2 nodes
    std::string capturePath;
    int captureFrame = 30;
    u64 frameCounter = 0;
    gpu::ReadbackPtr captureRb;

    HWND hwnd = nullptr;
    float dpi = 1.0f;
    gpu::CommandContext* ctx = nullptr;
    std::vector<std::unique_ptr<DocView>> docs;
    int activeDoc = -1;
    int focusDocRequest = -1;

    Tool tool = Tool::Brush;
    BrushSettings brush;
    BrushSettings eraser;
    float4 fg = {0.05f, 0.05f, 0.06f, 1};  // sRGB straight
    float4 bg = {1, 1, 1, 1};
    bool editBg = false;
    int gradientShape = 0;
    float gradientOpacity = 1.0f;

    // stroke state
    bool stroking = false;
    bool strokeErase = false;
    float2 smoothPos, lastDabPos, lastStrokeEnd;
    float lastPressure = 1.0f;
    float dabRemainder = 0;
    bool haveStrokeEnd = false;
    bool gradientDrag = false;
    float2 gradientStart, gradientEnd;
    bool panning = false;
    bool spaceHeld = false;
    ImVec2 lastMouse;
    float2 cursorDoc;
    bool cursorInDoc = false;

    // async GPU readbacks
    gpu::ReadbackPtr pickReadback;
    struct PendingExport { std::string path; gpu::ReadbackPtr rb; };
    std::vector<PendingExport> exports;

    // dialogs / panels
    bool showNewDoc = false;
    bool showShaderErrors = false;
    bool showDemo = false;
    bool layoutBuilt = false;
    bool resetLayout = false;
    char newName[64] = "Sans titre";
    int newW = 1920, newH = 1080, newPrecision = 0, newMode = 0, newBackground = 0;
    std::optional<DocState> editBefore;  // undo snapshot while a widget is active
    DocState frameBefore;                // properties panel state at frame start
    bool frameBeforeValid = false;
    NodeEditorState nodeEd;
    std::string statusMsg;
    double statusTime = 0;
    std::string pendingDrop;
};

}  // namespace sx
