#pragma once
#include <map>
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
#include "ai/Comfy.h"
#include "render/MeshPaint.h"
#include "mesh/UvUnwrap.h"
#include "mesh/MeshBake.h"
#include "render/PathTracer.h"
#include "io/VideoWriter.h"
#include "io/GifWriter.h"
#include "io/Lensfun.h"
#include "io/RawLoad.h"
#include "shaderixel_plugin.h"
#include "ui/Search.h"
#include "ui/CodeEdit.h"
#include "doc/ShapePaths.h"

#include <mutex>

#include <atomic>
#include <future>
#include <thread>

#include <imgui.h>
#include <memory>
#include <optional>
#include <unordered_map>

namespace sx {
// Lays out the compacted HLSL of the library effects of a folder (--format-library); number of files rewritten.
int FormatLibraryCode(const std::string& dir);

std::string ExeDir();  // folder of the executable

enum class Tool {
    Move, RectSelect, EllipseSelect, Lasso, PolyLasso, Wand,
    Brush, Eraser, Clone, Heal, Gradient, Bucket, Blur, Sharpen, Smudge, Dodge, Burn, Sponge, Liquify,
    Picker, Hand, Zoom, Crop, Text, Shape, QuickSelect, Pen, MagneticLasso,
    Patch, ContentMove, RedEye, MagicEraser, BackgroundEraser, HistoryBrush, PatternStamp, Projection, LayerTransform, Trim, PolyFill, Particles, Plugin, Count
};
inline bool IsRetouchTool(Tool t) {
    return (t >= Tool::Clone && t <= Tool::Liquify && t != Tool::Gradient && t != Tool::Bucket) || t == Tool::BackgroundEraser || t == Tool::HistoryBrush ||
           t == Tool::PatternStamp;
}
inline bool IsPaintTool(Tool t) { return t == Tool::Brush || t == Tool::Eraser || IsRetouchTool(t); }

struct DocView;
struct PhotoJob;
struct UvJob;
struct LinkSession;  // Unreal live link (UnrealLink.cpp)
// Material presets (MaterialLayers.cpp).
int MaterialPresetCount();
// Paint simulation presets (Paint3DSim.cpp).
int SimPresetCount();
const char* SimPresetName(int i);
const char* SimPresetHelp(int i);
const char* MaterialPresetName(int i);
void MaterialPresetValues(int i, float3& color, float& rough, float& metal, float& height, bool& hasColor, bool& hasRough, bool& hasMetal, bool& hasHeight);

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
    std::string group;  // built-in actions: their section in the editor
};
// Shared by the Actions panel, the action editor and the batch (Actions.cpp).
int ActionCmdCount();
const char* ActionCmdId(int i);
const char* ActionCmdLabel(const std::string& cmd);
Json ActionDefaultParams(const std::string& cmd);
std::string ActionStepSummary(const ActionStep& s);
std::string ActionStepDescription(const ActionStep& s);  // full sentence: what the step does exactly
const std::vector<Action>& BuiltinActions();             // simple ready-made actions (conversions, sizes...)
Json ActionToJson(const Action& a);
Action ActionFromJson(const Json& j);

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
    std::unique_ptr<PtRenderState> render;    // render mode scene (3D projects)
    std::unique_ptr<PtRenderState> viewRT;    // ray-traced 3D view (its own scene: model + floor)
    std::shared_ptr<LinkSession> link;        // opened from the Unreal plugin (sent back on Ctrl+S)
    std::unordered_map<Id, gpu::TexturePtr> snapshots;  // history brush sources (layer state before the first edit)
    int viewMode = 0;                         // 0 final image, 1 active layer only, 2 up to the active layer
    std::string displayLutKey;                // color management chain of displayLut
    gpu::TexturePtr displayLut;
    int displayLutN = 0;
    // Inner document of a smart object (open in this tab): its parent and the object.
    struct {
        DocView* parent = nullptr;
        Id smart = 0;
        u64 syncKey = 0;
    } smartLink;
    // Graph of a function node (open in this tab): its parent document and the node.
    struct {
        DocView* parent = nullptr;
        Id node = 0;
        u64 syncKey = 0;
    } funcLink;
};

// Execution flow being run (GraphExec.cpp).
struct ExecItem {
    enum Kind : u8 { Run, Follow, Loop } kind;
    Id node;
    int port;
    int index;
    Id owner;  // function whose graph the node is in (0 = the document's graph)
};
struct ExecRun {
    DocView* view = nullptr;
    Id owner = 0;     // function node whose graph runs (0 = the document's graph)
    Id event = 0;
    std::string label;
    std::vector<ExecItem> items;  // to do (stack)
    DocState before;
    int steps = 0;
    bool waiting = false, pendingSent = false;  // ComfyUI step
    Id pendingNode = 0, pendingOwner = 0;
    Id rootOwner = 0;  // function whose event started the run
    comfy::Job pendingJob;
    gpu::ReadbackPtr pendingImage;
};
bool ValueEditor(const char* label, Value& v);

// Function library (GraphFunctions.cpp).
struct FunctionLibEntry {
    std::string name, kind, path, desc;
    int inputs = 0, outputs = 0;
};
std::vector<FunctionLibEntry>& FunctionLibrary();
u64& FunctionLibraryGen();
extern std::function<Id(Document&, float2, const std::string&)> g_insertLibraryFunction;

struct NodeEditorState {
    // Comment frames: selected, hovered (part 1 header, 2 resize corner), dragged with the nodes inside.
    Id selComment = 0, hoverComment = 0;
    int hoverCommentPart = 0, commentDrag = 0;
    std::vector<Id> commentNodes;
    std::unordered_map<Id, ImVec4> commentRects;  // screen rect of each drawn comment title (demos)
    std::optional<DocState> commentBefore;
    // Search in the graph (Ctrl+F).
    bool searchOpen = false;
    char findText[64] = {};
    // Value edited on a node (unlinked value input): dragged (numbers) or in a small popup.
    Id valNode = 0;
    int valPin = -1;
    bool valDrag = false, valMoved = false, valPopup = false, valChanged = false;
    float valMouseX = 0;
    Value valStart;
    std::optional<DocState> valBefore;
    std::unordered_map<u64, ImVec4> valRects;  // screen rect of each drawn value field (node << 8 | pin), for demos
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
    bool minimapDrag = false;
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
    bool libCode = false;  // library graph effect: its code shown instead of the graph
    Id bigCode = 0;        // custom node whose code is open in the large editor window
    Id ctxFrom = 0, ctxTo = 0;          // add menu opened from a dropped link: its output, or its input
    int ctxFromOut = 0, ctxToIn = 0;
    bool ctxSensitive = true;           // add menu: only the nodes that fit the link
    int demoAdd = 0;                    // demos: opens the add menu (1 plain, 2 from an output, 3 with a search)
    std::set<Id> multi;                 // selected nodes (the details panel shows `selected`)
    bool boxing = false;                // box selection from an empty spot
    ImVec2 boxStart;
    bool frameAll = false;              // frame the whole graph on the next frame (a graph was opened)
    std::string marksKey;               // shader errors mapped to the HLSL nodes (code lines marked red)
    std::map<Id, std::vector<ui::CodeMark>> codeMarks, declMarks;
    Id pinEditNode = 0;                 // unconnected input edited in place (double click on its label)
    Id selComment = 0, dragComment = 0, sizeComment = 0, renameComment = 0;  // comment frames
    std::vector<Id> commentNodes;       // nodes moved with the dragged comment
    std::string commentText;
    int pinEditIn = 0;
    bool nodePreviews = true;           // result previews drawn in the nodes (effect graphs)
    double lastEdit = 0;                // time of the last change (node previews wait for a pause)
    std::string cachedJson;
    EffectGraph graph;
    std::optional<DocState> pendingBefore;
    // Library page and edit session (EffectLibrary.cpp).
    bool browse = false;
    int browseKind = 0;  // 0 effects, 1 blends, 2 brushes
    std::string browseSearch, browseSel;
    bool browseGraphs = false;  // library page: only the items made with a graph
    bool unlock = false;
    struct Session {
        bool active = false;
        DocState before;
        BrushSettings brushBefore, eraserBefore;
        std::string prevWindow;
        int prevSideTab = 0;
        bool prevHideBottom = false;
        std::string libId;  // original opened for editing ("" = copy)
        int libKind = 0;
    } session;
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
inline constexpr const char* kPalettePayload = "SX_PALETTE";  // node palette item: "<0|1><key>"

class App {
    friend struct PluginGlue;  // C host table of the plugins (Plugins.cpp)

public:
    int Run();

    // ---------------------------------------------------------------- plugins (Plugins.cpp, sdk/include/shaderixel_plugin.h)
    struct PluginEntry {
        std::string id, name, version, author, description, dir, dll, error;
        struct Script { std::string menu, run, description; };
        std::vector<Script> scripts;  // script plugins: menu commands running a program
        struct Panel { std::string title, run; };
        std::vector<Panel> panels;    // script plugins: windows drawn from a program's JSON
        bool enabled = true, loaded = false;
        void* module = nullptr;  // HMODULE
        std::string loadedCopy;
        void (*unload)(void*) = nullptr;
        void* unloadUser = nullptr;
        long long dllTime = 0;  // write time of the DLL when loaded (automatic reload)
    };
    struct PluginCommand { std::string plugin, path; SxCommandFn fn; void* user; };
    struct PluginPanel { std::string plugin, title; SxPanelFn fn; void* user; bool open = false; };
    struct PluginTool { std::string plugin, name, help; SxToolFn fn; void* user; };
    struct PluginOp { std::string plugin, op; };
    struct PluginCall { std::string plugin; AiResult result; SxResultFn done; void* user; };
    struct PluginProc { std::string plugin, label, log; void* process; };
    // Script panel: a long-running program (plugin.json "panels") describing its window in JSON lines on
    // stdout; the host draws it and sends the user's actions on its stdin (see docs/PLUGINS.md).
    struct ScriptPanel {
        std::string plugin, title, run, dir;
        bool open = false;
        void* process = nullptr;  // HANDLE
        void* stdinWrite = nullptr;
        std::shared_ptr<struct ScriptPanelIo> io;
        Json ui = Json::Array();
        std::string error;
    };
    std::vector<ScriptPanel> scriptPanels;
    void StartScriptPanel(ScriptPanel& sp);
    void StopScriptPanel(ScriptPanel& sp);
    void SendScriptPanel(ScriptPanel& sp, const Json& msg);
    void DrawScriptPanel(ScriptPanel& sp);
    bool pluginAutoReload = false;  // developers: a rebuilt DLL is reloaded by itself
    double pluginReloadCheck = 0;
    bool InstallPluginZip(const std::string& zip, std::string* error);
    bool CreatePluginSkeleton(const std::string& name, int kind, std::string* dirOut, std::string* error);  // 0 Python window, 1 Python command, 2 C++
    std::string pluginNewName;
    int pluginNewKind = 0;
    struct PluginRead { std::string plugin; gpu::ReadbackPtr rb; int format; SxPixelsFn done; void* user; };
    struct PluginListener { std::string plugin; SxEventFn fn; void* user; };
    std::vector<PluginListener> pluginListeners;
    Json pluginSettings;           // {plugin id: {key: value}} (plugins_settings.json)
    bool pluginSettingsDirty = false;
    const void* pluginLastDoc = nullptr;
    u64 pluginLastEdit = 0;
    void PluginEvent(int event, const std::string& text);
    std::vector<PluginRead> pluginReads;
    std::vector<PluginEntry> plugins;
    std::vector<PluginCommand> pluginCommands;
    std::vector<PluginPanel> pluginPanels;
    std::vector<PluginTool> pluginTools;
    std::vector<PluginOp> pluginOps;
    std::vector<PluginCall> pluginCalls;
    std::vector<PluginProc> pluginProcs;
    std::set<std::string> pluginsDisabled;
    std::string pluginCurrent;  // plugin being called (its registrations are tagged with it)
    int pluginTool = -1;
    bool pluginToolDown = false, pluginManager = false;
    struct { ImVec2 canvasMin; float2 pan; float zoom = 1; ImDrawList* dl = nullptr; } pluginDraw;
    void InitPlugins();
    void ScanPlugins();
    std::vector<std::string> PluginDirs();
    std::string PluginWorkspace(const std::string& plugin);
    bool LoadPlugin(PluginEntry& p);
    void UnloadPlugin(PluginEntry& p);
    void RemovePluginRegistrations(const std::string& id);
    void ShutdownPlugins();
    void UpdatePlugins();
    void FinishPluginCall(const std::string& plugin, const AiResult& r, SxResultFn done, void* user);
    void RunPluginScript(const PluginEntry& p, const PluginEntry::Script& s);
    void CallPlugin(const std::string& plugin, const std::function<void()>& fn);
    void DrawPluginsMenu();
    void DrawPluginPanels();
    void DrawPluginManager();
    void PluginToolInput(DocView& v, ImVec2 canvasMin, bool hovered);
    // ---------------------------------------------------------------- search everywhere (SearchUi.cpp, Search.cpp)
    std::string searchText;
    bool searchOpen = false, searchFocusRequest = false, searchHighlight = true, searchCrawlChecked = false, searchWasCrawling = false;
    int searchSel = 0;
    void InitSearch();
    void SaveSearch();
    void UpdateSearch();
    void DrawSearchBox();
    void ActivateSearchResult(const SearchEntry& e);
    struct SearchRow {
        int group = 0;  // 0..2 UI (entry), 3 effect, 4 brush, 5 smart material, 6 library function
        std::string label, context, payload;
        SearchEntry entry;
    };
    std::vector<SearchRow> searchRows, recentSearches;
    std::string searchRowsQuery = "\x01";
    void BuildSearchRows(const std::string& q);
    void RunSearchRow(const SearchRow& r);
    void RevealWindow(const std::string& name);
    void PluginTool3DInput(DocView& v, float2 local, u32 w, u32 h, bool hovered);  // tool events over the model (texture pixels)
    Surface* PluginSurface(unsigned layer, Id* id = nullptr);

    // Workspace state (saved in the ImGui ini / layouts, see Workspace.cpp).
    static constexpr int kSideTabs = 7;  // Général, Outils, Effets, Fusion, Peinture, 3D, IA
    int sideTab = 0;
    int sideTabRequest = -1;  // selects a side tab on the next frame
    bool hideBottom = false, hideRight = false;
    ImGuiID bottomZone = 0, rightZone = 0;
    std::vector<bool> extraWindows;  // "Fenêtre 2..." (open flags)
    struct PanelOpenFlag { bool open = true; };
    std::map<std::string, PanelOpenFlag> panelOpen;  // closed panels (tab cross)
    static constexpr int kLayoutMigration = 4;
    int layoutMigration = 0, layoutMigrationFrames = 0;  // saved layouts from older versions start at 0
    int layoutKind = 0;  // workspace currently built for: 0 image documents, 1 Paint 3D projects
    std::vector<std::string> focusAfterLayout;

private:
    std::string pendingLayoutIni, pendingLayoutName, layoutName;
    std::vector<std::string> savedLayouts;
    bool layoutSavePopup = false;
    void RegisterWorkspaceSettings();
    float ToolbarWidth() const;
    void DrawSidePanel();
    void ShowSideTab(int tab);
    struct SideZone {
        ImGuiID want = 0;  // panel tab chosen by the user
        int count = 0;     // tabs seen last frame
        int guard = 0;     // frames during which the chosen panel is enforced
    };
    std::unordered_map<ImGuiID, SideZone> sideTabSelection;  // dock node of a side tab -> its chosen panel
    int sideTabShown = -1, sideTabRestore = 0;
    void DockNewPanel(int tab, bool lower);
    // Node palette (NodeEditor.cpp): adds nodes to the layer graph (0) or the effect graph (1).
    int paletteFocus = 0;
    struct {
        bool pending = false, hasPos = false;
        int target = 0;
        std::string key;
        ImVec2 screenPos;
    } paletteAdd;
    std::string paletteSearch;
    void DrawNodePalette();
    void MigrateLayout();
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
    // Panels opened large in their own OS window (Workspace.cpp): corner icon, Fenêtre menu, F11 on the hovered panel.
    struct PopOut {
        ImGuiID dock = 0;  // where it was docked (to put it back)
        bool out = false;
        int pending = 0;   // frames before placing it (the undock happens at the next frame)
    };
    std::map<std::string, PopOut> popOuts;
    struct PopCorner {
        std::string name;
        ImVec2 rmin, rmax;
        bool hovered = false;
    };
    std::vector<PopCorner> popCorners;
    void TogglePopOut(const std::string& name);
    bool IsPoppedOut(const std::string& name) const { auto it = popOuts.find(name); return it != popOuts.end() && it->second.out; }
    void UpdatePopOuts();
    void PopOutCorner(const char* name);  // right after Begin(): the corner icon of a panel
    void DrawPopOutCorners();
    void DrawPopOutMenu();
    // Unreal export of a Paint 3D project (UnrealExport.cpp): mesh (OBJ), textures, material description (.sxmat.json).
    std::string exportUnrealDir;  // --export-unreal <folder>
    bool unrealDialog = false;
    Json UnrealSettings(Document& d);
    std::string ExportForUnreal(DocView& v, const std::string& folder);  // returns the description path
    void DrawUnrealExportDialog();
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
    void CreatePbrChannels(DocView& v, bool project = false);
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
    void SaveDocument(DocView& v, const std::string& path, bool asCopy = false);
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
    void SetView3DPreset(MeshScene& s, int k);  // 0 front, 1 back, 2 left, 3 right, 4 top, 5 bottom (orthographic)
    void MeshPrepare3D(DocView& v);
    EffectInstance* EditedEffect(DocView& v, std::string* where);
    void HandleShortcuts();

    // Property editing helpers (undo-aware)
    // onlyGroup: draw the parameters of that //@group only (Camera Raw sections).
    bool EffectParamsUI(EffectInstance& fx, const char* label, const char* onlyGroup = nullptr);
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
    void BrushStrokePreview(BrushSettings& b, const struct BrushDef* td, float h);
    void BrushTipPicker(BrushSettings& b);
    std::vector<Id> strokeTargets;    // 3D project: layers of the current 2D stroke (several channels)
    std::vector<float4> strokeColors;
    void ApplyStrokeSlots(DocView& v) { v.renderer.StrokeApplySlots(*v.doc, *ctx, strokeErase ? eraser : brush, strokeColors, strokeErase); }
    void BeginEditSession(const char* prevWindow);
    void EndEditSession(bool cancel);
    std::string LibraryTextOf(const EffectInstance& fx);
    bool SaveOriginal(std::string* err);
    void OpenLibraryItem(int kind, const std::string& id, bool original);
    void DrawEffectLibraryPage();
    void DrawEditSessionBar();
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
    // Working view: the toolbar offers the canvas tools or the 3D painting tools after it, not after the project kind.
    int canvasShownFrame = -10, view3dShownFrame = -10;
    int workView = 0;                                  // 0 canvas, 1 3D view (last clicked when both are visible)
    Tool viewTool[2] = {Tool::Brush, Tool::Brush};     // last tool of each view
    bool Use3DTools();
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
    void PlaceSmartFile(DocView& v, const std::string& path, bool linked, bool nativeSize = false);
    // Drag and drop: by the window under the drop point (render scene, graph / canvas, or a new document).
    struct DropRequest { std::string path; float x = 0, y = 0; };
    std::vector<DropRequest> pendingDrops;
    bool RouteDrop(const DropRequest& r);
    void AddImageNode(DocView& v, const std::string& path);  // node mode: image surface + Transformation node
    // Layer transform tool (LayerTransform.cpp): move / rotate / scale / pivot of image layers without loss
    // (smart objects keep the whole image), snapping grids, 3 speeds, world / relative axes.
    struct {
        int mode = 0;                 // 0 move, 1 rotate, 2 scale, 3 pivot
        bool world = true;            // axes of the document (world) or of the layer (relative)
        bool arrows = true;           // gizmo arrows / ring in the viewport
        bool overflow = true;         // show the parts outside the document
        bool lockAspect = true;
        bool snap[3] = {false, false, false};  // move (px), rotate (deg), scale
        float step[3] = {10.0f, 15.0f, 0.1f};
        int drag = -1;
        float2 startMouse, prevMouse, virt;
        double accRot = 0, accLog[2] = {0, 0};
        float2 sPos, sScale, sPivot;
        float sRot = 0;
        DocState before;
        bool changed = false;
        ImVec2 barMin, barMax;        // settings bar (last frame): not a canvas click
        ImVec2 canvasMin;             // view origin (last frame), for scripted tests
    } lxf;
    // What the transform tool acts on: a smart layer (Image: whole image kept), or a "Transformation"
    // effect (Draw layers: fill / text / shape filter; node mode: the Transform node).
    struct LayerXfTarget {
        Layer* layer = nullptr;       // smart layer (corners)
        EffectInstance* fx = nullptr; // transform_2d effect
        float W = 0, H = 0;           // transformed rectangle (contents, or the document for effects)
        bool created = false;         // converted to a smart object / Transform added just now
    };
    bool LayerXfResolve(DocView& v, bool create, LayerXfTarget& t);
    // Graph tools (GraphTools.cpp): bake / upscale / ComfyUI nodes.
    enum class ToolAction : u8 { Bake, ToggleLive, ConvertToImage, Export };
    struct ToolRequest { DocView* view; Id node; ToolAction action; int tries; };
    struct ToolJob { DocView* view = nullptr; Id node = 0; u64 inHash = 0; gpu::ReadbackPtr image; };
    struct ToolExportJob { gpu::ReadbackPtr image; std::string path; };
    std::vector<ToolRequest> toolRequests;
    std::vector<ToolJob> toolJobs;
    std::vector<ToolExportJob> toolExports;
    std::set<Id> toolBusy;
    bool graphFocused = false;       // the graph editor had the focus last frame (its own keys)
    bool showMinimap = true;         // graph editor minimap (M)
    bool graphFrameRequest = false;  // frame all nodes in the graph editor (demos)
    std::set<Id> graphFrameOnly;     // ... or only these
    bool ViewAlive(const DocView* v) const;
    void RequestToolAction(DocView& v, Id node, ToolAction a);
    bool ToolBakeNow(DocView& v, Id node, int tries);
    void ToolLaunchComfy(ToolJob& job);
    void ToolConvertToImage(DocView& v, Id node);
    void ToolExport(DocView& v, Id node);
    void UpdateGraphTools();
    void ToolNodeUI(DocView& v, Node& n);
    // Functions (GraphFunctions.cpp)
    void InitFunctions();
    void ScanFunctionLibrary();
    Id InsertLibraryFunction(DocView& v, const std::string& path, float2 pos);
    void FunctionFromSelection(DocView& v, const std::set<Id>& selection, const std::string& funcKind);
    gpu::Texture* NodeOutputTexture(DocView& v, Id src, u8 channel);
    void EditFunction(DocView& parent, Id fid);
    void UpdateFunctionEdits();
    bool FunctionApplyNow(DocView& v, Id fid, int tries);
    void SaveFunctionToLibrary(DocView& v, Id fid);
    void FunctionNodeUI(DocView& v, Node& n);
    std::pair<DocView*, Id> functionEditRequest{nullptr, 0};  // opened next frame (outside the graph window)
    // Typed nodes and execution (GraphExec.cpp)
    std::unique_ptr<ExecRun> execRun;
    void ValueInputsUI(DocView& v, Node& n);
    void GraphVariablesUI(DocView& v, Graph& g);
    void TypedNodeUI(DocView& v, Node& n);
    void FunctionSettingsUI(DocView& v, Node& f);
    void EffectPinsUI(DocView& v, Node& n);
    void StartExec(DocView& v, Id owner, Id event);
    const Graph* ExecGraph(ExecRun& r);
    void FinishExec(const std::string& msg);
    void ExecStoreResult(ExecRun& r, Id node, gpu::Texture* tex, gpu::TexturePtr full);
    void UpdateExec();
    // Trim sheets (TrimSheet.cpp)
    struct {
        int preset = 0;
        std::string custom = "256, 128, 128, 64, 64, 0";
        int splitCols = 4;
        float seamBlend = 0.25f;
        float previewOffset = 0.5f;
        int hoverTrim = -1, hoverEdge = -1;  // edge: 0 left, 1 top, 2 right, 3 bottom
        bool dragging = false;
        DocState before;
    } trimUi;
    void DrawTrimPanel();
    void DrawZoneToggles();
    // Resources panel (ResourceBrowser.cpp): image / mesh explorer, lights and objects
    struct ExplorerThumbData {
        std::vector<u8> rgba;
        int w = 0, h = 0;
        std::string info;
    };
    struct ExplorerThumbEntry {
        gpu::TexturePtr tex;
        std::shared_ptr<std::future<ExplorerThumbData>> job;
        std::string info;
        bool done = false;
    };
    struct {
        bool loaded = false;
        int tab = 0, running = 0;
        std::string dirImages, dirMeshes, filter, listedKey, savedDir[3];
        std::vector<std::string> pins, folders, files;
        double listedTime = 0;
        float thumb = 96;
        std::map<std::string, ExplorerThumbEntry> thumbs;
    } explorer;
    void LoadExplorerSettings();
    void SaveExplorerSettings();
    gpu::Texture* ExplorerThumb(const std::string& path, bool mesh, std::string* info);
    void AddScenePreset(int kind);
    void DrawResourceBrowser();
    int explorerTabRequest = -1;  // tab to show (demos)
    // AI inpainting (AiInpaint.cpp)
    struct {
        bool busy = false, launched = false;
        DocView* view = nullptr;
        gpu::ReadbackPtr image, mask;
        std::string prompt, negative, workflow;
        int provider = 0;  // 0 ComfyUI, 1 OpenAI, 2 Gemini, 3 local test
        int variations = 1, grow = 8, feather = 24, margin = 30;
        float denoise = 1.0f;
        Rect crop;
        std::vector<u8> maskFull;  // grown + feathered mask (document size)
        std::vector<std::vector<u8>> results;
        std::vector<gpu::TexturePtr> thumbs;
        Id layer = 0, surface = 0;
        int current = 0;
    } inpaint;
    void EnsureInpaintMask(DocView& v);
    void StartInpaint(DocView& v);
    void UpdateInpaint();
    void FinishInpaint(const std::vector<std::vector<u8>>& results, const std::string& err);
    void ShowInpaintVariant(int k);
    void DrawInpaintPanel();  // fold / unfold the graph zone, quick layer pick (Workspace.cpp)
    // Templates (Templates.cpp)
    struct TemplateEntry {
        std::string name, category, desc, path, thumb;
        bool user = false;
        u32 w = 0, h = 0;
    };
    std::vector<TemplateEntry> templates;
    std::map<std::string, gpu::TexturePtr> templateThumbs;
    struct ThumbJob {
        gpu::ReadbackPtr image;
        std::string path;
        std::vector<Rect> trims;  // drawn over the thumbnail (trim sheets)
    };
    std::vector<ThumbJob> thumbJobs;
    struct {
        std::string category, search, saveName, saveCategory = "Mes modèles", saveDesc;
        int selected = -1, tabRequest = 0;
        bool confirmDelete = false, saveRequest = false;
    } tplUi;
    void ScanTemplates();
    bool NewFromTemplate(const TemplateEntry& t);
    void SaveAsTemplate(DocView& v, const std::string& name, const std::string& category, const std::string& desc, const std::string& dir = "");
    struct BuiltinTemplate { DocView* view; std::string name, category, desc; };
    std::vector<BuiltinTemplate> BuildBuiltinTemplates();  // TemplatesBuiltin.cpp
    void UpdateTemplateJobs();
    gpu::Texture* TemplateThumb(const std::string& path);
    void DrawSaveTemplateDialog();
    void TrimOverlay(DocView& v, ImVec2 canvasMin, ImVec2 avail, ImDrawList* dl);
    void TrimInput(DocView& v, ImVec2 canvasMin, bool hovered);
    void TrimSelectPixels(DocView& v, int idx);
    void TrimMakeSeamless(DocView& v, int idx);
    void TrimIdLayer(DocView& v);
    bool ExportTrimLayout(DocView& v, const std::string& path);
    void TrimApplyLayout(DocView& v, std::vector<Trim> trims, const std::string& label);
    void TrimPreview3D(DocView& v);
    // SubGraph: a Shaderixel document (.sxl / .sx3d) used as a smart object, its output baked as the image.
    std::unique_ptr<Document> loadedSubDoc;  // set by LoadImageTexture for a document (embedded placement takes it)
    std::vector<std::pair<u64, std::shared_ptr<DocView>>> deferredViews;  // released once the GPU is done with them
    void LayerTransformInput(DocView& v, ImVec2 canvasMin, bool hovered);
    void LayerTransformOverlay(DocView& v, ImVec2 canvasMin, ImVec2 avail, ImDrawList* dl);
    bool LayerTransformOverflow(DocView& v, gpu::Texture*& content, float inv[9]);  // outside-document preview
    void LayerImagePropertiesUI(DocView& v, Layer& l);
    // Unreal .uasset (UAssetSupport.cpp): textures decoded natively, meshes converted by Unreal in the background.
    struct UAssetJob {
        std::string path, name;
        std::future<std::pair<std::string, std::string>> result;  // glb, error
        std::vector<std::function<void(const std::string&)>> done;
    };
    std::vector<std::unique_ptr<UAssetJob>> uassetJobs;
    std::set<std::string> uassetFailed;
    unsigned char* LoadImagePixels(const std::string& path, int& w, int& h);  // RGBA8 (stbi_image_free), .uasset too
    void ConvertUAssetAsync(const std::string& path, std::function<void(const std::string& glb)> done);
    void UpdateUAssetJobs();
    bool OpenUAsset(const std::string& path, bool asLayer);  // true: handled (mesh, error)
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
        std::string name, error, exportFolder, exportStem, lastExport, key;
        std::vector<ActionStep> steps;
        std::vector<int> srcIndex;  // index of each step in its action (editor highlight)
        size_t next = 0;
        int retries = 0;
        bool exporting = false;
        gpu::ReadbackPtr wait;
        std::function<void(bool)> done;
    } actionRun;
    std::string actionLastExport;  // last file written by an Exporter step
    int exportQuality = 92;        // JPG quality of ExportImage
    // Action editor (ActionsEditor.cpp).
    int actionStepSel = -1, builtinSel = -1;  // builtinSel >= 0: a built-in action is shown
    std::string actionFxFilter;
    bool actionEditorFocus = false;
    struct BatchLog {
        std::string file, out, msg;
        int state = 0;  // 0 done, 1 skipped, 2 error
    };
    struct {
        bool open = false, running = false, cancel = false, subfolders = false, keepTree = true, opened = false;
        int sourceKind = 0;  // 0 folder, 1 open documents
        int format = 1;      // 0 none (Exporter steps only), 1 PNG, 2 JPG, 3 TGA, 4 BMP, 5 PSD, 6 SXL, 7 same as the source
        int quality = 92, conflict = 2, stage = 0;  // conflict: 0 replace, 1 skip, 2 numbered copy
        std::string source, dest, pattern = "{nom}", extFilter;
        std::vector<std::string> chain;  // "u:<name>" my actions, "b:<name>" built-in
        std::vector<std::string> files, errors;
        std::vector<DocView*> views;     // source = open documents
        std::vector<BatchLog> log;
        size_t index = 0, done = 0, skipped = 0;
        DocView* view = nullptr;
    } batch;
    const Action* FindActionKey(const std::string& key) const;
    bool StartActionSteps(DocView& v, const std::string& name, const std::vector<ActionStep>& steps, int first, int last, const std::string& key,
                          std::function<void(bool)> done = {});
    void WriteImageAs(DocView& v, const std::string& path, const std::string& format, int quality);
    std::vector<std::string> BatchScan() const;
    std::string BatchName(size_t i, const Document* d) const;    // d null: preview, sizes unknown
    std::string BatchFolder(size_t i, const Document* d) const;
    void ImportActions();
    void ExportActions(int index);  // -1: all
    void DrawActionEditor();
    void DrawActionInspector(ActionStep& s, bool readOnly);
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
    // Camera Raw workspace (CameraRawUi.cpp): development of the active layer with the "camera_raw" and
    // "lens_profile" filters (lensfun profiles), big preview, before / after, OK / Annuler.
    struct {
        bool open = false;
        DocView* view = nullptr;
        Id layer = 0;
        std::optional<DocState> before;
        bool compare = false;      // show the image before the development
        int zoom = 0;              // 0 fit, 1 100 %
        float2 pan = {0.5f, 0.5f}; // 100 %: view center (fraction of the image)
        u64 changeFrame = 0;
        u64 lastHash = 0;
        bool firstFrame = false;
        size_t histStart = 0;      // history entries of the session, merged into one on OK
        std::vector<char> savedEnabled;  // states of the Photo RAW filters while comparing
        gpu::TexturePtr tex;       // preview
        bool pickWB = false;       // white balance eyedropper active
        gpu::ReadbackPtr wbRb;     // picked pixels (layer source)
        gpu::ReadbackPtr statsRb;  // downsampled source for the automatic settings
        gpu::TexturePtr statsTex;
        int statsWant = 0;         // 1 white balance, 2 tones
        int preset = -1;
        char presetName[64] = "";
        bool presetWB = false;
        int localSel = -1;         // selected local adjustment (index among the layer's photo_raw_local filters)
        int dragHandle = -1;       // 0 A / center, 1 B, 2 both (linear), 3 radius x, 4 radius y
        float2 viewOrigin = {0, 0};
        float viewInv = 1;
        ImVec2 viewPos = {0, 0};
    } camRaw;
    // Local adjustments overlay (handles on the Photo RAW preview); true while a handle is dragged.
    bool PhotoRawLocalHandles(Layer& l, ImVec2 p0, ImVec2 size, bool activated, bool active);
    void PhotoRawLocalsUI(DocView& v, Layer& l);
    void PhotoRawCompare(Layer* l, bool on);
    std::optional<EffectInstance> rawClipboard;  // Photo RAW settings copied (Copier / Coller)
    void PhotoRawPresetsUI(EffectInstance& fx, DocView& v);
    void PhotoRawAuto(DocView& v, Layer& l, int want);
    void PhotoRawPoll(DocView& v, Layer& l);
    // Action step "photo_raw" (batch development): 0 done, 1 wait, 2 error.
    int PhotoRawStep(DocView& v, const Json& p, std::string& error, int& retries);
    bool LensPickFromShot(const Document& d);  // lensPick <- the lens of the photo's metadata
    Json PhotoRawSettings(const EffectInstance& fx);  // {id: value} differing from the defaults (+ "tone")
    void PhotoRawToAction(DocView& v, Layer& l, bool openBatch);
    // Lens picker (lensfun) state shared by Camera Raw and the effect's properties.
    struct {
        char filter[128] = "";
        int lens = -1;          // index in Lensfun().lenses
        float focal = 0, aperture = 0, distance = 1000, crop = 0;
        std::string status;
        const void* autoFor = nullptr;  // document whose shot preselected the lens
        bool applyDist = true, applyTca = true, applyVig = true;
    } lensPick;
    std::string lensfunDir;  // setting ("": automatic)
    std::string uiLanguage = "en";  // interface language (ui/Lang.h), English by default
    void OpenCameraRaw(DocView& v);
    void DrawCameraRaw();
    void EnsureLensfun(bool reload = false);
    // Lens database status + lens / focal / aperture picker; writes the profile into fx (a lens_profile
    // effect). doc: its shot metadata preselects the lens. True when fx changed.
    bool LensfunPickerUI(EffectInstance& fx, const Document* doc);
    void ApplyLensPick(EffectInstance& fx);  // the lensPick choice -> lens_profile parameters
    void DrawPhotoDialog();
    // Paint 3D projects (Paint3D.cpp).
    struct {
        bool open = false;
        int mesh = 0, resolution = 1, exportTemplate = 1, basePreset = 1;
        std::string meshPath;
        bool channels[7] = {true, true, true, true, false, true, true};
        bool autoUv = false;  // xatlas UVs at creation (meshes without usable UVs)
        bool skipPaintLayer = false;  // no "Peinture 1" (the caller adds its layers first)
    } p3dNew;
    struct {
        bool use[7] = {true, true, false, false, false, false, false};
        float rough = 0.45f, metal = 0.0f, height = 0.6f;
        float4 emissive = {1, 0.6f, 0.2f, 1};
        // 3D brush dynamics (View3D.cpp).
        bool tip = true;     // the programmable tip of the brush (Pinceaux panel)
        float jitterSize = 0, jitterAngle = 0, scatter = 0, jitterFlow = 0, angle = 0;
        bool follow = false;  // dab angle follows the stroke
        float lazy = 0;       // lazy mouse distance (screen px)
        int polyMode = 1;     // polygon fill: 0 triangle, 1 flat face, 2 UV island, 3 mesh part, 4 whole model
        // Paint simulation (Paint3DSim.cpp): particles per second, size (model units), gravity, adhesion, life (s), spread.
        int simPreset = 0;
        float simRate = 60, simSize = 0.015f, simGravity = 1, simAdhesion = 0.5f, simLife = 2.5f, simSpread = 0.5f;
    } brush3D;
    // Polygon fill (Paint3DTools.cpp): mesh adjacency, rebuilt when the mesh changes.
    struct MeshTopology {
        const void* scene = nullptr;
        u32 revision = ~0u;
        size_t tris = 0;
        std::vector<u32> face, island, part;  // group of each triangle
    } meshTopo;
    int polyHover = -1;
    struct SimParticle {
        float3 p, v, n;
        float life = 1, size = 0.01f, alpha = 1, trail = 0;
        bool onSurface = false, alive = true;
    };
    struct {
        DocView* view = nullptr;
        MeshRaycaster rc;
        const void* scene = nullptr;
        u32 rev = ~0u;
        std::vector<SimParticle> parts;
        bool stroking = false;
        float emitAcc = 0;
        u32 rng = 777;
        double last = 0;
    } sim;
    bool CameraRay(MeshScene& s, float2 local, u32 w, u32 h, float3& o, float3& d);
    float Rand01();
    void SimEmit(DocView& v, float2 local, u32 w, u32 h, float dt);
    void SimStep(DocView& v, float dt, std::vector<float4>& dabs, std::vector<float4>& ex);
    void UpdateSimulation(DocView& v, bool emitting, float2 local, u32 w, u32 h, const std::vector<Id>& targets, const std::vector<float4>& colors);
    int PickMeshTriangle(MeshScene& s, float2 screen, u32 w, u32 h);
    void BuildMeshTopology(MeshScene& s);
    std::vector<u32> PolyFillTriangles(MeshScene& s, int tri, int mode);
    void PolygonFill(DocView& v, int tri, bool erase);
    void DrawPolyHighlight(MeshScene& s, int tri, ImVec2 origin, u32 w, u32 h);
    struct {
        gpu::TexturePtr tex;
        int w = 0, h = 0;
        std::string path;
        float size = 0.4f, rotation = 0, opacity = 1;
        bool imageColor = true;
        // Stencil (Substance-like): the image stays on screen, the brush paints through it.
        bool stencil = false, stencilInvert = false, stencilLuminance = true, stencilPlaced = false;
        float stencilThreshold = 0.5f, stencilContrast = 2.0f;  // luminance mode: mid-tone images made black and white
        float2 stencilPos;  // view-local pixels
    } decal;
    struct PackJob {
        std::string path;
        bool unity = false;
        gpu::ReadbackPtr ao, rough, metal;
    };
    std::vector<PackJob> packJobs;
    std::string layoutSlot[2];        // saved workspace of each kind
    Json MeshToJson(const MeshData& m);
    bool MeshFromJson(const Json& j, MeshData& m);
    void NewPaint3DProject();
    Id AddPaintLayer3D(DocView& v, const std::string& name);
    bool IsPaintLayer3D(Document& d, Id material);
    std::vector<Id> Paint3DTargets(DocView& v, std::vector<float4>& colors);
    void DrawLayers3DPanel();
    void DrawBrushMaterialPanel();
    bool LoadDecal(const std::string& path);
    void ExportProjectTextures(DocView& v, const std::string& basePath);
    void UpdatePaint3D();
    void DrawPaint3DNewDialog();
    void SwitchWorkspaceKind();
    // Automatic UVs (UvTool.cpp, xatlas).
    struct {
        bool open = false, hasResult = false, preview = false, transfer = true;
        int preset = 0;
        UnwrapOptions options{.resolution = 0};
        std::string message;
        std::shared_ptr<MeshData> previousMesh;
        std::unique_ptr<MeshScene> scene;
        const void* sceneFor = nullptr;
        gpu::TexturePtr checker;
    } uvTool;
    std::shared_ptr<UvJob> uvJob;
    void StartUvUnwrap(DocView& v);
    void ApplyUvUnwrap(DocView& v);
    void DrawUvToolWindow();
    gpu::Texture* UvPreviewRender(DocView& v, u32 w, u32 h);
    // Render mode (RenderMode.cpp): path traced scene of a 3D project.
    PtRenderState& RenderStateFor(DocView& v);
    std::shared_ptr<MeshData> RenderObjectMesh(DocView& v, PtObject& o);
    DocView* RenderHiddenProject(PtRenderState& r, const std::string& path);  // imported .sx3d (not a tab)
    bool ImportIntoRender(DocView& v, const std::string& path);              // model / .sx3d into the render scene
    Json RenderToJson(const PtRenderState& r, const std::string& baseDir = "");
    void RenderFromJson(DocView& v, const Json& j, const std::string& baseDir = "");
    // Scenes (RenderScenes.cpp): scene files, presets, gathering the files a scene uses.
    struct ScenePreset { std::string name, desc, path; bool user = false; };
    std::vector<ScenePreset> scenePresets;
    struct {
        int preset = 0, mode = 0;  // mode: 0 everything, 1 lighting + environment, 2 add the objects
        std::string saveName, saveDesc;
    } sceneUi;
    bool SaveScene(DocView& v, const std::string& path, const std::string& name, const std::string& desc);
    bool LoadScene(DocView& v, const std::string& path, int mode);
    void ScanScenePresets();
    void GatherSceneFiles(DocView& v);
    void DrawScenesUI(DocView& v);
    struct BuiltinScene { std::string name, desc; PtRenderState* state; };
    void SaveBuiltinScenes(DocView& v, const std::string& dir);
    gpu::Texture* UpdateRender(DocView& v, u32 w, u32 h);
    int RenderSamples(PtRenderState& r);
    void ExportRender(DocView& v, const std::string& path);
    void ExportRenderPasses(DocView& v, const std::string& path);  // image + albedo, normal, depth, mask
    void DrawRenderView();
    void DrawRenderScenePanel();
    void DemoExample3D();  // example projects for the package (DemoExamples.cpp)
    // Unreal live link (UnrealLink.cpp).
    void InitLink();
    void ShutdownLink();
    bool OpenLinkSession(const std::string& sessionFile);
    void SendToUnreal(DocView& v, bool save = true);
    bool LinkLive(DocView& v) const;
    bool LinkBusy() const;
    void SetLinkLive(DocView& v, bool live);
    void UpdateLink();
    bool linkInstanceFile = false;
    std::string pendingLinkOpen;  // --link <session.json>, opened once the app runs
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
    // Fill material with explicit values (sRGB color; channels without a value are left out). Returns its id.
    Id AddMaterialLayerValues(DocView& v, const std::string& name, const float3* color, const float* rough, const float* metal, const float* height,
                              bool fullMask);
    // Smart masks and smart materials (SmartMaterials.cpp).
    struct {
        DocView* view = nullptr;
        std::shared_ptr<std::atomic<float>> progress;
        std::shared_ptr<std::atomic<bool>> cancel;
        std::future<MeshMaps> job;
        u32 revision = 0;
        bool running = false;
    } mapsBake;
    std::map<Id, std::string> smartMaskDone;  // mask surface -> settings it was generated with
    int mapsResolution = 1024;
    void StartMeshMapsBake(DocView& v);
    // Baker (Baker.cpp): maps from the project's model or a high poly model.
    struct {
        bool open = false, running = false, useHigh = false, toChannels = true, heightToChannel = false, exportPng = false;
        std::string highPath, exportDir;
        BakeOptions opt;
        int sizeIndex = 2, idMode = 0;
        DocView* view = nullptr;
        std::shared_ptr<std::atomic<float>> progress;
        std::shared_ptr<std::atomic<bool>> cancel;
        std::future<BakeResult> job;
        u32 revision = 0;
        double startTime = 0;
    } baker;
    void StartBake(DocView& v);
    void PollBake();
    void DrawBakerWindow();
    void DrawView3DRenderSettings(MeshScene& s);
    gpu::Texture* UpdateViewRayTrace(DocView& v, u32 w, u32 h);
    gpu::Texture* UpdateRenderState(DocView& v, PtRenderState& r, u32 w, u32 h);
    bool DrawPostSettings(PostSettings& p, bool withExposure);  // shared by the 3D view and the render mode
    void WriteLayerPixels(Document& d, Id surface, const std::vector<float>& src, int channels, int w, int h, bool dataChannel);
    Id AddBakedLayer(DocView& v, PbrChannel c, const std::string& name, const std::vector<float>& src, int channels, int size);
    void PollMeshMapsBake();
    void UpdateSmartMasks(DocView& v);
    void DrawSmartMaskEditor(DocView& v, Id material);
    // Projected materials (Projection.cpp).
    std::map<std::string, gpu::TexturePtr> projImages;  // source -> image
    std::map<Id, std::string> projectionDone;          // layer surface -> settings it was generated with
    gpu::Texture* ProjectionImage(const std::string& source);
    Id AddProjectedMaterial(DocView& v, const std::string& source, const std::string& name);
    void UpdateProjections(DocView& v);
    void DrawProjectionEditor(DocView& v, Id material);
    void DrawMaterialFilters(DocView& v, Id material);
    void SoloMaterial(DocView& v, Id material);  // Alt + click on a 3D layer's visibility
    // 3D folders: a group per channel group (same material id, shared mask).
    bool IsFolder3D(Document& d, Id material);
    Id Folder3DOf(Document& d, Id material);
    int Folder3DDepth(Document& d, Id material);
    Id CreateFolder3D(DocView& v, Id wrap);
    void MoveIntoFolder3D(DocView& v, Id material, Id folder);
    void MoveOutOfFolder3D(DocView& v, Id material);
    void UngroupFolder3D(DocView& v, Id folder);
    // 3D eyedropper: the material under a point of the 3D view goes to the brush material.
    bool PickMeshUv(MeshScene& s, float2 sp, u32 w, u32 h, float2& uv);
    void StartMaterialPick(DocView& v, float2 sp, u32 w, u32 h);
    void PollMaterialPick();
    struct {
        gpu::ReadbackPtr rb[4];  // base color, roughness, metal, height
        bool active = false;
    } matPick;
    // Graph: a relay node inserted on the link src -> dst (input, channel), at graph position pos.
    void InsertRelay(DocView& v, Id src, Id dst, int input, u8 channel, float2 pos);  // Alt + click on a 3D layer's visibility
    // Texture export presets (TextureExport.cpp).
    struct TexExportOptions {
        bool open = false;
        int preset = 0, depth = 0, size = 0, normal = -1, padding = 8;
        std::string folder, name, pattern = "{nom}_{carte}";
    } texExport;
    struct TexExportJob {
        DocView* view = nullptr;
        TexExportOptions opt;
        std::array<gpu::ReadbackPtr, (int)PbrChannel::Count> ch{};
        bool has[(int)PbrChannel::Count] = {};
        gpu::TexturePtr bakedNormal;
        gpu::ReadbackPtr pos;
        u32 width = 0, height = 0;
        std::string name;
        std::future<std::string> result;
    };
    std::vector<TexExportJob> texExportJobs;
    void StartTextureExport(DocView& v);
    void PollTextureExports();
    void DrawTextureExportDialog();
    // Presentation (Marmoset-like): lighting presets, camera bookmarks, focus picking, turntable export.
    struct TurntableState {
        bool open = false, running = false;
        int frames = 120, fps = 30, width = 1920, height = 1080, samples = 96;
        bool viewSize = false, png = false, mp4 = true, gif = false, fixedLighting = true;
        std::string path;  // .mp4 (images: <name>_0001.png beside it)
        DocView* view = nullptr;
        int frame = 0, settle = 0, written = 0;
        float yaw0 = 0, env0 = 0, lightYaw0 = 0;
        int samples0 = 0;
        std::vector<float> xYaw0;
        std::vector<gpu::ReadbackPtr> pending;  // in frame order
        std::unique_ptr<VideoWriter> video;
        std::unique_ptr<GifWriter> gifw;
        std::string error;
    } turntable;
    struct {
        bool on = false;
        float t = 0;
        CameraBookmark from, to;
    } camAnim;
    bool focusPick = false;  // next click on the 3D view sets the depth of field's focus
    void DrawLightingSettings(MeshScene& s);  // + ApplyLightingPreset (below App)
    void DrawCameraBookmarks(MeshScene& s);
    void UpdateCameraAnimation(MeshScene& s);
    bool PickFocus(MeshScene& s, float2 local, u32 w, u32 h);
    void DrawTurntableDialog();
    void StartTurntable(DocView& v);
    void TurntableFrame(DocView& v, u32& w, u32& h);
    void TurntableCapture(DocView& v, gpu::Texture* img);
    void StopTurntable();
    void PollTurntable();
    void ApplySmartMaterial(DocView& v, const Json& def);
    std::vector<std::string> SmartMaterialDirs();
    std::string SmartMaterialUserDir();
    gpu::Texture* SmartMaterialThumb(DocView& v, const std::string& name);
    bool DrawSmartMaterialGrid(DocView& v);
    bool smartGridRequest = false;  // opens the smart material grid (demos)
    std::vector<Json> smartList;
    bool smartListLoaded = false;
    bool SaveSmartMaterial(DocView& v, const std::string& name, const std::string& desc);
    void SaveViewCrop(const gpu::Readback& rb, ImVec4 rect, const std::string& path);
    void PollSmartMaterialSave();
    std::string smartSaveName, smartSaveDesc;
    struct {
        gpu::ReadbackPtr rb;
        ImVec4 rect;
        std::string path, name;
    } smartSave;
    std::unordered_map<std::string, gpu::TexturePtr> smartThumbs;  // previews (thumbs/<name>.png)
    const std::vector<Json>& SmartMaterials();
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
    struct PendingExport { std::string path; gpu::ReadbackPtr rb; Rect crop; int quality = 92; };  // crop: part of the image written (empty = all)
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
    // Reference image for previews (effect thumbnails, function inputs with nothing linked).
    gpu::Texture* ReferenceTexture(DocView& v);
    // Reference images (N x N, RGBA8): 0 logo on a background, 1 color chart, 2 gradient, 3 checker, 4 white, 5 black.
    std::vector<u8> ReferencePixels(int kind, int N);
    // Image analysis panel (Analysis.cpp): scopes from a reduced sRGB copy of the image or of the active layer / node.
    // Before / after: a snapshot of the image compared on the canvas (split, alone, difference).
    struct {
        gpu::TexturePtr snap, reduced;
        u32 reducedFactor = 0;
        DocView* view = nullptr;
        int mode = 1;            // 1 split, 2 snapshot only, 3 difference (shown when a snapshot exists and `on`)
        bool on = false;
        float split = 0.5f;      // fraction of the view width
        bool dragging = false;
    } abCompare;
    void TakeSnapshot(DocView& v);
    // Custom shapes (ShapeLibrary.cpp).
    std::vector<CustomShape> userShapes;
    bool userShapesLoaded = false;
    std::string shapeNewName;
    gpu::TexturePtr shapeAtlas;  // picker icons
    u64 shapeAtlasKey = 0;
    std::vector<CustomShape>& UserShapes();
    bool SaveUserShape(const std::string& name, std::vector<SubPath> path);
    bool ShapePicker(ShapeStyle& s, DocView* v);  // true: the shape changed
    bool CompareSplitInput(DocView& v, ImVec2 canvasMin, ImVec2 size, bool hovered);
    void DrawCompareMenu();
    struct AnalysisSampler {
        float2 pos;
        gpu::ReadbackPtr rb;
        float4 value;
        bool valid = false;
        u64 frame = 0;
    };
    struct AnalysisStat {
        float min = 0, max = 0, mean = 0, median = 0, sd = 0, clipLow = 0, clipHigh = 0;
    };
    struct {
        int source = 0;          // 0 whole image, 1 active layer / node
        int waveMode = 0;        // 0 luma, 1 RGB overlay, 2 RGB parade
        int histoChannel = 0;
        bool logHisto = false, quad = false;
        float gain = 1.0f;       // scope brightness
        gpu::ReadbackPtr rb;
        DocView* view = nullptr;
        u64 hash = 0, frame = 0, version = 0;
        bool valid = false;
        u32 w = 0, h = 0, srcW = 0, srcH = 0, waveW = 0;
        std::vector<float> px;   // RGBA 0..1 (sRGB)
        u64 bins[5][256] = {};   // R, G, B, luma, alpha
        AnalysisStat stats[5];
        float transparent = 0;
        std::vector<u32> wave[4];
        std::vector<u32> vs;
        std::vector<float> vsColor, spectrum;
        std::vector<std::pair<float4, float>> palette;  // dominant colors and their share
        gpu::TexturePtr tex[6];  // waveform, vectorscope, spectrum, false color, sharpness map, difference
        u64 texKey[6] = {};
        // Sharpness (local variance of the Laplacian) and noise (Immerkaer) of the analysed copy.
        std::vector<float> sharpMap;
        u32 sharpW = 0, sharpH = 0;
        float sharpness = 0, noise = 0;
        // Comparison with another layer (same reduced size).
        Id compareLayer = 0;
        bool compareWanted = false;
        gpu::ReadbackPtr rbB;
        u64 hashB = 0, hashBFor = 0, frameB = 0, versionB = 0;
        std::vector<float> pxB;
        float diffMean = 0, diffMax = 0, psnr = 0, changedShare = 0;
        gpu::ReadbackPtr probe;
        float4 probeValue;
        bool probeValid = false, wantProbe = false;
        float2 probePos{-1, -1};
        u64 probeFrame = 0;
        int demoTab = -1;        // demos: tab to select
        int overlay = 0;         // on the canvas: 1 zebras, 2 clipping, 3 false color, 4 focus peaking, 5 luminance
        bool selectionOnly = false;  // only the pixels of the selection
        gpu::ReadbackPtr rbSel;      // selection reduced to the analysis size
        std::vector<float> sel;      // coverage 0..1 per analysed pixel (empty: none)
        u64 selVersion = 0;
        // Color samplers pinned on the canvas (Shift+click with the eyedropper), read continuously.
        std::vector<AnalysisSampler> samplers;
        float zebra = 0.95f, peaking = 0.12f;
    } analysis;
    void UpdateAnalysis();
    void ComputeAnalysis();
    gpu::Texture* AnalysisImage(DocView& v, int kind);
    void DrawAnalysis();
    // Effect previews (effect editor and its library page): simulated inputs run through an effect.
    struct {
        int src[4] = {0, 2, 4, 3};  // per input: logo, chart, gradient, checker, white, black, active layer, whole image, file
        std::string path[4];
        bool show = true;           // preview column of the effect editor
        bool node = false;          // the selected node instead of the effect output
        std::map<std::string, gpu::TexturePtr> refs;
        struct Live { gpu::TexturePtr tex; u64 hash = 0, frame = 0; } live[4];
        struct Slot { gpu::TexturePtr out; u64 hash = 0; };
        std::map<u64, Slot> slots;   // 0 effect, 1 node, 2 library page, 0x100000000 + id: previews on the nodes
        std::map<Id, std::pair<std::string, std::string>> nodeGraphs;  // node -> (key, graph with the output on it)
        std::string nodeKey, nodeGraph;  // node preview: graph with the output on the selected node
    } efPv;
    gpu::Texture* PreviewSource(DocView& v, int input, u64& hash);
    // The effect on the simulated inputs (cached per slot); `waiting`: an update is compiling.
    gpu::Texture* RenderEffectPreview(DocView& v, const EffectInstance& fx, int inputs, u64 slot, bool* waiting = nullptr);
    void DrawPreviewSources(DocView& v, int inputs, bool blend);
    void DrawPreviewImage(DocView& v, gpu::Texture* t, u64 key, u64 hash, float width, float maxHeight = 0);
    // Custom code: declarations (inputs/params) + code + compile + save to library.
    // Returns true when the instance changed. `errorNode`: node whose compile errors to show (0 = find).
    bool CustomCodeUI(DocView& v, EffectInstance& fx, Id errorNode, const char* key);
    void ConvertToCustomCode(DocView& v, EffectInstance& fx);
    void DrawLayerPreview();
    struct {
        std::string search, category, selected;
        bool favoritesOnly = false, favLoaded = false;
        std::set<std::string> favorites;
        std::string family;        // "" = all
        int origin = 0;            // 0 all, 1 Shaderixel, 2 CyTexture
        bool showVariants = false, recentOnly = false, recentLoaded = false;
        bool graphOnly = false;    // only the effects made with a node graph
        std::vector<std::string> recent;  // last applied, most recent first
        int source = 0;            // 0 active layer, 1 whole image, 2 reference image
        int refKind = 0;           // reference: 0 logo on a background, 1 color chart, 2 image file
        std::string refPath, refKey;
        gpu::TexturePtr refTex;
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
    bool Automated() const { return demoMode != 0 || testEffects || !capturePath.empty() || !dumpPath.empty() || !cliSavePath.empty() || !exportUnrealDir.empty(); }  // hidden, pinned cursor while a slider/drag is dragged
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
    float2 meshLazy, meshLineEnd;
    bool meshLineValid = false;
    u32 meshRng = 12345;
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

// Lighting presets of the 3D view (Presentation.cpp).
int LightingPresetCount();
void ApplyLightingPreset(MeshScene& s, int index);

}  // namespace sx
