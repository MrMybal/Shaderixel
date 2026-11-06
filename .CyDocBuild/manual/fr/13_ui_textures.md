---
id: ui-textures
title: Textures d'interface et découpes
description: Utilisez 32 bits flottants pour UV et distances signées.
section: Créer / Interfaces
order: 12
sourceRevision: 0efd2dbf1fc264ca
---

Utilisez 32 bits flottants pour UV et distances signées. Une image UV est une donnée ; passez par un échantillonneur pour voir le skin plutôt que les coordonnées colorées.

```image
{"id": "nine-slice", "src": "assets/manual/nine_slice_fr.png", "alt": "Les neuf régions préservent les coins et répètent le centre.", "controls": "minimal"}
```

```image
{"id": "uv-chain", "src": "assets/manual/uv_chain_fr.png", "alt": "Transformez les coordonnées puis échantillonnez l’image source.", "controls": "minimal"}
```

## Nine-slice et eight-slice {#nine-slice-and-eight-slice}

1. Insérez UI UV Nine Slice Panel et connectez le skin source.
2. Réglez région source et taille logique s'il provient d'un atlas.
3. Choisissez les unités de marge et les marges gauche/haut/droite/bas.
4. Réglez taille du widget en pixels du document, position et ancrage.
5. Choisissez séparément Center mode et Edge mode.
6. Connectez Image à la sortie ; UV Map et Coverage sont des données intermédiaires.

Les marges fractionnaires se rapportent à la région source ; les pixels à sa taille logique. Tile répète le centre ; Mirror tile alterne le sens. Un centre Transparent crée une bordure eight-slice. Les petites destinations compriment proportionnellement les bordures.

## Chaîne de coordonnées {#coordinate-chains}

Un champ stocke rouge=U, vert=V, bleu=couverture, alpha=1. Construisez les coordonnées, appliquez transformations/adressage/masques puis échantillonnez. UI UV Sample reçoit image, champ puis masque optionnel.

Clamp, Repeat, Mirror et Transparent diffèrent aux limites. Gouttières et retrait des atlas évitent de lire les cellules voisines. Le visualiseur de champ montre couverture ou distorsion.

## Toolkit et export {#toolkit-and-export}

UI Toolkit ajoute formes/distances signées et opérations booléennes, bordures, flux/parallaxe, transitions, motifs et états. Les distances exigent du flottant et ne doivent pas subir des réglages colorimétriques ordinaires.

Temps, progression, survol, pression et désactivation sont des valeurs explicites. Ces outils génèrent des textures ; ils ne capturent pas les événements d'un widget de jeu.

UI UV Bake Skin PNG exporte tout le canevas sur Export PNG. Pour un sprite serré, adaptez la taille du document. Les exemples sont examples/ui_uv_library_demo.sxl et examples/ui_toolkit_demo.sxl ; leurs guides techniques sont dans docs/.

[Graphe principal](doc:image-graph)
[Export](doc:export)
