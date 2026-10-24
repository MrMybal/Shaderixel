# Shaderixel — Feuille de route

## ✅ Fait

**Socle**
- D3D12 + Slang (HLSL) compilés à l'exécution, en asynchrone, avec cache et rechargement à chaud ; pipelines compute et graphiques
- Graphe nodal toujours rendu ; modes Calques / Nodal avec conversion dans les deux sens (vers calques seulement si le graphe est linéaire)
- Évaluation GPU avec cache par signature de contenu : 0 calcul quand rien ne change

**Photoshop**
- 27 modes de fusion, opacité, masques de fusion, fusion en gamma ou linéaire
- Calques de pixels, de remplissage, de réglage, de texte, de forme ; groupes (Traverser) ; masques d'écrêtage ; filtres non destructifs par calque
- Pinceau et gomme (pression, flux, dureté, espacement, lissage), dégradé, pot de peinture (tolérance, contigu), pipette
- Retouche : tampon de duplication, correcteur, goutte, netteté, doigt, densité -/+, éponge, remplissage d'après le contenu (PatchMatch, sans IA, Maj+F5), pièce, déplacement basé sur le contenu, yeux rouges, gomme magique, gomme d'arrière-plan, forme d'historique (instantanés)
- Sélections : rectangle, ellipse, lasso, polygonal, baguette, plage de couleurs, depuis un calque ; ajout/soustraction/intersection, contour progressif, dilater/contracter, intervertir, mode masque
- Transformation manuelle (échelle, rotation, pivot, perspective) et outil Déplacement ; calque par copier/couper, copier/coller
- Menu Image : taille de l'image, zone de travail, rotations, symétries, recadrage
- 37 effets dont Courbes, Niveaux, Teinte/Saturation, flous, styles de calque (ombre portée/interne, lueur, contour, incrustations, biseau)
- Texte (polices système, multi-ligne, alignement, approche, interligne, styles de caractère sur des plages : police, taille, couleur, approche, faux gras, souligné, capitales, décalage de ligne de base ; styles de paragraphe : alignement, interligne, espace avant, retraits, style de caractère de base ; texte sur tracé : décalage, côté)
- Formes vectorielles SDF (rectangle arrondi, ellipse, ligne, polygone, étoile ; fond + contour ; opérations booléennes : unir, soustraire, intersection, exclure, jusqu'à 8 composants, outil Forme avec Maj / Alt / Maj+Alt)
- Historique groupé, annulable partout

**Fichiers**
- Format natif `.sxl` (JSON + pixels compressés XPRESS), import et export PSD avec calques, import PNG/JPG/TGA/BMP/HDR, export PNG/JPG/TGA/BMP

**Spécifique Shaderixel**
- Nœuds HLSL personnalisés et **éditeur d'effets visuel** (façon éditeur de matériaux Unreal) avec **export vers un matériau Unreal** (script Python : nœud Custom + paramètres + textures)
- **IA** : pont MCP compatible CyToolsCore (26 opérations `shaderixel.*`, écriture soumise à autorisation), compagnon CyUEMCPSDK lancé automatiquement, assistant intégré, génération d'images OpenAI / Gemini
- **Peinture 3D** : vue 3D (OBJ, primitives), document en texture, pinceau projeté avec occultation

## Prochaines étapes

### Performance / grands formats
- Rendu par tuiles : **fait** (au-delà de ~36 MP : tuiles 512 px, halos d'effets, zones modifiées, rendu progressif visible d'abord ; identique au pixel près au rendu complet). Surfaces creuses : **fait** (ressources réservées D3D12, mémoire allouée par tuiles de 64 Ko seulement là où on peint ; démo 16K : 11,3 → 4,75 Go). Historique de peinture hors VRAM (anciennes retouches compressées en mémoire système, rechargées à l'annulation) : **fait**. Aperçus de nœuds en mode tuiles (détail de 512 px au centre, mis en cache) : **fait**. Fond uni sans mémoire (très grands documents : remplissage de couleur + calque vide pour peindre ; démo 16K : 4,75 → 2,7 Go) : **fait**
- Recalcul limité aux régions modifiées
- Réduction d'affichage en zoom arrière : **fait** (moyenne par boîte incrémentale)
- Histogramme GPU : **fait** (panneau Histogramme, statistiques, fond de l'éditeur de courbes)
- Mode 8 bits natif

### Photoshop
- Déplacer le contour de sélection (glisser à l'intérieur, flèches) : **fait** , Transformer la sélection : **fait** , Sélection rapide (croissance géodésique sensible aux bords) : **fait**
- Blend-If (options de fusion) : **fait** (gris, poignées séparables, import/export PSD), par canal R/V/B : **fait**
- Tracés vectoriels : **fait** (outil Plume, édition Ctrl, panneau Tracés : sélection, remplissage, contour au pinceau ; sauvegardés dans le .sxl). Sélection → tracé (contour sous-pixel, simplification, angles conservés ; aller-retour à 0,1 % près) : **fait**. Tracés dans les PSD (ressources 2000+, export et import, vérifiés avec psd-tools) : **fait**. Formes personnalisées (bibliothèque fournie, depuis un tracé, import SVG ; champ de distance calculé sur le processeur) : **fait**
- Fluidité (pinceau : déformation avant, tourbillons, contraction, dilatation, reconstruction) : **fait**
- LUT 3D (.cube) : **fait** (réglage Correspondance de couleur, LUT embarquée dans le .sxl, type de paramètre `lut` disponible pour les effets)
- Déformation (Édition > Déformation, ou depuis la transformation manuelle : grille de Bézier 4×4, points, poignées, glisser la surface, sélection incluse) : **fait**
- Marionnette (épingles, moindres carrés mobiles : rigide / normal / distorsion, densité du maillage) et déformation de la perspective (plans disposés sur l'image, scinder / ajouter, puis déformer avec une homographie par plan, redresser verticales / horizontales) : **fait**
- Actions (enregistrement des commandes : taille, zone de travail, rotation, recadrage, filtres avec leurs paramètres, calques, sélection, remplir, fusionner, aplatir, exporter ; étapes activables, réordonnables, paramètres éditables, étape « opération » pour les outils IA / MCP) et traitement par lots (dossier source, sous-dossiers, destination, format, suffixe) : **fait**
- Compositions de calques (visibilité, apparence, position des objets dynamiques ; appliquer, mettre à jour, exporter) et plans de travail (groupes délimités par leur masque, ajoutés à droite, préréglages de taille, export d'une image par plan) : **fait**
- Gestion des couleurs (moteur Windows ICM) : profil du document (attribuer / convertir, profils incorporés PNG / JPEG conservés), profil du moniteur, épreuvage (Affichage > Épreuvage, Ctrl+Alt+Y, profil, mode de rendu), couleurs non imprimables (Ctrl+Alt+Maj+Y), mode CMJN (gamut du profil, affichage simulé, export TIFF CMJN avec profil), mode Lab, sélecteur CMJN et Lab, effet Réglage Lab, tons directs (calques d'encre en Produit avec masque) : **fait**
- Photo : ouverture RAW (DNG, CR2 / CR3 / NEF / ARW... via Windows Imaging Component et l'extension Raw Image), TIFF / WebP / HEIF, filtre Photo RAW (balance des blancs, exposition, tons, texture, clarté, correction du voile, vibrance, saturation, netteté, vignettage, grain), Photomerge (points caractéristiques, RANSAC, perspective / cylindrique / repositionnement, masques de fondu), alignement automatique des calques, fusion automatique (panorama ou empilement de mise au point), fusion HDR 32 bits (alignement, estimation des expositions) : **fait**
- Ressources : import des pinceaux Photoshop .abr (pointes échantillonnées v1, v2, v6 / v10 -> pinceaux texturés de la bibliothèque), motifs (bibliothèque, motifs par défaut, Définir un motif, Remplir avec un motif, outil Tampon de motif, import .pat), nuancier (panneau, bibliothèques .aco / .ase / .gpl, enregistrement .aco) : **fait**
- Export PSD avec calques : **fait** (8 bits RLE, groupes, masques, écrêtage, modes ; réglages/remplissages/filtres rastérisés). PSB et 16 bits (ZIP avec prédiction) en export et import : **fait**. Calques de réglage natifs à l'export et à l'import (luminosité / contraste, niveaux, courbes, teinte / saturation, négatif, isohélie, seuil, exposition ; les autres restent rastérisés) : **fait**

- Recadrage en perspective, Rogner, galerie de flous (diaphragme, inclinaison-décalage, objectif avec profondeur et bokeh), séparation de fréquences, coller en perspective (plan à 4 coins, mosaïque), zone de netteté, pinceau mélangeur, correcteur localisé, remplacement de couleur, effets d'éclairage, déformation du texte, exporter sous (tailles multiples, calques séparés), symétrie de peinture, éditeur de dégradé, règle et rotation paramétrée : **fait**
- Mise à l'échelle selon le contenu (seam carving, sélection protégée ou supprimée) : **fait** ; animation image par image (panneau Animation : états des calques par image, durées, interpolation, lecture, export GIF / MP4, pelures d'oignon) : **fait**
- Règles, repères, grille et magnétisme : **fait**
- Fusionner vers le bas / les calques visibles / aplatir : **fait**

### 3D
- Import glTF 2.0 (.gltf / .glb) et FBX : **fait** ; plusieurs matériaux (une tuile chacun, export par matériau) : **fait** ; UDIM (tuiles rangées dans la texture, export nom_Canal.1001.png, OBJ en espace UDIM) : **fait**
- Canaux PBR (couleur, rugosité, métal, normale, émissif, occlusion) en groupes de calques, rendu PBR GGX, export textures + script de matériau Unreal : **fait** , canal Hauteur (relief en vue 3D, cuit dans la normale à l'export) : **fait** , éclairage HDRI (préfiltré par rugosité, intensité/rotation) : **fait** , affichage d'un canal seul : **fait**, fond HDRI visible (flou réglable) : **fait**
- Pinceau en espace UV / triplanar, masquage par ID de matériau

### IA
- Remplissage génératif depuis la sélection (OpenAI /images/edits avec masque, Gemini) : **fait** (chaîne validée avec SX_AI_MOCK=1 ; appels réels non testés sans clé)
- ComfyUI : graphe intégré (texte → image, image → image) ou workflow exporté au format API, prompt/graine/taille/image substitués, résultat importé en calque : **fait** (validé avec tools/mock_comfyui.py ; pas encore avec un vrai serveur)
- Opérations MCP : graphe nodal (graph_info, node_add/link/update/delete), graphes d’effets visuels (effect_graph_ops/get/set avec compilation), tracé depuis la sélection : **fait**

### Multi-plateforme
- Backend Vulkan : **fait** (ShaderixelVk.exe sous Windows, mêmes shaders via SPIR-V ; écarts avec D3D12 limités à la précision numérique). Format `.sxl` v2 portable (deflate maison, morceaux en parallèle ; v1 XPRESS toujours lisible sous Windows) : **fait**. Surfaces creuses Vulkan (images à résidence partielle) : **fait** (16K : 8,35 → 4,8 Go). Reste : couche plateforme Linux (fenêtre, dialogues, stylet), backend Metal (macOS)

### Espace de travail
- Colonne de droite en onglets (Général, Outils, Effets, Fusion, Peinture, 3D), chacun avec son propre agencement de panneaux : **fait**
- Zone du graphe et colonne de droite masquables (Tab / Maj+Tab, barre d'état) : **fait** ; colonne d'outils ajustée à ses icônes : **fait**
- Multi-fenêtres / multi-écrans : panneaux détachables hors de la fenêtre principale (viewports ImGui, D3D12 et Vulkan), « Nouvelle fenêtre » pour regrouper des panneaux sur un autre écran : **fait**
- Dispositions enregistrées (Fenêtre > Dispositions) avec onglet, zones masquées et fenêtres : **fait**
- Glisser-déposer des effets : nœud de calque (filtre), lien (inséré), vide (nouveau nœud), liste des calques, image : **fait**

### Fusions
- Fusions séparées des effets (`//@blend`, panneau Fusions avec modes standard et fusions de bibliothèque, aperçus sur le calque actif) : **fait**
- Nœud « HLSL personnalisé » à broches déclarées (float à float4, textures lues à n'importe quel UV, plusieurs sorties, déclarations) ; les 182 presets importés portent leur graphe (`//@graph` : code d'origine dans le nœud, textures, UV, taille du canevas et paramètres câblés), ouvert via « Modifier une copie (graphe) » ; `--test-effects` compile aussi ces graphes : **fait**
- Import HLSL (Fichier > Importer un effet HLSL, dépôt d'un .hlsl, Éditeur d'effets > Importer du HLSL) : la fonction ou les annotations `//@in` `//@out` `//@name` `//@category` `//@kind` `//@main` créent le nœud, ses entrées (type, défaut, plage, libellé, catégorie) et sorties ; skill IA `.claude/skills/shaderixel-hlsl-effect` + outil MCP `shaderixel.effect_from_hlsl` (compilation vérifiée, document ou bibliothèque) : **fait**
- Fusions en HLSL (Propriétés) ou en graphe (éditeur visuel : entrées A dessous, B calque, masque, nœuds « Opacité du calque » et « Composer la fusion »), enregistrables dans la bibliothèque : **fait**

### Peinture 3D (façon Substance Painter)
- Mode Paint 3D (projets .sx3d, espace de travail dédié) : nouveau projet (maillage, résolution, canaux, modèle d'export, matériau de base), calques 3D (peinture multi-canal ou matériau à masque partagé), Matériau du pinceau (couleur, rugosité, métal, hauteur, émissif ; préréglages), outil Projection (décalque projeté depuis la vue), exports séparés / Unreal ORM / Unity mask map, maillage enregistré dans le fichier : **fait**
- UV automatiques (xatlas) : préréglages, réglages des îlots et de l'empaquetage, aperçu des UV actuelles / nouvelles (îlots colorés), damier sur le modèle, application avec report de la peinture existante, option à la création d'un projet : **fait**
- Calques de matériau : un remplissage par canal PBR (couleur, rugosité, métal, hauteur, émissif) partageant un masque ; peindre le masque (2D ou sur le modèle) applique tous les canaux : **fait**
- Étagère de matériaux, pile des calques de matériau, propriétés par canal, espace « Peinture 3D » (vue 3D + texture) : **fait**
- À venir : masques intelligents (courbure, occlusion, usure des arêtes), matériaux texturés, plusieurs ensembles de textures

### Pinceaux programmables
- Pinceaux codés dans l'application (`shaders/brushes/*.slang`, prélude `brush_common.slang`) : `Tip(Brush b)` dessine la pointe (couleur + couverture, position locale, pression, graine, temps, distance, direction), `Dynamics(inout Dab d)` place chaque touche (suivre le trait, dispersion, taille/rotation/teinte aléatoires, variation dans le temps) ; réglages `//@spacing`, `//@airbrush` (touches par seconde tant qu'on reste appuyé), `//@extent`, `//@selfflow`, paramètres `//@param` réglables dans Options : **fait**
- Panneau « Pinceaux » : liste par catégories + recherche, aperçu de trait, éditeur de code (Ctrl+Entrée), nouveau pinceau depuis un modèle, enregistrement dans la bibliothèque : **fait**
- 4 exemples (plume, aérographe, fusain, dispersion d'étoiles) + 33 pinceaux CyTexture ; `--test-effects` compile aussi les pinceaux, démo `--demo brushes` : **fait**
- Pinceaux en graphe (éditeur visuel : nœud « Touche du pinceau », pointe ronde, aléatoire par touche ; placement : espacement, aérographe, suivre le trait, dispersion, taille/angle/opacité aléatoires), enregistrables (graphe conservé `//@graph`) : **fait**
- Textures de pinceau (`//@texture Nom "Libellé" textures/image.png`, jusqu'à 4, lues par `BrushTex(Nom, uv)` / `Texture2DSample`) ; images choisies dans les réglages du pinceau (fichier ou bibliothèque `shaders/brushes/textures`) ; nœud « Texture du pinceau » dans les graphes ; TextureBrush de CyTexture importé (34/34) : **fait**
- Tablette : Windows Ink ou Wintab (Options > Tablette), pression avec courbe réglable, inclinaison, rotation du stylet, vitesse transmises aux pinceaux (`d.tilt`, `d.twist`, `d.speed`, `TiltAmount`, `TiltAngle`) ; réactions réglables dans les graphes de pinceau (inclinaison -> angle / taille, rotation -> angle, vitesse -> taille) ; zone d'essai dans l'éditeur de pinceau : **fait** (Wintab non testé sans pilote installé)
- Correction : positions de peinture en coordonnées écran avec le multi-fenêtres (décalage de la barre de titre)
- À venir : gomme/masque avec pointes programmables testés en profondeur

### Import CyTexture
- 182 presets de CyTexture importés (Dessin 31, Effets 92, Fusions 47, Masques 12) depuis les définitions de `CyTextureMaterialPresetGenerator.cpp` (lu, jamais modifié) : `tools/cytexture/extract_presets.py` puis `convert_presets.py` ; chaque texture devient une entrée de nœud, les 221 effets compilent. Fusions utilisables comme mode de fusion de calque (« Fusions d'effets »). « Advanced » : 93 sur 94 importés via `tools/unreal` (export depuis l’éditeur Unreal, graphes de matériau et de fonctions traduits) ; pinceaux : 33 sur 34 importés en pinceaux programmables (`tools/cytexture/convert_brushes.py`, reste TextureBrush qui lit une texture)

## Peinture liquide (fait)

Film de peinture simulé sur GPU (shaders/fluid_paint.slang, render/FluidPaint.cpp, ui/FluidPaintUi.cpp) : tous les canaux PBR à la fois, gravité du modèle 3D ou d'un plan incliné, prise, absorption, relief, brillance humide ; une étape d'historique. Coutures UV traversées, gouttes qui retombent plus bas, reprise de couleur, « Faire couler le calque » : **fait**.

## Paint 3D : matériaux multiples (fait)

Une tuile de texture par matériau (UV remappées, grille carrée en puissance de deux), export d'un jeu de textures par matériau, enregistré dans le projet. Peindre en limitant au matériau survolé, tuiles sur un projet existant (peinture reportée) : **fait**.
