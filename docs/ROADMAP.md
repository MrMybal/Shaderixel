# Shaderixel — Feuille de route

## ✅ Socle (fait)
- D3D12 + Slang (HLSL) compilés à l'exécution, en asynchrone, avec cache et rechargement à chaud
- Modes Calques / Nodal, conversion dans les deux sens (vers calques seulement si le graphe est linéaire)
- 27 modes de fusion Photoshop, opacité, masques de fusion, fusion en gamma ou linéaire
- Calques de pixels, de remplissage et de réglage, filtres non destructifs par calque
- Pinceau et gomme : pression tablette, flux/opacité, dureté, espacement, lissage, Maj+clic pour une ligne droite
- Dégradé (5 formes), pot de peinture (remplit tout le calque), pipette, main, zoom
- 27 effets (réglages, flous, netteté, esthétiques, déformations, générateurs, composition)
- Nœud HLSL personnalisé éditable dans l'app
- Annuler/rétablir (structure + pixels), panneau Historique
- Import PNG/JPG/TGA/BMP/PSD (aplati)/HDR, export PNG/JPG/TGA/BMP

## Phase 1 — Outils Photoshop essentiels
- **Sélections** : rectangle, ellipse, lasso, polygone, baguette magique, plage de couleurs, contour progressif ; masque de sélection GPU appliqué à tous les outils et filtres
- Déplacement et transformation libre (échelle, rotation, perspective) des calques
- Groupes de calques (mode traverser), masques d'écrêtage, Blend-If
- Retouche : tampon, correcteur, doigt, densité +/-, éponge (à porter de `CyTextureLayer.usf`)
- Pot de peinture avec remplissage par zone (flood fill GPU)
- Texte (calques de texte vectoriels)
- Courbes (éditeur de courbe), LUT 3D
- Formes vectorielles et tracés
- Recadrage et taille de la zone de travail

## Phase 2 — Format et performance
- Format natif `.sxl` (graphe JSON + surfaces compressées zstd, chargement progressif)
- Import PSD avec calques (en s'appuyant sur `CyTexturePsdImporter`), export PSD aplati
- **Rendu par tuiles et textures sparse** (ressources réservées D3D12) pour le 16K sans saturer la VRAM
- Recalcul limité aux régions modifiées (actuellement : la surface entière en aval)
- Mipmaps d'affichage, histogramme et table de couleurs GPU
- Format 8 bits sRGB natif par document

## Phase 3 — Éditeur d'effets
- Graphe visuel façon éditeur de matériaux d'Unreal (math, UV, bruit, échantillonnage, constantes, paramètres) qui génère le code `Effect()`
- Nœud « Custom HLSL » à entrées typées
- Mode HLSL pur avec un vrai éditeur de code (coloration, erreurs en ligne)
- Sous-graphes et nœuds de groupe réutilisables
- **Export vers un matériau Unreal** (Custom node HLSL + paramètres), branché sur CyTexture

## Phase 4 — Peinture 3D (façon Substance Painter)
- Chargement de maillages (glTF/FBX/OBJ), vue 3D, UV
- Projection du pinceau dans l'espace écran vers les UV, avec corrections de joints (dilatation)
- Canaux PBR (couleur de base, rugosité, métallique, normale, hauteur) sous forme de « documents liés »
- Export des canaux et d'un matériau Unreal

## Phase 5 — IA (via CyToolsCore / CyUEMCPSDK)
- Serveur MCP intégré : piloter calques, nœuds, outils et rendu depuis l'extérieur
- Client : assistant dans l'app, génération et retouche d'images (ComfyUI, OpenAI, Gemini…) via le processus compagnon CyToolsCore
- Résultats IA insérés comme calques ou nœuds

## Phase 6 — Multi-plateforme
- Backends Vulkan (Linux) et Metal (macOS) derrière `gpu::Device`
- Slang cible déjà SPIR-V et Metal : les shaders restent identiques
