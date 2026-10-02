#pragma once
// Application shell: window, frame loop, documents, panels.

#include "doc/Document.h"
#include "doc/History.h"
#include "render/Renderer.h"
#include "platform/Platform.h"
#include "doc/DocFile.h"
#include "doc/EffectGraph.h"

#include <future>

#include <imgui.h>
#include <memory>
#include <optional>
#include <unordered_map>

namespace sx {

enum class Tool {
    Move, RectSelect, EllipseSelect, Lasso, PolyLasso, Wand,
    Brush, Eraser, Clone, Heal, Gradient, Bucket, Blur, Sharpen, Smudge, Dodge, Burn, Sponge,
    Picker, Hand, Zoom, Crop, Text, Shape, Count
};
inline bool IsRetouchTool(Tool t) { return t >= Tool::Clone && t <= Tool::Sponge && t != Tool::Gradient && t != Tool::Bucket; }
inline bool IsPaintTool(Tool t) { return t == Tool::Brush || t == Tool::Eraser || IsRetouchTool(t); }

struct DocView;

// Free transform / move session on one surface (+ the selection, if any).
struct TransformState {
    bool active = false;
    DocView* view = nullptr;
    bool pivotCustom = false;
    bool moveOnly = false;      // move tool: commit on mouse release
    Id surface = 0;
    Id selection = 0;           // selection surface at start (0 = none)
    gpu::TexturePtr original;   // surface content before the transform
    gpu::ReadbackPtr boundsRb;  // pending content bounds
    Rect src;                   // region being transformed
    // Parametric state (rotation/scale); `free` switches to independent corners.
    bool freeCorners = false;
    float2 center;
    float w = 1, h = 1;         // signed sizes in pixels
    float angle = 0;            // radians
    float2 corners[4];          // TL TR BR BL in document pixels (always up to date)
    float2 pivot;
    // Interaction
    int drag = -1;              // 0-3 corners, 4-7 edges, 8 move, 9 rotate, 10 pivot
    float2 dragStart;
    float2 startCenter, startPivot, startCorners[4];
    float startW = 1, startH = 1, startAngle = 0;
    bool dirty = false;
    int interp = 2;
};

struct DocView {
    std::unique_ptr<Document> doc;
    History history;
    Renderer renderer;
    float zoom = 1.0f;
    float2 pan;               // screen offset (canvas-local) of document origin
    bool fitPending = true;
    gpu::TexturePtr viewTex;
    gpu::Texture* lastImage = nullptr;
    std::unordered_map<Id, u64> textHashes;   // surface -> rasterized style hash
    std::unordered_map<Id, Rect> textRects;   // surface -> text bounds
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

struct EffectEditorState {
    float2 pan = {30, 50};
    float zoom = 1.0f;
    Id selected = 0, dragNode = 0;
    bool moved = false;
    bool dragLink = false;
    Id linkFrom = 0, linkTo = 0;
    int linkFromOut = 0, linkToIn = 0;
    float2 ctxPos;
    char search[64] = {};
    std::string cachedJson;
    EffectGraph graph;
    std::optional<DocState> pendingBefore;
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
    void CloseDocument(int index, bool force = false);
    void SaveDocument(DocView& v, const std::string& path);
    void SaveCurrent(bool saveAs);
    bool LoadSxl(const std::string& path);
    void UpdateSaves();
    void DrawClosePrompt();
    void UpdateTitle();
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
    void DrawSelectionMenu();
    void DrawImageMenu();
    void SyncTextLayers(DocView& v);
    void TextInput(DocView& v, ImVec2 canvasMin, bool hovered);
    void TextOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl);
    void TextPropertiesUI(DocView& v, Layer& l);
    void ShapePropertiesUI(DocView& v, Layer& l);
    void DrawImageDialogs();
    void ApplyCanvasOp(DocView& v, u32 w, u32 h, const float m[6], int interp, const char* label);
    void RotateCanvas(DocView& v, int op);
    void ApplyCrop(DocView& v, Rect r);
    void CropInput(DocView& v, ImVec2 canvasMin, bool hovered);
    void CropOverlay(DocView& v, ImVec2 canvasMin, ImVec2 canvasSize, ImDrawList* dl);
    Rect CropRect() const;
    void DrawSelectionDialog();
    void DrawHistory();
    void DrawStatusBar();
    void DrawNewDocDialog();
    void DrawShaderErrors();
    void DrawEffectEditor();
    EffectInstance* EditedEffect(DocView& v, std::string* where);
    void HandleShortcuts();

    // Property editing helpers (undo-aware)
    bool EffectParamsUI(EffectInstance& fx, const char* label);
    void TrackUndo(const char* label);
    void LayerMenuItems();
    void EffectMenu(const std::function<void(const std::string&)>& onPick, bool generators, bool filters);

    // Canvas interaction
    void CanvasInput(DocView& v, ImVec2 canvasMin, ImVec2 canvasSize, bool hovered);
    void PaintSamples(DocView& v, ImVec2 canvasMin, const std::vector<PointerSample>& samples, bool release);
    void EmitDabs(DocView& v, const std::vector<Dab>& dabs);
    BrushSettings& CurrentBrush() { return tool == Tool::Eraser ? eraser : IsRetouchTool(tool) ? retouch : brush; }
    float2 ScreenToDoc(const DocView& v, ImVec2 canvasMin, ImVec2 p) const;
    float4 LinearColor(const float4& srgb) const;
    void Undo();
    void Redo();
    void Status(const std::string& msg);

    // Selections & transform (Tools.cpp)
    SelMode ModeFromModifiers() const;
    void SelectionToolInput(DocView& v, ImVec2 canvasMin, bool hovered, const std::vector<PointerSample>& samples);
    void DrawSelectionOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl);
    void FinishPolygon(DocView& v);
    void UpdateWand(DocView& v);
    bool TransformBegin(DocView& v, bool moveOnly);
    void TransformInput(DocView& v, ImVec2 canvasMin, bool hovered);
    void TransformApply(DocView& v);
    void TransformCommit(DocView& v);
    void TransformCancel(DocView& v);
    void TransformDrawOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl);
    void TransformUpdateCorners();
    void Nudge(DocView& v, float dx, float dy);
    void FillOrErase(DocView& v, bool erase);
    void LayerViaCopy(DocView& v, bool cut);
    void CopyToClipboard(DocView& v, bool cut);
    void PasteClipboard(DocView& v);
    void SelectFromLayer(DocView& v, const Layer& l, SelMode mode);
    void ApplySelectionAsMask(DocView& v, Id layerId);
    // Add a mask (always, or only when a selection exists) filled from the selection.
    void MaskFromSelection(DocView& v, Id layerId, bool always);

    // Automation (command line): --test-effects, --demo [nodes], --capture <png> [--frames N]
    void ParseCommandLine();
    void RunDemo();
    void DemoTick();
    bool testEffects = false;
    int demoMode = 0;              // 0 none, 1 layers, 2 nodes, 3 selection, 4 transform
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
    BrushSettings retouch;
    float retouchStrength = 0.5f;
    int dodgeRange = 1;
    bool spongeSaturate = false;
    bool cloneAligned = true;
    bool cloneSourceSet = false;
    float2 cloneSource;
    bool cloneOffsetValid = false;
    int2 cloneOffset;
    float bucketTolerance = 32;
    bool bucketContiguous = true;
    bool bucketSampleMerged = false;
    Tool toolGroupChoice[32] = {};
    float4 fg = {0.05f, 0.05f, 0.06f, 1};  // sRGB straight
    float4 bg = {1, 1, 1, 1};
    bool editBg = false;
    int gradientShape = 0;
    // selection options
    int selModeOption = 0;          // 0 new, 1 add, 2 subtract, 3 intersect
    float selFeather = 0;
    bool selAntialias = true;
    float wandTolerance = 32;       // 0..255
    bool wandContiguous = true;
    bool wandSampleMerged = false;
    bool quickMask = false;
    // selection interaction
    bool selDragging = false;
    SelMode selDragMode = SelMode::Replace;
    float2 selStart, selEnd;
    std::vector<float2> lassoPts;
    bool polyActive = false;
    struct WandRequest { gpu::ReadbackPtr rb; int x = 0, y = 0; SelMode mode = SelMode::Replace; DocView* view = nullptr; bool fill = false; Id surface = 0; };
    WandRequest wand;
    int selDialog = 0;              // 1 feather, 2 expand, 3 contract, 4 color range
    float selDialogValue = 4;
    TransformState xf;
    TextStyle textDefaults;
    ShapeStyle shapeDefaults;
    bool shapeDragging = false;
    float2 shapeStart;
    float4 shapeRectStart;
    bool focusTextEdit = false;
    bool textDragging = false;
    float2 textDragStart, textPosStart;
    char fontFilter[64] = {};
    int imageDialog = 0;            // 1 image size, 2 canvas size
    int dlgW = 0, dlgH = 0, dlgAnchor = 4, dlgInterp = 2;
    bool dlgKeepRatio = true;
    bool cropActive = false, cropDragging = false;
    float2 cropStart, cropEnd;
    gpu::ReadbackPtr cropBoundsRb;
    DocView* cropBoundsView = nullptr;
    gpu::TexturePtr clipboard;
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
    struct PendingSave {
        std::string path;
        Json meta;
        std::vector<SurfaceBlob> blobs;
        std::vector<gpu::ReadbackPtr> readbacks;
        std::future<std::string> writing;  // error message ("" = ok)
        bool started = false;
    };
    std::vector<std::unique_ptr<PendingSave>> saves;
    int closePrompt = -1;        // document index awaiting save/discard, -2 = quitting
    bool quitAfterPrompt = false;
    bool quitting = false;
    std::string lastTitle;

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
    EffectEditorState efEd;
    std::string statusMsg;
    double statusTime = 0;
    std::string pendingDrop;
};

}  // namespace sx
