---
id: masks
title: Sélections, masques et tracés
description: 1.
section: Manuel
order: 4
parent: layers
sourceRevision: 7d0d484943cf6b02
---

## Construire puis affiner un masque {#build-and-refine-a-mask}

1. Sélectionnez avec l’outil adapté ou Plage de couleurs. Ajoutez/retirez les régions avant le contour progressif.
2. Inspectez sur fond contrasté ; affinez douceur et régions isolées. Sélectionner et masquer et Zone de netteté sont des chemins différents, pas des sélections identiques.
3. Ajoutez un masque et choisissez sa vignette. Noir cache, blanc révèle et gris couvre partiellement. Une sélection active limite encore les traits.
4. Revenez à la vignette image pour retoucher la couleur. Vérifiez écrêtage et Blend If si un masque blanc ne suffit pas.

Appliquer une image combine une source/canal dans une cible ; Calculs combine des canaux pour sélection/masque. Vérifiez document/calque/canal source, destination, fusion et inversion avant validation. Ces commandes ne reconnaissent pas automatiquement les sujets. Préservez les masques originaux pendant les essais.

Une sélection limite une opération à une zone. Un masque de calque règle sa couverture. L'écrêtage utilise la couverture d'un autre calque. Ces mécanismes peuvent agir ensemble.

## Sélections {#selections}

Utilisez Rectangle/Ellipse (M / Maj+M), Lasso (L), Lasso polygonal (Maj+L), Sélection rapide (W) ou Baguette magique (Maj+W). Les options règlent ajout, soustraction et bords.

Ctrl+A sélectionne tout ; Ctrl+D désélectionne ; Ctrl+Maj+D rétablit la sélection ; Ctrl+Maj+I l'inverse. Maj+F6 ouvre le contour progressif. Masque rapide (Q) donne une vue orientée masque.

Si un trait ne touche qu'une petite région, cherchez une sélection active avant de modifier le pinceau.

## Masques de calque et écrêtage {#layer-masks-and-clipping}

Ajoutez un masque dans Calque ou les contrôles de calque. Sa vignette sélectionnée reçoit la peinture : blanc révèle, noir cache, gris révèle partiellement. Resélectionnez la vignette de l'image pour peindre en couleur.

Ctrl+Alt+G active l'écrêtage d'un calque hors groupe. Il utilise la couverture de la base ; il ne crée pas un masque peint indépendant.

## Masques du graphe {#graph-masks}

Un masque peut être une image ou un canal connecté à une entrée. Certains attendent une couverture linéaire ; un PNG numérique en niveaux de gris importé comme image couleur subit une courbe de transfert.

Masks Imported Matte to Coverage convertit ces valeurs en couverture linéaire. Alpha Extraction and Cleanup affine l'alpha. Évitez de multiplier deux fois un alpha déjà appliqué.

## Tracés {#paths}

Plume (P) crée des tracés. Le panneau Tracés les remplit, les contourne ou les transforme en sélection. Créer un tracé depuis la sélection dépend d'une tolérance. Gardez le tracé d'origine pour de futures modifications géométriques.

[Peinture et retouche](doc:painting)
[Graphe d'images](doc:image-graph)
