// Text layers: rasterization sync, text tool, move, properties.
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

namespace sx {

void App::SyncTextLayers(DocView& v) {
    Document& d = *v.doc;
    if (d.Mode() != DocMode::Layers) return;
    bool any = false;
    for (auto& l : d.S().layers) {
        if (l.kind == LayerKind::Shape && l.surface) {
            u64 h = l.shape.Hash() ^ ((u64)d.Width() << 32) ^ d.Height() ^ 0x5A5A;
            auto it = v.textHashes.find(l.surface);
            if (it == v.textHashes.end() || it->second != h) {
                d.CancelPendingClear(l.surface);
                v.renderer.RenderShape(d, *ctx, l.surface, l.shape);
                v.textHashes[l.surface] = h;
                const float4& r = l.shape.rect;
                float pad = l.shape.strokeWidth + 2;
                Rect box{(int)(std::min(r.x, r.z) - pad), (int)(std::min(r.y, r.w) - pad), (int)(std::max(r.x, r.z) + pad), (int)(std::max(r.y, r.w) + pad)};
                for (auto& p : l.shape.parts)
                    if (p.op == 0 || p.op == 3)
                        box.Merge({(int)(std::min(p.rect.x, p.rect.z) - pad), (int)(std::min(p.rect.y, p.rect.w) - pad), (int)(std::max(p.rect.x, p.rect.z) + pad),
                                   (int)(std::max(p.rect.y, p.rect.w) + pad)});
                v.textRects[l.surface] = box;
            }
            continue;
        }
        if (l.kind != LayerKind::Text || !l.surface) continue;
        TextStyleTable table{&d.S().charStyles, &d.S().paraStyles};
        u64 h = l.text.Hash() ^ ((u64)d.Width() << 32) ^ d.Height() ^ (table.Hash() * 31);
        auto it = v.textHashes.find(l.surface);
        if (it != v.textHashes.end() && it->second == h) continue;
        std::vector<float> rgba;
        Rect r;
        if (!RasterizeText(l.text, (int)d.Width(), (int)d.Height(), rgba, r, &table)) continue;
        d.pendingClears.push_back({l.surface, {0, 0, 0, 0}});
        v.renderer.Prepare(d, *ctx);
        if (!r.Empty() && !rgba.empty())
            v.renderer.Import(d, *ctx, l.surface, rgba.data(), (u32)r.Width(), (u32)r.Height(), true, {r.x0, r.y0});
        v.textHashes[l.surface] = h;
        v.textRects[l.surface] = r;
        any = true;
    }
    (void)any;
}

void App::TextInput(DocView& v, ImVec2 canvasMin, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    Document& d = *v.doc;
    if (d.Mode() != DocMode::Layers) {
        if (hovered && ImGui::IsMouseClicked(0)) Status("Les calques de texte sont disponibles en mode calques");
        return;
    }
    float2 dp = Snap(v, ScreenToDoc(v, canvasMin, io.MousePos));
    Layer* al = d.ActiveLayer();
    // Shape tool: drag a new shape.
    if (tool == Tool::Shape) {
        const bool combine = al && al->kind == LayerKind::Shape && (io.KeyShift || io.KeyAlt) && al->shape.parts.size() < 8;
        if (hovered && ImGui::IsMouseClicked(0) && combine) {
            // Photoshop: Shift unite, Alt subtract, Shift+Alt intersect.
            ShapePart p;
            p.op = io.KeyShift && io.KeyAlt ? 2 : io.KeyAlt ? 1 : 0;
            p.type = shapeDefaults.type == 2 || shapeDefaults.type == 5 ? 0 : shapeDefaults.type;  // components: built-in kinds
            p.sides = shapeDefaults.sides;
            p.radius = shapeDefaults.radius;
            p.rect = {dp.x, dp.y, dp.x + 1, dp.y + 1};
            editBefore = frameBefore;
            al->shape.parts.push_back(p);
            shapeDragging = true;
            shapePartDrag = true;
            shapeStart = dp;
        }
        if (shapeDragging && shapePartDrag) {
            Layer* l = d.ActiveLayer();
            if (l && l->kind == LayerKind::Shape && !l->shape.parts.empty())
                l->shape.parts.back().rect = {std::min(shapeStart.x, dp.x), std::min(shapeStart.y, dp.y), std::max(shapeStart.x, dp.x), std::max(shapeStart.y, dp.y)};
            if (!ImGui::IsMouseDown(0)) {
                shapeDragging = shapePartDrag = false;
                if (editBefore) v.history.PushState("Composant de forme", *editBefore, d.S());
                editBefore.reset();
            }
            return;
        }
        if (hovered && ImGui::IsMouseClicked(0)) {
            shapeDragging = true;
            shapeStart = dp;
            editBefore = frameBefore;
            ShapeStyle s = shapeDefaults;
            s.fill = fg;
            s.stroke = bg;
            s.rect = {dp.x, dp.y, dp.x + 1, dp.y + 1};
            d.AddShapeLayer(s);
        }
        if (shapeDragging) {
            Layer* l = d.ActiveLayer();
            if (l && l->kind == LayerKind::Shape) {
                float2 e = dp;
                if (io.KeyShift) {
                    float2 dd = e - shapeStart;
                    if (l->shape.type == 2) {
                        float a = std::round(std::atan2(dd.y, dd.x) / 0.785398f) * 0.785398f, len = Length(dd);
                        e = shapeStart + float2{std::cos(a) * len, std::sin(a) * len};
                    } else {
                        float m = std::max(std::abs(dd.x), std::abs(dd.y));
                        e = shapeStart + float2{dd.x < 0 ? -m : m, dd.y < 0 ? -m : m};
                    }
                }
                if (l->shape.type == 2) l->shape.rect = {shapeStart.x, shapeStart.y, e.x, e.y};
                else l->shape.rect = {std::min(shapeStart.x, e.x), std::min(shapeStart.y, e.y), std::max(shapeStart.x, e.x), std::max(shapeStart.y, e.y)};
            }
            if (!ImGui::IsMouseDown(0)) {
                shapeDragging = false;
                if (editBefore) v.history.PushState("Forme", *editBefore, d.S());
                editBefore.reset();
            }
        }
        return;
    }
    if (tool == Tool::Move && al && al->kind == LayerKind::Shape) {
        if (hovered && ImGui::IsMouseClicked(0)) {
            shapeDragging = true;
            shapeStart = dp;
            shapeRectStart = al->shape.rect;
            editBefore = frameBefore;
        }
        if (shapeDragging) {
            float2 dd = dp - shapeStart;
            if (io.KeyShift) { if (std::abs(dd.x) > std::abs(dd.y)) dd.y = 0; else dd.x = 0; }
            float2 step = {shapeRectStart.x + dd.x - al->shape.rect.x, shapeRectStart.y + dd.y - al->shape.rect.y};
            al->shape.rect = {shapeRectStart.x + dd.x, shapeRectStart.y + dd.y, shapeRectStart.z + dd.x, shapeRectStart.w + dd.y};
            for (auto& p : al->shape.parts) p.rect = {p.rect.x + step.x, p.rect.y + step.y, p.rect.z + step.x, p.rect.w + step.y};
            if (!ImGui::IsMouseDown(0)) {
                shapeDragging = false;
                if (editBefore) v.history.PushState("Déplacer la forme", *editBefore, d.S());
                editBefore.reset();
            }
        }
        return;
    }
    bool onActiveText = false;
    if (al && al->kind == LayerKind::Text) {
        auto it = v.textRects.find(al->surface);
        if (it != v.textRects.end()) {
            const Rect& r = it->second;
            onActiveText = dp.x >= r.x0 - 4 && dp.x <= r.x1 + 4 && dp.y >= r.y0 - 4 && dp.y <= r.y1 + 4;
        }
    }
    if (hovered && ImGui::IsMouseClicked(0)) {
        if (tool == Tool::Move || onActiveText) {
            if (al && al->kind == LayerKind::Text) {
                textDragging = true;
                textDragStart = dp;
                textPosStart = al->text.pos;
                editBefore = frameBefore;
            }
            if (tool == Tool::Text) focusTextEdit = true;
        } else {
            Edit e(d, v.history, "Nouveau texte");
            TextStyle s = textDefaults;
            s.color = fg;
            s.pos = {std::round(dp.x), std::round(dp.y)};
            // Click on the active path: the text follows it.
            if (VectorPath* vp = ActivePath(d)) {
                auto polys = FlattenPath(*vp, 0.5f);
                if (!polys.empty() && polys[0].size() >= 2) {
                    const auto& pl = polys[0];
                    float best = 1e30f, at = 0, acc = 0;
                    for (size_t i = 1; i < pl.size(); ++i) {
                        float2 a = pl[i - 1], b = pl[i], ab = b - a;
                        float len = Length(ab);
                        float tt = len > 0 ? std::clamp(((dp.x - a.x) * ab.x + (dp.y - a.y) * ab.y) / (len * len), 0.0f, 1.0f) : 0.0f;
                        float dd = Length(dp - (a + ab * tt));
                        if (dd < best) { best = dd; at = acc + len * tt; }
                        acc += len;
                    }
                    if (best * v.zoom < 10.0f) {
                        s.path = pl;
                        s.pathOffset = at;
                        Status("Texte sur le tracé « " + vp->name + " » (Propriétés : décalage, côté)");
                    }
                }
            }
            if (s.content.empty()) s.content = "Texte";
            d.AddTextLayer(s);
            e.Commit();
            focusTextEdit = true;
            FocusPanel("Propriétés");
        }
    }
    if (textDragging) {
        Layer* l = d.ActiveLayer();
        if (l && l->kind == LayerKind::Text) {
            float2 delta = dp - textDragStart;
            if (io.KeyShift) { if (std::abs(delta.x) > std::abs(delta.y)) delta.y = 0; else delta.x = 0; }
            l->text.pos = {std::round(textPosStart.x + delta.x), std::round(textPosStart.y + delta.y)};
        }
        if (!ImGui::IsMouseDown(0)) {
            textDragging = false;
            if (editBefore && l && !(l->text.pos == textPosStart)) v.history.PushState("Déplacer le texte", *editBefore, d.S());
            editBefore.reset();
        }
    }
    if (hovered && tool == Tool::Text) ImGui::SetMouseCursor(onActiveText ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_TextInput);
}

void App::TextOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl) {
    Document& d = *v.doc;
    Layer* al = d.ActiveLayer();
    if (!al || (al->kind != LayerKind::Text && al->kind != LayerKind::Shape) || (tool != Tool::Text && tool != Tool::Move && tool != Tool::Shape)) return;
    auto it = v.textRects.find(al->surface);
    if (it == v.textRects.end() || it->second.Empty()) return;
    const Rect& r = it->second;
    ImVec2 a(canvasMin.x + v.pan.x + r.x0 * v.zoom, canvasMin.y + v.pan.y + r.y0 * v.zoom);
    ImVec2 b(canvasMin.x + v.pan.x + r.x1 * v.zoom, canvasMin.y + v.pan.y + r.y1 * v.zoom);
    dl->AddRect(a, b, ui::col::Accent2, 0, 0, 1.0f);
    if (al->kind == LayerKind::Text && al->text.path.size() >= 2) {
        std::vector<ImVec2> pts;
        for (auto& p : al->text.path) pts.push_back(ImVec2(canvasMin.x + v.pan.x + p.x * v.zoom, canvasMin.y + v.pan.y + p.y * v.zoom));
        dl->AddPolyline(pts.data(), (int)pts.size(), IM_COL32(120, 180, 255, 200), 0, 1.5f);
    }
    if (al->kind == LayerKind::Text) {
        ImVec2 anchor(canvasMin.x + v.pan.x + al->text.pos.x * v.zoom, canvasMin.y + v.pan.y + al->text.pos.y * v.zoom);
        dl->AddCircleFilled(anchor, 3.5f, ui::col::Accent2);
    }
}

void App::TextPropertiesUI(DocView& v, Layer& l) {
    Document& d = *v.doc;
    TextStyle& t = l.text;
    bool changed = false;
    ImGui::SeparatorText("Texte");
    if (focusTextEdit) {
        ImGui::SetKeyboardFocusHere();
        focusTextEdit = false;
    }
    const std::string oldContent = t.content;
    bool edited = ImGui::InputTextMultiline("##content", &t.content, ImVec2(-1, 90 * dpi));
    if (ImGuiInputTextState* st = ImGui::GetInputTextState(ImGui::GetItemID())) {
        // Selection kept (in codepoints) for the character / paragraph styles.
        int a = std::min(st->GetSelectionStart(), st->GetSelectionEnd()), b = std::max(st->GetSelectionStart(), st->GetSelectionEnd());
        textSelStart = Utf8CodepointIndex(t.content, a);
        textSelEnd = Utf8CodepointIndex(t.content, b);
        textSelLayer = l.id;
    }
    if (edited) {
        // Runs follow the edit (common prefix / suffix).
        auto oc = Utf8Length(oldContent), nc = Utf8Length(t.content);
        size_t pre = 0;
        while (pre < oldContent.size() && pre < t.content.size() && oldContent[pre] == t.content[pre]) ++pre;
        int preCp = Utf8CodepointIndex(oldContent, (int)pre);
        int delta = nc - oc;
        for (auto& r : t.runs) {
            if (r.start >= preCp) r.start = std::max(preCp, r.start + delta);
            if (r.end > preCp) r.end = std::max(preCp, r.end + delta);
        }
        std::erase_if(t.runs, [](const TextRun& r) { return r.end <= r.start; });
        changed = true;
        std::string first = t.content.substr(0, std::min<size_t>(t.content.find('\n'), 32));
        if (!first.empty()) l.name = first;
    }
    TrackUndo("Modifier le texte");
    std::string fname = t.font.empty() ? "segoeui" : Narrow(Utf8Path(t.font).stem().wstring());
    if (ImGui::BeginCombo("Police", fname.c_str(), ImGuiComboFlags_HeightLarge)) {
        ImGui::InputTextWithHint("##ff", "Filtrer...", fontFilter, sizeof(fontFilter));
        for (auto& f : SystemFonts()) {
            if (fontFilter[0] && f.name.find(fontFilter) == std::string::npos) continue;
            if (ImGui::Selectable(f.name.c_str(), f.path == t.font)) {
                t.font = f.path;
                changed = true;
                v.history.PushState("Police", frameBefore, d.S());
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::SliderFloat("Taille", &t.size, 4, 2000, "%.0f px", ImGuiSliderFlags_Logarithmic)) changed = true;
    TrackUndo("Taille du texte");
    float c[4] = {t.color.x, t.color.y, t.color.z, t.color.w};
    if (ImGui::ColorEdit4("Couleur", c, ImGuiColorEditFlags_AlphaBar)) { t.color = {c[0], c[1], c[2], c[3]}; changed = true; }
    TrackUndo("Couleur du texte");
    if (ImGui::Combo("Alignement", &t.align, "Gauche\0Centre\0Droite\0")) {
        changed = true;
        v.history.PushState("Alignement", frameBefore, d.S());
    }
    if (ImGui::SliderFloat("Interligne", &t.lineSpacing, 0.5f, 3.0f, "%.2f")) changed = true;
    TrackUndo("Interligne");
    if (ImGui::SliderFloat("Approche", &t.tracking, -200, 800, "%.0f")) changed = true;
    TrackUndo("Approche");
    if (ImGui::Checkbox("Faux gras", &t.faux)) changed = true;
    TrackUndo("Faux gras");
    if (ImGui::DragFloat2("Position", &t.pos.x, 1.0f, -100000, 100000, "%.0f")) changed = true;
    TrackUndo("Position du texte");
    if (!t.runs.empty() || !t.paraStyles.empty())
        ImGui::TextDisabled("%zu plage(s) de style, %zu paragraphe(s) stylé(s) (panneau Styles de texte)", t.runs.size(),
                            (size_t)std::count_if(t.paraStyles.begin(), t.paraStyles.end(), [](Id p) { return p != 0; }));
    ImGui::SeparatorText("Texte sur tracé");
    if (t.path.size() >= 2) {
        if (ImGui::DragFloat("Décalage", &t.pathOffset, 1.0f, -100000, 100000, "%.0f px")) changed = true;
        TrackUndo("Décalage sur le tracé");
        if (ImGui::Checkbox("De l'autre côté", &t.pathFlip)) changed = true;
        TrackUndo("Côté du tracé");
        if (ImGui::Button("Détacher du tracé")) {
            Edit e(d, v.history, "Détacher du tracé");
            t.path.clear();
            e.Commit();
            changed = true;
        }
    } else {
        VectorPath* vp = ActivePath(d);
        ImGui::BeginDisabled(!vp);
        if (ImGui::Button("Placer sur le tracé actif") && vp) {
            auto polys = FlattenPath(*vp, 0.5f);
            if (!polys.empty() && polys[0].size() >= 2) {
                Edit e(d, v.history, "Texte sur tracé");
                t.path = polys[0];
                t.pathOffset = 0;
                e.Commit();
                changed = true;
            }
        }
        ImGui::EndDisabled();
        if (!vp) ImGui::TextDisabled("Tracez un chemin à la plume (P), puis cliquez dessus\navec l'outil Texte, ou utilisez ce bouton.");
    }
    if (changed) d.dirty = true;
}

void App::ShapePropertiesUI(DocView& v, Layer& l) {
    Document& d = *v.doc;
    ShapeStyle& s = l.shape;
    ImGui::SeparatorText("Forme");
    if (ImGui::Combo("Type", &s.type, "Rectangle\0Ellipse\0Ligne\0Polygone\0Étoile\0Personnalisée\0")) {
        v.history.PushState("Type de forme", frameBefore, d.S());  // custom: the picker below gives an outline right away
    }
    if (s.type == 5) {
        ImGui::TextDisabled("Forme : %s", s.customName.empty() ? "?" : s.customName.c_str());
        if (ShapePicker(s, &v)) v.history.PushState("Forme personnalisée", frameBefore, d.S());
    }
    if (ImGui::DragFloat4("Cadre", &s.rect.x, 1.0f, -100000, 100000, "%.0f")) d.dirty = true;
    TrackUndo("Cadre de la forme");
    if (s.type == 0) { ImGui::SliderFloat("Rayon des coins", &s.radius, 0, 1000, "%.0f px"); TrackUndo("Rayon"); }
    if (s.type == 3 || s.type == 4) { ImGui::SliderInt("Côtés / branches", &s.sides, 3, 32); TrackUndo("Côtés"); }
    if (s.type == 4) { ImGui::SliderFloat("Creux", &s.radius, 0.05f, 0.95f, "%.2f"); TrackUndo("Creux"); }
    if (s.type != 2) { ImGui::SliderFloat("Rotation", &s.rotation, -180, 180, "%.1f°"); TrackUndo("Rotation"); }
    if (s.type != 2) {
        ImGui::Checkbox("Fond", &s.fillOn);
        TrackUndo("Fond");
        ImGui::SameLine();
        ImGui::ColorEdit4("##fill", &s.fill.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        TrackUndo("Couleur de fond");
    }
    ImGui::Checkbox(s.type == 2 ? "Trait" : "Contour", &s.strokeOn);
    TrackUndo("Contour");
    ImGui::SameLine();
    ImGui::ColorEdit4("##stroke", &s.stroke.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
    TrackUndo("Couleur du contour");
    ImGui::SliderFloat("Épaisseur", &s.strokeWidth, 0.5f, 300, "%.1f px", ImGuiSliderFlags_Logarithmic);
    TrackUndo("Épaisseur");
    ImGui::SeparatorText("Opérations booléennes");
    static const char* ops[] = {"Unir", "Soustraire", "Intersection", "Exclure"};
    int remove = -1;
    for (int i = 0; i < (int)s.parts.size(); ++i) {
        ShapePart& p = s.parts[i];
        ImGui::PushID(i);
        ImGui::SetNextItemWidth(110 * dpi);
        if (ImGui::Combo("##op", &p.op, "Unir\0Soustraire\0Intersection\0Exclure\0")) v.history.PushState("Opération booléenne", frameBefore, d.S());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110 * dpi);
        if (ImGui::Combo("##type", &p.type, "Rectangle\0Ellipse\0Ligne\0Polygone\0Étoile\0")) v.history.PushState("Type de composant", frameBefore, d.S());
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) remove = i;
        ImGui::DragFloat4("Cadre", &p.rect.x, 1.0f, -100000, 100000, "%.0f");
        TrackUndo("Cadre du composant");
        if (p.type == 0) { ImGui::SliderFloat("Rayon", &p.radius, 0, 1000, "%.0f px"); TrackUndo("Rayon"); }
        if (p.type >= 3) { ImGui::SliderInt("Côtés", &p.sides, 3, 32); TrackUndo("Côtés"); }
        if (p.type != 2) { ImGui::SliderFloat("Rotation", &p.rotation, -180, 180, "%.1f°"); TrackUndo("Rotation"); }
        ImGui::PopID();
    }
    if (remove >= 0) {
        Edit e(d, v.history, "Supprimer le composant");
        s.parts.erase(s.parts.begin() + remove);
        e.Commit();
    }
    if (s.parts.size() < 8) {
        for (int op = 0; op < 4; ++op) {
            if (op) ImGui::SameLine();
            if (ImGui::SmallButton(ops[op])) {
                // New component overlapping the shape's right half.
                Edit e(d, v.history, "Composant de forme");
                ShapePart p;
                p.op = op;
                float w = std::abs(s.rect.z - s.rect.x), h = std::abs(s.rect.w - s.rect.y);
                float cx = std::max(s.rect.x, s.rect.z) - w * 0.15f, cy = (s.rect.y + s.rect.w) * 0.5f;
                p.rect = {cx - w * 0.3f, cy - h * 0.3f, cx + w * 0.3f, cy + h * 0.3f};
                s.parts.push_back(p);
                e.Commit();
            }
        }
        ImGui::TextDisabled("Outil Forme sur ce calque : Maj unir, Alt soustraire,\nMaj+Alt intersection.");
    }
}

}  // namespace sx
