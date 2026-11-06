---
id: delivery
title: Livrer des assets UI, PBR et multicouches
description: Gardez un original natif modifiable et exportez une livraison séparée.
section: Manuel
order: 41
parent: export
sourceRevision: 840eb5f6e6ce6c7a
---

Gardez un original natif modifiable et exportez une livraison séparée. Confirmez l’attente du logiciel destinataire : couleur affichée, carte numérique, canaux groupés, document multicouche ou matériau de moteur.

```image
{"id": "delivery", "src": "assets/manual/delivery_fr.png", "alt": "De l’original modifiable aux textures livrées vérifiées", "controls": "minimal"}
```

## Images UI transparentes {#transparent-ui-images}

1. Terminez panneau/texte/icône aux dimensions en pixels voulues. Le zoom ne change pas la taille exportée.
2. Réservez de la place aux ombres/lueurs ou recadrez volontairement. Un générateur de fond opaque supprime la transparence.
3. Utilisez Exporter sous en PNG avec l’alpha voulu. JPEG exige un fond puisqu’il n’a pas d’alpha.
4. Rouvrez l’image et vérifiez dimensions/bords sur fonds clair et sombre. Un atlas à découpe exige des marges/régions conformes à l’image exportée.
5. Fournissez marges et origine des coordonnées au widget destinataire. Le PNG ne contient pas de graphe UV Shaderixel modifiable.

## Presets d’export de matériau {#material-export-presets}

Exporter les textures du projet assemble les canaux et peut intégrer la hauteur aux normales exportées. Choisissez jeu de matériaux, dossier, nom, résolution, format, convention de normales et marge. Les cartes numériques sont écrites comme données ; configurez leurs indicateurs d’import dans le moteur.

- Unreal Engine ORM : BaseColor, Normal DirectX, ORM R=AO/V=rugosité/B=métallicité, Emissive.
- Unity HDRP : BaseMap, Normal OpenGL, MaskMap R=métal/V=AO/B=0/A=lissé, Emissive.
- Unity URP/Built-in métallique : BaseMap, Normal OpenGL, MetallicSmoothness RVB=métal/A=lissé, Occlusion et Emission séparées.
- glTF/Blender : BaseColor, Normal OpenGL, OcclusionRoughnessMetallic R=AO/V=rugosité/B=métal, Emissive.
- Godot : Albedo, Normal OpenGL, ORM avec les mêmes canaux, Emission.
- Textures séparées : couleur, rugosité, métal, normale DirectX, émissif, AO et hauteur.

Le lissé vaut 1-rugosité. DirectX/OpenGL diffèrent par le sens vert/Y ; convertissez une seule fois. Ces presets décrivent les fichiers, pas le comportement automatique de l’importeur destinataire. Vérifiez chaque slot, notamment les cartes groupées et les normales.

## Marge, taille et précision {#padding-size-and-precision}

La marge des îlots étend les pixels valides dans la gouttière pour limiter les débordements du filtrage. Elle ne répare pas un mauvais dépliage ni un détail absent. Inspectez les vues lointaines/avec mipmaps, pas seulement la texture à plat.

Choisissez PNG 16 bits ou une sortie adaptée si précision et dégradés comptent. Un document float 32 bits exporté par un outil PNG du graphe donne encore une image 8 bits. Agrandissement et IA ne reconstituent pas une mesure réelle de rugosité ou profondeur.

## Échange multicouche et impression {#layered-exchange-and-print}

PSD/PSB permet une livraison multicouche 8/16 bits sans équivalent exact pour tous les effets procéduraux. Ouvrez le fichier dans le logiciel destinataire et comparez aspect et édition. Gardez .sxl/.sx3d comme original.

Pour imprimer, choisissez un profil ICC de destination adapté et inspectez épreuvage/gamut avant TIFF CMJN. Une épreuve à l’écran ne transforme pas un PNG en document CMJN. Les cartes numériques ne doivent pas subir de conversion d’impression.

## Chemins Unreal {#unreal-paths}

Export modèle/matériau et ShaderixelLink livrent un projet à Unreal. L’export de l’Éditeur d’effets produit plutôt du HLSL Custom et un script de matériau, avec des limites propres. L’import d’une texture .uasset diffère de la conversion de maillage assistée par moteur, qui peut nécessiter Unreal Editor installé.

## Vérifier la livraison {#handoff-checklist}

- Rouvrez originaux et exports depuis leur emplacement final.
- Vérifiez dimensions, alpha, normale Y, espaces colorimétriques et ordre des canaux.
- Incluez images liées, polices, modèles/HDRI et workflows nécessaires.
- Nommez distinctement les variantes. Un export du graphe peut remplacer sa cible ; la politique des lots se règle séparément.
- Vérifiez les erreurs et retirez clés/chemins privés des rapports partagés.

[Opérations d’export](doc:export)
[Couleur et cartes numériques](doc:color)
[Formats de fichiers](doc:formats)
