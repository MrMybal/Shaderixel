#pragma once
#include <set>
// Application shell: window, frame loop, documents, panels.

#include "doc/Document.h"
#include "doc/History.h"
#include "render/Renderer.h"
#include "platform/Platform.h"
#include "doc/DocFile.h"
#include "io/PsdExport.h"
#include "doc/EffectGraph.h"
#include "ai/Bridge.h"
#include "render/MeshPaint.h"

#include <mutex>

#include <atomic>
#include <future>
#include <thread>

#include <imgui.h>
#include <memory>
#include <optional>
#include <unordered_map>

namespace sx {

std::string ExeDir();  // folder of the executable

enum class Tool {
    Move, RectSelect, EllipseSelect, Lasso, PolyLasso, Wand,
    Brush, Eraser, Clone, Heal, Gradient, Bucket, Blur, Sharpen, Smudge, Dodge, Burn, Sponge, Liquify,
    Picker, Hand, Zoom, Crop, Text, Shape, QuickSelect, Pen, MagneticLasso,
    Patch, ContentMove, RedEye, MagicEraser, BackgroundEraser, HistoryBrush, PatternStamp, Count
};
inline bool IsRetouchTool(Tool t) {
    return (t >= Tool::Clone && t <= Tool::Liquify && t != Tool::Gradient && t != Tool::Bucket) || t == Tool::BackgroundEraser || t == Tool::HistoryBrush ||
           t == Tool::PatternStamp;
}
inline bool IsPaintTool(Tool t) { return t == Tool::Brush || t == Tool::Eraser || IsRetouchTool(t); }

struct DocView;
struct PhotoJob;

// Free transform / move session on one surface (+ the selection, if any).
// Actions (Actions.cpp): a command with JSON parameters, replayed on any document.
struct ActionStep {
    std::string cmd;
    Json params;
    bool enabled = true;
};
struct Action {
    std::string name;
    std::vector<ActionStep> steps;
};

struct TransformState {
    bool active = false;
    DocView* view = nullptr;
    bool pivotCustom = false;
    bool moveOnly = false;      // move tool: commit on mouse release
    Id surface = 0;
    Id selection = 0;           // selection surface at start (0 = none)
    bool selectionOnly = false; // Select > Transform Selection: transform the outline only
    Id smartLayer = 0;          // smart object layer: the corners change, the pixels are re-placed from the source
    float2 smartStart[4];
    DocState before;            // selection-only: state to restore / undo to
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
    // Warp: bicubic Bezier patch over the region (4x4 control points, row-major).
    bool warp = false;
    bool warpPending = false;  // switch to warp once the bounds are known
    float2 grid[16], startGrid[16];
    int warpDrag = -1;         // control point, 16 = surface drag
    float2 warpUV;             // patch parameters of a surface drag
    // Mesh warps (MeshWarp.cpp): 0 = Bezier warp, 1 = puppet, 2 = perspective.
    int warpKind = 0;
    WarpMesh mesh;             // last mesh drawn (pin placement, overlay)
    std::vector<float2> pinSrc, pinDst;
    int pinSel = -1, pinDrag = -1;
    int puppetMode = 0;        // 0 rigid, 1 normal (similarity), 2 distort (affine)
    int puppetDensity = 1;
    bool showMesh = true;
    std::vector<float2> layoutPts, warpPts;
    std::vector<std::array<int, 4>> planes;
    int perspPhase = 0, ptDrag = -1, planeSel = 0;
    float2 dragOffset;
};

struct DocView {
    std::unique_ptr<Document> doc;
    History history;
    Renderer renderer;
    float zoom = 1.0f;
    float2 pan;               // screen offset (canvas-local) of document origin
    bool fitPending = true;
    int fitHold = 0;          // frames during which a fit follows canvas resizes (dock layout settling)
    float fitW = 0, fitH = 0;
    gpu::TexturePtr viewTex;
    gpu::Texture* lastImage = nullptr;
    std::unordered_map<Id, u64> textHashes;   // surface -> rasterized style hash
    std::unordered_map<Id, Rect> textRects;   // surface -> text bounds
    std::unique_ptr<MeshScene> mesh;          // 3D painting scene (optional)
    std::unordered_map<Id, gpu::TexturePtr> snapshots;  // history brush sources (layer state before the first edit)
    std::string displayLutKey;                // color management chain of displayLut
    gpu::TexturePtr displayLut;
    int displayLutN = 0;
    // Inner document of a smart object (open in this tab): its parent and the object.
    struct {
        DocView* parent = nullptr;
        Id smart = 0;
        u64 syncKey = 0;
    } smartLink;
};

struct NodeEditorState {
    float2 pan = {40, 40};
    float zoom = 1.0f;
    Id dragNode = 0;
    bool draggingLink = false;
    Id linkFrom = 0;          // output side
    Id linkTo = 0;            // input side when dragging from an input
    int linkInput = -1;
    u8 linkChannel = 0;       // output used on the source side (0 RGBA, 1 R, 2 G, 3 B, 4 A)
    float2 contextPos;        // graph position of right click
    std::optional<DocState> moveBefore;
    char search[64] = {};
    std::set<Id> selection;   // selected nodes (moved / deleted together)
    bool boxSelecting = false;
    float2 boxStart;          // graph position
    // Link dropped on empty space: the node picked in the menu gets connected.
    Id pendFrom = 0, pendTo = 0;
    int pendInput = 0;
    u8 pendChannel = 0;
};

struct EffectEditorState {
    int target = 0;  // 0 effect of the layer / node, 1 blend of the layer, 2 current brush
    int filterIndex = -1;  // target 0 in layer mode: filter edited (-1 = the layer's own effect / first editable filter)
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

struct AssistantState {
    struct Msg {
        bool user;
        std::string text;
    };
    std::string sessionId, conversationId, input;
    std::vector<Msg> log;
    bool busy = false;
    std::mutex streamMutex;
    std::string streaming;
    long long eventCursor = 0;
    void Reset() {
        sessionId.clear();
        conversationId.clear();
        log.clear();
        busy = false;
        std::lock_guard lock(streamMutex);
        streaming.clear();
        eventCursor = 0;
    }
};

// Rendered content of the active layer (filters included) / of everything below it.
gpu::Texture* ActiveLayerContent(DocView& v, u64& hash);
gpu::Texture* LayerBelow(DocView& v, u64& hash);

// Drag & drop payload: a library effect id (effects browser -> graph, layers, canvas).
inline constexpr const char* kEffectPayload = "SX_EFFECT";

class App {
public:
    int Run();

    // Workspace state (saved in the ImGui ini / layouts, see Workspace.cpp).
    static constexpr int kSideTabs = 7;  // Général, Outils, Effets, Fusion, Peinture, 3D, IA
    int sideTab = 0;
    int sideTabRequest = -1;  // selects a side tab on the next frame
    bool hideBottom = false, hideRight = false;
    ImGuiID bottomZone = 0, rightZone = 0;
    std::vector<bool> extraWindows;  // "Fenêtre 2..." (open flags)
    struct PanelOpenFlag { bool open = true; };
    std::map<std::string, PanelOpenFlag> panelOpen;  // closed panels (tab cross)

private:
    std::string pendingLayoutIni, pendingLayoutName, layoutName;
    std::vector<std::string> savedLayouts;
    bool layoutSavePopup = false;
    void RegisterWorkspaceSettings();
    float ToolbarWidth() const;
    void DrawSidePanel();
    void ShowSideTab(int tab);
    void DrawExtraWindows();
    void NewExtraWindow();
    bool Shown(const char* window) const;
    void ScanLayouts();
    void SaveLayout(const std::string& name);
    void LoadLayout(const std::string& name);
    void DeleteLayout(const std::string& name);
    void ApplyPendingLayout();
    void DrawWindowMenu();
    void DrawPanelsMenu();
    void DrawLayoutSavePopup();

    bool Init();
    void Shutdown();
    void Frame();

    // Documents
    DocView* Active();
    void NewDocument(u32 w, u32 h, Precision p, DocMode mode, int background, const std::string& name);
    void OpenImage(const std::string& path, bool asLayer);
    void ExportImage(const std::string& path);
    void ExportPsd(const std::string& path, int depth = 8);
    void MergeDown(DocView& v);
    // PBR material channels (Material.cpp)
    Id FindChannelGroup(Document& d, PbrChannel c);
    gpu::Texture* ChannelTexture(DocView& v, PbrChannel c);
    MeshMaterial GatherMaterial(DocView& v);
    void CreatePbrChannels(DocView& v);
    void ExportPbrTextures(DocView& v, const std::string& pngBasePath);
    // Recent files (stored next to the executable).
    std::vector<std::string> recentFiles;
    void LoadRecent();
    void AddRecent(const std::string& path);
    float strokeWidth = 4;
    int strokePosition = 1;
    float strokeOpacity = 1;
    // Rulers, guides, grid, snapping (Guides.cpp)
    float RulerSize() const;
    float2 Snap(const DocView& v, float2 p) const;
    void DrawGridAndGuides(DocView& v, ImVec2 canvasMin, ImVec2 avail, ImDrawList* dl);
    bool GuideInput(DocView& v, ImVec2 canvasMin, ImVec2 avail, bool canvasHovered);
    void DrawRulers(DocView& v, ImVec2 origin, ImVec2 canvasMin, ImVec2 avail);
    bool showRulers = true, showGuides = true, showGrid = false, snapEnabled = true;
    float gridStep = 100;
    int gridSubdiv = 4;
    struct GuideDrag {
        bool active = false;
        const DocView* view = nullptr;
        bool vertical = false;
        int index = -1;  // existing guide being moved, -1 = new
        float pos = 0;
    } guideDrag;
    void MergeVisible(DocView& v, bool flatten);
    void PollPsdExport();
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
    void DrawHistogram();
    void UpdateHistogram();
    struct HistogramState {
        gpu::ReadbackPtr rb;
        const void* view = nullptr;
        u64 version = 0, frame = 0;
        bool valid = false;
        u64 bins[4][256] = {};
        float norm[4][256] = {};  // 0..1 for drawing (curves background)
    } histo;
    int histoChannel = 0;
    void DrawStatusBar();
    void DrawNewDocDialog();
    void DrawShaderErrors();
    void DrawEffectEditor();
    // AI (AiOps.cpp / AiPanel.cpp)
    void RegisterAiOps();
    void LoadAiSettings();
    void SaveAiSettings();
    void AiAsync(std::function<std::function<void()>()> fn);
    void PollAiJobs();
    void DrawAiPanel();
    void DrawImageGenPanel();
    void DrawView3D();
    void AttachMesh(DocView& v, MeshData data);
    void MeshPrepare3D(DocView& v);
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
    void EmitDabs(DocView& v, const std::vector<Dab>& dabs, const std::vector<float4>* extra = nullptr);
    // Tablet: pressure curve (exponent) and the pen state of each dab.
    float penPressureGamma = 1.0f;
    float strokeSpeed = 0;     // document pixels / second (smoothed)
    double lastSampleTime = 0;
    // Brush textures (//@texture): images loaded once per path; bound into BrushSettings::tipTexBound.
    std::map<std::string, gpu::TexturePtr> brushTextures;
    gpu::Texture* BrushTexture(const std::string& path);
    void BindTipTextures(BrushSettings& b, const struct BrushDef& td);
    std::string TipTexturePath(const BrushSettings& b, const struct BrushDef& td, size_t i) const;
    bool TipTexturesUI(BrushSettings& b, const struct BrushDef& td);
    u64 BrushPreviewVersion(const BrushSettings& b, const struct BrushDef& td) const;
    std::vector<PointerSample> frameSamples;  // pointer samples of this frame (canvas, 3D view, brush pad)
    struct {
        bool stroking = false, ready = false;
        float2 last;
        float remainder = 0, dist = 0;
        double start = 0;
        u32 dabs = 0;
    } pad;
    void DrawBrushPad(DocView& v, BrushSettings& b);
    float PenPressure(float raw) const { return std::pow(std::clamp(raw, 0.0f, 1.0f), penPressureGamma); }
    float4 PenExtra(const PointerSample& s) const {  // tilt x, tilt y (deg), rotation (rad), speed
        return s.pen ? float4{s.tiltX, s.tiltY, s.twist * 0.01745329f, strokeSpeed} : float4{0, 0, 0, strokeSpeed};
    }
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
    void QuickSelectInput(DocView& v, ImVec2 canvasMin, bool hovered, const std::vector<PointerSample>& samples);
    void UpdateQuickSelect(DocView& v);
    // Pen tool / paths (PenTool.cpp)
    void PenInput(DocView& v, ImVec2 canvasMin, bool hovered);
    void DrawPenOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl);
    void DrawPathsPanel();
    VectorPath* ActivePath(Document& doc);
    bool PathToSelection(DocView& v, SelMode mode);
    void FillPath(DocView& v);
    void StrokePath(DocView& v);
    struct PenState {
        bool drawing = false;    // adding anchors to the last subpath of the active path
        bool dragging = false;   // shaping the handles of the anchor just placed
        int dragPt = 0;
        float2 dragOrigin;
        int editSub = -1, editPt = -1, editPart = -1;  // Ctrl+drag target (0 anchor, 1 in, 2 out)
        float2 editStart;
        PathAnchor editOrig;
        std::optional<DocState> before;
    } pen;
    void DrawQuickSelectOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl);
    struct QuickSelState {
        gpu::ReadbackPtr rb;       // analyzed image (RGBA8), cached while unchanged
        const void* view = nullptr;
        Id source = 0;             // 0 = merged image
        u64 version = 0;
        std::vector<float2> pts;   // current stroke (document pixels)
        bool painting = false, pending = false;
        SelMode mode = SelMode::Replace;
    } qsel;
    float qselSize = 40, qselTolerance = 40;
    bool qselMerged = true;
    bool TransformBegin(DocView& v, bool moveOnly);
    bool TransformSelectionBegin(DocView& v);
    void TransformInput(DocView& v, ImVec2 canvasMin, bool hovered);
    void TransformApply(DocView& v);
    void TransformCommit(DocView& v);
    void TransformCancel(DocView& v);
    void TransformDrawOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl);
    void TransformUpdateCorners();
    bool WarpBegin(DocView& v);
    void WarpFromCorners();
    void WarpInput(DocView& v, ImVec2 canvasMin, bool hovered);
    void WarpDrawOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl);
    void MeshWarpBegin(DocView& v, int kind);  // 1 puppet, 2 perspective
    void PerspectiveInitPlanes();
    WarpMesh BuildMeshWarp();
    bool MeshWarpChanged();
    void MeshWarpInput(DocView& v, ImVec2 canvasMin, bool hovered);
    void MeshWarpDrawOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl);
    void MeshWarpOptions();
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
    bool aiTest = false;  // --ai-test: start the companion and allow writes (automation)
    void RunDemo16k();
    int demoMode = 0;              // 0 none, 1 layers, 2 nodes, 3 selection, 4 transform
    std::string capturePath, dumpPath, cliSavePath;
    bool autoQuit = false;
    std::vector<std::string> focusAtStart;
    gpu::ReadbackPtr dumpRb;
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
    int liquifyMode = 0;  // 0 push, 1 twirl cw, 2 twirl ccw, 3 pucker, 4 bloat, 5 reconstruct
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
    std::vector<float4> recentColors;  // most recent first (sRGB)
    int colorSliderMode = 0;           // 0 RGB, 1 HSV
    void PushRecentColor(float4 c);
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
    // Outline move: a click first probes the selection under the cursor (async readback).
    gpu::ReadbackPtr selProbe;
    float2 selProbeStart;
    bool selMoving = false;
    int2 selMoveOffset;
    SelMode selDragMode = SelMode::Replace;
    float2 selStart, selEnd;
    std::vector<float2> lassoPts;
    bool polyActive = false;
    struct WandRequest { gpu::ReadbackPtr rb; int x = 0, y = 0; SelMode mode = SelMode::Replace; DocView* view = nullptr; bool fill = false, erase = false; Id surface = 0; };
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
    bool shapePartDrag = false;
    int textSelStart = 0, textSelEnd = 0;  // last selection in the text field (codepoints)
    Id textSelLayer = 0;
    Id charStyleSel = 0, paraStyleSel = 0;
    void DrawTextStylesPanel();
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
    // Programmable brushes: per-stroke dab information.
    double strokeStart = 0;
    float strokeDist = 0, lastDirAngle = 0, airAccum = 0;
    u32 dabCounter = 0;
    std::shared_ptr<const struct BrushDef> TipDef(const BrushSettings& b);
    void SelectTip(BrushSettings& b, const std::string& id);
    void DrawBrushPanel();
    void DrawBlendPanel();
    // Smart objects (SmartObjects.cpp).
    gpu::TexturePtr LoadImageTexture(DocView& v, const std::string& path, u32& w, u32& h);
    SmartObject* NewSmartObject(Document& d, const std::string& name, gpu::TexturePtr content);
    Layer* AddSmartLayer(DocView& v, SmartObject& so, const float2 corners[4]);
    void SyncSmartLayers(DocView& v);
    void ConvertToSmartObject(DocView& v);
    void PlaceSmartFile(DocView& v, const std::string& path, bool linked);
    void ReplaceSmartContents(DocView& v, SmartObject& so, const std::string& path);
    void UpdateLinkedSmartObjects(DocView& v, bool force);
    void EditSmartContents(DocView& parent, Layer& l);
    void UpdateSmartChildren();
    void DetachSmartChild(DocView& v);
    void SmartPropertiesUI(DocView& v, Layer& l);
    void SmartObjectsMenu(DocView* v);
    // Selection tools (SelectTools.cpp): Color Range, Select and Mask.
    struct {
        bool open = false;
        int preset = 0, mode = 0, pickOp = 0;
        float fuzz = 40;
        bool invert = false;
        std::vector<float4> samples;  // straight linear colors
        gpu::ReadbackPtr pick;
        gpu::Texture* mask = nullptr;
        u64 hash = 0, version = 0;
    } colorRange;
    struct {
        bool open = false;
        Renderer::RefineParams params;
        int view = 0, output = 0;
        gpu::Texture* mask = nullptr;
        gpu::Texture* preview = nullptr;
        u64 hash = 0;
    } selectMask;
    bool SelectToolsCanvasInput(DocView& v, float2 dp, bool click);
    void DrawSelectToolsOverlay(DocView& v, ImVec2 d0, ImVec2 d1, ImDrawList* dl);
    void DrawColorRangeWindow();
    void DrawSelectMaskWindow();
    // Magnetic lasso (MagneticLasso.cpp): edge cost map + live-wire path.
    struct {
        gpu::ReadbackPtr rb;
        std::vector<float> cost;
        int w = 0, h = 0, scale = 1;
        float2 anchor, lastCursor;
        std::vector<float2> live;
        std::vector<size_t> anchors;  // lassoPts sizes at each anchor
        float anchorEvery = 90.0f;    // document pixels between automatic anchors
    } magLasso;
    void MagneticBuildCost();
    std::vector<float2> MagneticPath(float2 from, float2 to);
    void MagneticLassoInput(DocView& v, ImVec2 canvasMin, bool hovered);
    void MagneticLassoUndoAnchor();
    // Select > Subject / Sky (SelectSubject.cpp).
    struct {
        bool active = false, sky = false;
        DocView* view = nullptr;
        gpu::ReadbackPtr rb;
        std::future<int> proc;
        std::string in, out;
        int w = 0, h = 0;
    } subjectJob;
    std::future<int> subjectInstall;
    bool subjectInstallPrompt = false;
    void StartSelectSubject(DocView& v, bool sky);
    void UpdateSelectSubject();
    void InstallSubjectModel();
    void UpdateSubjectInstall();
    // Retouching (Retouch2.cpp): content-aware fill, Patch / content-aware move, red eye, magic eraser, history brush.
    struct FillJob {
        bool active = false;
        DocView* view = nullptr;
        Id surface = 0, selection = 0;
        gpu::ReadbackPtr pixels, mask;
        int cx = 0, cy = 0, cw = 0, ch = 0;
        std::vector<float> rgba;
        std::vector<u8> hole;
        std::future<bool> work;
        std::atomic<bool> cancel{false};
        std::atomic<float> progress{0};
        std::function<void()> then;
        FillJob() = default;
        FillJob& operator=(FillJob&& o) noexcept {
            active = o.active; view = o.view; surface = o.surface; selection = o.selection;
            pixels = std::move(o.pixels); mask = std::move(o.mask);
            cx = o.cx; cy = o.cy; cw = o.cw; ch = o.ch;
            rgba = std::move(o.rgba); hole = std::move(o.hole); work = std::move(o.work);
            cancel = o.cancel.load(); progress = o.progress.load(); then = std::move(o.then);
            return *this;
        }
    } fillJob;
    struct {
        gpu::ReadbackPtr probe;
        float2 start;
        bool dragging = false;
    } patchDrag;
    float patchBlend = 0.4f, redEyeSize = 40, redEyeDarken = 0.5f, bgEraserTolerance = 0.25f;
    void StartContentAwareFill(DocView& v, std::function<void()> then = {});
    void UpdateContentAwareFill();
    void PatchToolInput(DocView& v, ImVec2 canvasMin, bool hovered);
    void ClickToolInput(DocView& v, ImVec2 canvasMin, bool hovered);
    void TakeHistorySnapshot(DocView& v, Id surface);
    // Actions and batch processing (Actions.cpp).
    std::vector<Action> actions;
    int actionSel = 0, actionRecordInto = -1;
    bool actionRecording = false, actionReplaying = false, actionRename = false;
    Id recordFilterLayer = 0;
    std::unordered_map<const ActionStep*, std::string> stepJsonEdit;
    struct ActionRun {
        bool active = false;
        DocView* view = nullptr;
        std::string name, error, exportFolder, lastExport;
        std::vector<ActionStep> steps;
        size_t next = 0;
        int retries = 0;
        bool exporting = false;
        gpu::ReadbackPtr wait;
        std::function<void(bool)> done;
    } actionRun;
    struct {
        bool open = false, running = false, cancel = false, subfolders = false, saveResult = true;
        int action = 0, format = 0, stage = 0;
        std::string source, dest, suffix;
        std::vector<std::string> files, errors;
        size_t index = 0, done = 0;
        DocView* view = nullptr;
    } batch;
    std::string ActionsPath() const;
    void LoadActions();
    void SaveActions();
    void RecordAction(const std::string& cmd, Json params = Json::Object());
    void RecordSelect(const char* what) {
        Json p = Json::Object();
        p["what"] = what;
        RecordAction("select", p);
    }
    void FinalizeRecordedFilter();
    void StopRecording();
    bool StartAction(DocView& v, int index, std::function<void(bool)> done = {});
    int ExecActionStep(DocView& v, const ActionStep& s);  // 0 done, 1 wait, 2 failed
    void UpdateActionRun();
    void StartBatch();
    void UpdateBatch();
    void DrawActionsPanel();
    // Layer comps and artboards (LayerComps.cpp).
    Id compRename = 0;
    void CaptureComp(Document& d, LayerComp& c);
    void ApplyComp(DocView& v, Id compId, bool undoable);
    void DrawLayerCompsPanel();
    void ExportComps(DocView& v, const std::string& basePath);
    void WriteRectMask(Document& d, Id surface, const Rect& r);
    void NewArtboard(DocView& v);
    void Prepare(DocView& v);
    void DrawArtboards(DocView& v, ImVec2 canvasMin, ImDrawList* dl);
    void ExportArtboards(DocView& v, const std::string& basePath);
    void ArtboardPropertiesUI(DocView& v, Layer& l);
    // Color management (ColorManage.cpp, io/ColorMgmt).
    struct {
        bool on = false, gamutWarning = false, useMonitor = true;
        std::string profile;  // "" = the document's CMYK profile
        int intent = 1;
    } proof;
    std::string monitorProfile;
    int colorDialog = 0;  // 1 assign, 2 convert
    std::string colorDialogProfile;
    bool spotDialog = false;
    std::string spotName = "Encre 1";
    float4 spotColor = {0.9f, 0.1f, 0.4f, 1};
    float spotSolidity = 1;
    struct CmykExport { std::string path; gpu::ReadbackPtr rb; std::string rgbProfile, cmykProfile; int intent = 1; };
    std::vector<CmykExport> cmykExports;
    std::string DocProfile(DocView* v) const;
    std::string CmykProfileFor(DocView* v) const;
    void DisplayColorLut(DocView& v, ViewParams& vp);
    bool ApplyColorLutToDocument(DocView& v, const std::vector<std::string>& chain, int intent, const std::string& label);
    void SetColorMode(DocView& v, int mode);
    void OpenedImageProfile(DocView& v, const std::string& path);
    void ExportCmykTiff(DocView& v, const std::string& path);
    void UpdateColorJobs();
    void NewSpotColor(DocView& v, const std::string& name, float4 srgb, float solidity);
    void DrawProofMenu();
    void DrawColorEditMenu();
    void DrawColorModeMenu();
    void DrawColorDialogs();
    // Photo tools (PhotoMerge.cpp): panorama, auto-align / auto-blend, HDR merge, RAW through WIC.
    std::shared_ptr<PhotoJob> photoJob;
    struct {
        bool open = false, blend = true;
        int kind = 1, layout = 0, blendMode = 0;
        std::vector<std::string> files;
    } photoDlg;
    void StartPhotoJob(int kind, DocView* v);
    void UpdatePhotoJob();
    void ApplyPhotoJob(PhotoJob& J);
    void WriteMaskData(Document& d, Id surface, const std::vector<float>& m, int mw, int mh);
    bool OpenWithWic(const std::string& path, bool asLayer);
    void DrawPhotoDialog();
    // Resources (Resources.cpp): patterns, swatches, .abr / .pat import.
    struct PatternEntry {
        std::string name, path;
        int w = 0, h = 0;
        std::vector<u8> rgba;
        gpu::TexturePtr thumb;
    };
    std::vector<PatternEntry> patterns;
    bool patternsScanned = false;
    int patternSel = 0;
    float patternScale = 1;
    gpu::TexturePtr patternTiled;
    u64 patternTiledKey = 0;
    struct {
        bool active = false;
        DocView* view = nullptr;
        gpu::ReadbackPtr image, bounds;
    } patternDefine;
    struct SwatchEntry {
        std::string name;
        float4 srgb;
    };
    std::vector<SwatchEntry> swatches;
    std::string swatchPath;
    bool swatchesLoaded = false;
    std::string PatternDir() const;
    void EnsureDefaultPatterns();
    void ScanPatterns();
    bool LoadPattern(PatternEntry& p);
    gpu::Texture* PatternTiled(DocView& v, int index, float scale);
    void FillWithPattern(DocView& v, int index, float scale);
    void DefinePattern(DocView& v);
    void UpdateResources();
    void ImportPatFile(const std::string& path);
    void DrawPatternsPanel();
    std::string SwatchDir() const;
    void LoadSwatchLibrary(const std::string& path);
    void SaveSwatches();
    void EnsureSwatches();
    void DrawSwatchesPanel();
    void ImportAbrFile(const std::string& path);
    void DrawBatchWindow();
    Id smartEditRequest = 0;
    void ColorPickerBody();  // Couleur panel / toolbar popup
    bool colorPopupRequest = false;
    // Closable panels (tab cross), reopened from Affichage > Panneaux; saved with the layout.
    bool* PanelFlag(const char* name) { return &panelOpen[name].open; }
    bool PanelIsOpen(const char* name) const { auto it = panelOpen.find(name); return it == panelOpen.end() || it->second.open; }
    // Material layers (Substance-like): one fill per PBR channel group sharing a mask.
    Id materialSel = 0;
    std::optional<DocState> materialEditBefore;
    bool paint3DLayoutRequest = false;
    std::vector<Id> MaterialIds(Document& d);
    Layer* MaterialChannelLayer(Document& d, Id material, PbrChannel c);
    Layer* AnyMaterialLayer(Document& d, Id material);
    Layer* AddMaterialChannel(Document& d, Id material, PbrChannel c, const std::string& name, Id mask, float4 value, float opacity);
    void AddMaterialLayer(DocView& v, int preset);
    void SelectMaterial(Document& d, Id material);
    void ApplyPaint3DLayout();
    void DrawMaterialPanel();
    float4 PaintColor(Document& d);  // brush color: fg, gray on masks, white on material masks
    void OpenGraphEditor(int target);  // 0 effect, 1 blend of the layer, 2 brush
    // Library (Library.cpp): shipped items protected, user items (//@user) lockable (//@locked).
    enum class LibKind { Effect, Blend, Brush };
    std::string LibraryPath(LibKind k, const std::string& name);
    int LibraryState(const std::string& path);  // 0 free, 1 user item, 2 protected
    bool LibraryItemLocked(LibKind k, const std::string& id);
    bool IsUserLibraryItem(LibKind k, const std::string& id);
    void SetLibraryLock(LibKind k, const std::string& id, bool lock);
    bool SaveToLibrary(LibKind k, const std::string& name, const std::string& category, const std::string& text, bool lock, bool replace, std::string* err);
    bool LibrarySavePopup(const char* popupId, LibKind k, const std::string& defName, const std::string& defCategory, const std::string& text,
                          std::string* savedId = nullptr);
    // Effect editor: code and library views (graphs have their canvas).
    void DrawLibraryItemView(DocView& v, EffectInstance& fx, bool blend);
    void DrawEffectCodeEditor(DocView& v, EffectInstance& fx);
    // HLSL import (annotated HLSL or a function -> custom node with its pins).
    bool CreateEffectFromHlsl(DocView& v, const std::string& source, std::string* error);
    void DrawHlslImportPopup(EffectGraph* g, float2 pos, bool* changed);
    std::string hlslImportBuffer, hlslImportError;
    bool hlslImportRequest = false;
    void DrawBrushCodeEditor(DocView& v, BrushSettings& b);
    void DropEffectOnLayer(DocView& v, Id layer, const std::string& effectId);
    struct {
        std::string search, buffer, bufferKey, errors;
        std::string saveName, saveCategory = "Mes pinceaux";
    } brushEd;
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
    struct PendingExport { std::string path; gpu::ReadbackPtr rb; Rect crop; };  // crop: part of the image written (empty = all)
    std::vector<PendingExport> exports;
    struct PsdExportState {
        PsdExportJob job;
        bool writing = false;
        std::atomic<bool> done{false};
        bool ok = false;
        std::string error;
    };
    std::shared_ptr<PsdExportState> psdExport;
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
    bool showAbout = false, aboutAtStart = false;
    gpu::TexturePtr logoTex;  // embedded logo (resource 2), loaded on the first frame
    bool logoTried = false;
    void LoadLogo();
    void DrawAbout();
    void DrawWelcome();
    // Effects: one entry point for every way of applying a library effect.
    enum class FxApply { Filter, Adjustment, Destructive, Node };
    void ApplyLibraryEffect(DocView& v, const std::string& id, FxApply how);
    void DrawEffectBrowser();
    // Custom code: declarations (inputs/params) + code + compile + save to library.
    // Returns true when the instance changed. `errorNode`: node whose compile errors to show (0 = find).
    bool CustomCodeUI(DocView& v, EffectInstance& fx, Id errorNode, const char* key);
    void ConvertToCustomCode(DocView& v, EffectInstance& fx);
    void DrawLayerPreview();
    struct {
        std::string search, category, selected;
        bool favoritesOnly = false, favLoaded = false;
        std::set<std::string> favorites;
        int source = 0;            // 0 active layer, 1 whole image
        float thumb = 120;
        gpu::TexturePtr sourceSmall;
        u64 sourceHash = 0, sourceFrame = 0;
        std::map<std::string, gpu::TexturePtr> previews;
    } fxBrowser;
    struct {
        std::string search, category;
        gpu::TexturePtr a, b;  // small copies of below / layer
        u64 sourceHash = 0, sourceFrame = 0;
        std::map<std::string, gpu::TexturePtr> previews;
    } blendBrowser;
    void UpdateRelativeDrag();
    // Scripted run (--demo, --capture, --dump-image, --save): never takes the focus or the real cursor.
    bool Automated() const { return demoMode != 0 || testEffects || !capturePath.empty() || !dumpPath.empty() || !cliSavePath.empty(); }  // hidden, pinned cursor while a slider/drag is dragged
    // Selection -> work path (selection read back, traced on the CPU).
    struct {
        DocView* view = nullptr;
        gpu::ReadbackPtr mask;
        float tolerance = 2.0f;
    } selToPath;
    void MakePathFromSelection(DocView& v);
    void UpdateSelectionToPath();
    bool showShaderErrors = false;
    bool showDemo = false;
    bool multiWindowTest = false;  // --multiwindow: viewports enabled in a scripted run
    bool layoutBuilt = false;
    bool resetLayout = false;
    char newName[64] = "Sans titre";
    int newW = 1920, newH = 1080, newPrecision = 0, newMode = 0, newBackground = 0;
    std::optional<DocState> editBefore;  // undo snapshot while a widget is active
    DocState frameBefore;                // properties panel state at frame start
    bool frameBeforeValid = false;
    NodeEditorState nodeEd;
    EffectEditorState efEd;
    bool meshStroking = false;
    bool meshDebugCanvas = false;
    float2 meshLastDab;
    float meshDabRemainder = 0;
    AiBridge bridge;
    AiSettings aiSettings;
    std::string settingsPath;
    AssistantState assistant;
    std::vector<std::future<std::function<void()>>> aiJobs;
    std::string genPrompt;
    int genSize = 0;
    bool genUseImage = false, genNewDoc = false, genBusy = false;
    void StartImageGeneration();
    void UpdateImageGeneration();  // provider, prompt and options from the Génération IA panel
    gpu::ReadbackPtr genRefReadback;
    std::function<void(std::string)> genPendingLaunch;
    // Generative fill (GenFill.cpp)
    struct GenFillState {
        DocView* view = nullptr;
        gpu::ReadbackPtr image, mask;
        std::string prompt;
        Id selection = 0;
        bool busy = false, launched = false;
    } genFill;
    void StartGenerativeFill(DocView& v, const std::string& prompt);
    void UpdateGenerativeFill();
    std::string statusMsg;
    double statusTime = 0;
    std::string pendingDrop;
};

}  // namespace sx
