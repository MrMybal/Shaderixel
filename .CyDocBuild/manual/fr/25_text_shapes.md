---
id: text-shapes
title: Texte, formes et géométrie booléenne
description: Choisissez Texte (T), cliquez pour créer le calque puis saisissez le contenu dans Propriétés.
section: Manuel
order: 24
parent: layers
sourceRevision: 7b75f4ecbc5653d9
---

## Texte éditable {#editable-text}

Choisissez Texte (T), cliquez pour créer le calque puis saisissez le contenu dans Propriétés. Cliquer un texte existant l'édite. Réglez police, taille, couleur, alignement et interligne avant le placement.

Gardez le texte éditable pour corriger orthographe et mise en page. Un dégradé écrêté au-dessus colore les glyphes via leur couverture. Les filtres agissent sur le rendu sans changer le contenu.

Les polices doivent être disponibles sur l'autre machine pour une édition reproductible. Pour livrer une image, exportez la composition. Convertir en pixels rend les glyphes peignables ; annuler restaure le texte.

## Styles de texte {#text-styles}

Styles de texte conserve des réglages typographiques séparés du contenu. Vérifiez police disponible, alignement, interligne et cadre après application à un autre texte. Un calque reste modifiable pendant que ses pixels calculés passent dans les filtres.

## Formes {#shapes}

Pour une géométrie exacte, saisissez largeur/hauteur propres à la forme en pixels, avec proportions désactivées si nécessaire. Rotation, contour, ombre et transformation de calque peuvent changer les limites visibles. Utilisez des décimales si voulu et inspectez l’anticrénelage à la taille finale.

Choisissez Forme (U), tracez le cadre puis réglez type, remplissage, contour et géométrie dans Propriétés. Rectangle, ellipse, polygone, étoile et ligne ont des paramètres distincts. Le cadre précède rotation, contour et filtres.

Les composants booléens combinent par union, soustraction, intersection ou exclusion selon les options. Vérifiez ordre et liste : soustraire n'est pas cacher. Modifiez le composant voulu plutôt que le calque entier.

## Badge d'interface {#practical-ui-badge}

1. Créez un rectangle arrondi sur transparence.
2. Entrez ses dimensions en pixels sans verrouiller les proportions.
3. Ajoutez le texte et alignez les calques.
4. Ajoutez ombre/contour en gardant de la place dans le canevas.
5. Sauvegardez le document natif et exportez un PNG transparent.

[Dimensions](doc:dimensions)
[Calques](doc:layers)
