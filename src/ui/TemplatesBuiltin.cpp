// Shipped templates, built by the app itself (--demo maketemplates, SX_TPLDIR = output folder):
// each is created as a document, rendered, then saved as a template (.sxl + .json + .png).
#include "ui/App.h"

#include <cmath>

namespace sx {

namespace {

void SetParam(EffectInstance& fx, const char* id, float4 v) {
    auto def = fx.Def();
    if (!def) return;
    for (size_t k = 0; k < def->params.size(); ++k)
        if (def->params[k].id == id) {
            while (fx.params.size() <= k) fx.params.push_back(def->params[fx.params.size()].def);
            fx.params[k] = v;
        }
}

}  // namespace

std::vector<App::BuiltinTemplate> App::BuildBuiltinTemplates() {
    std::vector<BuiltinTemplate> out;
    auto make = [&](u32 w, u32 h, DocMode mode, int bg, const char* name, const char* cat, const char* desc) -> Document& {
        NewDocument(w, h, Precision::Half16, mode, bg, name);
        out.push_back({docs.back().get(), name, cat, desc});
        return *docs.back()->doc;
    };
    auto fill = [](Document& d, const char* effect, const char* name) -> Layer& {
        Layer& l = d.AddEffectLayer(LayerKind::Fill, effect, 0);
        l.name = name;
        return l;
    };

    // ---- Textures
    {
        Document& d = make(2048, 2048, DocMode::Layers, 3, "Texture tuilable 2048", "Textures",
                           "Bruit de base, calque de peinture et un raccord en haut de la pile : tout ce qui est dessous se répète sans couture.");
        Layer& n = fill(d, "noise", "Bruit (base)");
        SetParam(n.effect, "colorA", {0.18f, 0.16f, 0.14f, 1});
        SetParam(n.effect, "colorB", {0.45f, 0.40f, 0.33f, 1});
        d.AddPixelLayer("Peinture");
        Layer& s = d.AddEffectLayer(LayerKind::Adjustment, "trim_seamless");
        s.name = "Raccord (tuilable)";
        SetParam(s.effect, "rect", {0, 0, 2048, 2048});
        SetParam(s.effect, "blendX", {0.2f, 0, 0, 0});
        SetParam(s.effect, "blendY", {0.2f, 0, 0, 0});
        d.SyncGraph();
    }
    {
        Document& d = make(1024, 1024, DocMode::Layers, 3, "Matière 1024 (réglages)", "Textures",
                           "Couleur de base, détail de bruit en Incrustation et réglages finaux (niveaux, teinte) déjà en place.");
        Layer& c = fill(d, "solid_color", "Couleur de base");
        SetParam(c.effect, "color", {0.32f, 0.22f, 0.14f, 1});
        Layer& n = d.AddEffectLayer(LayerKind::Fill, "noise");
        n.name = "Détail (bruit)";
        n.blend = BlendMode::Overlay;
        n.opacity = 0.6f;
        d.AddPixelLayer("Peinture");
        d.AddEffectLayer(LayerKind::Adjustment, "levels").name = "Niveaux";
        d.AddEffectLayer(LayerKind::Adjustment, "hue_saturation").name = "Teinte / saturation";
        d.SyncGraph();
    }

    // ---- Trim sheets
    auto trimDoc = [&](u32 size, int preset, const char* name, const char* desc) {
        Document& d = make(size, size, DocMode::Layers, 3, name, "Trim sheets", desc);
        Layer& c = fill(d, "solid_color", "Fond");
        SetParam(c.effect, "color", {0.16f, 0.16f, 0.17f, 1});
        d.AddPixelLayer("Peinture");
        std::vector<int> hs;
        for (float f : TrimPresets()[preset].fractions) hs.push_back(std::max(1, (int)std::lround(f * size)));
        d.S().trimSheet.trims = TrimStrips(size, size, hs, 0);
        d.SyncGraph();
    };
    trimDoc(2048, 0, "Trim sheet 2048 - Architecture", "Bandes larges et fines pour murs, sols, moulures ; outil Trim et panneau Trim sheet pour ajuster.");
    trimDoc(1024, 1, "Trim sheet 1024 - Détails", "Beaucoup de bandes fines (joints, câbles, bords) ; chaque bande se rend raccordable dans le panneau Trim sheet.");
    {
        Document& d = make(2048, 2048, DocMode::Layers, 1, "Atlas 4 x 4 (sprites, tuiles)", "Trim sheets",
                           "16 cases de 512 px (trims sans raccord), repères à chaque case : sprites, icônes, tuiles d'atlas.");
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c) {
                Trim t;
                t.name = "Case " + std::to_string(r * 4 + c + 1);
                t.r = {c * 512, r * 512, (c + 1) * 512, (r + 1) * 512};
                t.tileX = false;
                d.S().trimSheet.trims.push_back(t);
            }
        for (int k = 1; k < 4; ++k) {
            d.S().guidesX.push_back(k * 512.0f);
            d.S().guidesY.push_back(k * 512.0f);
        }
    }

    // ---- Graphic design
    {
        Document& d = make(1024, 1024, DocMode::Layers, 1, "Logo 1024", "Graphisme", "Fond transparent, forme d'accent et texte centrés, prêts à modifier.");
        ShapeStyle s;
        s.type = 1;
        s.rect = {192, 192, 832, 832};
        s.fill = {0.42f, 0.30f, 0.95f, 1};
        d.AddShapeLayer(s).name = "Forme";
        TextStyle t = textDefaults;
        t.content = "LOGO";
        t.size = 190;
        t.color = {1, 1, 1, 1};
        t.align = 1;
        t.pos = {512, 400};
        d.AddTextLayer(t).name = "Texte";
        d.SyncGraph();
    }
    {
        Document& d = make(1280, 720, DocMode::Layers, 3, "Miniature vidéo 1280 x 720", "Graphisme", "Fond en dégradé, bandeau, titre et sous-titre.");
        Layer& g = fill(d, "gradient_fill", "Dégradé");
        SetParam(g.effect, "colorA", {0.02f, 0.03f, 0.12f, 1});
        SetParam(g.effect, "colorB", {0.35f, 0.08f, 0.45f, 1});
        SetParam(g.effect, "angle", {30, 0, 0, 0});
        ShapeStyle s;
        s.rect = {0, 470, 1280, 600};
        s.fill = {1.0f, 0.78f, 0.10f, 1};
        d.AddShapeLayer(s).name = "Bandeau";
        TextStyle t = textDefaults;
        t.content = "TITRE DE LA VIDÉO";
        t.size = 110;
        t.color = {1, 1, 1, 1};
        t.pos = {70, 170};
        d.AddTextLayer(t).name = "Titre";
        TextStyle st = textDefaults;
        st.content = "Sous-titre";
        st.size = 70;
        st.color = {0.08f, 0.06f, 0.10f, 1};
        st.pos = {70, 490};
        d.AddTextLayer(st).name = "Sous-titre";
        d.SyncGraph();
    }
    {
        Document& d = make(1500, 500, DocMode::Layers, 3, "Bannière 1500 x 500", "Graphisme", "Bannière de profil : dégradé, motif de bruit léger et titre.");
        Layer& g = fill(d, "gradient_fill", "Dégradé");
        SetParam(g.effect, "colorA", {0.05f, 0.20f, 0.35f, 1});
        SetParam(g.effect, "colorB", {0.10f, 0.55f, 0.55f, 1});
        SetParam(g.effect, "angle", {0, 0, 0, 0});
        Layer& n = d.AddEffectLayer(LayerKind::Fill, "noise");
        n.name = "Grain";
        n.blend = BlendMode::SoftLight;
        n.opacity = 0.35f;
        TextStyle t = textDefaults;
        t.content = "Votre nom";
        t.size = 120;
        t.color = {1, 1, 1, 1};
        t.align = 1;
        t.pos = {750, 170};
        d.AddTextLayer(t).name = "Titre";
        d.SyncGraph();
    }
    {
        Document& d = make(1024, 1024, DocMode::Layers, 1, "Icône d'application 1024", "Graphisme", "Carré arrondi (grille d'icône) et lettre centrale.");
        ShapeStyle s;
        s.rect = {64, 64, 960, 960};
        s.radius = 200;
        s.fill = {0.10f, 0.55f, 0.95f, 1};
        d.AddShapeLayer(s).name = "Fond de l'icône";
        TextStyle t = textDefaults;
        t.content = "A";
        t.size = 520;
        t.color = {1, 1, 1, 1};
        t.align = 1;
        t.pos = {512, 210};
        d.AddTextLayer(t).name = "Lettre";
        d.SyncGraph();
    }

    // ---- Game art
    {
        Document& d = make(128, 128, DocMode::Layers, 1, "Sprite 128 (pixel art)", "Game art", "Petit document transparent pour le pixel art (zoom et grille).");
        d.AddPixelLayer("Contour");
        d.SyncGraph();
    }
    {
        Document& d = make(1024, 256, DocMode::Layers, 1, "Planche d'animation 8 images", "Game art", "8 cases de 128 px (trims et repères) pour une animation en sprite sheet.");
        for (int k = 0; k < 8; ++k) {
            Trim t;
            t.name = "Image " + std::to_string(k + 1);
            t.r = {k * 128, 64, (k + 1) * 128, 192};
            t.tileX = false;
            d.S().trimSheet.trims.push_back(t);
            if (k) d.S().guidesX.push_back(k * 128.0f);
        }
        d.AddPixelLayer("Animation");
        d.SyncGraph();
    }

    // ---- Photo
    {
        Document& d = make(1920, 1080, DocMode::Layers, 0, "Retouche photo", "Photo",
                           "Calques de réglage prêts au-dessus de l'image (Photo RAW, courbes, teinte) : collez ou glissez votre photo dans Arrière-plan.");
        d.AddEffectLayer(LayerKind::Adjustment, "camera_raw").name = "Photo RAW";
        d.AddEffectLayer(LayerKind::Adjustment, "curves").name = "Courbes";
        d.AddEffectLayer(LayerKind::Adjustment, "hue_saturation").name = "Teinte / saturation";
        d.SyncGraph();
    }

    // ---- Node graphs
    {
        Document& d = make(1024, 1024, DocMode::Nodes, 3, "Graphe nodal - générateur", "Nodal",
                           "Bruit -> dégradé de couleurs -> sortie, avec l'échelle du bruit en valeur reliée : point de départ d'une texture procédurale.");
        Graph& g = d.S().graph;
        Node& n = d.AddEffectNode("noise", "", {0, 0});
        const Id noise = n.id;
        Node& m = d.AddEffectNode("gradient_map", "", {340, 0});
        const Id gm = m.id;
        d.Link(noise, gm, 0);
        d.Link(gm, g.output, 0);
        if (Node* o = g.Find(g.output)) o->pos = {680, 0};
        d.S().activeNode = gm;
    }
    return out;
}

}  // namespace sx
