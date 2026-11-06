---
id: blending
title: Fusion et Blend If
description: La fusion combine fond et premier plan.
section: Manuel
order: 7
parent: layers
sourceRevision: 7372ef77f531eeee
---

La fusion combine fond et premier plan. L'opacité règle la contribution du premier plan. Le mode agit sur l'interaction des couleurs ; masques et alpha déterminent la couverture.

## Modes et fusions shader {#modes-and-shader-blends}

Choisissez le mode dans Propriétés ou Fusion. Normal compose selon la couverture ; Produit, Écran et les autres modes modifient les couleurs différemment. Une fusion shader a des entrées fond, calque et masque ; permuter les deux images peut changer le résultat.

Un générateur opaque peut cacher tous les calques inférieurs en Normal. Pour une surimpression, réglez transparence, opacité ou mode.

## Blend If {#blend-if}

Ces options conditionnent la visibilité aux tons du calque ou de l'image sous-jacente. Choisissez Gris ou un canal, puis les seuils sombres/clairs.

Chaque barre expose quatre valeurs ordonnées de 0 à 255 : début sombre, fin sombre, début clair et fin claire. Séparer les poignées adoucit la transition ; Alt+glisser les sépare. Les quatre champs sous la barre permettent une saisie exacte, en maintenant l'ordre.

Vérifiez les deux barres si des zones d'un calque opaque disparaissent.

## Couleur et alpha {#color-and-alpha}

Shaderixel utilise des couleurs linéaires. Affichage > Fusion en sRGB change la composition du document et peut modifier les tons moyens. L'échantillonnage prémultiplié des shaders évite les franges sombres, indépendamment du choix d'un format d'export avec alpha.

[Couleur et données](doc:color)
[Masques](doc:masks)
