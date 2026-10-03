// Canvas viewport, toolbar and painting tools.
#include "ui/App.h"
#include "doc/Brushes.h"
#include "ui/Theme.h"

#include <imgui_internal.h>

namespace sx {

namespace {
struct ToolInfo { Tool tool; int icon; const char* name; const char* key; };
const ToolInfo kToolInfo[] = {
    {Tool::Move, ui::IconMove, "Déplacement", "V"},
    {Tool::RectSelect, ui::IconRectSel, "Rectangle de sélection", "M"},
    {Tool::EllipseSelect, ui::IconEllipseSel, "Ellipse de sélection", "M"},
    {Tool::Lasso, ui::IconLasso, "Lasso", "L"},
    {Tool::PolyLasso, ui::IconPolyLasso, "Lasso polygonal", "L"},
    {Tool::Wand, ui::IconWand, "Baguette magique", "W"},
    {Tool::QuickSelect, ui::IconQuickSelect, "Sélection rapide", "W"},
    {Tool::Pen, ui::IconPen, "Plume", "P"},
    {Tool::Brush, ui::IconBrush, "Pinceau", "B"},
    {Tool::Eraser, ui::IconEraser, "Gomme", "E"},
    {Tool::Clone, ui::IconStamp, "Tampon de duplication", "S"},
    {Tool::Heal, ui::IconHeal, "Correcteur", "J"},
    {Tool::Gradient, ui::IconGradient, "Dégradé", "G"},
    {Tool::Bucket, ui::IconBucket, "Pot de peinture", "G / K"},
    {Tool::Blur, ui::IconDrop, "Goutte (flou)", "R"},
    {Tool::Sharpen, ui::IconSharpen, "Netteté", "R"},
    {Tool::Smudge, ui::IconFinger, "Doigt", "R"},
    {Tool::Dodge, ui::IconDodge, "Densité -", "O"},
    {Tool::Burn, ui::IconBurn, "Densité +", "O"},
    {Tool::Sponge, ui::IconSponge, "Éponge", "O"},
    {Tool::Liquify, ui::IconLiquify, "Fluidité", "R"},
    {Tool::Picker, ui::IconPicker, "Pipette", "I"},
    {Tool::Hand, ui::IconHand, "Main", "H / Espace"},
    {Tool::Zoom, ui::IconZoom, "Zoom", "Z"},
    {Tool::Crop, ui::IconCrop, "Recadrage", "C"},
    {Tool::Text, ui::IconText, "Texte", "T"},
    {Tool::Shape, ui::IconShape, "Forme", "U"},
};

// Toolbar slots; tools sharing a slot appear in a flyout (right-click).
const std::vector<std::vector<Tool>> kGroups = {
    {Tool::Move},
    {Tool::RectSelect, Tool::EllipseSelect},
    {Tool::Lasso, Tool::PolyLasso},
    {Tool::QuickSelect, Tool::Wand},
    {Tool::Crop},
    {Tool::Brush},
    {Tool::Eraser},
    {Tool::Clone},
    {Tool::Heal},
    {Tool::Text},
    {Tool::Pen},
    {Tool::Shape},
    {Tool::Gradient, Tool::Bucket},
    {Tool::Blur, Tool::Sharpen, Tool::Smudge, Tool::Liquify},
    {Tool::Dodge, Tool::Burn, Tool::Sponge},
    {Tool::Picker},
    {Tool::Hand},
    {Tool::Zoom},
};

const ToolInfo& Info(Tool t) {
    for (auto& i : kToolInfo) if (i.tool == t) return i;
    return kToolInfo[0];
}
}  // namespace

void App::DrawToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6 * dpi, 8 * dpi));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4 * dpi, 3 * dpi));
    if (ImGui::Begin("Outils", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar)) {
        float size = std::min(ImGui::GetContentRegionAvail().x, 38.0f * dpi);
        for (size_t g = 0; g < kGroups.size(); ++g) {
            const auto& group = kGroups[g];
            if (g == 1 || g == 5 || g == 14) {
                ImVec2 c = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddLine(ImVec2(c.x + 4, c.y + 1), ImVec2(c.x + size - 4, c.y + 1), ui::col::Border);
                ImGui::Dummy(ImVec2(size, 3 * dpi));
            }
            Tool shown = group[0];
            bool inGroup = std::find(group.begin(), group.end(), tool) != group.end();
            if (inGroup) toolGroupChoice[g] = tool;
            if (toolGroupChoice[g] != Tool::Move || g == 0) {
                if (std::find(group.begin(), group.end(), toolGroupChoice[g]) != group.end()) shown = toolGroupChoice[g];
            }
            const ToolInfo& info = Info(shown);
            char tip[128];
            snprintf(tip, sizeof(tip), "%s (%s)%s", info.name, info.key, group.size() > 1 ? "\nClic droit : autres outils" : "");
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::PushID((int)g);
            if (ui::IconButton("t", info.icon, inGroup, size, tip)) {
                if (xf.active && xf.view) TransformCommit(*xf.view);
                tool = shown;
            }
            if (group.size() > 1) {
                ImGui::GetWindowDrawList()->AddTriangleFilled(ImVec2(pos.x + size - 2, pos.y + size - 7), ImVec2(pos.x + size - 2, pos.y + size - 2),
                                                              ImVec2(pos.x + size - 7, pos.y + size - 2), ui::col::TextDim);
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("##flyout");
                if (ImGui::BeginPopup("##flyout")) {
                    for (Tool t : group) {
                        const ToolInfo& ti = Info(t);
                        ImVec2 ip = ImGui::GetCursorScreenPos();
                        float h = ImGui::GetFrameHeight();
                        char label[96];
                        snprintf(label, sizeof(label), "      %s", ti.name);
                        if (ImGui::Selectable(label, tool == t, 0, ImVec2(220 * dpi, h))) {
                            if (xf.active && xf.view) TransformCommit(*xf.view);
                            tool = t;
                        }
                        ui::DrawIcon(ImGui::GetWindowDrawList(), ti.icon, ImVec2(ip.x + h * 0.5f, ip.y + h * 0.5f), h * 0.6f, ui::col::Text);
                        ImGui::SameLine(200 * dpi);
                        ImGui::TextDisabled("%s", ti.key);
                    }
                    ImGui::EndPopup();
                }
            }
            ImGui::PopID();
        }
        ImGui::Spacing();
        // Foreground / background swatches (Photoshop style).
        ImVec2 p = ImGui::GetCursorScreenPos();
        float sw = size * 0.62f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 bgMin(p.x + size - sw, p.y + size - sw);
        dl->AddRectFilled(bgMin, ImVec2(bgMin.x + sw, bgMin.y + sw), ImGui::ColorConvertFloat4ToU32(ImVec4(bg.x, bg.y, bg.z, 1)), 3);
        dl->AddRect(bgMin, ImVec2(bgMin.x + sw, bgMin.y + sw), ui::col::Border, 3);
        dl->AddRectFilled(p, ImVec2(p.x + sw, p.y + sw), ImGui::ColorConvertFloat4ToU32(ImVec4(fg.x, fg.y, fg.z, 1)), 3);
        dl->AddRect(p, ImVec2(p.x + sw, p.y + sw), IM_COL32(200, 200, 210, 255), 3);
        ImGui::SetCursorScreenPos(p);
        if (ImGui::InvisibleButton("##fg", ImVec2(sw, sw))) { editBg = false; ImGui::SetWindowFocus("Couleur"); }
        ui::HelpTooltip("Couleur de premier plan");
        ImGui::SetCursorScreenPos(bgMin);
        if (ImGui::InvisibleButton("##bg", ImVec2(sw, sw))) { editBg = true; ImGui::SetWindowFocus("Couleur"); }
        ui::HelpTooltip("Couleur d'arrière-plan");
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + size + 4));
        if (ui::IconButton("swap", ui::IconSwap, false, size * 0.5f, "Permuter (X)")) std::swap(fg, bg);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}


float2 App::ScreenToDoc(const DocView& v, ImVec2 canvasMin, ImVec2 p) const {
    return {(p.x - canvasMin.x - v.pan.x) / v.zoom, (p.y - canvasMin.y - v.pan.y) / v.zoom};
}

void App::FitView(DocView& v, ImVec2 avail) {
    float z = std::min(avail.x / v.doc->Width(), avail.y / v.doc->Height()) * 0.92f;
    v.zoom = std::clamp(z, 0.01f, 64.0f);
    v.pan = {(avail.x - v.doc->Width() * v.zoom) * 0.5f, (avail.y - v.doc->Height() * v.zoom) * 0.5f};
}

void App::DrawCanvas() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool visible = ImGui::Begin("Canevas", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoTitleBar);
    ImGui::PopStyleVar();
    DocView* v = Active();
    if (!visible && v) {
        // Hidden canvas (other tab active): keep the document evaluated for previews and the 3D view.
        v->renderer.Prepare(*v->doc, *ctx);
        SyncTextLayers(*v);
        v->lastImage = v->renderer.Evaluate(*v->doc, *ctx);
    }
    if (!visible || !v) {
        if (visible) DrawWelcome();
        ImGui::End();
        return;
    }
    Document& doc = *v->doc;
    const ImVec2 rulerOrigin = ImGui::GetCursorScreenPos();
    ImVec2 canvasMin = rulerOrigin;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (float R = RulerSize(); R > 0) {
        canvasMin.x += R;
        canvasMin.y += R;
        avail.x = std::max(1.0f, avail.x - R);
        avail.y = std::max(1.0f, avail.y - R);
        ImGui::SetCursorScreenPos(canvasMin);
    }
    u32 vw = (u32)std::max(1.0f, avail.x), vh = (u32)std::max(1.0f, avail.y);
    if (v->fitPending || (v->fitHold > 0 && (std::abs(avail.x - v->fitW) > 1 || std::abs(avail.y - v->fitH) > 1))) {
        if (v->fitPending) v->fitHold = 30;
        FitView(*v, avail);
        v->fitPending = false;
        v->fitW = avail.x;
        v->fitH = avail.y;
    }
    if (v->fitHold > 0) --v->fitHold;

    // Interaction first so this frame's dabs are visible this frame.
    ImGui::InvisibleButton("##canvas", avail, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    bool hovered = ImGui::IsItemHovered();
    v->renderer.Prepare(doc, *ctx);
    SyncTextLayers(*v);
    if (xf.active && xf.view != v) TransformCancel(*xf.view);
    UpdateWand(*v);
    UpdateQuickSelect(*v);
    if (!GuideInput(*v, canvasMin, avail, hovered)) CanvasInput(*v, canvasMin, avail, hovered);
    TransformApply(*v);

    {
        // Tiled mode renders the visible area first.
        float iz = 1.0f / v->zoom;
        float x0 = -v->pan.x * iz, y0 = -v->pan.y * iz;
        v->renderer.SetVisibleRect({(int)std::floor(x0), (int)std::floor(y0), (int)std::ceil(x0 + vw * iz), (int)std::ceil(y0 + vh * iz)});
    }
    gpu::Texture* image = v->renderer.Evaluate(doc, *ctx);
    v->lastImage = image;
    if (!v->viewTex || v->viewTex->Width() != vw || v->viewTex->Height() != vh)
        v->viewTex = gpu::Dev().CreateTexture(vw, vh, gpu::Format::RGBA8, "Viewport");
    ViewParams vp;
    vp.origin = {-v->pan.x / v->zoom, -v->pan.y / v->zoom};
    vp.invZoom = 1.0f / v->zoom;
    vp.viewW = vw;
    vp.viewH = vh;
    vp.nearest = v->zoom >= 2.0f;
    Surface* sel = doc.Selection();
    vp.selection = (sel && (!(xf.active && !xf.moveOnly) || xf.selectionOnly)) ? sel->tex.get() : nullptr;
    if (selMoving && sel) vp.selection = v->renderer.OffsetSelectionPreview(doc, *ctx, selMoveOffset);
    vp.quickMask = quickMask;
    vp.time = (float)ImGui::GetTime();
    v->renderer.Display(*ctx, v->renderer.DisplaySource(*ctx, image, vp.invZoom), doc.Width(), doc.Height(), vp, v->viewTex.get());

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(v->viewTex.get()), canvasMin, ImVec2(canvasMin.x + vw, canvasMin.y + vh));
    DrawGridAndGuides(*v, canvasMin, avail, dl);

    // Document border
    ImVec2 d0(canvasMin.x + v->pan.x, canvasMin.y + v->pan.y);
    ImVec2 d1(d0.x + doc.Width() * v->zoom, d0.y + doc.Height() * v->zoom);
    dl->AddRect(ImVec2(d0.x - 1, d0.y - 1), ImVec2(d1.x + 1, d1.y + 1), IM_COL32(0, 0, 0, 120));

    // Overlays
    ImVec2 mouse = ImGui::GetMousePos();
    cursorDoc = ScreenToDoc(*v, canvasMin, mouse);
    cursorInDoc = hovered && cursorDoc.x >= 0 && cursorDoc.y >= 0 && cursorDoc.x < doc.Width() && cursorDoc.y < doc.Height();
    bool panMode = tool == Tool::Hand || spaceHeld || panning;
    if (hovered && !panMode && tool == Tool::QuickSelect) {
        float r = std::max(qselSize * 0.5f * v->zoom, 1.0f);
        dl->AddCircle(mouse, r + 1, IM_COL32(0, 0, 0, 160), 0, 1.5f);
        dl->AddCircle(mouse, r, IM_COL32(255, 255, 255, 220), 0, 1.0f);
        const char* sign = ImGui::GetIO().KeyAlt ? "-" : (doc.Selection() ? "+" : "");
        if (*sign) dl->AddText(ImVec2(mouse.x + 4, mouse.y - 16), IM_COL32_WHITE, sign);
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    } else if (hovered && !panMode && IsPaintTool(tool) && !(ImGui::GetIO().KeyAlt && tool == Tool::Brush)) {
        const BrushSettings& b = CurrentBrush();
        float r = std::max(b.size * 0.5f * v->zoom, 1.0f);
        dl->AddCircle(mouse, r + 1, IM_COL32(0, 0, 0, 160), 0, 1.5f);
        dl->AddCircle(mouse, r, IM_COL32(255, 255, 255, 220), 0, 1.0f);
        if (b.hardness < 0.95f) dl->AddCircle(mouse, r * std::max(b.hardness, 0.05f), IM_COL32(255, 255, 255, 70), 0, 1.0f);
        if (r < 4) {
            dl->AddLine(ImVec2(mouse.x - 6, mouse.y), ImVec2(mouse.x + 6, mouse.y), IM_COL32_WHITE);
            dl->AddLine(ImVec2(mouse.x, mouse.y - 6), ImVec2(mouse.x, mouse.y + 6), IM_COL32_WHITE);
        }
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    } else if (hovered && panMode) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    DrawSelectionOverlay(*v, canvasMin, dl);
    DrawQuickSelectOverlay(*v, canvasMin, dl);
    DrawPenOverlay(*v, canvasMin, dl);
    CropOverlay(*v, canvasMin, avail, dl);
    TextOverlay(*v, canvasMin, dl);
    TransformDrawOverlay(*v, canvasMin, dl);
    if ((tool == Tool::Clone || tool == Tool::Heal) && cloneSourceSet) {
        float2 src = cloneSource;
        if (stroking && cloneOffsetValid) src = cursorDoc + float2{(float)cloneOffset.x, (float)cloneOffset.y};
        ImVec2 s(canvasMin.x + v->pan.x + src.x * v->zoom, canvasMin.y + v->pan.y + src.y * v->zoom);
        dl->AddCircle(s, std::max(CurrentBrush().size * 0.5f * v->zoom, 3.0f), IM_COL32(255, 255, 255, 140));
        dl->AddLine(ImVec2(s.x - 7, s.y), ImVec2(s.x + 7, s.y), IM_COL32_WHITE);
        dl->AddLine(ImVec2(s.x, s.y - 7), ImVec2(s.x, s.y + 7), IM_COL32_WHITE);
    }
    if (gradientDrag) {
        ImVec2 a(canvasMin.x + v->pan.x + gradientStart.x * v->zoom, canvasMin.y + v->pan.y + gradientStart.y * v->zoom);
        ImVec2 b(canvasMin.x + v->pan.x + gradientEnd.x * v->zoom, canvasMin.y + v->pan.y + gradientEnd.y * v->zoom);
        dl->AddLine(a, b, IM_COL32(0, 0, 0, 160), 3);
        dl->AddLine(a, b, IM_COL32_WHITE, 1.5f);
        dl->AddCircleFilled(a, 4, IM_COL32_WHITE);
        dl->AddCircleFilled(b, 4, ui::col::Accent);
    }
    // Paint-target hint
    if (hovered && (IsPaintTool(tool) || tool == Tool::Gradient || tool == Tool::Bucket || tool == Tool::Move) && !doc.PaintTarget()) {
        const char* msg = doc.Mode() == DocMode::Layers ? "Sélectionnez un calque de pixels (non verrouillé)" : "Sélectionnez un nœud « Calque pixel »";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        ImVec2 p(mouse.x + 16, mouse.y + 16);
        dl->AddRectFilled(ImVec2(p.x - 6, p.y - 4), ImVec2(p.x + ts.x + 6, p.y + ts.y + 4), IM_COL32(20, 20, 24, 230), 4);
        dl->AddText(p, IM_COL32(255, 200, 120, 255), msg);
    }
    if (doc.Mode() == DocMode::Layers && doc.S().editMask) {
        dl->AddText(ImVec2(canvasMin.x + 10, canvasMin.y + 8), IM_COL32(255, 255, 255, 200), "Édition du masque de fusion");
    }
    // Effect dropped on the image: filter (or blend) of the active layer.
    if (const ImGuiPayload* pl = ImGui::GetDragDropPayload(); pl && pl->IsDataType(kEffectPayload) && doc.Mode() == DocMode::Layers && doc.ActiveLayer()) {
        ImRect r(canvasMin, ImVec2(canvasMin.x + avail.x, canvasMin.y + avail.y));
        if (ImGui::BeginDragDropTargetCustom(r, ImGui::GetID("##canvasdrop"))) {
            dl->AddRect(r.Min, r.Max, ui::col::Accent2, 0, 0, 3.0f);
            if (const ImGuiPayload* got = ImGui::AcceptDragDropPayload(kEffectPayload)) DropEffectOnLayer(*v, doc.S().activeLayer, (const char*)got->Data);
            ImGui::EndDragDropTarget();
        }
    }
    DrawRulers(*v, rulerOrigin, canvasMin, avail);
    ImGui::End();
}

void App::CanvasInput(DocView& v, ImVec2 canvasMin, ImVec2 canvasSize, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    Document& doc = *v.doc;
    ImVec2 mouse = io.MousePos;
    std::vector<PointerSample> samples = platform::TakeSamples();

    // Zoom with the wheel around the cursor.
    if (hovered && io.MouseWheel != 0.0f) {
        float factor = std::pow(1.2f, io.MouseWheel);
        float nz = std::clamp(v.zoom * factor, 0.01f, 128.0f);
        float2 local = {mouse.x - canvasMin.x, mouse.y - canvasMin.y};
        v.pan = local - (local - v.pan) * (nz / v.zoom);
        v.zoom = nz;
    }

    // Panning: middle mouse, space, hand tool.
    bool wantPan = (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) ||
                   (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && (tool == Tool::Hand || spaceHeld));
    if (wantPan && !stroking) panning = true;
    if (panning) {
        v.pan = v.pan + float2{io.MouseDelta.x, io.MouseDelta.y};
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle) && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) panning = false;
        return;
    }

    float2 dp = ScreenToDoc(v, canvasMin, mouse);
    bool click = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool alt = io.KeyAlt;

    if (xf.active) {
        TransformInput(v, canvasMin, hovered);
        return;
    }
    bool vectorLayer = doc.ActiveLayer() && (doc.ActiveLayer()->kind == LayerKind::Text || doc.ActiveLayer()->kind == LayerKind::Shape) && doc.Mode() == DocMode::Layers;
    if (tool == Tool::Text || tool == Tool::Shape || (tool == Tool::Move && vectorLayer)) {
        TextInput(v, canvasMin, hovered);
        return;
    }
    if (tool == Tool::Move) {
        if (click && TransformBegin(v, true)) TransformInput(v, canvasMin, hovered);
        return;
    }
    if (tool == Tool::Crop) {
        CropInput(v, canvasMin, hovered);
        return;
    }
    if (tool == Tool::RectSelect || tool == Tool::EllipseSelect || tool == Tool::Lasso || tool == Tool::PolyLasso || tool == Tool::Wand) {
        SelectionToolInput(v, canvasMin, hovered, samples);
        return;
    }
    if (tool == Tool::QuickSelect) {
        QuickSelectInput(v, canvasMin, hovered, samples);
        return;
    }
    if (tool == Tool::Pen) {
        PenInput(v, canvasMin, hovered);
        return;
    }

    // Clone / heal source (Alt+click)
    if ((tool == Tool::Clone || tool == Tool::Heal) && alt && click) {
        cloneSource = dp;
        cloneSourceSet = true;
        cloneOffsetValid = false;
        Status("Source du tampon définie");
        return;
    }
    // Eyedropper (also Alt with paint tools)
    bool picker = tool == Tool::Picker || (alt && (tool == Tool::Brush || tool == Tool::Gradient || tool == Tool::Bucket));
    if (picker && !stroking) {
        if (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left) && v.lastImage && !pickReadback &&
            dp.x >= 0 && dp.y >= 0 && dp.x < doc.Width() && dp.y < doc.Height())
            pickReadback = v.renderer.ReadPixel(*ctx, v.lastImage, (int)dp.x, (int)dp.y);
        return;
    }

    if (tool == Tool::Zoom && click) {
        float factor = alt ? 0.5f : 2.0f;
        float nz = std::clamp(v.zoom * factor, 0.01f, 128.0f);
        float2 local = {mouse.x - canvasMin.x, mouse.y - canvasMin.y};
        v.pan = local - (local - v.pan) * (nz / v.zoom);
        v.zoom = nz;
        return;
    }

    Id target = doc.PaintTarget();
    if (tool == Tool::Bucket && click && target && !wand.rb) {
        int x = (int)std::floor(dp.x), y = (int)std::floor(dp.y);
        if (x < 0 || y < 0 || x >= (int)doc.Width() || y >= (int)doc.Height()) return;
        gpu::Texture* src = bucketSampleMerged ? v.renderer.EvaluateComplete(doc, *ctx) : doc.GetSurface(target)->tex.get();
        if (!src) return;
        wand.rb = v.renderer.ExportRGBA8(*ctx, src);
        wand.x = x;
        wand.y = y;
        wand.view = &v;
        wand.fill = true;
        wand.surface = target;
        return;
    }

    if (tool == Tool::Gradient) {
        if (click && target) {
            gradientDrag = true;
            gradientStart = gradientEnd = dp;
        }
        if (gradientDrag) {
            gradientEnd = dp;
            if (io.KeyShift) {  // constrain to 45 degrees
                float2 d = gradientEnd - gradientStart;
                float a = std::round(std::atan2(d.y, d.x) / (3.14159265f / 4)) * (3.14159265f / 4);
                float len = Length(d);
                gradientEnd = gradientStart + float2{std::cos(a), std::sin(a)} * len;
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                gradientDrag = false;
                if (target && Length(gradientEnd - gradientStart) > 1.0f)
                    v.renderer.ApplyGradient(doc, *ctx, v.history, target, gradientStart, gradientEnd, gradientShape,
                                             LinearColor(fg), LinearColor(bg), gradientOpacity, "Dégradé");
            }
        }
        return;
    }

    if (!IsPaintTool(tool)) return;
    if ((tool == Tool::Clone || tool == Tool::Heal) && click && !cloneSourceSet) {
        Status("Alt+clic pour définir la source du tampon");
        return;
    }

    if (click && target && !stroking) {
        if (v.renderer.BeginStroke(doc, *ctx, target)) {
            stroking = true;
            strokeErase = tool == Tool::Eraser;
            if (tool == Tool::Clone || tool == Tool::Heal) {
                if (!cloneOffsetValid || !cloneAligned) {
                    cloneOffset = {(int)std::lround(cloneSource.x - dp.x), (int)std::lround(cloneSource.y - dp.y)};
                    cloneOffsetValid = true;
                }
            }
            if (!samples.empty() && samples.back().pen && samples.back().eraser) strokeErase = true;
            float pressure = platform::PenActive() ? platform::LastPressure() : 1.0f;
            float2 start = dp;
            if (io.KeyShift && haveStrokeEnd) start = lastStrokeEnd;  // Shift+click: straight line
            smoothPos = lastDabPos = start;
            lastPressure = pressure;
            dabRemainder = 0;
            // First dab
            std::vector<PointerSample> first{{mouse.x, mouse.y, pressure, platform::PenActive(), false}};
            const BrushSettings& b = strokeErase ? eraser : CurrentBrush();
            std::vector<Dab> dabs;
            float rad = b.size * 0.5f * (b.pressureSize ? std::max(pressure, 0.05f) : 1.0f);
            float alpha = b.flow * (b.pressureOpacity ? pressure : 1.0f);
            dabs.push_back({start.x, start.y, rad, alpha});
            strokeStart = ImGui::GetTime();
            strokeDist = 0;
            dabCounter = 0;
            airAccum = 0;
            std::vector<float4> extra{{pressure, lastDirAngle, 0.0f, 0.0f}};
            EmitDabs(v, dabs, &extra);
            if (io.KeyShift && haveStrokeEnd) {
                samples.insert(samples.begin(), first.begin(), first.end());
                smoothPos = start;
            }
        }
    }
    if (stroking) {
        bool release = !ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (samples.empty()) samples.push_back({mouse.x, mouse.y, platform::PenActive() ? platform::LastPressure() : 1.0f, platform::PenActive(), false});
        PaintSamples(v, canvasMin, samples, release);
        // Airbrush brushes keep painting at the cursor while the button is held.
        if (!release && (tool == Tool::Brush || tool == Tool::Eraser))
            if (auto td = TipDef(strokeErase ? eraser : CurrentBrush()); td && td->airbrush > 0) {
                const BrushSettings& b = strokeErase ? eraser : CurrentBrush();
                airAccum += io.DeltaTime * td->airbrush;
                std::vector<Dab> dabs;
                std::vector<float4> extra;
                for (; airAccum >= 1.0f && dabs.size() < 256; airAccum -= 1.0f) {
                    float rad = b.size * 0.5f * (b.pressureSize ? std::max(lastPressure, 0.05f) : 1.0f);
                    dabs.push_back({smoothPos.x, smoothPos.y, rad, b.flow * (b.pressureOpacity ? lastPressure : 1.0f)});
                    extra.push_back({lastPressure, lastDirAngle, (float)(ImGui::GetTime() - strokeStart), strokeDist});
                }
                if (!dabs.empty()) EmitDabs(v, dabs, &extra);
            }
        if (release) {
            static const char* names[] = {"", "", "", "", "", "", "Pinceau", "Gomme", "Tampon de duplication", "Correcteur", "", "",
                                          "Goutte", "Netteté", "Doigt", "Densité -", "Densité +", "Éponge", "Fluidité"};
            int ti = (int)tool;
            v.renderer.EndStroke(doc, *ctx, v.history, strokeErase ? "Gomme" : (ti < 19 && names[ti][0] ? names[ti] : "Pinceau"));
            stroking = false;
            lastStrokeEnd = lastDabPos;
            haveStrokeEnd = true;
        }
    }
}

void App::PaintSamples(DocView& v, ImVec2 canvasMin, const std::vector<PointerSample>& samples, bool release) {
    const BrushSettings& b = strokeErase ? eraser : CurrentBrush();
    std::vector<Dab> dabs;
    std::vector<float4> extra;  // pressure, direction angle, time, distance (programmable brushes)
    const float now = (float)(ImGui::GetTime() - strokeStart);
    auto tipDef = TipDef(b);
    const float tipSpacing = tipDef && tipDef->spacing > 0 ? tipDef->spacing : b.spacing;
    float smooth = std::clamp(b.smoothing, 0.0f, 0.98f);
    auto emitTo = [&](float2 target, float pressure) {
        float2 d = target - lastDabPos;
        float len = Length(d);
        float spacing = std::max(1.0f, b.size * std::max(tipSpacing, 0.01f) * (b.pressureSize ? std::max(pressure, 0.1f) : 1.0f));
        float t = spacing - dabRemainder;
        if (len > 1e-3f) lastDirAngle = std::atan2(d.y, d.x);
        while (t <= len) {
            float k = t / std::max(len, 1e-5f);
            float2 p = lastDabPos + d * k;
            float pr = lastPressure + (pressure - lastPressure) * k;
            float rad = b.size * 0.5f * (b.pressureSize ? std::max(pr, 0.05f) : 1.0f);
            float alpha = b.flow * (b.pressureOpacity ? pr : 1.0f);
            dabs.push_back({p.x, p.y, rad, alpha});
            extra.push_back({pr, lastDirAngle, now, strokeDist + t});
            t += spacing;
        }
        strokeDist += len;
        dabRemainder = len - (t - spacing);
        if (dabRemainder < 0) dabRemainder = 0;
        // Advance anchor to target while keeping remainder for next segment.
        if (len > 0) {
            lastDabPos = target;
            lastPressure = pressure;
        }
    };
    for (const auto& s : samples) {
        float2 p = ScreenToDoc(v, canvasMin, ImVec2(s.x, s.y));
        float pressure = s.pen ? s.pressure : 1.0f;
        // Exponential smoothing (lazy brush).
        smoothPos = smoothPos + (p - smoothPos) * (1.0f - smooth);
        emitTo(smoothPos, pressure);
    }
    if (release && smooth > 0) {
        float2 p = ScreenToDoc(v, canvasMin, ImGui::GetIO().MousePos);
        emitTo(p, lastPressure);
    }
    if (tool == Tool::Liquify && liquifyMode >= 1 && liquifyMode <= 4) {
        // Twirl / pucker / bloat act continuously while the button is held: one dab per frame.
        dabs.clear();
        dabs.push_back({smoothPos.x, smoothPos.y, b.size * 0.5f, b.flow});
    }
    if (dabs.empty()) return;
    EmitDabs(v, dabs, &extra);
}

void App::EmitDabs(DocView& v, const std::vector<Dab>& dabs, const std::vector<float4>* extra) {
    Document& doc = *v.doc;
    if (strokeErase || tool == Tool::Brush || tool == Tool::Eraser) {
        float4 color = LinearColor(fg);
        if (doc.S().editMask) {
            float l = 0.2126f * color.x + 0.7152f * color.y + 0.0722f * color.z;
            color = {l, l, l, 1};
        }
        const BrushSettings& bs = strokeErase ? eraser : brush;
        if (auto td = TipDef(bs)) {
            // Programmable tip: shape, color and placement come from the brush code.
            EffectInstance tip;
            tip.params = bs.tipParams;
            static const std::vector<float4> none;
            if (v.renderer.StrokeProgram(doc, *ctx, dabs, extra ? *extra : none, *td, tip, bs, color, strokeErase, dabCounter)) {
                dabCounter += (u32)dabs.size();
                return;
            }
        }
        v.renderer.StrokeDabs(doc, *ctx, dabs, strokeErase ? eraser : brush, color, strokeErase);
        return;
    }
    if (tool == Tool::Smudge) {
        v.renderer.SmudgeDabs(doc, *ctx, dabs, retouch, retouchStrength);
        return;
    }
    if (tool == Tool::Liquify) {
        v.renderer.LiquifyDabs(doc, *ctx, dabs, retouch, liquifyMode, retouchStrength);
        return;
    }
    RetouchParams rp;
    rp.strength = retouchStrength;
    rp.srcOffset = cloneOffset;
    rp.range = dodgeRange;
    switch (tool) {
        case Tool::Clone: rp.mode = RetouchMode::Clone; break;
        case Tool::Heal: rp.mode = RetouchMode::Heal; break;
        case Tool::Blur: rp.mode = RetouchMode::Blur; break;
        case Tool::Sharpen: rp.mode = RetouchMode::Sharpen; break;
        case Tool::Dodge: rp.mode = RetouchMode::Dodge; break;
        case Tool::Burn: rp.mode = RetouchMode::Burn; break;
        default: rp.mode = spongeSaturate ? RetouchMode::Saturate : RetouchMode::Desaturate; break;
    }
    v.renderer.RetouchDabs(doc, *ctx, dabs, retouch, rp);
}

}  // namespace sx
