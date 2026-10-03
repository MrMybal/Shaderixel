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
- Retouche : tampon de duplication, correcteur, goutte, netteté, doigt, densité -/+, éponge
- Sélections : rectangle, ellipse, lasso, polygonal, baguette, plage de couleurs, depuis un calque ; ajout/soustraction/intersection, contour progressif, dilater/contracter, intervertir, mode masque
- Transformation manuelle (échelle, rotation, pivot, perspective) et outil Déplacement ; calque par copier/couper, copier/coller
- Menu Image : taille de l'image, zone de travail, rotations, symétries, recadrage
- 37 effets dont Courbes, Niveaux, Teinte/Saturation, flous, styles de calque (ombre portée/interne, lueur, contour, incrustations, biseau)
- Texte (polices système, multi-ligne, alignement, approche, interligne)
- Formes vectorielles SDF (rectangle arrondi, ellipse, ligne, polygone, étoile ; fond + contour)
- Historique groupé, annulable partout

**Fichiers**
- Format natif `.sxl` (JSON + pixels compressés XPRESS), import et export PSD avec calques, import PNG/JPG/TGA/BMP/HDR, export PNG/JPG/TGA/BMP

**Spécifique Shaderixel**
- Nœuds HLSL personnalisés et **éditeur d'effets visuel** (façon éditeur de matériaux Unreal) avec **export vers un matériau Unreal** (script Python : nœud Custom + paramètres + textures)
- **IA** : pont MCP compatible CyToolsCore (26 opérations `shaderixel.*`, écriture soumise à autorisation), compagnon CyUEMCPSDK lancé automatiquement, assistant intégré, génération d'images OpenAI / Gemini
- **Peinture 3D** : vue 3D (OBJ, primitives), document en texture, pinceau projeté avec occultation

## Prochaines étapes

### Performance / grands formats
- Rendu par tuiles : **fait** (au-delà de ~36 MP : tuiles 512 px, halos d'effets, zones modifiées, rendu progressif visible d'abord ; identique au pixel près au rendu complet). Surfaces creuses : **fait** (ressources réservées D3D12, mémoire allouée par tuiles de 64 Ko seulement là où on peint ; démo 16K : 11,3 → 4,75 Go). Reste : historique de peinture hors VRAM, aperçus de nœuds en mode tuiles, fond uni sans mémoire
- Recalcul limité aux régions modifiées
- Réduction d'affichage en zoom arrière : **fait** (moyenne par boîte incrémentale)
- Histogramme GPU : **fait** (panneau Histogramme, statistiques, fond de l'éditeur de courbes)
- Mode 8 bits natif

### Photoshop
- Déplacer le contour de sélection (glisser à l'intérieur, flèches) : **fait** , Transformer la sélection : **fait** , Sélection rapide (croissance géodésique sensible aux bords) : **fait**
- Blend-If (options de fusion) : **fait** (gris, poignées séparables, import/export PSD), par canal R/V/B : **fait**
- Tracés vectoriels : **fait** (outil Plume, édition Ctrl, panneau Tracés : sélection, remplissage, contour au pinceau ; sauvegardés dans le .sxl). Sélection → tracé (contour sous-pixel, simplification, angles conservés ; aller-retour à 0,1 % près) : **fait**. Tracés dans les PSD (ressources 2000+, export et import, vérifiés avec psd-tools) : **fait**. Reste : formes personnalisées depuis un tracé
- Fluidité (pinceau : déformation avant, tourbillons, contraction, dilatation, reconstruction) : **fait**
- LUT 3D (.cube) : **fait** (réglage Correspondance de couleur, LUT embarquée dans le .sxl, type de paramètre `lut` disponible pour les effets)
- Déformation (Édition > Déformation, ou depuis la transformation manuelle : grille de Bézier 4×4, points, poignées, glisser la surface, sélection incluse) : **fait** ; reste : marionnette
- Export PSD avec calques : **fait** (8 bits RLE, groupes, masques, écrêtage, modes ; réglages/remplissages/filtres rastérisés). PSB et 16 bits (ZIP avec prédiction) en export et import : **fait**. Reste : calques de réglage natifs

- Règles, repères, grille et magnétisme : **fait**
- Fusionner vers le bas / les calques visibles / aplatir : **fait**

### 3D
- Import glTF 2.0 (.gltf / .glb) : **fait** ; reste : FBX, plusieurs matériaux / UDIM
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
- Calques de matériau : un remplissage par canal PBR (couleur, rugosité, métal, hauteur, émissif) partageant un masque ; peindre le masque (2D ou sur le modèle) applique tous les canaux : **fait**
- Étagère de matériaux, pile des calques de matériau, propriétés par canal, espace « Peinture 3D » (vue 3D + texture) : **fait**
- À venir : masques intelligents (courbure, occlusion, usure des arêtes), matériaux texturés, plusieurs ensembles de textures

### Pinceaux programmables
- Pinceaux codés dans l'application (`shaders/brushes/*.slang`, prélude `brush_common.slang`) : `Tip(Brush b)` dessine la pointe (couleur + couverture, position locale, pression, graine, temps, distance, direction), `Dynamics(inout Dab d)` place chaque touche (suivre le trait, dispersion, taille/rotation/teinte aléatoires, variation dans le temps) ; réglages `//@spacing`, `//@airbrush` (touches par seconde tant qu'on reste appuyé), `//@extent`, `//@selfflow`, paramètres `//@param` réglables dans Options : **fait**
- Panneau « Pinceaux » : liste par catégories + recherche, aperçu de trait, éditeur de code (Ctrl+Entrée), nouveau pinceau depuis un modèle, enregistrement dans la bibliothèque : **fait**
- 4 exemples (plume, aérographe, fusain, dispersion d'étoiles) + 33 pinceaux CyTexture ; `--test-effects` compile aussi les pinceaux, démo `--demo brushes` : **fait**
- Pinceaux en graphe (éditeur visuel : nœud « Touche du pinceau », pointe ronde, aléatoire par touche ; placement : espacement, aérographe, suivre le trait, dispersion, taille/angle/opacité aléatoires), enregistrables (graphe conservé `//@graph`) : **fait**
- À venir : textures d'entrée pour les pinceaux (TextureBrush), gomme/masque avec pointes programmables testés en profondeur

### Import CyTexture
- 182 presets de CyTexture importés (Dessin 31, Effets 92, Fusions 47, Masques 12) depuis les définitions de `CyTextureMaterialPresetGenerator.cpp` (lu, jamais modifié) : `tools/cytexture/extract_presets.py` puis `convert_presets.py` ; chaque texture devient une entrée de nœud, les 221 effets compilent. Fusions utilisables comme mode de fusion de calque (« Fusions d'effets »). « Advanced » : 93 sur 94 importés via `tools/unreal` (export depuis l’éditeur Unreal, graphes de matériau et de fonctions traduits) ; pinceaux : 33 sur 34 importés en pinceaux programmables (`tools/cytexture/convert_brushes.py`, reste TextureBrush qui lit une texture)
