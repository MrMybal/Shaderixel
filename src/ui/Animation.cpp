// Frame animation (Animation panel, like Photoshop's frame timeline): each frame is the state of the layers
// (visibility, opacity, blend, filters on/off, smart object placement) shown for a delay. The selected frame
// follows the edits made to the layers; frames are played back, interpolated (tween) and exported as an
// animated GIF or an MP4 video.
#include "ui/App.h"
#include "ui/Filter.h"
#include "io/GifWriter.h"
#include "io/VideoWriter.h"

#include <imgui_internal.h>
#include <cmath>
#include <deque>

namespace sx {

namespace {
constexpr u64 kAnimThumbBit = 1ull << 56;
constexpr u32 kVideoFps = 30;

bool SameEntry(const LayerComp::Entry& a, const LayerComp::Entry& b) {
    if (a.layer != b.layer || a.visible != b.visible || a.opacity != b.opacity || a.blend != b.blend || a.filterOn != b.filterOn) return false;
    for (int k = 0; k < 4; ++k)
        if (a.corners[k].x != b.corners[k].x || a.corners[k].y != b.corners[k].y) return false;
    return true;
}

bool SameEntries(const std::vector<LayerComp::Entry>& a, const std::vector<LayerComp::Entry>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (!SameEntry(a[i], b[i])) return false;
    return true;
}

int FrameIndex(const DocState& s, Id id) {
    for (int i = 0; i < (int)s.animFrames.size(); ++i)
        if (s.animFrames[i].id == id) return i;
    return -1;
}

u64 HashEntries(const std::vector<LayerComp::Entry>& es) {
    u64 h = 1469598103934665603ull;
    auto mix = [&](u64 v) { h = (h ^ v) * 1099511628211ull; };
    for (auto& e : es) {
        mix(e.layer);
        mix(e.visible);
        u32 bits;
        memcpy(&bits, &e.opacity, 4);
        mix(bits);
        mix((u64)e.blend);
        for (u8 b : e.filterOn) mix(b);
        for (auto& c : e.corners) {
            memcpy(&bits, &c.x, 4);
            mix(bits);
            memcpy(&bits, &c.y, 4);
            mix(bits);
        }
    }
    return h;
}

std::string DelayLabel(float d) {
    char b[32];
    if (d <= 0) snprintf(b, sizeof(b), "0 s");
    else if (d < 1) snprintf(b, sizeof(b), "%.2g s", d);
    else snprintf(b, sizeof(b), "%.1f s", d);
    return b;
}
}  // namespace

struct App::AnimExportJob {
    DocView* view = nullptr;
    bool video = false;
    std::string path;
    GifWriter gif;
    VideoWriter vid;
    int next = 0, written = 0, total = 0;
    std::deque<std::pair<gpu::ReadbackPtr, float>> pending;  // readbacks in frame order, with their delays
    std::vector<LayerComp::Entry> restore;
    Id restoreFrame = 0;
    std::string error;
};

void App::AnimCaptureEntries(Document& d, std::vector<LayerComp::Entry>& out) {
    LayerComp tmp;
    CaptureComp(d, tmp);
    out = std::move(tmp.entries);
}

void App::ApplyAnimEntries(DocView& v, const std::vector<LayerComp::Entry>& entries) {
    Document& d = *v.doc;
    bool moved = false;
    for (auto& en : entries) {
        Layer* l = d.FindLayer(en.layer);
        if (!l) continue;  // deleted since
        l->visible = en.visible;
        l->opacity = en.opacity;
        l->blend = en.blend;
        for (size_t i = 0; i < l->filters.size() && i < en.filterOn.size(); ++i) l->filters[i].enabled = en.filterOn[i] != 0;
        if (l->kind == LayerKind::Smart)
            for (int k = 0; k < 4; ++k)
                if (l->smartCorners[k].x != en.corners[k].x || l->smartCorners[k].y != en.corners[k].y) {
                    l->smartCorners[k] = en.corners[k];
                    moved = true;
                }
    }
    d.SyncGraph();
    if (moved) {
        Prepare(v);
        SyncSmartLayers(v);
    }
}

void App::ApplyAnimFrame(DocView& v, Id frameId) {
    DocState& s = v.doc->S();
    const int i = FrameIndex(s, frameId);
    if (i < 0) return;
    const std::vector<LayerComp::Entry> entries = s.animFrames[i].entries;  // (copy: SyncGraph may touch the state)
    s.activeFrame = frameId;
    ApplyAnimEntries(v, entries);
}

void App::AnimTween(DocView& v, int index, int count) {
    Document& d = *v.doc;
    auto& frames = d.S().animFrames;
    if (index < 0 || index + 1 >= (int)frames.size() || count < 1) return;
    Edit e(d, v.history, "Interpoler les images");
    const AnimFrame A = frames[index], B = frames[index + 1];
    auto find = [](const AnimFrame& f, Id layer) -> const LayerComp::Entry* {
        for (auto& en : f.entries) if (en.layer == layer) return &en;
        return nullptr;
    };
    std::vector<AnimFrame> added;
    for (int k = 1; k <= count; ++k) {
        const float t = (float)k / (count + 1);
        AnimFrame f;
        f.id = d.NewId();
        f.delay = A.delay;
        for (auto& a : A.entries) {
            const LayerComp::Entry* b = find(B, a.layer);
            LayerComp::Entry en = a;
            if (b) {
                // A layer shown in one frame only fades in / out.
                const float oa = a.visible ? a.opacity : 0.0f, ob = b->visible ? b->opacity : 0.0f;
                en.visible = a.visible || b->visible;
                en.opacity = oa + (ob - oa) * t;
                for (int c = 0; c < 4; ++c) en.corners[c] = a.corners[c] + (b->corners[c] - a.corners[c]) * t;
                if (t >= 0.5f) {
                    en.blend = b->blend;
                    en.filterOn = b->filterOn;
                }
            }
            f.entries.push_back(std::move(en));
        }
        added.push_back(std::move(f));
    }
    frames.insert(frames.begin() + index + 1, added.begin(), added.end());
    e.Commit();
    Status(std::to_string(count) + " image(s) interpolée(s)");
}

void App::StartAnimExport(DocView& v, const std::string& path, bool video) {
    if (animExport) return;
    Document& d = *v.doc;
    if (d.S().animFrames.empty()) return;
    anim.playing = false;
    auto J = std::make_shared<AnimExportJob>();
    J->view = &v;
    J->video = video;
    J->path = path;
    J->total = (int)d.S().animFrames.size();
    AnimCaptureEntries(d, J->restore);
    J->restoreFrame = d.S().activeFrame;
    std::string err;
    const bool ok = video ? J->vid.Open(path, d.Width(), d.Height(), kVideoFps, &err)
                          : J->gif.Open(path, kVideoFps, std::min<u32>(d.Width(), 1920), &err, (u32)std::max(0, d.S().animLoops));
    if (!ok) {
        Status("Export de l'animation impossible : " + err);
        return;
    }
    animExport = J;
    Status("Export de l'animation...");
}

void App::AnimTick() {
    // Export: one frame rendered per tick (a few in flight), written in order once read back.
    if (animExport) {
        AnimExportJob& J = *animExport;
        bool alive = false;
        for (auto& dv : docs) alive |= dv.get() == J.view;
        if (!alive) {
            animExport.reset();
            return;
        }
        DocView& v = *J.view;
        auto& frames = v.doc->S().animFrames;
        if (J.next < (int)frames.size() && J.pending.size() < 3) {
            ApplyAnimEntries(v, frames[J.next].entries);
            Prepare(v);
            J.pending.push_back({v.renderer.ExportRGBA8(*ctx, v.renderer.EvaluateComplete(*v.doc, *ctx)), frames[J.next].delay});
            ++J.next;
        }
        while (!J.pending.empty() && (!J.pending.front().first || J.pending.front().first->ready)) {
            auto [rb, delay] = J.pending.front();
            J.pending.pop_front();
            if (!rb || rb->data.empty()) {
                J.error = "rendu impossible";
                continue;
            }
            if (J.video) {
                const int n = std::max(1, (int)std::lround(delay * kVideoFps));
                for (int k = 0; k < n; ++k) J.vid.Write(rb->data.data(), rb->width, rb->height);
            } else {
                J.gif.Write(rb->data.data(), rb->width, rb->height, delay);
            }
            ++J.written;
        }
        if (J.next >= (int)frames.size() && J.pending.empty()) {
            const bool closed = J.video ? J.vid.Close() : J.gif.Close();
            ApplyAnimEntries(v, J.restore);
            v.doc->S().activeFrame = J.restoreFrame;
            if (!closed && J.error.empty()) J.error = "écriture du fichier impossible";
            Status(J.error.empty() ? "Animation exportée : " + std::to_string(J.written) + " image(s), " + J.path : "Export de l'animation : " + J.error);
            animExport.reset();
        }
        return;
    }
    DocView* v = Active();
    if (!v || v->doc->Mode() != DocMode::Layers) {
        anim.playing = false;
        return;
    }
    DocState& s = v->doc->S();
    if (anim.playing) {
        if (anim.view != v || s.animFrames.empty()) {
            anim.playing = false;
            return;
        }
        int i = std::max(0, FrameIndex(s, s.activeFrame));
        anim.acc += ImGui::GetIO().DeltaTime;
        bool changed = false;
        for (int guard = 0; guard < 64 && anim.acc >= std::max(s.animFrames[i].delay, 0.01f); ++guard) {
            anim.acc -= std::max(s.animFrames[i].delay, 0.01f);
            if (++i >= (int)s.animFrames.size()) {
                i = 0;
                ++anim.played;
                if (s.animLoops > 0 && anim.played >= s.animLoops) {
                    anim.playing = false;
                    i = (int)s.animFrames.size() - 1;  // (stops on the last frame)
                    changed = true;
                    break;
                }
            }
            changed = true;
        }
        if (changed) ApplyAnimFrame(*v, s.animFrames[i].id);
        return;
    }
    // The selected frame records the edits made to the layers (visibility, opacity, placement...).
    const int i = FrameIndex(s, s.activeFrame);
    if (i < 0) return;
    std::vector<LayerComp::Entry> now;
    AnimCaptureEntries(*v->doc, now);
    if (!SameEntries(now, s.animFrames[i].entries)) {
        // Layers missing from the current state (deleted) are kept in the frame (undo may bring them back).
        for (auto& en : s.animFrames[i].entries) {
            bool found = false;
            for (auto& x : now) found |= x.layer == en.layer;
            if (!found && !v->doc->FindLayer(en.layer)) now.push_back(en);
        }
        if (!SameEntries(now, s.animFrames[i].entries)) {
            s.animFrames[i].entries = std::move(now);
            v->doc->dirty = true;
        }
    }
}

void App::DrawAnimationPanel() {
    if (!ImGui::Begin("Animation", PanelFlag("Animation"))) { ImGui::End(); return; }
    DocView* v = Active();
    if (!v || v->doc->Mode() != DocMode::Layers) {
        ImGui::TextDisabled(v ? "Disponible en mode calques" : "Aucun document");
        ImGui::End();
        return;
    }
    Document& d = *v->doc;
    DocState& s = d.S();
    auto& frames = s.animFrames;
    const bool exporting = animExport != nullptr;
    int cur = FrameIndex(s, s.activeFrame);
    if (frames.empty()) {
        ImGui::TextWrapped("Animation image par image : chaque image mémorise l'état des calques (visibilité, opacité, fusion, filtres, position des objets dynamiques) "
                           "et sa durée. Modifiez les calques pour composer l'image sélectionnée.");
        if (ImGui::Button("Créer l'animation")) {
            Edit e(d, v->history, "Créer l'animation");
            AnimFrame f;
            f.id = d.NewId();
            AnimCaptureEntries(d, f.entries);
            frames.push_back(std::move(f));
            s.activeFrame = frames.back().id;
            e.Commit();
        }
        ImGui::End();
        return;
    }
    if (cur < 0) {
        ApplyAnimFrame(*v, frames[0].id);
        cur = 0;
    }
    ImGui::BeginDisabled(exporting);
    // Transport
    auto go = [&](int i) {
        anim.playing = false;
        ApplyAnimFrame(*v, frames[std::clamp(i, 0, (int)frames.size() - 1)].id);
    };
    if (ImGui::Button("|<")) go(0);
    ImGui::SetItemTooltip("Première image");
    ImGui::SameLine();
    if (ImGui::ArrowButton("##prev", ImGuiDir_Left)) go(cur > 0 ? cur - 1 : (int)frames.size() - 1);
    ImGui::SetItemTooltip("Image précédente");
    ImGui::SameLine();
    if (ImGui::Button(anim.playing ? "Arrêter" : "Lecture", ImVec2(70 * dpi, 0))) {
        anim.playing = !anim.playing;
        anim.view = v;
        anim.acc = 0;
        anim.played = 0;
    }
    ImGui::SameLine();
    if (ImGui::ArrowButton("##next", ImGuiDir_Right)) go((cur + 1) % (int)frames.size());
    ImGui::SetItemTooltip("Image suivante");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110 * dpi);
    const int loopOpts[] = {1, 3, 0};
    int loopSel = s.animLoops == 1 ? 0 : s.animLoops == 0 ? 2 : 1;
    if (ImGui::Combo("##loops", &loopSel, "Une fois\0Trois fois\0En boucle\0")) {
        Edit e(d, v->history, "Répétitions de l'animation");
        s.animLoops = loopOpts[loopSel];
        e.Commit();
    }
    ImGui::SetItemTooltip("Répétitions (lecture et GIF)");
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (ImGui::Button("Dupliquer l'image")) {
        Edit e(d, v->history, "Nouvelle image");
        AnimFrame f = frames[cur];
        f.id = d.NewId();
        frames.insert(frames.begin() + cur + 1, f);
        s.activeFrame = f.id;
        e.Commit();
        cur = cur + 1;
    }
    ImGui::SetItemTooltip("Ajoute une copie de l'image sélectionnée juste après elle.");
    ImGui::SameLine();
    ImGui::BeginDisabled(cur + 1 >= (int)frames.size());
    if (ImGui::Button("Interpoler...")) ImGui::OpenPopup("##tween");
    ImGui::SetItemTooltip("Ajoute des images intermédiaires entre l'image sélectionnée et la suivante\n(opacités et positions des objets dynamiques interpolées).");
    ImGui::EndDisabled();
    if (ImGui::BeginPopup("##tween")) {
        ImGui::SetNextItemWidth(120 * dpi);
        ImGui::SliderInt("Images à ajouter", &anim.tweenCount, 1, 30);
        if (ImGui::Button("Interpoler", ImVec2(-1, 0))) {
            AnimTween(*v, cur, anim.tweenCount);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(frames.size() <= 1);
    if (ImGui::Button("Supprimer")) {
        Edit e(d, v->history, "Supprimer l'image");
        frames.erase(frames.begin() + cur);
        cur = std::min(cur, (int)frames.size() - 1);
        e.Commit();
        ApplyAnimFrame(*v, frames[cur].id);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (ImGui::Button("Exporter GIF...")) {
        std::string f = platform::SaveFileDialog(hwnd, L"GIF animé\0*.gif\0", L"gif");
        if (!f.empty()) StartAnimExport(*v, f, false);
    }
    ImGui::SameLine();
    if (ImGui::Button("Exporter vidéo...")) {
        std::string f = platform::SaveFileDialog(hwnd, L"Vidéo MP4\0*.mp4\0", L"mp4");
        if (!f.empty()) StartAnimExport(*v, f, true);
    }
    ImGui::EndDisabled();
    if (exporting) {
        ImGui::SameLine();
        ImGui::ProgressBar((float)animExport->written / std::max(1, animExport->total), ImVec2(160 * dpi, 0));
    }
    float total = 0;
    for (auto& f : frames) total += f.delay;
    ImGui::SameLine();
    ImGui::TextDisabled("%zu images, %.2f s", frames.size(), total);

    // Frames (thumbnails); drag one onto another to move it.
    const float th = std::max(48.0f, std::min(110 * dpi, ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() * 2 - 14 * dpi));
    const float aspect = (float)d.Width() / std::max(1u, d.Height());
    const float tw = std::clamp(th * aspect, 40 * dpi, 220 * dpi);
    ImGui::BeginChild("##frames", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    int moveFrom = -1, moveTo = -1, toApply = -1;
    for (int i = 0; i < (int)frames.size(); ++i) {
        AnimFrame& f = frames[i];
        ImGui::PushID((int)f.id);
        if (i) ImGui::SameLine();
        ImGui::BeginGroup();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const bool sel = i == cur;
        if (ImGui::InvisibleButton("##card", ImVec2(tw + 8 * dpi, th + 8 * dpi))) toApply = i;
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("SX_ANIMFRAME", &i, sizeof(int));
            ImGui::Text("Image %d", i + 1);
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("SX_ANIMFRAME")) {
                moveFrom = *(const int*)pl->Data;
                moveTo = i;
            }
            ImGui::EndDragDropTarget();
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, ImVec2(p.x + tw + 8 * dpi, p.y + th + 8 * dpi), ImGui::GetColorU32(sel ? ImGuiCol_HeaderActive : ImGuiCol_FrameBg), 4 * dpi);
        const u64 key = kAnimThumbBit | (f.id & 0xffffffffffffull);
        gpu::Texture* t = anim.thumbs.count(f.id) ? v->renderer.CachedThumbnail(key) : nullptr;
        const ImVec2 a(p.x + 4 * dpi, p.y + 4 * dpi), b(p.x + 4 * dpi + tw, p.y + 4 * dpi + th);
        if (t) dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(t), a, b);
        else dl->AddText(ImVec2(a.x + 6 * dpi, a.y + 6 * dpi), ImGui::GetColorU32(ImGuiCol_TextDisabled), "...");
        char num[16];
        snprintf(num, sizeof(num), "%d", i + 1);
        dl->AddRectFilled(a, ImVec2(a.x + ImGui::CalcTextSize(num).x + 8 * dpi, a.y + ImGui::GetTextLineHeight() + 2 * dpi), IM_COL32(0, 0, 0, 160));
        dl->AddText(ImVec2(a.x + 4 * dpi, a.y + 1 * dpi), IM_COL32_WHITE, num);
        if (sel) dl->AddRect(p, ImVec2(p.x + tw + 8 * dpi, p.y + th + 8 * dpi), ImGui::GetColorU32(ImGuiCol_CheckMark), 4 * dpi, 0, 2 * dpi);
        // Delay
        ImGui::SetNextItemWidth(tw + 8 * dpi);
        if (ImGui::BeginCombo("##delay", DelayLabel(f.delay).c_str())) {
            const float presets[] = {0.0f, 0.05f, 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f};
            for (float pr : presets)
                if (ImGui::Selectable(DelayLabel(pr).c_str(), f.delay == pr)) {
                    Edit e(d, v->history, "Durée de l'image");
                    f.delay = pr;
                    e.Commit();
                }
            ImGui::Separator();
            float dv = f.delay;
            ImGui::SetNextItemWidth(120 * dpi);
            if (ImGui::DragFloat("##custom", &dv, 0.01f, 0.0f, 60.0f, "%.2f s")) f.delay = std::clamp(dv, 0.0f, 60.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) d.dirty = true;
            if (ImGui::Selectable("Toutes les images à cette durée")) {
                Edit e(d, v->history, "Durée des images");
                for (auto& o : frames) o.delay = f.delay;
                e.Commit();
            }
            ImGui::EndCombo();
        }
        ImGui::EndGroup();
        ImGui::PopID();
    }
    ImGui::EndChild();
    // Thumbnails: a frame whose layers' state or the document changed is rendered again, one every few frames
    // (applied, evaluated, previewed, then the current state put back).
    if (!anim.playing && !exporting && ++anim.thumbWait >= 8) {
        const u64 content = ((u64)v->history.Cursor() << 32) ^ v->history.Count() ^ ((u64)(u32)tw << 48) ^ ((u64)(u32)th << 40) ^ ((u64)d.Width() << 20) ^ d.Height();
        for (int i = 0; i < (int)frames.size(); ++i) {
            const u64 want = HashEntries(frames[i].entries) ^ content;
            if (anim.thumbs.count(frames[i].id) && anim.thumbs[frames[i].id] == want) continue;
            std::vector<LayerComp::Entry> keep;
            if (i != cur) {
                AnimCaptureEntries(d, keep);
                ApplyAnimEntries(*v, frames[i].entries);
                Prepare(*v);
            }
            if (gpu::Texture* img = v->renderer.EvaluateComplete(d, *ctx))
                v->renderer.Thumbnail(*ctx, kAnimThumbBit | (frames[i].id & 0xffffffffffffull), img, want, (u32)tw, (u32)th);
            if (i != cur) ApplyAnimEntries(*v, keep);
            anim.thumbs[frames[i].id] = want;
            anim.thumbWait = 0;
            break;
        }
    }
    if (toApply >= 0 && !exporting) go(toApply);
    if (moveFrom >= 0 && moveTo >= 0 && moveFrom != moveTo && !exporting) {
        Edit e(d, v->history, "Déplacer l'image");
        AnimFrame f = frames[moveFrom];
        frames.erase(frames.begin() + moveFrom);
        frames.insert(frames.begin() + moveTo, f);
        e.Commit();
    }
    ImGui::End();
}

}  // namespace sx
