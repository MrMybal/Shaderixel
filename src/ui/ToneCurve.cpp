#include "ui/ToneCurve.h"
#include "ui/CyUiState.h"
#include <cyui/modules/Curves.h>
#include <algorithm>
#include <cmath>

namespace sx::ui {
// CyUI renders the tone curve. These gestures remain host adapters because tone
// curves require bounded endpoints, single-click insertion and drag-out deletion.
struct ToneCurveView {
    cyui::CurveState view;
    CurvePoints before, published;
    std::vector<cyui::ItemId> ids, beforeIds;
    cyui::ItemId nextId=1;
    const CurvePoints* owner = nullptr;
    int drag = -1;
};
bool CurveEditor(const char* id, CurvePoints& pts, ImU32, float size, bool& activated,
                 bool& released, bool& cancelled, const float* histogram,
                 const std::function<float(const CurvePoints&,float)>& evaluate) {
    activated = released = cancelled = false;
    if (pts.size() < 2) pts = {{0,0},{1,1}};
    cyui::ScopedID scope(id);
    auto& state = ui::CyUiViewState<ToneCurveView>(ImGui::GetID("toneView"));
    if (state.owner != &pts) { state = {}; state.owner = &pts; }
    if (state.published != pts) {
        if (state.drag>=0) { state.drag=-1; state.before.clear(); cancelled=true; }
        state.ids.clear();
        for (size_t i=0;i<pts.size();++i) state.ids.push_back(state.nextId++);
    }
    std::vector<cyui::CurvePoint> points;
    for (size_t i=0; i<pts.size(); ++i) points.push_back({state.ids[i], pts[i].x, pts[i].y});
    cyui::CurveOptions options;
    options.readOnly = true; options.handles = false;
    options.evaluate = [&evaluate](cyui::ArrayView<cyui::CurvePoint> preview, double x) {
        CurvePoints curve;
        for (size_t i=0;i<preview.count;++i) curve.push_back({(float)preview[i].time,(float)preview[i].value});
        return double(evaluate(curve,(float)x));
    };
    const ImVec2 p0=ImGui::GetCursorScreenPos(), p1(p0.x+size,p0.y+size);
    cyui::curveEditor("##curve", state.view, points, ImVec2(size,size), options);
    const bool hovered=ImGui::IsItemHovered();
    auto toScreen=[&](float2 c) { return ImVec2(p0.x+(float)state.view.view.x.fraction(c.x)*size,p1.y-(float)state.view.view.y.fraction(c.y)*size); };
    auto toCurve=[&](ImVec2 m) { return float2{std::clamp((float)state.view.view.x.value((m.x-p0.x)/size),0.f,1.f),std::clamp((float)state.view.view.y.value((p1.y-m.y)/size),0.f,1.f)}; };
    if (histogram) {
        auto* dl=ImGui::GetWindowDrawList();
        for (int i=0;i<256;++i) {
            const float x=p0.x+size*(i+.5f)/256.f;
            dl->AddLine(ImVec2(x,p1.y),ImVec2(x,p1.y-histogram[i]*size*.9f),
                        cyui::alpha(cyui::palette().textDim,.2f),std::max(1.f,size/256.f));
        }
    }
    bool changed=false;
    const auto mouse=ImGui::GetIO().MousePos;
    int hot=-1;
    for (int i=0;i<(int)pts.size();++i) {
        const auto p=toScreen(pts[i]);
        if (std::abs(p.x-mouse.x)<cyui::dp(7.f) && std::abs(p.y-mouse.y)<cyui::dp(7.f)) hot=i;
    }
    if (hovered && ImGui::IsMouseClicked(0)) {
        activated=true; state.before=pts; state.beforeIds=state.ids;
        if (hot>=0) state.drag=hot;
        else {
            const auto p=toCurve(mouse);
            auto it=std::lower_bound(pts.begin(),pts.end(),p,[](const auto& a,const auto& b){return a.x<b.x;});
            state.drag=(int)(it-pts.begin()); pts.insert(it,p);
            state.ids.insert(state.ids.begin()+state.drag,state.nextId++); changed=true;
        }
    }
    if (hovered && ImGui::IsMouseClicked(1) && hot>0 && hot<(int)pts.size()-1) {
        activated=released=changed=true; pts.erase(pts.begin()+hot); state.ids.erase(state.ids.begin()+hot); state.drag=-1;
    }
    if (state.drag>=0 && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        pts=state.before; state.ids=state.beforeIds;
        state.drag=-1; changed=cancelled=true;
    }
    if (state.drag>=0 && state.drag<(int)pts.size()) {
        if (ImGui::IsMouseDown(0)) {
            const int i=state.drag;
            const float lo=i>0?pts[i-1].x+.01f:0.f, hi=i+1<(int)pts.size()?pts[i+1].x-.01f:1.f;
            if (i>0 && i<(int)pts.size()-1 && std::max(p0.y-mouse.y,mouse.y-p1.y)>cyui::dp(24.f)) {
                pts.erase(pts.begin()+i); state.ids.erase(state.ids.begin()+i); state.drag=-1; changed=true;
            } else {
                auto p=toCurve(mouse); p.x=std::clamp(p.x,lo,std::max(lo,hi));
                if (!(p==pts[i])) { pts[i]=p; changed=true; }
            }
        } else { state.drag=-1; released=true; }
    }
    if (state.drag<0 && !state.before.empty() && !ImGui::IsMouseDown(0) && !cancelled) {
        released=true; state.before.clear();
    }
    if (cancelled) state.before.clear();
    state.published=pts;
    return changed;
}
}  // namespace
