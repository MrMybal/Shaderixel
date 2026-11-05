# Choisir une bibliothèque

Recherchez une famille dans la Bibliothèque de nœuds. La référence Effets décrit les métadonnées et valeurs initiales des shaders livrés. Fonctions décrit les .sxfn, broches, variables exposées et événements. Ces références sont générées depuis les fichiers distribués.

## Photo, composition et graphisme

Photo Essentials regroupe développement, contraste local, virage, grain et vignette. Masks fournit détourage colorimétrique, sélection de luminance, nettoyage de matte et extraction alpha. Strength et les masques limitent la correction.

Graph Layers crée des fonds et panneaux procéduraux ; Graph Effects assemble des réglages. Les .sxfn appartiennent au graphe principal ; les shaders sous-jacents peuvent avoir leurs propres graphes d'effet.

Game Art offre grille de pixels, palettes, contours, états de sprite et contrôle de transparence. Prévoyez du vide pour contours et ombres. Les autres familles couvrent matières, styles, impression/scan et logos.

## Textures et interfaces

PBR et Textures couvrent nettoyage, estimations de cartes, normales, empaquetage, raccords et export. ComfyUI exige un serveur avec les modèles et workflows nécessaires.

UI UV couvre coordonnées, découpes, atlas, répétition et échantillonnage. UI Toolkit ajoute formes SDF, bordures, flux/parallaxe, transitions, motifs et états.

## Direct ou exécuté

Un sous-graphe direct suit ses entrées. Un outil d'export ou d'IA agit sur un événement explicite. Un résultat conservé ne change qu'après réexécution ou effacement. Les presets utilisateur ne rejoignent pas automatiquement la référence embarquée.

[PBR](#pbr)
[Textures UI](#ui_textures)
[IA et agrandissement](#ai)
