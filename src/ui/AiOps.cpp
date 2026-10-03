// Operations exposed to AI agents through the CyToolsCore bridge (MCP tools).
#include "ui/App.h"
#include "doc/EffectGraph.h"
#include "gpu/ShaderSystem.h"

#include <stb_image.h>
#include <stb_image_write.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace sx {

namespace {
const char* kBlendKeys[] = {"normal", "multiply", "screen", "overlay", "linear_dodge", "subtract", "darken", "lighten", "difference",
                            "color_dodge", "color_burn", "hard_light", "soft_light", "dissolve", "linear_burn", "vivid_light",
                            "linear_light", "pin_light", "hard_mix", "exclusion", "divide", "hue", "saturation", "color",
                            "luminosity", "darker_color", "lighter_color"};
const char* kKindKeys[] = {"pixel", "fill", "adjustment", "group", "text", "shape"};

int BlendFromKey(const std::string& k) {
    for (int i = 0; i < (int)BlendMode::Count; ++i) if (k == kBlendKeys[i]) return i;
    if (k == "add") return (int)BlendMode::LinearDodge;
    return -1;
}

Json Arr(std::initializer_list<float> v) {
    Json a = Json::Array();
    for (float x : v) a.Push(x);
    return a;
}

Json ParamValueJson(const ParamDef& p, const float4& v) {
    switch (p.type) {
        case ParamType::Color: return Arr({LinearToSrgb(v.x), LinearToSrgb(v.y), LinearToSrgb(v.z), v.w});
        case ParamType::Float2: return Arr({v.x, v.y});
        case ParamType::Float3: return Arr({v.x, v.y, v.z});
        case ParamType::Float4: return Arr({v.x, v.y, v.z, v.w});
        case ParamType::Bool: return Json(v.x > 0.5f);
        case ParamType::Curve: return Json("curve");
        case ParamType::Lut: return Json("lut");
        default: return Json(v.x);
    }
}

bool ApplyParamValue(const ParamDef& p, const Json& j, float4& v) {
    switch (p.type) {
        case ParamType::Color: {
            if (!j.IsArray() || j.Size() < 3) return false;
            v = {SrgbToLinear(j.At(0).F()), SrgbToLinear(j.At(1).F()), SrgbToLinear(j.At(2).F()), j.Size() > 3 ? j.At(3).F(1) : 1.0f};
            return true;
        }
        case ParamType::Float2:
        case ParamType::Float3:
        case ParamType::Float4:
            if (!j.IsArray() || j.Size() < 2) return false;
            for (size_t k = 0; k < j.Size() && k < 4; ++k) v[(int)k] = j.At(k).F();
            return true;
        case ParamType::Enum:
            if (j.GetType() == Json::Type::String) {
                for (size_t i = 0; i < p.enumItems.size(); ++i) if (p.enumItems[i] == j.S()) { v.x = (float)i; return true; }
                return false;
            }
            v.x = j.F();
            return true;
        case ParamType::Curve:
        case ParamType::Lut: return false;
        default: v.x = j.F(); return true;
    }
}

Json EffectJson(const EffectInstance& fx) {
    Json o = Json::Object();
    auto def = fx.Def();
    o["effect"] = fx.effectId.empty() ? (fx.graph.empty() ? "custom" : "graph") : fx.effectId;
    if (def) o["name"] = def->name;
    o["enabled"] = fx.enabled;
    Json params = Json::Object();
    if (def)
        for (size_t i = 0; i < def->params.size(); ++i) params[def->params[i].id] = ParamValueJson(def->params[i], i < fx.params.size() ? fx.params[i] : def->params[i].def);
    o["params"] = params;
    return o;
}

std::string ApplyParams(EffectInstance& fx, const Json& params) {
    auto def = fx.Def();
    if (!def || !params.IsObject()) return "";
    if (fx.params.size() < def->params.size()) fx.params.resize(def->params.size());
    std::string unknown;
    for (auto& [k, v] : params.Items()) {
        bool found = false;
        for (size_t i = 0; i < def->params.size(); ++i)
            if (def->params[i].id == k || def->params[i].label == k) {
                found = ApplyParamValue(def->params[i], v, fx.params[i]);
                break;
            }
        if (!found) unknown += (unknown.empty() ? "" : ", ") + k;
    }
    return unknown;
}

Json LayerJson(const Layer& l) {
    Json o = Json::Object();
    o["id"] = l.id;
    o["name"] = l.name;
    o["kind"] = kKindKeys[(int)l.kind];
    o["visible"] = l.visible;
    o["opacity"] = l.opacity;
    o["blend"] = l.kind == LayerKind::Group && l.passThrough ? "pass_through" : kBlendKeys[(int)l.blend];
    if (l.parent) o["parent"] = l.parent;
    if (l.clipped) o["clipped"] = true;
    if (l.locked) o["locked"] = true;
    if (l.mask) o["mask"] = true;
    if (l.kind == LayerKind::Fill || l.kind == LayerKind::Adjustment) o["effect"] = EffectJson(l.effect);
    if (!l.filters.empty()) {
        Json f = Json::Array();
        for (auto& x : l.filters) f.Push(EffectJson(x));
        o["filters"] = f;
    }
    if (l.kind == LayerKind::Text) {
        Json t = Json::Object();
        t["content"] = l.text.content;
        t["size"] = l.text.size;
        t["color"] = Arr({l.text.color.x, l.text.color.y, l.text.color.z, l.text.color.w});
        t["x"] = l.text.pos.x;
        t["y"] = l.text.pos.y;
        t["align"] = l.text.align;
        t["font"] = l.text.font;
        o["text"] = t;
    }
    if (l.kind == LayerKind::Shape) {
        Json s = Json::Object();
        s["type"] = l.shape.type;
        s["rect"] = Arr({l.shape.rect.x, l.shape.rect.y, l.shape.rect.z, l.shape.rect.w});
        o["shape"] = s;
    }
    return o;
}

Json Schema(std::initializer_list<std::pair<const char*, Json>> props, std::initializer_list<const char*> required = {}) {
    Json s = Json::Object();
    s["type"] = "object";
    Json p = Json::Object();
    for (auto& [k, v] : props) p[k] = v;
    s["properties"] = p;
    Json r = Json::Array();
    for (auto* k : required) r.Push(k);
    s["required"] = r;
    return s;
}

Json T(const char* type, const char* desc = nullptr) {
    Json j = Json::Object();
    j["type"] = type;
    if (desc) j["description"] = desc;
    return j;
}

Json ColorT(const char* desc) {
    Json j = Json::Object();
    j["type"] = "array";
    j["items"] = T("number");
    j["description"] = std::string(desc) + " [r, g, b, a] en sRGB 0..1";
    return j;
}

float4 ColorFrom(const Json& j, float4 def) {
    if (!j.IsArray() || j.Size() < 3) return def;
    return {j.At(0).F(), j.At(1).F(), j.At(2).F(), j.Size() > 3 ? j.At(3).F(1) : 1.0f};
}
}  // namespace

void App::RegisterAiOps() {
    auto need = [this](AiResult& err) -> DocView* {
        DocView* v = Active();
        if (!v) err = AiResult::Fail("NoDocument", "Aucun document ouvert dans Shaderixel");
        return v;
    };

    bridge.Register({"shaderixel.document_info", "Décrit le document actif : taille, mode, calques (ids, types, modes de fusion, effets et paramètres).",
                     Schema({}), false, {}, [this, need](const Json&, const std::string&) {
                         AiResult e;
                         DocView* v = need(e);
                         if (!v) return e;
                         Document& d = *v->doc;
                         Json j = Json::Object();
                         j["name"] = d.Name();
                         j["width"] = d.Width();
                         j["height"] = d.Height();
                         j["mode"] = d.Mode() == DocMode::Layers ? "layers" : "nodes";
                         j["precision"] = d.GetPrecision() == Precision::Float32 ? "float32" : "half16";
                         j["activeLayer"] = d.S().activeLayer;
                         j["hasSelection"] = d.S().selection != 0;
                         Json layers = Json::Array();
                         for (int i = (int)d.S().layers.size() - 1; i >= 0; --i) layers.Push(LayerJson(d.S().layers[i]));
                         j["layers"] = layers;  // top first
                         j["blendModes"] = [] { Json a = Json::Array(); for (auto* k : kBlendKeys) a.Push(k); return a; }();
                         return AiResult::Ok(j);
                     }});

    bridge.Register({"shaderixel.effects_list", "Liste les effets disponibles (id, nom, catégorie, entrées, paramètres avec types et bornes).",
                     Schema({}), false, {}, [](const Json&, const std::string&) {
                         Json arr = Json::Array();
                         for (auto& e : Effects().All()) {
                             Json o = Json::Object();
                             o["id"] = e->id;
                             o["name"] = e->name;
                             o["category"] = e->category;
                             o["generator"] = e->inputs.empty();
                             Json ps = Json::Array();
                             static const char* types[] = {"float", "int", "bool", "color", "float2", "enum", "curve"};
                             for (auto& p : e->params) {
                                 Json q = Json::Object();
                                 q["id"] = p.id;
                                 q["label"] = p.label;
                                 q["type"] = types[(int)p.type];
                                 q["default"] = ParamValueJson(p, p.def);
                                 if (p.type == ParamType::Float || p.type == ParamType::Int) { q["min"] = p.min; q["max"] = p.max; }
                                 if (!p.enumItems.empty()) { Json it = Json::Array(); for (auto& s : p.enumItems) it.Push(s); q["items"] = it; }
                                 ps.Push(q);
                             }
                             o["params"] = ps;
                             arr.Push(o);
                         }
                         Json j = Json::Object();
                         j["effects"] = arr;
                         return AiResult::Ok(j);
                     }});

    bridge.Register({"shaderixel.render", "Rend l'image composite du document en PNG (fichier dans l'espace de travail du job).",
                     Schema({{"file", T("string", "nom du fichier (défaut render.png)")}}), false, {},
                     [this, need](const Json& p, const std::string& workspace) {
                         AiResult e;
                         DocView* v = need(e);
                         if (!v) return e;
                         gpu::Texture* img = v->renderer.EvaluateComplete(*v->doc, *ctx);
                         if (!img) return AiResult::Fail("NotReady", "Le rendu n'est pas encore prêt, réessayez");
                         auto rb = v->renderer.ExportRGBA8(*ctx, img);
                         std::string name = p["file"].S("render.png");
                         if (name.find_first_of("/\\:") != std::string::npos) name = "render.png";
                         std::string dir = workspace.empty() ? Narrow(fs::temp_directory_path().wstring()) : workspace;
                         std::string path = Narrow((Utf8Path(dir) / Utf8Path(name)).wstring());
                         AiResult r;
                         r.done = false;
                         r.poll = [rb, path](AiResult& out) {
                             if (!rb || !rb->ready) return false;
                             if (!stbi_write_png(path.c_str(), rb->width, rb->height, 4, rb->data.data(), rb->width * 4)) {
                                 out.errorCode = "WriteFailed";
                                 out.errorMessage = "écriture impossible : " + path;
                                 return true;
                             }
                             Json j = Json::Object();
                             j["file"] = path;
                             j["width"] = rb->width;
                             j["height"] = rb->height;
                             out.result = j;
                             return true;
                         };
                         return r;
                     }});

    bridge.Register({"shaderixel.document_new", "Crée un nouveau document.",
                     Schema({{"width", T("integer")}, {"height", T("integer")}, {"name", T("string")},
                             {"mode", T("string", "layers (défaut) ou nodes")}, {"background", T("string", "white, transparent ou black")}},
                            {"width", "height"}),
                     true, {}, [this](const Json& p, const std::string&) {
                         int w = std::clamp(p["width"].I(1024), 1, 16384), h = std::clamp(p["height"].I(1024), 1, 16384);
                         std::string bgs = p["background"].S("white");
                         NewDocument((u32)w, (u32)h, Precision::Half16, p["mode"].S("") == "nodes" ? DocMode::Nodes : DocMode::Layers,
                                     bgs == "transparent" ? 1 : bgs == "black" ? 2 : 0, p["name"].S("Document IA"));
                         Json j = Json::Object();
                         j["width"] = w;
                         j["height"] = h;
                         return AiResult::Ok(j);
                     }});

    bridge.Register({"shaderixel.layer_add",
                     "Ajoute un calque au-dessus du calque actif. kind: pixel, fill (générateur), adjustment (réglage), text, shape, group. "
                     "effect = id d'effet (voir effects_list), params = {id: valeur}.",
                     Schema({{"kind", T("string")}, {"name", T("string")}, {"effect", T("string")}, {"params", T("object")},
                             {"blend", T("string")}, {"opacity", T("number")},
                             {"text", T("object", "{content, size, color[r,g,b,a], x, y, align(0-2), font}")},
                             {"shape", T("object", "{type(0 rect,1 ellipse,2 line,3 polygon,4 star), rect[x0,y0,x1,y1], fill[...], stroke[...], strokeWidth, radius, sides}")}},
                            {"kind"}),
                     true, {}, [this, need](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = need(e);
                         if (!v) return e;
                         Document& d = *v->doc;
                         if (d.Mode() != DocMode::Layers) return AiResult::Fail("Unsupported", "Document en mode nodal");
                         std::string kind = p["kind"].S("pixel");
                         Edit edit(d, v->history, "IA : ajouter un calque");
                         Id id = 0;
                         std::string unknown;
                         if (kind == "pixel") id = d.AddPixelLayer(p["name"].S("Calque IA")).id;
                         else if (kind == "group") {
                             Layer& l = d.AddPixelLayer("tmp");
                             Id pid = l.id;
                             Layer& g = d.GroupLayer(pid);
                             id = g.id;
                             d.RemoveLayer(pid);
                             if (Layer* gl = d.FindLayer(id)) gl->name = p["name"].S("Groupe IA");
                         } else if (kind == "fill" || kind == "adjustment") {
                             std::string eff = p["effect"].S("");
                             if (!Effects().Find(eff)) return AiResult::Fail("InvalidParameters", "effet inconnu : " + eff);
                             Layer& l = d.AddEffectLayer(kind == "fill" ? LayerKind::Fill : LayerKind::Adjustment, eff);
                             unknown = ApplyParams(l.effect, p["params"]);
                             if (p.Has("name")) l.name = p["name"].S();
                             id = l.id;
                         } else if (kind == "text") {
                             const Json& t = p["text"];
                             TextStyle s;
                             s.content = t["content"].S("Texte");
                             s.size = t["size"].F(72);
                             s.color = ColorFrom(t["color"], {0, 0, 0, 1});
                             s.pos = {t["x"].F(d.Width() * 0.1f), t["y"].F(d.Height() * 0.1f)};
                             s.align = t["align"].I(0);
                             s.font = t["font"].S("");
                             id = d.AddTextLayer(s).id;
                         } else if (kind == "shape") {
                             const Json& t = p["shape"];
                             ShapeStyle s;
                             s.type = t["type"].I(0);
                             s.rect = {d.Width() * 0.25f, d.Height() * 0.25f, d.Width() * 0.75f, d.Height() * 0.75f};
                             if (t["rect"].IsArray() && t["rect"].Size() == 4) s.rect = {t["rect"].At(0).F(), t["rect"].At(1).F(), t["rect"].At(2).F(), t["rect"].At(3).F()};
                             s.fill = ColorFrom(t["fill"], s.fill);
                             s.strokeOn = t.Has("stroke");
                             s.stroke = ColorFrom(t["stroke"], s.stroke);
                             s.strokeWidth = t["strokeWidth"].F(s.strokeWidth);
                             s.radius = t["radius"].F(s.radius);
                             s.sides = t["sides"].I(s.sides);
                             id = d.AddShapeLayer(s).id;
                         } else {
                             return AiResult::Fail("InvalidParameters", "kind inconnu : " + kind);
                         }
                         if (Layer* l = d.FindLayer(id)) {
                             if (p.Has("blend")) { int b = BlendFromKey(p["blend"].S("")); if (b >= 0) l->blend = (BlendMode)b; }
                             if (p.Has("opacity")) l->opacity = std::clamp(p["opacity"].F(1), 0.0f, 1.0f);
                             if (p.Has("name") && kind != "group") l->name = p["name"].S();
                         }
                         d.SyncGraph();
                         edit.Commit();
                         Json j = Json::Object();
                         j["id"] = id;
                         if (!unknown.empty()) j["warning"] = "paramètres inconnus : " + unknown;
                         return AiResult::Ok(j);
                     }});

    bridge.Register({"shaderixel.layer_update",
                     "Modifie un calque : name, visible, opacity, blend, clipped, params (de l'effet), text, shape, move (+1 monter / -1 descendre).",
                     Schema({{"id", T("integer")}, {"name", T("string")}, {"visible", T("boolean")}, {"opacity", T("number")},
                             {"blend", T("string")}, {"clipped", T("boolean")}, {"params", T("object")}, {"text", T("object")},
                             {"shape", T("object")}, {"move", T("integer")}},
                            {"id"}),
                     true, {}, [this, need](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = need(e);
                         if (!v) return e;
                         Document& d = *v->doc;
                         Layer* l = d.FindLayer(p["id"].U());
                         if (!l) return AiResult::Fail("InvalidParameters", "calque introuvable");
                         Edit edit(d, v->history, "IA : modifier le calque");
                         std::string unknown;
                         if (p.Has("name")) l->name = p["name"].S();
                         if (p.Has("visible")) l->visible = p["visible"].B();
                         if (p.Has("opacity")) l->opacity = std::clamp(p["opacity"].F(1), 0.0f, 1.0f);
                         if (p.Has("clipped")) l->clipped = p["clipped"].B();
                         if (p.Has("blend")) {
                             std::string b = p["blend"].S("");
                             if (b == "pass_through" && l->kind == LayerKind::Group) l->passThrough = true;
                             else if (int bi = BlendFromKey(b); bi >= 0) { l->blend = (BlendMode)bi; l->passThrough = false; }
                         }
                         if (p.Has("params")) unknown = ApplyParams(l->effect, p["params"]);
                         if (p.Has("text") && l->kind == LayerKind::Text) {
                             const Json& t = p["text"];
                             if (t.Has("content")) l->text.content = t["content"].S();
                             if (t.Has("size")) l->text.size = t["size"].F();
                             if (t.Has("color")) l->text.color = ColorFrom(t["color"], l->text.color);
                             if (t.Has("x")) l->text.pos.x = t["x"].F();
                             if (t.Has("y")) l->text.pos.y = t["y"].F();
                             if (t.Has("align")) l->text.align = t["align"].I();
                             if (t.Has("font")) l->text.font = t["font"].S();
                         }
                         if (p.Has("shape") && l->kind == LayerKind::Shape) {
                             const Json& t = p["shape"];
                             if (t["rect"].IsArray() && t["rect"].Size() == 4) l->shape.rect = {t["rect"].At(0).F(), t["rect"].At(1).F(), t["rect"].At(2).F(), t["rect"].At(3).F()};
                             if (t.Has("fill")) l->shape.fill = ColorFrom(t["fill"], l->shape.fill);
                             if (t.Has("stroke")) { l->shape.stroke = ColorFrom(t["stroke"], l->shape.stroke); l->shape.strokeOn = true; }
                             if (t.Has("strokeWidth")) l->shape.strokeWidth = t["strokeWidth"].F();
                             if (t.Has("type")) l->shape.type = t["type"].I();
                         }
                         Id lid = l->id;
                         if (p.Has("move")) d.MoveLayerSibling(lid, p["move"].I() > 0 ? 1 : -1);
                         d.SyncGraph();
                         edit.Commit();
                         Json j = Json::Object();
                         j["id"] = lid;
                         if (!unknown.empty()) j["warning"] = "paramètres inconnus : " + unknown;
                         return AiResult::Ok(j);
                     }});

    bridge.Register({"shaderixel.layer_delete", "Supprime un calque (et son contenu s'il s'agit d'un groupe).", Schema({{"id", T("integer")}}, {"id"}), true, {},
                     [this, need](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = need(e);
                         if (!v) return e;
                         Document& d = *v->doc;
                         if (!d.FindLayer(p["id"].U())) return AiResult::Fail("InvalidParameters", "calque introuvable");
                         Edit edit(d, v->history, "IA : supprimer le calque");
                         d.RemoveLayer(p["id"].U());
                         edit.Commit();
                         return AiResult::Ok(Json::Object());
                     }});

    bridge.Register({"shaderixel.filter_add", "Ajoute un filtre non destructif (effet ou style de calque) à un calque.",
                     Schema({{"layer", T("integer")}, {"effect", T("string")}, {"params", T("object")}}, {"layer", "effect"}), true, {},
                     [this, need](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = need(e);
                         if (!v) return e;
                         Document& d = *v->doc;
                         Layer* l = d.FindLayer(p["layer"].U());
                         if (!l) return AiResult::Fail("InvalidParameters", "calque introuvable");
                         auto def = Effects().Find(p["effect"].S(""));
                         if (!def) return AiResult::Fail("InvalidParameters", "effet inconnu");
                         Edit edit(d, v->history, "IA : filtre");
                         EffectInstance fx;
                         fx.effectId = def->id;
                         fx.params = def->DefaultParams();
                         std::string unknown = ApplyParams(fx, p["params"]);
                         l->filters.push_back(fx);
                         d.SyncGraph();
                         edit.Commit();
                         Json j = Json::Object();
                         j["index"] = (int)l->filters.size() - 1;
                         if (!unknown.empty()) j["warning"] = "paramètres inconnus : " + unknown;
                         return AiResult::Ok(j);
                     }});

    bridge.Register({"shaderixel.effect_set_code",
                     "Remplace l'effet d'un calque de réglage/remplissage par du code HLSL/Slang personnalisé "
                     "(format : //@name, //@param id type défaut min max \"Label\", puis float4 Effect(EffectContext ctx)).",
                     Schema({{"layer", T("integer")}, {"code", T("string")}}, {"layer", "code"}), true, {},
                     [this, need](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = need(e);
                         if (!v) return e;
                         Document& d = *v->doc;
                         Layer* l = d.FindLayer(p["layer"].U());
                         if (!l || (l->kind != LayerKind::Fill && l->kind != LayerKind::Adjustment)) return AiResult::Fail("InvalidParameters", "calque de réglage ou de remplissage requis");
                         Edit edit(d, v->history, "IA : code d'effet");
                         auto old = l->effect.Def();
                         l->effect.customCode = p["code"].S("");
                         l->effect.graph.clear();
                         l->effect.effectId.clear();
                         l->effect.SyncParams(old);
                         d.SyncGraph();
                         edit.Commit();
                         auto def = l->effect.Def();
                         Json j = Json::Object();
                         j["name"] = def ? def->name : "";
                         if (def && !def->error.empty()) j["metadataError"] = def->error;
                         j["note"] = "La compilation GPU est asynchrone : relisez document_info / shaderixel.shader_errors.";
                         return AiResult::Ok(j);
                     }});

    bridge.Register({"shaderixel.shader_errors", "Erreurs de compilation des shaders (effets personnalisés compris).", Schema({}), false, {},
                     [](const Json&, const std::string&) {
                         Json arr = Json::Array();
                         for (auto& f : gpu::Shaders().FailedPrograms()) {
                             Json o = Json::Object();
                             o["name"] = f->name;
                             o["errors"] = f->errors;
                             arr.Push(o);
                         }
                         Json j = Json::Object();
                         j["failed"] = arr;
                         return AiResult::Ok(j);
                     }});

    bridge.Register({"shaderixel.image_import", "Importe une image (PNG, JPG, PSD, HDR...) comme nouveau calque.",
                     Schema({{"path", T("string")}}, {"path"}), true, {"path"}, [this, need](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = need(e);
                         std::string path = p["path"].S("");
                         if (!fs::exists(Utf8Path(path))) return AiResult::Fail("InvalidParameters", "fichier introuvable");
                         OpenImage(path, v != nullptr);
                         Json j = Json::Object();
                         if (DocView* nv = Active()) j["activeLayer"] = nv->doc->S().activeLayer;
                         return AiResult::Ok(j);
                     }});

    bridge.Register({"shaderixel.select", "Sélection : shape = rect, ellipse, all, none ; rect [x0,y0,x1,y1] ; mode replace/add/subtract/intersect ; feather (px).",
                     Schema({{"shape", T("string")}, {"rect", T("array")}, {"mode", T("string")}, {"feather", T("number")}}, {"shape"}), true, {},
                     [this, need](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = need(e);
                         if (!v) return e;
                         Document& d = *v->doc;
                         std::string s = p["shape"].S("rect");
                         std::string m = p["mode"].S("replace");
                         SelMode mode = m == "add" ? SelMode::Add : m == "subtract" ? SelMode::Subtract : m == "intersect" ? SelMode::Intersect : SelMode::Replace;
                         if (s == "none") { v->renderer.Deselect(d, v->history); return AiResult::Ok(Json::Object()); }
                         float4 r = {0, 0, (float)d.Width(), (float)d.Height()};
                         if (p["rect"].IsArray() && p["rect"].Size() == 4) r = {p["rect"].At(0).F(), p["rect"].At(1).F(), p["rect"].At(2).F(), p["rect"].At(3).F()};
                         SelShape shape = s == "ellipse" ? SelShape::Ellipse : s == "all" ? SelShape::All : SelShape::Rect;
                         v->renderer.SelectShape(d, *ctx, v->history, shape, r, mode, true, p["feather"].F(0), "IA : sélection");
                         return AiResult::Ok(Json::Object());
                     }});

    bridge.Register({"shaderixel.fill", "Remplit la sélection (ou tout le calque) du calque de pixels actif ou désigné avec une couleur.",
                     Schema({{"color", ColorT("couleur")}, {"layer", T("integer")}, {"opacity", T("number")}}, {"color"}), true, {},
                     [this, need](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = need(e);
                         if (!v) return e;
                         Document& d = *v->doc;
                         if (p.Has("layer")) { d.S().activeLayer = p["layer"].U(); d.S().editMask = false; }
                         Id target = d.PaintTarget();
                         if (!target) return AiResult::Fail("InvalidParameters", "le calque actif n'est pas un calque de pixels");
                         v->renderer.FillSelection(d, *ctx, &v->history, target, 0, LinearColor(ColorFrom(p["color"], {0, 0, 0, 1})), p["opacity"].F(1), "IA : remplir");
                         return AiResult::Ok(Json::Object());
                     }});

    bridge.Register({"shaderixel.paint_stroke", "Peint un trait au pinceau sur le calque de pixels actif (ou `layer`). points = [[x, y, pression?], ...].",
                     Schema({{"points", T("array")}, {"layer", T("integer")}, {"size", T("number")}, {"hardness", T("number")},
                             {"color", ColorT("couleur")}, {"opacity", T("number")}, {"erase", T("boolean")}},
                            {"points"}),
                     true, {}, [this, need](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = need(e);
                         if (!v) return e;
                         Document& d = *v->doc;
                         if (p.Has("layer")) { d.S().activeLayer = p["layer"].U(); d.S().editMask = false; }
                         Id target = d.PaintTarget();
                         if (!target) return AiResult::Fail("InvalidParameters", "le calque actif n'est pas un calque de pixels");
                         BrushSettings b = brush;
                         b.size = p["size"].F(20);
                         b.hardness = p["hardness"].F(0.8f);
                         b.opacity = p["opacity"].F(1);
                         std::vector<Dab> dabs;
                         const auto& pts = p["points"].Elements();
                         for (size_t i = 0; i < pts.size(); ++i) {
                             if (!pts[i].IsArray() || pts[i].Size() < 2) continue;
                             float2 a{pts[i].At(0).F(), pts[i].At(1).F()};
                             float pa = pts[i].Size() > 2 ? pts[i].At(2).F(1) : 1.0f;
                             if (i + 1 < pts.size() && pts[i + 1].IsArray() && pts[i + 1].Size() >= 2) {
                                 float2 c{pts[i + 1].At(0).F(), pts[i + 1].At(1).F()};
                                 float pc = pts[i + 1].Size() > 2 ? pts[i + 1].At(2).F(1) : 1.0f;
                                 float len = Length(c - a);
                                 float step = std::max(1.0f, b.size * 0.1f);
                                 for (float t = 0; t < len; t += step) {
                                     float k = t / std::max(len, 1e-4f);
                                     float pr = pa + (pc - pa) * k;
                                     dabs.push_back({a.x + (c.x - a.x) * k, a.y + (c.y - a.y) * k, b.size * 0.5f * pr, 1.0f});
                                 }
                             } else {
                                 dabs.push_back({a.x, a.y, b.size * 0.5f * pa, 1.0f});
                             }
                         }
                         if (dabs.empty()) return AiResult::Fail("InvalidParameters", "aucun point");
                         if (!v->renderer.BeginStroke(d, *ctx, target)) return AiResult::Fail("NotReady", "pinceau indisponible, réessayez");
                         v->renderer.StrokeDabs(d, *ctx, dabs, b, LinearColor(ColorFrom(p["color"], fg)), p["erase"].B(false));
                         v->renderer.EndStroke(d, *ctx, v->history, "IA : pinceau");
                         Json j = Json::Object();
                         j["dabs"] = (int)dabs.size();
                         return AiResult::Ok(j);
                     }});

    bridge.Register({"shaderixel.undo", "Annule la dernière opération.", Schema({}), true, {}, [this](const Json&, const std::string&) {
                         Undo();
                         return AiResult::Ok(Json::Object());
                     }});
    bridge.Register({"shaderixel.redo", "Rétablit la dernière opération annulée.", Schema({}), true, {}, [this](const Json&, const std::string&) {
                         Redo();
                         return AiResult::Ok(Json::Object());
                     }});

    bridge.Register({"shaderixel.document_save", "Enregistre le document (.sxl) ou exporte une image (.png, .jpg) au chemin donné.",
                     Schema({{"path", T("string")}}, {"path"}), true, {}, [this, need](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = need(e);
                         if (!v) return e;
                         std::string path = p["path"].S("");
                         std::string ext = Narrow(Utf8Path(path).extension().wstring());
                         for (auto& c : ext) c = (char)tolower((unsigned char)c);
                         if (ext == ".sxl") SaveDocument(*v, path);
                         else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") ExportImage(path);
                         else return AiResult::Fail("InvalidParameters", "extension non supportée");
                         Json j = Json::Object();
                         j["path"] = path;
                         j["note"] = "écriture asynchrone (terminée en quelques images)";
                         return AiResult::Ok(j);
                     }});

    // ------------------------------------------------------------------ node graph (node mode)

    const char* kNodeKinds[] = {"output", "surface", "effect", "blend"};
    auto nodeJson = [kNodeKinds](const Node& n) {
        Json o = Json::Object();
        o["id"] = n.id;
        o["kind"] = kNodeKinds[(int)n.kind];
        o["name"] = n.name;
        o["pos"] = Arr({n.pos.x, n.pos.y});
        Json in = Json::Array();
        for (int i = 0; i < (int)n.inputs.size(); ++i) {
            Json pin = Json::Object();
            pin["index"] = i;
            pin["name"] = NodeInputName(n, i);
            pin["from"] = n.inputs[i];
            in.Push(pin);
        }
        o["inputs"] = in;
        if (n.bypass) o["bypass"] = true;
        if (n.kind == NodeKind::Effect) o["effect"] = EffectJson(n.fx);
        if (n.kind == NodeKind::Blend) {
            o["blend"] = kBlendKeys[(int)n.blend];
            o["opacity"] = n.opacity;
        }
        return o;
    };
    auto needNodes = [this, need](AiResult& err) -> DocView* {
        DocView* v = need(err);
        if (v && v->doc->Mode() != DocMode::Nodes) {
            err = AiResult::Fail("WrongMode", "Document en mode calques : le graphe est généré depuis les calques (créez un document en mode nodal)");
            return nullptr;
        }
        return v;
    };

    bridge.Register({"shaderixel.graph_info",
                     "Graphe du document en mode nodal : nœuds (id, type, nom, entrées reliées, effet et paramètres) et nœud de sortie.",
                     Schema({}), false, {}, [this, need, nodeJson](const Json&, const std::string&) {
                         AiResult e;
                         DocView* v = need(e);
                         if (!v) return e;
                         Document& d = *v->doc;
                         Json j = Json::Object();
                         j["mode"] = d.Mode() == DocMode::Layers ? "layers" : "nodes";
                         j["output"] = d.S().graph.output;
                         Json nodes = Json::Array();
                         for (auto& n : d.S().graph.nodes) nodes.Push(nodeJson(n));
                         j["nodes"] = nodes;
                         if (d.Mode() == DocMode::Layers) j["note"] = "graphe généré depuis les calques (lecture seule)";
                         return AiResult::Ok(j);
                     }});

    bridge.Register({"shaderixel.node_add",
                     "Ajoute un nœud (mode nodal) : kind = effect (avec `effect` id de la bibliothèque, ou `code` HLSL), surface (calque de pixels) "
                     "ou blend (fusion). Option : pos [x, y], params.",
                     Schema({{"kind", T("string")}, {"effect", T("string")}, {"code", T("string")}, {"pos", T("array")}, {"params", T("object")},
                             {"name", T("string")}},
                            {"kind"}),
                     true, {}, [this, needNodes, nodeJson](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = needNodes(e);
                         if (!v) return e;
                         Document& d = *v->doc;
                         std::string kind = p["kind"].S("");
                         float2 pos = {p["pos"].At(0).F(0), p["pos"].At(1).F(0)};
                         Edit edit(d, v->history, "IA : nœud");
                         Node* n = nullptr;
                         std::string warn;
                         if (kind == "effect") {
                             std::string code = p["code"].S("");
                             std::string id = p["effect"].S("");
                             if (code.empty() && !Effects().Find(id)) return AiResult::Fail("InvalidParameters", "effet inconnu (voir shaderixel.effects_list)");
                             n = &d.AddEffectNode(code.empty() ? id : "", code, pos);
                             warn = ApplyParams(n->fx, p["params"]);
                         } else if (kind == "surface") {
                             n = &d.AddNode(NodeKind::Surface, pos);
                         } else if (kind == "blend") {
                             n = &d.AddNode(NodeKind::Blend, pos);
                         } else {
                             return AiResult::Fail("InvalidParameters", "kind = effect, surface ou blend");
                         }
                         if (!p["name"].S("").empty()) n->name = p["name"].S("");
                         Json j = nodeJson(*n);
                         edit.Commit();
                         if (!warn.empty()) j["warning"] = "paramètres inconnus : " + warn;
                         return AiResult::Ok(j);
                     }});

    bridge.Register({"shaderixel.node_link",
                     "Relie la sortie du nœud `from` à l'entrée `input` (index, 0 par défaut) du nœud `to` ; from = 0 débranche. "
                     "Le nœud de sortie a une entrée (index 0).",
                     Schema({{"from", T("integer")}, {"to", T("integer")}, {"input", T("integer")}}, {"from", "to"}), true, {},
                     [this, needNodes](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = needNodes(e);
                         if (!v) return e;
                         Document& d = *v->doc;
                         Id from = p["from"].U(), to = p["to"].U();
                         int input = p["input"].I(0);
                         const Node* dst = d.S().graph.Find(to);
                         if (!dst) return AiResult::Fail("InvalidParameters", "nœud `to` introuvable");
                         if (input < 0 || input >= std::max(1, NodeInputCount(*dst))) return AiResult::Fail("InvalidParameters", "entrée hors limites");
                         Edit edit(d, v->history, "IA : liaison");
                         if (from == 0) d.Unlink(to, input);
                         else if (!d.Link(from, to, input)) return AiResult::Fail("InvalidParameters", "liaison impossible (nœud introuvable ou cycle)");
                         edit.Commit();
                         return AiResult::Ok(Json::Object());
                     }});

    bridge.Register({"shaderixel.node_update",
                     "Modifie un nœud : name, params (effet), blend et opacity (fusion), bypass, pos.",
                     Schema({{"id", T("integer")}, {"name", T("string")}, {"params", T("object")}, {"blend", T("string")}, {"opacity", T("number")},
                             {"bypass", T("boolean")}, {"pos", T("array")}},
                            {"id"}),
                     true, {}, [this, needNodes, nodeJson](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = needNodes(e);
                         if (!v) return e;
                         Document& d = *v->doc;
                         Node* n = d.S().graph.Find(p["id"].U());
                         if (!n) return AiResult::Fail("InvalidParameters", "nœud introuvable");
                         Edit edit(d, v->history, "IA : nœud");
                         std::string warn;
                         if (p.Has("name")) n->name = p["name"].S(n->name);
                         if (p.Has("params") && n->kind == NodeKind::Effect) warn = ApplyParams(n->fx, p["params"]);
                         if (p.Has("blend")) {
                             int b = BlendFromKey(p["blend"].S(""));
                             if (b < 0) return AiResult::Fail("InvalidParameters", "mode de fusion inconnu");
                             n->blend = (BlendMode)b;
                         }
                         if (p.Has("opacity")) n->opacity = std::clamp(p["opacity"].F(1), 0.0f, 1.0f);
                         if (p.Has("bypass")) n->bypass = p["bypass"].B(false);
                         if (p.Has("pos")) n->pos = {p["pos"].At(0).F(n->pos.x), p["pos"].At(1).F(n->pos.y)};
                         Json j = nodeJson(*n);
                         edit.Commit();
                         if (!warn.empty()) j["warning"] = "paramètres inconnus : " + warn;
                         return AiResult::Ok(j);
                     }});

    bridge.Register({"shaderixel.node_delete", "Supprime un nœud (sauf la sortie).", Schema({{"id", T("integer")}}, {"id"}), true, {},
                     [this, needNodes](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = needNodes(e);
                         if (!v) return e;
                         Document& d = *v->doc;
                         Id id = p["id"].U();
                         if (!d.S().graph.Find(id)) return AiResult::Fail("InvalidParameters", "nœud introuvable");
                         if (id == d.S().graph.output) return AiResult::Fail("InvalidParameters", "le nœud de sortie ne peut pas être supprimé");
                         Edit edit(d, v->history, "IA : supprimer le nœud");
                         d.RemoveNode(id);
                         edit.Commit();
                         return AiResult::Ok(Json::Object());
                     }});

    // ------------------------------------------------------------------ visual effect graphs

    bridge.Register({"shaderixel.effect_graph_ops",
                     "Opérations disponibles pour les graphes d'effets visuels (op, libellé, catégorie, entrées avec défaut, sorties).",
                     Schema({}), false, {}, [](const Json&, const std::string&) {
                         Json a = Json::Array();
                         for (const EGOpDef& op : EffectGraphOps()) {
                             Json o = Json::Object();
                             o["op"] = op.op;
                             o["label"] = op.label;
                             o["category"] = op.category;
                             Json in = Json::Array();
                             for (auto& pin : op.inputs) {
                                 Json q = Json::Object();
                                 q["name"] = pin.name;
                                 q["default"] = pin.defValue;
                                 in.Push(q);
                             }
                             o["inputs"] = in;
                             Json out = Json::Array();
                             for (auto* name : op.outputs) out.Push(name);
                             o["outputs"] = out;
                             if (op.hasValue) o["hasValue"] = true;
                             a.Push(o);
                         }
                         Json j = Json::Object();
                         j["ops"] = a;
                         j["example"] = EffectGraph::Default().ToJson();
                         return AiResult::Ok(j);
                     }});

    // Effect instance of a layer (adjustment/fill) or an effect node.
    auto effectTarget = [](Document& d, const Json& p, std::string& err) -> EffectInstance* {
        if (p.Has("layer")) {
            Layer* l = d.FindLayer(p["layer"].U());
            if (!l || (l->kind != LayerKind::Adjustment && l->kind != LayerKind::Fill)) { err = "calque de réglage/remplissage introuvable"; return nullptr; }
            return &l->effect;
        }
        Node* n = d.S().graph.Find(p["node"].U());
        if (!n || n->kind != NodeKind::Effect) { err = "nœud d'effet introuvable (paramètre `layer` ou `node`)"; return nullptr; }
        if (d.Mode() == DocMode::Layers) { err = "en mode calques, désignez le calque (`layer`)"; return nullptr; }
        return &n->fx;
    };

    bridge.Register({"shaderixel.effect_graph_get",
                     "Graphe d'effet visuel d'un calque (`layer`) ou d'un nœud (`node`) : JSON du graphe, HLSL généré, erreurs.",
                     Schema({{"layer", T("integer")}, {"node", T("integer")}}), false, {}, [this, need, effectTarget](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = need(e);
                         if (!v) return e;
                         std::string err;
                         EffectInstance* fx = effectTarget(*v->doc, p, err);
                         if (!fx) return AiResult::Fail("InvalidParameters", err);
                         if (fx->graph.empty()) return AiResult::Fail("InvalidParameters", "cet effet n'est pas un graphe visuel");
                         EffectGraph g;
                         if (!EffectGraph::Deserialize(fx->graph, g)) return AiResult::Fail("Internal", "graphe illisible");
                         EGCompileResult cr = CompileEffectGraph(g);
                         Json j = Json::Object();
                         j["graph"] = g.ToJson();
                         j["hlsl"] = cr.code;
                         if (!cr.error.empty()) j["error"] = cr.error;
                         return AiResult::Ok(j);
                     }});

    bridge.Register({"shaderixel.effect_graph_set",
                     "Remplace l'effet d'un calque de réglage/remplissage (`layer`) ou d'un nœud d'effet (`node`) par un graphe visuel "
                     "(`graph`, même format que effect_graph_get / l'exemple de effect_graph_ops). Renvoie le HLSL généré ou l'erreur.",
                     Schema({{"layer", T("integer")}, {"node", T("integer")}, {"graph", T("object")}}, {"graph"}), true, {},
                     [this, need, effectTarget](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = need(e);
                         if (!v) return e;
                         Document& d = *v->doc;
                         EffectGraph g;
                         if (!EffectGraph::FromJson(p["graph"], g)) return AiResult::Fail("InvalidParameters", "graphe invalide");
                         EGCompileResult cr = CompileEffectGraph(g);
                         if (!cr.error.empty()) {
                             AiResult r = AiResult::Fail("CompileError", cr.error);
                             return r;
                         }
                         std::string err;
                         if (!effectTarget(d, p, err)) return AiResult::Fail("InvalidParameters", err);
                         Edit edit(d, v->history, "IA : graphe d'effet");
                         EffectInstance* fx = effectTarget(d, p, err);
                         auto old = fx->Def();
                         fx->effectId.clear();
                         fx->customCode.clear();
                         fx->graph = g.Serialize();
                         fx->SyncParams(old);
                         if (Node* n = p.Has("node") ? d.S().graph.Find(p["node"].U()) : nullptr) {
                             n->name = g.name;
                             n->inputs.resize(NodeInputCount(*n), 0);
                         }
                         d.SyncGraph();
                         edit.Commit();
                         Json j = Json::Object();
                         size_t at = cr.code.find("float4 Effect(");
                         j["hlsl"] = at == std::string::npos ? cr.code : cr.code.substr(at);  // body only (helpers omitted)
                         return AiResult::Ok(j);
                     }});

    // ------------------------------------------------------------------ paths

    bridge.Register({"shaderixel.path_from_selection",
                     "Crée un tracé (panneau Tracés) qui suit le contour de la sélection ; tolerance en pixels (2 par défaut).",
                     Schema({{"tolerance", T("number")}}), true, {}, [this, need](const Json& p, const std::string&) {
                         AiResult e;
                         DocView* v = need(e);
                         if (!v) return e;
                         if (!v->doc->Selection()) return AiResult::Fail("InvalidParameters", "aucune sélection");
                         selToPath.tolerance = std::clamp(p["tolerance"].F(2.0f), 0.25f, 50.0f);
                         MakePathFromSelection(*v);
                         // Finished when the readback is traced (a few frames).
                         AiResult r;
                         r.done = false;
                         size_t before = v->doc->S().paths.size();
                         r.poll = [this, v, before](AiResult& out) {
                             if (selToPath.mask) return false;
                             bool alive = false;
                             for (auto& d : docs) alive |= d.get() == v;
                             if (!alive || v->doc->S().paths.size() <= before) {
                                 out = AiResult::Fail("Failed", "aucun contour trouvé");
                                 return true;
                             }
                             const VectorPath& path = v->doc->S().paths.back();
                             size_t anchors = 0;
                             for (auto& sp : path.subpaths) anchors += sp.pts.size();
                             Json j = Json::Object();
                             j["id"] = path.id;
                             j["name"] = path.name;
                             j["subpaths"] = (int)path.subpaths.size();
                             j["anchors"] = (int)anchors;
                             out = AiResult::Ok(j);
                             return true;
                         };
                         return r;
                     }});
}

}  // namespace sx
