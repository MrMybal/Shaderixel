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
- **Sélections** (masque GPU 8 bits) : rectangle, ellipse, lasso, lasso polygonal, baguette magique (contiguë ou non, tolérance), plage de couleurs, depuis les pixels d'un calque (Ctrl+clic) ; modes nouvelle/ajout/soustraction/intersection (Maj/Alt) ; contour progressif, dilater, contracter, intervertir, tout/désélectionner/resélectionner ; pointillés animés et mode masque (Q)
- Tous les outils respectent la sélection (pinceau, gomme, dégradé, remplissage, suppression)
- Calque par copier/couper (Ctrl+J / Ctrl+Maj+J), copier/couper/coller ; masque de fusion et calques de réglage créés depuis la sélection
- **Transformation manuelle** (Ctrl+T) : échelle (Maj proportionnel, Alt depuis le centre), rotation (Maj 15°), pivot, perspective/déformation (Ctrl+coin), interpolation plus proche/bilinéaire/bicubique, la sélection suit ; **outil Déplacement** (V, flèches 1/10 px)
- Historique groupé (une opération = une étape même si elle touche pixels + sélection)
- **Retouche** : tampon de duplication (Alt+clic, aligné), correcteur, goutte, netteté, doigt, densité -/+ (plages), éponge ; pot de peinture avec remplissage par zone (tolérance, contigu, tous les calques)
- **Groupes de calques** (mode Traverser ou fusion, opacité, masque, imbrication, repli) et **masques d'écrêtage** (Ctrl+Alt+G)
- **Format natif .sxl** (JSON + pixels compressés XPRESS en parallèle), invite « enregistrer les modifications »
- **Import PSD avec calques** (8/16/32 bits, RLE/ZIP, groupes, masques, modes, réglages convertis, remplissages unis)
- **Menu Image** : taille de l'image (bicubique), taille de la zone de travail (ancrage), rotations, symétries, **outil Recadrage**, recadrer selon la sélection — tout est annulable
- **Courbes** (éditeur par canal, spline monotone), filtre photo, correction sélective — 30 effets au total

## Phase 1 — Outils Photoshop essentiels
- Déplacer le contour de sélection seul, transformer la sélection, sélection rapide (pinceau)
- Transformation des calques de remplissage/réglage (transformations non destructives)
- Blend-If (options de fusion), styles de calque (ombre portée, contour, lueur...)
- Texte (calques de texte vectoriels)
- LUT 3D (.cube), histogramme
- Formes vectorielles et tracés
- Export PSD avec calques

## Phase 2 — Format et performance
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
