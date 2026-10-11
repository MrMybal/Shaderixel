// Example native plugin for Shaderixel (sdk/include/shaderixel_plugin.h).
//
// - a menu command (Plugins > Exemple > Dire bonjour);
// - a window (Plugins > Fenêtres > Exemple de plugin): document size, layer count, a button adding a
//   blur filter to the active layer, star settings;
// - a canvas tool "Étoile": a click paints a star (preview while hovering);
// - an operation "exemple.etoile" ({x, y, radius}) for the assistant, actions and other plugins.
//
// Built with Shaderixel (CMake target ShaderixelExamplePlugin) into <exe>/plugins/Exemple/ next to plugin.json.
#include "shaderixel_plugin.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

const SxHost* H = nullptr;

struct State {
    float radius = 60.0f;
    int branches = 5;
    float color[4] = {1.0f, 0.75f, 0.1f, 1.0f};
    float brushSize = 8.0f;
    std::string info = "(cliquez sur Analyser)";
    std::string opResult;
} S;

// Number after "key": in a JSON text (enough for this example; use a JSON library in a real plugin).
double JsonNumber(const char* json, const char* key, double def) {
    std::string k = std::string("\"") + key + "\":";
    const char* p = json ? std::strstr(json, k.c_str()) : nullptr;
    return p ? std::atof(p + k.size()) : def;
}
int CountOf(const char* json, const char* needle) {
    int n = 0;
    for (const char* p = json; p && (p = std::strstr(p, needle)); p += std::strlen(needle)) ++n;
    return n;
}

// Points of a star polyline (closed), as JSON [[x, y], ...].
std::string StarPoints(float cx, float cy, float r, int branches) {
    std::string pts = "[";
    const int n = branches * 2;
    for (int i = 0; i <= n; ++i) {
        const float a = -1.5707963f + 3.14159265f * i / branches;
        const float rr = (i % 2) ? r * 0.45f : r;
        char b[64];
        std::snprintf(b, sizeof(b), "%s[%.1f,%.1f]", i ? "," : "", cx + std::cos(a) * rr, cy + std::sin(a) * rr);
        pts += b;
    }
    return pts + "]";
}

void PaintStar(float x, float y, float r, SxResultFn done, void* user) {
    char params[4096];
    std::snprintf(params, sizeof(params), "{\"points\":%s,\"size\":%.1f,\"hardness\":0.9,\"color\":[%.3f,%.3f,%.3f,1]}",
                  StarPoints(x, y, r, S.branches).c_str(), S.brushSize, S.color[0], S.color[1], S.color[2]);
    H->call("shaderixel.paint_stroke", params, done, user);
}

// ---------------------------------------------------------------- command
void SayHello(void*) {
    int w = 0, h = 0;
    char msg[128];
    if (H->document_size(&w, &h)) std::snprintf(msg, sizeof(msg), "Bonjour depuis le plugin Exemple ! Document : %d x %d", w, h);
    else std::snprintf(msg, sizeof(msg), "Bonjour depuis le plugin Exemple ! (aucun document)");
    H->status(msg);
    H->log(msg);
}

// ---------------------------------------------------------------- window
void OnInfo(void*, int ok, const char* json) {
    if (!ok) { S.info = std::string("Erreur : ") + json; return; }
    char b[160];
    std::snprintf(b, sizeof(b), "%d x %d px, %d calque(s), calque actif %d", (int)JsonNumber(json, "width", 0), (int)JsonNumber(json, "height", 0),
                  CountOf(json, "\"kind\":"), (int)JsonNumber(json, "activeLayer", 0));
    S.info = b;
}
void OnBlurInfo(void*, int ok, const char* json) {
    if (!ok) { H->status("Exemple : pas de document"); return; }
    char params[160];
    std::snprintf(params, sizeof(params), "{\"layer\":%d,\"effect\":\"gaussian_blur\"}", (int)JsonNumber(json, "activeLayer", 0));
    H->call("shaderixel.filter_add", params, [](void*, int ok2, const char* r) { H->status(ok2 ? "Exemple : flou ajouté au calque actif" : r); }, nullptr);
}

void DrawPanel(void*) {
    const SxUi* ui = H->ui;
    ui->separator("Document");
    if (ui->button("Analyser")) H->call("shaderixel.document_info", "{}", OnInfo, nullptr);
    ui->tooltip("Appelle l'opération shaderixel.document_info.");
    ui->same_line();
    ui->text_disabled(S.info.c_str());
    if (ui->button("Flou sur le calque actif")) H->call("shaderixel.document_info", "{}", OnBlurInfo, nullptr);
    ui->tooltip("Ajoute un filtre non destructif (shaderixel.filter_add).");
    ui->separator("Outil Étoile");
    ui->slider_float("Rayon", &S.radius, 5, 400, "%.0f px");
    ui->slider_int("Branches", &S.branches, 3, 12);
    ui->slider_float("Épaisseur", &S.brushSize, 1, 60, "%.0f px");
    ui->color_edit("Couleur", S.color);
    if (ui->button("Choisir l'outil Étoile")) H->select_tool("Étoile");
    ui->text_wrapped("Un clic sur le canevas peint une étoile sur le calque de pixels actif (aperçu au survol).");
}

// ---------------------------------------------------------------- tool
void StarTool(void*, const SxToolEvent* e) {
    if (e->type == SX_TOOL_PRESS) {
        PaintStar(e->x, e->y, S.radius * (0.5f + 0.5f * e->pressure), [](void*, int ok, const char* r) { if (!ok) H->status(r); }, nullptr);
    } else if (e->type == SX_TOOL_DRAW) {
        // Preview: the star outline at the cursor.
        const int n = S.branches * 2;
        float px = 0, py = 0;
        for (int i = 0; i <= n; ++i) {
            const float a = -1.5707963f + 3.14159265f * i / S.branches;
            const float rr = (i % 2) ? S.radius * 0.45f : S.radius;
            const float x = e->x + std::cos(a) * rr, y = e->y + std::sin(a) * rr;
            if (i) H->draw_line(px, py, x, y, 0xC000C8FFu, 1.5f);
            px = x;
            py = y;
        }
    }
}

// ---------------------------------------------------------------- operation
const char* StarOperation(void*, const char* params) {
    const float x = (float)JsonNumber(params, "x", 100), y = (float)JsonNumber(params, "y", 100), r = (float)JsonNumber(params, "radius", S.radius);
    S.opResult = "{\"ok\":true}";
    PaintStar(x, y, r, [](void*, int ok, const char* res) { if (!ok) H->log(res); }, nullptr);
    return S.opResult.c_str();
}

void Unload(void*) { H->log("Exemple : déchargé"); }

}  // namespace

extern "C" SX_PLUGIN_EXPORT int sx_plugin_load(const SxHost* host, SxPlugin* plugin) {
    if (!host || host->api_version < SX_PLUGIN_API_VERSION) return 0;
    H = host;
    plugin->unload = Unload;
    host->add_command("Exemple/Dire bonjour", SayHello, nullptr);
    host->add_panel("Exemple de plugin", DrawPanel, nullptr);
    host->add_tool("Étoile", "Clic : peint une étoile (réglages dans la fenêtre Exemple de plugin)", StarTool, nullptr);
    host->add_operation("exemple.etoile", "Peint une étoile sur le calque de pixels actif (plugin Exemple).",
                        "{\"type\":\"object\",\"properties\":{\"x\":{\"type\":\"number\"},\"y\":{\"type\":\"number\"},\"radius\":{\"type\":\"number\"}},\"required\":[\"x\",\"y\"]}",
                        1, StarOperation, nullptr);
    host->log("Exemple : chargé");
    return 1;
}
