// Ruler tool (measure a distance and an angle, then straighten the image along the line) and Image > Rotate >
// Arbitrary (any angle; the edges: keep the size, crop the empty corners away, or enlarge to keep everything).
#include "ui/App.h"
#include "ui/Theme.h"

#include <cmath>

namespace sx {

void App::RotateCanvasArbitrary(DocView& v, float degrees, int edges) {
    const float W = (float)v.doc->Width(), H = (float)v.doc->Height();
    const float a = degrees * 3.14159265f / 180.0f, c = std::cos(a), s = std::sin(a);
    const float ac = std::abs(c), as = std::abs(s);
    float nw = W, nh = H;
    if (edges == 1) {
        // The largest centered rectangle of the same proportions inside the turned image (no empty corners).
        const float k = std::min(W / (W * ac + H * as), H / (W * as + H * ac));
        nw = W * k;
        nh = H * k;
    } else if (edges == 2) {
        nw = W * ac + H * as;
        nh = W * as + H * ac;
    }
    const u32 w = (u32)std::max(1L, std::lround(nw)), h = (u32)std::max(1L, std::lround(nh));
    // Destination pixel -> source: turned back around the centers (positive degrees: clockwise on screen).
    const float cx = W * 0.5f, cy = H * 0.5f, dx = w * 0.5f, dy = h * 0.5f;
    const float m[6] = {c, s, cx - (c * dx + s * dy), -s, c, cy - (-s * dx + c * dy)};
    {
        Json p = Json::Object();
        p["angle"] = degrees;
        p["edges"] = edges;
        RecordAction("rotate_arbitrary", p);
    }
    ApplyCanvasOp(v, w, h, m, 2, "Rotation de l'image");
}

void App::RulerInput(DocView& v, float2 dp, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        // Grab an end of the line, else draw a new one.
        const float grab = 8.0f / std::max(v.zoom, 1e-3f);
        ruler.drag = ruler.active && Length(ruler.b - dp) < grab ? 2 : ruler.active && Length(ruler.a - dp) < grab ? 1 : 0;
        if (!ruler.drag) {
            ruler.a = ruler.b = dp;
            ruler.active = true;
            ruler.drag = 2;
        }
    }
    if (ruler.drag) {
        float2 p = dp;
        if (io.KeyShift) {
            // Shift: multiples of 45 degrees.
            const float2 o = ruler.drag == 2 ? ruler.a : ruler.b;
            const float2 d = p - o;
            const float ang = std::round(std::atan2(d.y, d.x) / (3.14159265f / 4)) * (3.14159265f / 4), len = Length(d);
            p = o + float2{std::cos(ang) * len, std::sin(ang) * len};
        }
        (ruler.drag == 2 ? ruler.b : ruler.a) = p;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            ruler.drag = 0;
            if (Length(ruler.b - ruler.a) < 1) ruler.active = false;
        }
    }
}

float App::RulerAngle() const {
    const float2 d = ruler.b - ruler.a;
    return std::atan2(-d.y, d.x) * 180.0f / 3.14159265f;  // counter-clockwise from the horizontal, like Photoshop
}

void App::RulerOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl) {
    if (!ruler.active || tool != Tool::Ruler) return;
    auto S = [&](float2 p) { return ImVec2(canvasMin.x + v.pan.x + p.x * v.zoom, canvasMin.y + v.pan.y + p.y * v.zoom); };
    const ImVec2 a = S(ruler.a), b = S(ruler.b);
    dl->AddLine(a, b, IM_COL32(0, 0, 0, 180), 3.0f);
    dl->AddLine(a, b, IM_COL32_WHITE, 1.2f);
    for (ImVec2 p : {a, b}) {
        dl->AddLine(ImVec2(p.x - 6, p.y), ImVec2(p.x + 6, p.y), IM_COL32_WHITE, 1.2f);
        dl->AddLine(ImVec2(p.x, p.y - 6), ImVec2(p.x, p.y + 6), IM_COL32_WHITE, 1.2f);
    }
    char buf[96];
    snprintf(buf, sizeof(buf), "%.1f px   %.2f°", Length(ruler.b - ruler.a), RulerAngle());
    const ImVec2 t(b.x + 10, b.y + 6);
    dl->AddRectFilled(ImVec2(t.x - 4, t.y - 2), ImVec2(t.x + ImGui::CalcTextSize(buf).x + 4, t.y + ImGui::GetTextLineHeight() + 2), IM_COL32(0, 0, 0, 170), 3);
    dl->AddText(t, IM_COL32_WHITE, buf);
}

void App::RulerOptions(DocView* v) {
    ImGui::SeparatorText("Règle");
    ImGui::TextWrapped("Glissez pour mesurer (Maj : multiples de 45°) ; glissez une extrémité pour l'ajuster.");
    if (!ruler.active) return;
    const float2 d = ruler.b - ruler.a;
    ImGui::Text("Longueur : %.1f px", Length(d));
    ImGui::Text("Angle : %.2f°", RulerAngle());
    ImGui::Text("Début : %.0f, %.0f   L : %.0f   H : %.0f", ruler.a.x, ruler.a.y, std::abs(d.x), std::abs(d.y));
    ImGui::Combo("Bords", &ruler.edges, "Garder la taille\0Recadrer (sans coins vides)\0Agrandir (tout garder)\0");
    if (v && ImGui::Button("Redresser l'image")) {
        // The line becomes horizontal (or vertical when it is closer to vertical).
        float ang = RulerAngle();
        float turn = ang;  // clockwise rotation that brings the line to 0
        if (ang > 45) turn = ang - 90;
        else if (ang < -45) turn = ang + 90;
        if (ang > 135) turn = ang - 180;
        else if (ang < -135) turn = ang + 180;
        RotateCanvasArbitrary(*v, turn, ruler.edges);
        ruler.active = false;
    }
    ui::HelpTooltip("Tourne l'image pour que la ligne mesurée devienne horizontale (ou verticale) : horizon, façade, document.");
    ImGui::SameLine();
    if (ImGui::Button("Effacer")) ruler.active = false;
}

}  // namespace sx
