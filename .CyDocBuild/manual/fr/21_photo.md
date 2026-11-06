---
id: photo
title: Développement photo et analyse
description: 1.
section: Manuel
order: 20
parent: color
sourceRevision: 79e7ce2cf3f63229
---

```image
{"id": "photo-view", "src": "assets/manual/photo_view_fr.png", "alt": "Développement Photo RAW et réglages", "controls": "minimal"}
```

## Ordre de travail reproductible {#a-repeatable-editing-order}

1. Confirmez le profil et préservez l’original. Réglez exposition/balance sans écrêter les hautes lumières utiles.
2. Équilibrez ombres/lumières puis courbes et contraste local. Jugez peau, neutres et bords au zoom pertinent.
3. Corrigez localement par masques puis retouchez une copie de pixels. Réservez grain et netteté de sortie à la taille finale.
4. Comparez composition et surimpressions, désactivez le réglage pour l’avant/après, enregistrez en natif puis exportez séparément.

Photo RAW possède des réglages locaux/optiques selon prise en charge. La correction optique dépend des données appareil/objectif disponibles ; un décodeur/profil absent n’est pas réparé par un preset. Inspectez métadonnées et aperçu avant application.

## Panorama, alignement, HDR et planches {#panorama-alignment-hdr-and-contact-sheets}

Photomerge prend au moins deux fichiers avec dispositions automatique, perspective, cylindrique ou repositionnement et masques de fusion optionnels. Choisissez un recouvrement adapté puis inspectez mouvements et coutures. Il ne fournit pas de détail mesuré dans les coins non couverts.

Alignement automatique travaille sur les calques de pixels visibles avec celui du bas comme référence, en perspective ou translation. Fusion automatique crée des masques pour panorama ou empilement de netteté, sans créer de détail absent des sources. Gardez les calques pour affiner les masques.

Fusion HDR aligne plusieurs expositions d’une scène et crée un document linéaire 32 bits. Les mouvements peuvent laisser des artefacts. Affichage/tonemapping sont distincts de la plage conservée. Une planche contact dispose des images ; vérifiez page, libellés et destination avant un dossier complet.

Gardez l'original et utilisez filtres/réglages pour une édition réversible. Photo Essentials Develop combine des contrôles courants ; les effets individuels restent disponibles.

## Développer {#development}

Réglez balance des blancs et exposition, puis ombres/lumières, contraste local, vibrance et netteté. Strength mélange avec l'original ; un masque limite la correction.

Courbes règle les canaux. Correspondance de couleur charge une LUT .cube. Grain, virage et vignette sont des finitions ; ils ne restaurent pas des détails écrêtés.

Photo RAW propose un développement dédié. La lecture dépend du décodeur distribué et du format ; inspectez le décodage.

## Analyser et composer {#analysis-and-compositing}

Histogramme et Analyse d'image montrent luminosité, canaux et écrêtage. Les surimpressions du canevas sont des modes d'affichage ; revenez au résultat normal pour juger le rendu.

Avant/Après et la visibilité des calques aident à comparer. Vérifiez l'image analysée si l'histogramme semble vide ou inattendu.

Masks Color Key and Despill détoure par distance de couleur, sans reconnaissance sémantique. Matte Cleanup affine la couverture ; Composite with Matte compose avec un masque explicite.

Retouchez une cible de pixels ou une copie convertie et inspectez les contours à pleine échelle. Panorama, alignement, fusion, HDR, planches contact et lots se trouvent dans Automatisation et nécessitent des sources adaptées.

[Retouche](doc:painting)
[Fusion](doc:blending)
