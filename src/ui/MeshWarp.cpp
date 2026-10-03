// Puppet warp (pins, moving least squares: rigid / similarity / affine) and
// perspective warp (planes laid out on the image, then moved: one homography per
// plane). Both deform a mesh drawn by Renderer::WarpPreview, inside the transform
// session (xf) so commit / cancel / undo and the selection follow as for Warp.
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui_internal.h>
#include <cmath>

namespace sx {

namespace {

float Dot(float2 a, float2 b) { return a.x * b.x + a.y * b.y; }
float2 Perp(float2 a) { return {-a.y, a.x}; }

// Unit square -> quad (Heckbert), row-major 3x3 in double.
void SquareToQuadD(const float2* q, double* m) {
    double sx = q[0].x - q[1].x + q[2].x - q[3].x, sy = q[0].y - q[1].y + q[2].y - q[3].y;
    double g = 0, h = 0;
    if (std::abs(sx) > 1e-9 || std::abs(sy) > 1e-9) {
        double dx1 = q[1].x - q[2].x, dx2 = q[3].x - q[2].x, dy1 = q[1].y - q[2].y, dy2 = q[3].y - q[2].y;
        double den = dx1 * dy2 - dx2 * dy1;
        if (std::abs(den) > 1e-12) {
            g = (sx * dy2 - dx2 * sy) / den;
            h = (dx1 * sy - sx * dy1) / den;
        }
    }
    m[0] = q[1].x - q[0].x + g * q[1].x; m[1] = q[3].x - q[0].x + h * q[3].x; m[2] = q[0].x;
    m[3] = q[1].y - q[0].y + g * q[1].y; m[4] = q[3].y - q[0].y + h * q[3].y; m[5] = q[0].y;
    m[6] = g; m[7] = h; m[8] = 1;
}

bool Inv3(const double* m, double* o) {
    double a = m[0], b = m[1], c = m[2], d = m[3], e = m[4], f = m[5], g = m[6], h = m[7], i = m[8];
    double A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
    double det = a * A + b * B + c * C;
    if (std::abs(det) < 1e-15) return false;
    double r = 1.0 / det;
    o[0] = A * r; o[1] = -(b * i - c * h) * r; o[2] = (b * f - c * e) * r;
    o[3] = B * r; o[4] = (a * i - c * g) * r; o[5] = -(a * f - c * d) * r;
    o[6] = C * r; o[7] = -(a * h - b * g) * r; o[8] = (a * e - b * d) * r;
    return true;
}

void Mul3D(const double* a, const double* b, double* o) {
    double r[9];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) r[i * 3 + j] = a[i * 3] * b[j] + a[i * 3 + 1] * b[3 + j] + a[i * 3 + 2] * b[6 + j];
    for (int k = 0; k < 9; ++k) o[k] = r[k];
}

bool InQuad(const float2* q, float2 p) {
    bool pos = false, neg = false;
    for (int i = 0; i < 4; ++i) {
        float2 a = q[i], b = q[(i + 1) % 4];
        float c = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
        pos |= c > 0;
        neg |= c < 0;
    }
    return !(pos && neg);
}

float SegDist(float2 p, float2 a, float2 b) {
    float2 ab = b - a;
    float t = std::clamp(Dot(p - a, ab) / std::max(Dot(ab, ab), 1e-9f), 0.0f, 1.0f);
    return Length(p - (a + ab * t));
}

// Moving least squares deformation (Schaefer et al. 2006) of point v by pins p -> q.
float2 Mls(float2 v, const std::vector<float2>& p, const std::vector<float2>& q, int mode) {
    const size_t n = p.size();
    if (n == 0) return v;
    std::vector<float> w(n);
    float sw = 0;
    float2 ps{0, 0}, qs{0, 0};
    for (size_t i = 0; i < n; ++i) {
        float d2 = Dot(p[i] - v, p[i] - v);
        if (d2 < 1e-6f) return q[i];
        w[i] = 1.0f / d2;
        sw += w[i];
        ps = ps + p[i] * w[i];
        qs = qs + q[i] * w[i];
    }
    ps = ps * (1.0f / sw);
    qs = qs * (1.0f / sw);
    if (n == 1) return v + (q[0] - p[0]);
    float2 vp = v - ps;
    if (mode == 2) {
        // Affine: (v - p*) M + q*, M = (sum w p^T p)^-1 sum w p^T q.
        double a00 = 0, a01 = 0, a11 = 0, b00 = 0, b01 = 0, b10 = 0, b11 = 0;
        for (size_t i = 0; i < n; ++i) {
            float2 ph = p[i] - ps, qh = q[i] - qs;
            a00 += w[i] * ph.x * ph.x; a01 += w[i] * ph.x * ph.y; a11 += w[i] * ph.y * ph.y;
            b00 += w[i] * ph.x * qh.x; b01 += w[i] * ph.x * qh.y; b10 += w[i] * ph.y * qh.x; b11 += w[i] * ph.y * qh.y;
        }
        double det = a00 * a11 - a01 * a01;
        if (std::abs(det) > 1e-9 * (a00 * a11 + 1e-12)) {
            double i00 = a11 / det, i01 = -a01 / det, i11 = a00 / det;
            double m00 = i00 * b00 + i01 * b10, m01 = i00 * b01 + i01 * b11, m10 = i01 * b00 + i11 * b10, m11 = i01 * b01 + i11 * b11;
            return float2{(float)(vp.x * m00 + vp.y * m10), (float)(vp.x * m01 + vp.y * m11)} + qs;
        }
        mode = 1;  // collinear pins: similarity
    }
    float2 fr{0, 0};
    float mus = 0;
    float2 vpp = Perp(vp);
    for (size_t i = 0; i < n; ++i) {
        float2 ph = p[i] - ps, qh = q[i] - qs, php = Perp(ph);
        // A = w [ph; -ph^perp] [vp; -vp^perp]^T
        float a00 = w[i] * Dot(ph, vp), a01 = -w[i] * Dot(ph, vpp), a10 = -w[i] * Dot(php, vp), a11 = w[i] * Dot(php, vpp);
        fr.x += qh.x * a00 + qh.y * a10;
        fr.y += qh.x * a01 + qh.y * a11;
        mus += w[i] * Dot(ph, ph);
    }
    if (mode == 1) return mus > 1e-9f ? fr * (1.0f / mus) + qs : v + (qs - ps);
    float l = Length(fr);
    return l > 1e-9f ? fr * (Length(vp) / l) + qs : v + (qs - ps);
}

}  // namespace

void App::MeshWarpBegin(DocView& v, int kind) {
    if (!WarpBegin(v)) return;
    xf.warpKind = kind;
    xf.pinSrc.clear();
    xf.pinDst.clear();
    xf.layoutPts.clear();
    xf.warpPts.clear();
    xf.planes.clear();
    xf.perspPhase = 0;
    xf.dirty = true;
    Status(kind == 1 ? "Marionnette : cliquez pour poser des épingles, faites-les glisser. Entrée : valider, Échap : annuler."
                     : "Perspective : ajustez les plans sur l'image, puis « Déformer ». Entrée : valider, Échap : annuler.");
}

void App::PerspectiveInitPlanes() {
    if (!xf.layoutPts.empty()) return;
    const Rect& r = xf.src;
    xf.layoutPts = {{(float)r.x0, (float)r.y0}, {(float)r.x1, (float)r.y0}, {(float)r.x1, (float)r.y1}, {(float)r.x0, (float)r.y1}};
    xf.warpPts = xf.layoutPts;
    xf.planes = {{0, 1, 2, 3}};
    xf.planeSel = 0;
}

WarpMesh App::BuildMeshWarp() {
    WarpMesh m;
    const Rect& r = xf.src;
    const float w = (float)std::max(1, r.Width()), h = (float)std::max(1, r.Height());
    int n = xf.warpKind == 1 ? (xf.puppetDensity == 0 ? 24 : xf.puppetDensity == 1 ? 48 : 96) : 64;
    m.nx = w >= h ? n : std::max(4, (int)std::lround(n * w / h));
    m.ny = w >= h ? std::max(4, (int)std::lround(n * h / w)) : n;
    // Perspective: one homography per plane (layout -> warped).
    std::vector<std::array<double, 9>> H;
    if (xf.warpKind == 2) {
        PerspectiveInitPlanes();
        for (auto& pl : xf.planes) {
            float2 lq[4], wq[4];
            for (int k = 0; k < 4; ++k) { lq[k] = xf.layoutPts[pl[k]]; wq[k] = xf.warpPts[pl[k]]; }
            double a[9], b[9], ai[9];
            std::array<double, 9> M;
            SquareToQuadD(lq, a);
            SquareToQuadD(wq, b);
            if (!Inv3(a, ai)) { M = {1, 0, 0, 0, 1, 0, 0, 0, 1}; }
            else Mul3D(b, ai, M.data());
            H.push_back(M);
        }
    }
    const float lim = (float)std::max(xf.view ? xf.view->doc->Width() : 4096, xf.view ? xf.view->doc->Height() : 4096) * 3.0f;
    m.src.reserve((size_t)(m.nx + 1) * (m.ny + 1));
    m.dst.reserve(m.src.capacity());
    for (int y = 0; y <= m.ny; ++y)
        for (int x = 0; x <= m.nx; ++x) {
            float2 s{r.x0 + w * x / m.nx, r.y0 + h * y / m.ny};
            float2 d = s;
            if (xf.warpKind == 1) {
                d = Mls(s, xf.pinSrc, xf.pinDst, xf.puppetMode);
            } else if (xf.warpKind == 2 && xf.perspPhase == 1 && !H.empty()) {
                // The plane holding the point, or the nearest one (extrapolated).
                int best = 0;
                float bd = 1e30f;
                for (size_t k = 0; k < xf.planes.size(); ++k) {
                    float2 q[4];
                    for (int c = 0; c < 4; ++c) q[c] = xf.layoutPts[xf.planes[k][c]];
                    if (InQuad(q, s)) { best = (int)k; bd = 0; break; }
                    for (int c = 0; c < 4; ++c) {
                        float dd = SegDist(s, q[c], q[(c + 1) % 4]);
                        if (dd < bd) { bd = dd; best = (int)k; }
                    }
                }
                const double* M = H[best].data();
                double X = M[0] * s.x + M[1] * s.y + M[2], Y = M[3] * s.x + M[4] * s.y + M[5], Z = M[6] * s.x + M[7] * s.y + M[8];
                Z = std::max(Z, 0.05);
                d = {(float)std::clamp(X / Z, (double)-lim, (double)lim), (float)std::clamp(Y / Z, (double)-lim, (double)lim)};
            }
            m.src.push_back(s);
            m.dst.push_back(d);
        }
    return m;
}

bool App::MeshWarpChanged() {
    if (xf.warpKind == 1) {
        for (size_t i = 0; i < xf.pinSrc.size(); ++i) if (Length(xf.pinSrc[i] - xf.pinDst[i]) > 1e-3f) return true;
        return false;
    }
    if (xf.perspPhase != 1) return false;
    for (size_t i = 0; i < xf.layoutPts.size(); ++i) if (Length(xf.layoutPts[i] - xf.warpPts[i]) > 1e-3f) return true;
    return false;
}

void App::MeshWarpInput(DocView& v, ImVec2 canvasMin, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    float2 m = ScreenToDoc(v, canvasMin, io.MousePos);
    auto toScreen = [&](float2 p) { return ImVec2(canvasMin.x + v.pan.x + p.x * v.zoom, canvasMin.y + v.pan.y + p.y * v.zoom); };
    auto nearPt = [&](float2 p) { ImVec2 s = toScreen(p); float dx = s.x - io.MousePos.x, dy = s.y - io.MousePos.y; return dx * dx + dy * dy <= 81.0f; };
    bool click = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (xf.warpKind == 1) {
        int hit = -1;
        for (int i = (int)xf.pinDst.size() - 1; i >= 0 && hit < 0; --i) if (nearPt(xf.pinDst[i])) hit = i;
        if (hovered && xf.pinDrag < 0) ImGui::SetMouseCursor(hit >= 0 ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_Arrow);
        if (click) {
            if (hit >= 0 && io.KeyAlt) {
                xf.pinSrc.erase(xf.pinSrc.begin() + hit);
                xf.pinDst.erase(xf.pinDst.begin() + hit);
                xf.pinSel = -1;
                xf.dirty = true;
            } else if (hit >= 0) {
                xf.pinSel = xf.pinDrag = hit;
                xf.dragOffset = xf.pinDst[hit] - m;
            } else {
                // New pin: its source is the point of the image currently under the mouse (inverse of the mesh).
                const WarpMesh& wm = xf.mesh;
                bool found = false;
                float2 src = m;
                for (int y = 0; y < wm.ny && !found; ++y)
                    for (int x = 0; x < wm.nx && !found; ++x) {
                        int a = y * (wm.nx + 1) + x, b = a + 1, c = a + wm.nx + 1, d = c + 1;
                        for (auto tri : {std::array<int, 3>{a, b, d}, std::array<int, 3>{a, d, c}}) {
                            float2 p0 = wm.dst[tri[0]], p1 = wm.dst[tri[1]], p2 = wm.dst[tri[2]];
                            float den = (p1.y - p2.y) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.y - p2.y);
                            if (std::abs(den) < 1e-9f) continue;
                            float l0 = ((p1.y - p2.y) * (m.x - p2.x) + (p2.x - p1.x) * (m.y - p2.y)) / den;
                            float l1 = ((p2.y - p0.y) * (m.x - p2.x) + (p0.x - p2.x) * (m.y - p2.y)) / den;
                            float l2 = 1 - l0 - l1;
                            if (l0 >= -1e-4f && l1 >= -1e-4f && l2 >= -1e-4f) {
                                src = wm.src[tri[0]] * l0 + wm.src[tri[1]] * l1 + wm.src[tri[2]] * l2;
                                found = true;
                                break;
                            }
                        }
                    }
                if (found || wm.dst.empty()) {
                    xf.pinSrc.push_back(src);
                    xf.pinDst.push_back(m);
                    xf.pinSel = xf.pinDrag = (int)xf.pinDst.size() - 1;
                    xf.dragOffset = {0, 0};
                    xf.dirty = true;
                }
            }
        }
        if (xf.pinDrag >= 0 && down) {
            float2 p = m + xf.dragOffset;
            if (Length(p - xf.pinDst[xf.pinDrag]) > 1e-4f) {
                xf.pinDst[xf.pinDrag] = p;
                xf.dirty = true;
            }
        }
        if (!down) xf.pinDrag = -1;
        if (xf.pinSel >= 0 && xf.pinSel < (int)xf.pinDst.size() && !io.WantTextInput &&
            (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))) {
            xf.pinSrc.erase(xf.pinSrc.begin() + xf.pinSel);
            xf.pinDst.erase(xf.pinDst.begin() + xf.pinSel);
            xf.pinSel = -1;
            xf.dirty = true;
        }
        return;
    }
    // Perspective: points of the current phase.
    PerspectiveInitPlanes();
    std::vector<float2>& pts = xf.perspPhase == 0 ? xf.layoutPts : xf.warpPts;
    int hit = -1;
    for (int i = 0; i < (int)pts.size() && hit < 0; ++i) if (nearPt(pts[i])) hit = i;
    if (hovered && xf.ptDrag < 0) ImGui::SetMouseCursor(hit >= 0 ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_Arrow);
    if (click) {
        if (hit >= 0) {
            xf.ptDrag = hit;
            xf.dragOffset = pts[hit] - m;
        } else {
            for (size_t k = 0; k < xf.planes.size(); ++k) {
                float2 q[4];
                for (int c = 0; c < 4; ++c) q[c] = pts[xf.planes[k][c]];
                if (InQuad(q, m)) xf.planeSel = (int)k;
            }
        }
    }
    if (xf.ptDrag >= 0 && down) {
        float2 p = m + xf.dragOffset;
        if (Length(p - pts[xf.ptDrag]) > 1e-4f) {
            pts[xf.ptDrag] = p;
            if (xf.perspPhase == 0) xf.warpPts = xf.layoutPts;
            xf.dirty = true;
        }
    }
    if (!down) xf.ptDrag = -1;
}

void App::MeshWarpDrawOverlay(DocView& v, ImVec2 canvasMin, ImDrawList* dl) {
    auto toScreen = [&](float2 p) { return ImVec2(canvasMin.x + v.pan.x + p.x * v.zoom, canvasMin.y + v.pan.y + p.y * v.zoom); };
    if (xf.warpKind == 1) {
        const WarpMesh& wm = xf.mesh;
        if (xf.showMesh && !wm.dst.empty()) {
            int step = std::max(1, (int)std::ceil(12.0f / std::max(1e-3f, v.zoom * xf.src.Width() / wm.nx)));
            for (int y = 0; y <= wm.ny; y += step)
                for (int x = 0; x + step <= wm.nx; x += step)
                    dl->AddLine(toScreen(wm.dst[y * (wm.nx + 1) + x]), toScreen(wm.dst[y * (wm.nx + 1) + x + step]), IM_COL32(255, 255, 255, 70));
            for (int x = 0; x <= wm.nx; x += step)
                for (int y = 0; y + step <= wm.ny; y += step)
                    dl->AddLine(toScreen(wm.dst[y * (wm.nx + 1) + x]), toScreen(wm.dst[(y + step) * (wm.nx + 1) + x]), IM_COL32(255, 255, 255, 70));
        }
        for (size_t i = 0; i < xf.pinDst.size(); ++i) {
            ImVec2 p = toScreen(xf.pinDst[i]);
            bool sel = (int)i == xf.pinSel;
            dl->AddCircleFilled(p, 6.5f, IM_COL32(20, 20, 24, 255));
            dl->AddCircleFilled(p, 5.0f, sel ? ui::col::Accent2 : IM_COL32(250, 210, 60, 255));
            if (sel) dl->AddCircleFilled(p, 2.0f, IM_COL32(20, 20, 24, 255));
        }
        return;
    }
    if (xf.layoutPts.empty()) return;
    const std::vector<float2>& pts = xf.perspPhase == 0 ? xf.layoutPts : xf.warpPts;
    for (size_t k = 0; k < xf.planes.size(); ++k) {
        float2 q[4];
        for (int c = 0; c < 4; ++c) q[c] = pts[xf.planes[k][c]];
        bool sel = (int)k == xf.planeSel;
        // Inner grid (bilinear).
        for (int t = 1; t < 4; ++t) {
            float a = t / 4.0f;
            dl->AddLine(toScreen(q[0] + (q[1] - q[0]) * a), toScreen(q[3] + (q[2] - q[3]) * a), IM_COL32(255, 255, 255, 90));
            dl->AddLine(toScreen(q[0] + (q[3] - q[0]) * a), toScreen(q[1] + (q[2] - q[1]) * a), IM_COL32(255, 255, 255, 90));
        }
        ImVec2 s[4];
        for (int c = 0; c < 4; ++c) s[c] = toScreen(q[c]);
        dl->AddPolyline(s, 4, IM_COL32(0, 0, 0, 160), ImDrawFlags_Closed, 3.0f);
        dl->AddPolyline(s, 4, sel ? ui::col::Accent2 : IM_COL32(120, 180, 255, 255), ImDrawFlags_Closed, 1.5f);
        if (xf.perspPhase == 0) dl->AddQuadFilled(s[0], s[1], s[2], s[3], sel ? IM_COL32(120, 160, 255, 40) : IM_COL32(120, 160, 255, 18));
    }
    for (auto& p : pts) {
        ImVec2 c = toScreen(p);
        dl->AddRectFilled(ImVec2(c.x - 5, c.y - 5), ImVec2(c.x + 5, c.y + 5), IM_COL32_WHITE);
        dl->AddRect(ImVec2(c.x - 5, c.y - 5), ImVec2(c.x + 5, c.y + 5), IM_COL32(20, 20, 24, 255));
    }
}

void App::MeshWarpOptions() {
    if (xf.warpKind == 1) {
        if (ImGui::Combo("Mode", &xf.puppetMode, "Rigide\0Normal\0Distorsion\0")) xf.dirty = true;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Rigide : l'image ne s'étire pas (rotations locales).\nNormal : échelle locale autorisée.\nDistorsion : transformation affine libre.");
        if (ImGui::Combo("Densité", &xf.puppetDensity, "Moins de points\0Normale\0Plus de points\0")) xf.dirty = true;
        ImGui::Checkbox("Afficher le maillage", &xf.showMesh);
        ImGui::Text("%d épingle(s)", (int)xf.pinDst.size());
        ImGui::TextDisabled("Clic : poser une épingle. Glisser : déplacer.\nAlt+clic ou Suppr : retirer l'épingle.\nPosez des épingles fixes pour retenir l'image.");
        if (ImGui::Button("Retirer toutes les épingles")) {
            xf.pinSrc.clear();
            xf.pinDst.clear();
            xf.pinSel = -1;
            xf.dirty = true;
        }
        return;
    }
    PerspectiveInitPlanes();
    if (xf.perspPhase == 0) {
        ImGui::TextDisabled("Disposition : placez les coins des plans\nsur les faces de l'image (façades, sol...).");
        auto& pl = xf.planes[std::clamp(xf.planeSel, 0, (int)xf.planes.size() - 1)];
        auto mid = [&](int a, int b) {
            xf.layoutPts.push_back((xf.layoutPts[a] + xf.layoutPts[b]) * 0.5f);
            return (int)xf.layoutPts.size() - 1;
        };
        if (ImGui::Button("Scinder verticalement")) {
            std::array<int, 4> p = pl;
            int m1 = mid(p[0], p[1]), m2 = mid(p[3], p[2]);
            xf.planes[xf.planeSel] = {p[0], m1, m2, p[3]};
            xf.planes.push_back({m1, p[1], p[2], m2});
            xf.warpPts = xf.layoutPts;
            xf.dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Scinder horizontalement")) {
            std::array<int, 4> p = pl;
            int m1 = mid(p[0], p[3]), m2 = mid(p[1], p[2]);
            xf.planes[xf.planeSel] = {p[0], p[1], m2, m1};
            xf.planes.push_back({m1, m2, p[2], p[3]});
            xf.warpPts = xf.layoutPts;
            xf.dirty = true;
        }
        if (ImGui::Button("Ajouter un plan à droite")) {
            std::array<int, 4> p = pl;
            float2 dx = (xf.layoutPts[p[1]] - xf.layoutPts[p[0]]) * 0.5f;
            xf.layoutPts.push_back(xf.layoutPts[p[1]] + dx);
            int a = (int)xf.layoutPts.size() - 1;
            xf.layoutPts.push_back(xf.layoutPts[p[2]] + dx);
            int b = (int)xf.layoutPts.size() - 1;
            xf.planes.push_back({p[1], a, b, p[2]});
            xf.planeSel = (int)xf.planes.size() - 1;
            xf.warpPts = xf.layoutPts;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(xf.planes.size() < 2);
        if (ImGui::Button("Supprimer le plan")) {
            xf.planes.erase(xf.planes.begin() + xf.planeSel);
            xf.planeSel = 0;
        }
        ImGui::EndDisabled();
        if (ImGui::Button("Déformer >", ImVec2(-1, 0))) {
            xf.perspPhase = 1;
            xf.warpPts = xf.layoutPts;
            xf.dirty = true;
        }
        return;
    }
    ImGui::TextDisabled("Déformation : déplacez les coins,\nl'image suit chaque plan en perspective.");
    auto straighten = [&](bool vertical) {
        // Edges close to vertical (or horizontal) become exactly so; shared points stay shared.
        for (auto& pl : xf.planes)
            for (int c = 0; c < 4; ++c) {
                int a = pl[c], b = pl[(c + 1) % 4];
                float2 d = xf.warpPts[b] - xf.warpPts[a];
                bool isV = std::abs(d.x) < std::abs(d.y);
                if (isV != vertical) continue;
                if (vertical) { float x = (xf.warpPts[a].x + xf.warpPts[b].x) * 0.5f; xf.warpPts[a].x = xf.warpPts[b].x = x; }
                else { float y = (xf.warpPts[a].y + xf.warpPts[b].y) * 0.5f; xf.warpPts[a].y = xf.warpPts[b].y = y; }
            }
        xf.dirty = true;
    };
    if (ImGui::Button("Redresser les verticales")) straighten(true);
    ImGui::SameLine();
    if (ImGui::Button("Redresser les horizontales")) straighten(false);
    if (ImGui::Button("Réinitialiser")) {
        xf.warpPts = xf.layoutPts;
        xf.dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("< Disposition")) {
        xf.perspPhase = 0;
        xf.warpPts = xf.layoutPts;
        xf.dirty = true;
    }
}

}  // namespace sx
