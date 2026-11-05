# Formats et échanges entre logiciels

## Familles de fichiers

- Projets modifiables : .sxl et .sx3d. Ouvrez le natif pour retrouver pile/graphe plutôt qu’importer l’export aplati.
- Images courantes : PNG, JPEG, BMP, TGA, HDR, GIF et PSD/PSB multicouches. Ouvrir une image et éditer les images d’animation sont distincts ; n’attendez pas qu’un GIF recrée automatiquement une timeline modifiable.
- Décodeurs étendus : TIFF, WebP, HEIC/HEIF/JXR et familles RAW DNG, CR2/CR3, NEF, ARW, ORF, RW2, RAF notamment. Reconnaître une extension ne garantit pas tous les appareils/codecs dans chaque installation.
- Livraisons : PNG, JPEG, TIFF, TGA et BMP via le dialogue adapté ; PSD/PSB et GIF/MP4 animé ont leurs commandes propres.
- Ressources : effets/pinceaux .slang, fonctions .sxfn, modèles natifs avec métadonnées/vignette, matériaux intelligents JSON, LUT .cube et actions .sxactions. Utilisez l’éditeur/panneau adapté plutôt que les ouvrir comme bitmap.

Un format à alpha ne garantit pas une source transparente. Un fichier 16 bits n’est pas nécessairement flottant. Vérifiez le chemin de décodage/export et rouvrez un échantillon avant une grosse conversion.

## Images et documents en calques

Ouvrez les formats compatibles depuis Fichier ou Ressources. PSD/PSB conserve les informations prises en charge, sans garantir un aller-retour exact de tous les effets d'un autre logiciel. Gardez le projet natif comme original de travail.

Les RAW dépendent du décodeur et du fichier. Les LUT utilisent .cube. Environnements HDR, normales et données scalaires demandent leur interprétation propre plutôt qu'un étalonnage arbitraire.

## Maillages et Unreal

Les chemins disponibles comprennent OBJ, glTF/GLB et FBX selon la version construite. Vérifiez UV, matériaux, orientation et textures après import.

Lire une texture .uasset diffère de convertir un maillage .uasset. Cette conversion peut exiger Unreal Editor installé avec ses outils de commande/glTF. Copier un .uasset ne remplace pas un export assisté par le moteur.

Le connecteur ShaderixelLink échange des données de projet/matière avec Unreal. Vérifiez version, installation et réglages produits de normales, sRGB et canaux empaquetés.

## Transporter

Rassemblez images liées, polices, maillages, HDRI et workflows nécessaires. Les workflows API exigent encore leurs modèles/nœuds ComfyUI. Une image aplatie ne transporte pas son graphe éditable.

[Export](#export)
[Ressources](#resources)
