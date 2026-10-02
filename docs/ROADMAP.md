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
- **IA** : pont MCP compatible CyToolsCore (17 opérations `shaderixel.*`, écriture soumise à autorisation), compagnon CyUEMCPSDK lancé automatiquement, assistant intégré, génération d'images OpenAI / Gemini
- **Peinture 3D** : vue 3D (OBJ, primitives), document en texture, pinceau projeté avec occultation

## Prochaines étapes

### Performance / grands formats
- Rendu par tuiles : **fait** (au-delà de ~36 MP : tuiles 512 px, halos d'effets, zones modifiées, rendu progressif visible d'abord ; identique au pixel près au rendu complet). Surfaces creuses : **fait** (ressources réservées D3D12, mémoire allouée par tuiles de 64 Ko seulement là où on peint ; démo 16K : 11,3 → 4,75 Go). Reste : historique de peinture hors VRAM, aperçus de nœuds en mode tuiles, fond uni sans mémoire
- Recalcul limité aux régions modifiées
- Réduction d'affichage en zoom arrière : **fait** (moyenne par boîte incrémentale)
- Histogramme GPU : **fait** (panneau Histogramme, statistiques, fond de l'éditeur de courbes)
- Mode 8 bits natif

### Photoshop
- Déplacer le contour de sélection (glisser à l'intérieur, flèches) : **fait** ; reste : transformer la sélection, sélection rapide
- Blend-If (options de fusion) : **fait** (gris, poignées séparables, import/export PSD) ; reste : par canal R/V/B
- Tracés vectoriels (plume), formes personnalisées
- Fluidité (pinceau : déformation avant, tourbillons, contraction, dilatation, reconstruction) : **fait**
- LUT 3D (.cube) : **fait** (réglage Correspondance de couleur, LUT embarquée dans le .sxl, type de paramètre `lut` disponible pour les effets)
- Filtre de déformation (marionnette / grille)
- Export PSD avec calques : **fait** (8 bits RLE, groupes, masques, écrêtage, modes ; réglages/remplissages/filtres rastérisés). Reste : PSB (> 30000 px), 16 bits, calques de réglage natifs

### 3D
- glTF / FBX, plusieurs matériaux
- Canaux PBR liés (rugosité, métal, normale, hauteur) et rendu PBR
- Pinceau en espace UV / triplanar, masquage par ID de matériau

### IA
- ComfyUI (workflows API), retouche par inpainting depuis la sélection
- Opérations MCP supplémentaires (graphes d'effets, nœuds)

### Multi-plateforme
- Backends Vulkan (Linux) et Metal (macOS) derrière `gpu::Device` ; Slang cible déjà SPIR-V et Metal
