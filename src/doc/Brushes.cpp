#include "doc/Brushes.h"
#include "gpu/ShaderSystem.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace sx {

namespace {
BrushLibrary g_brushes;
}

BrushLibrary& Brushes() { return g_brushes; }

const char* kBrushTemplate =
    "//@name Mon pinceau\n"
    "//@category Personnalisé\n"
    "//@spacing 0.1\n"
    "//@param Grain float 0.3 0 1 \"Grain\"\n"
    "\n"
    "// Pointe : couleur (rgb) et couverture (a) d'un pixel de la touche.\n"
    "// b.uv (0-1 dans la touche), b.local (-1..1), b.pressure, b.color, b.hardness,\n"
    "// b.radius, b.angle, b.seed (aléa de la touche), b.time, b.distance, b.direction...\n"
    "float4 Tip(Brush b)\n"
    "{\n"
    "    float d = length(b.local);\n"
    "    float shape = 1.0 - smoothstep(b.hardness, 1.0, d);\n"
    "    float grain = lerp(1.0, Hash1(dot(floor(b.px), float2(0.123, 0.789)) + b.seed), Grain);\n"
    "    return float4(b.color, shape * grain);\n"
    "}\n"
    "\n"
    "// Facultatif : comment chaque touche est posée (décommentez ce qui vous intéresse).\n"
    "// //@extent 2 si les touches peuvent s'écarter du trait ou grossir.\n"
    "// //@airbrush 30 pour peindre en continu (touches par seconde) quand on reste appuyé.\n"
    "void Dynamics(inout Dab d)\n"
    "{\n"
    "    // d.angle = atan2(d.direction.y, d.direction.x);      // suit le mouvement\n"
    "    // d.pos += (Rand2(d.seed) - 0.5) * d.radius;          // dispersion\n"
    "    // d.radius *= lerp(0.6, 1.2, Rand(d.seed, 1));        // taille aléatoire\n"
    "    // d.opacity *= lerp(0.5, 1.0, Rand(d.seed, 2));       // opacité aléatoire\n"
    "    // d.angle += d.time * 6.0;                             // rotation dans le temps\n"
    "}\n";

BrushDefPtr ParseBrush(const std::string& id, const std::string& text) {
    auto b = std::make_shared<BrushDef>();
    b->id = id;
    b->text = text;
    b->def = ParseEffect(id, text);
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        if (line.rfind("//@", 0) != 0) continue;
        std::istringstream t(line.substr(3));
        std::string key;
        t >> key;
        if (key == "spacing") t >> b->spacing;
        else if (key == "airbrush") t >> b->airbrush;
        else if (key == "extent") t >> b->extent;
        else if (key == "selfflow") b->selfFlow = true;
    }
    b->extent = std::clamp(b->extent, 1.0f, 16.0f);
    b->dynamics = text.find("Dynamics(") != std::string::npos;
    return b;
}

std::string BuildBrushSource(const BrushDef& b) {
    const EffectDef& def = *b.def;
    std::string s = gpu::Shaders().File("brush_common.slang");
    s += "\ncbuffer BrushData : register(b1)\n{\n";
    s += "    float4 gColor;          // brush color, linear straight\n";
    s += "    float4 gBrushSettings;  // opacity, hardness, flow\n";
    s += EffectParamFields(def);
    s += "    float4 gDabs[" + std::to_string(kMaxBrushDabs * 2) + "];\n};\n\n";
    if (b.dynamics) s += "#define SX_DYNAMICS 1\n";
    if (b.selfFlow) s += "#define SX_SELFFLOW 1\n";
    s += "#line 1\n";
    s += def.body;
    s += R"(

Dab SXLoadDab(uint i)
{
    float4 a = gDabs[i * 2], e = gDabs[i * 2 + 1];
    Dab d;
    d.pos = a.xy;
    d.radius = a.z;
    d.opacity = a.w;
    d.pressure = e.x;
    d.direction = float2(cos(e.y), sin(e.y));
    d.time = e.z;
    d.distance = e.w;
    d.index = gDabBase + i;
    d.seed = Hash1(float(d.index) * 0.6180339 + 0.5);
    d.angle = 0.0;
    d.color = gColor.rgb;
    return d;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= gRectSize)) return;
    int2 px = gRectOrigin + int2(id.xy);
    if (any(px < 0) || any(px >= int2(gDocSize))) return;
    float2 p = float2(px) + 0.5;
    float m = gStroke[px];
    float4 acc = gStrokeColor[px];
    for (uint i = 0; i < gDabCount; ++i)
    {
        Dab d = SXLoadDab(i);
#ifdef SX_DYNAMICS
        Dynamics(d);
#endif
        float r = max(d.radius, 0.5);
        float2 l = Rotate(p - d.pos, -d.angle) / r;
        if (abs(l.x) > 1.0 || abs(l.y) > 1.0) continue;
        Brush b;
        b.uv = l * 0.5 + 0.5;
        b.local = l;
        b.px = p;
        b.docSize = float2(gDocSize);
        b.pressure = d.pressure;
        b.color = d.color;
        b.hardness = gHardness;
        b.flow = gFlow;
        b.radius = r;
        b.angle = d.angle;
        b.seed = d.seed;
        b.index = d.index;
        b.time = d.time;
        b.distance = d.distance;
        b.direction = d.direction;
        float4 t = Tip(b);
#ifdef SX_SELFFLOW
        float c = saturate(t.a);
#else
        float c = saturate(t.a) * saturate(d.opacity);
#endif
        acc = acc * (1.0 - c) + float4(max(t.rgb, 0.0), 1.0) * c;
        m = m + (1.0 - m) * c;
    }
    gStroke[px] = m;
    gStrokeColor[px] = acc;
    float4 pre = (gFlags & 2) ? float4(0, 0, 0, 0) : gPre.Load(int3(px, 0));
    float k = saturate(m * gOpacity);
    if (gFlags & 1) k *= gSel.Load(int3(px, 0));
    float3 paint = acc.a > 1e-5 ? acc.rgb / acc.a : gColor.rgb;
    float4 result;
    if (gMode == 1)
        result = pre * (1.0 - k);
    else
        result = float4(paint * gColor.a, gColor.a) * k + pre * (1.0 - k * gColor.a);
    gLayer[px] = result;
}
)";
    return s;
}

void BrushLibrary::Load(const std::string& dir_) {
    dir = dir_;
    brushes.clear();
    std::error_code ec;
    std::vector<fs::path> paths;
    for (auto& e : fs::directory_iterator(Utf8Path(dir), ec))
        if (e.path().extension() == ".slang") paths.push_back(e.path());
    std::sort(paths.begin(), paths.end());
    for (auto& p : paths)
        brushes.push_back(ParseBrush(Narrow(p.stem().wstring()), gpu::Shaders().File("brushes/" + Narrow(p.filename().wstring()))));
    shaderGeneration = gpu::Shaders().FileGeneration();
    Log("Brush library: %zu brushes", brushes.size());
}

void BrushLibrary::ReloadIfChanged() {
    if (!dir.empty() && gpu::Shaders().FileGeneration() != shaderGeneration) Load(dir);
}

BrushDefPtr BrushLibrary::Find(const std::string& id) const {
    for (auto& b : brushes)
        if (b->id == id) return b;
    return nullptr;
}

}  // namespace sx
