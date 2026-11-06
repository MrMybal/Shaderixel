---
id: layers
title: Calques, filtres et conversion en pixels
description: Les compositions de calques enregistrent des variantes d’état : visibilité, opacité, fusion, activation des filtres et placement des objets dynamiques.
section: Créer / Retouche 2D
order: 3
sourceRevision: bd823e511906b3db
---

Les compositions de calques enregistrent des variantes d’état : visibilité, opacité, fusion, activation des filtres et placement des objets dynamiques. Elles partagent les pixels des calques plutôt que copier le document. Nommez une composition, modifiez la pile et conservez une autre, puis rappelez/exportez l’état voulu. Peindre une source partagée peut affecter plusieurs variantes.

La composition se construit du bas vers le haut de la pile. Sélectionnez un calque pour ses Propriétés. La visibilité cache sa contribution ; le verrouillage interdit l'édition. Opacité et fusion règlent son mélange avec le dessous.

```image
{"id": "layers", "src": "assets/manual/layers_fr.png", "alt": "La conversion remplace le contenu ; masque, opacité et fusion restent éditables.", "controls": "minimal"}
```

## Types de calques {#layer-types}

- Pixels : contenu directement peignable.
- Remplissage / Draw : contenu procédural sans image d'entrée.
- Réglage : effet appliqué à la composition inférieure.
- Forme : géométrie, remplissage et contour éditables.
- Texte : contenu et typographie éditables.
- Objet dynamique : source réutilisable incorporée ou liée.
- Groupe : conteneur de calques enfants.

Ctrl+Maj+N ajoute un calque de pixels. Ctrl+J duplique ou crée un calque par copie lorsqu'une sélection existe. Ctrl+G groupe ; Ctrl+Maj+G dissocie. Changer l'ordre modifie la composition.

## Filtres non destructifs {#non-destructive-filters}

Un filtre de calque garde ses paramètres éditables ; désactivez-le ou retirez-le pour comparer. Un filtre destructif change directement les pixels. Un calque de réglage traite le dessous sans devenir une source peignable.

Ombres, lueurs, contours et biseaux dépendent de la couverture. Prévoyez de la place dans le canevas pour les débordements.

## Convertir en calque de pixels {#bake-to-pixel-layer}

Le bouton de Propriétés, le menu Calque et le menu contextuel proposent la conversion (Bake to Pixel Layer en anglais). Elle intègre contenu rendu, transformation et filtres dans des pixels à la taille du document. Générateurs, texte, formes, objets dynamiques et calques filtrés sont pris en charge.

Nom, ordre, visibilité, masque, opacité, écrêtage et fusion restent séparés et éditables. Les contrôles du contenu source sont remplacés par les pixels ; Ctrl+Z restaure le calque éditable.

Déverrouillez la cible d'abord. Groupes et réglages utilisent les commandes de fusion. Convertir une instance d'objet dynamique ne modifie ni sa source ni ses autres instances.

## Historique et instantanés {#history-and-snapshots}

Annuler/Rétablir parcourt les modifications de la session. Créer un instantané dans Historique mémorise l'état complet : calques, paramètres, pixels et profil couleur. Le sélectionner restaure cet état ; conservez le travail actuel avant de revenir à un ancien instantané. Ce n'est ni une image exportée ni un remplacement de la sauvegarde native.

## Fusionner {#merge-choices}

Ctrl+E fusionne vers le bas ; Ctrl+Maj+E fusionne les calques visibles. Aplatir combine le résultat visible. Gardez une copie native éditable avant de supprimer la pile d'origine.

[Masques](doc:masks)
[Fusion et Blend If](doc:blending)
