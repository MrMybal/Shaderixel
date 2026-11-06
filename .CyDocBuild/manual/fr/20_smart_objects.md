---
id: smart-objects
title: Objets dynamiques
description: 1.
section: Manuel
order: 19
parent: layers
sourceRevision: 39d2045db7712cf9
---

## Réutiliser un logo ou décalque {#reuse-a-logo-or-decal-safely}

1. Incorporez la source pour transporter le contenu avec le projet, ou liez-la si un original externe reste la référence.
2. Dupliquez/placez les instances et transformez-les séparément. Identifiez celles qui partagent la source avant de la modifier.
3. Actualisez après édition externe. Si le fichier a bougé, restaurez/reliez la bonne source plutôt que convertir un aperçu absent.
4. Placez effets/masques sur l’instance voulue. Convertissez seulement pour obtenir des pixels indépendants ; transformer l’instance ne réécrit pas le fichier maître.

Un objet dynamique sépare sa source du placement et de l'apparence de ses instances. La source peut être incorporée ou liée à un fichier.

## Sources et instances {#sources-and-instances}

Fichier propose les deux imports. Le contenu incorporé voyage avec le document. Le contenu lié se rafraîchit depuis une source qui doit rester disponible.

Des instances peuvent partager une source et avoir des placements indépendants. Modifier la source peut affecter toutes les instances ; transformer l'une ne redimensionne pas nécessairement la source.

Propriétés permet d'inspecter/modifier le contenu et d'actualiser un lien. Gardez les fichiers externes lors d'un déplacement de projet. Les filtres restent éditables sur chaque instance.

## Rendre peignable {#paintable-conversion}

Convertir en calque de pixels remplace le rendu et les filtres de l'instance sélectionnée par des pixels indépendants à la taille du document. Masque, opacité et fusion restent séparés.

Les autres instances et la source restent intactes. Ctrl+Z restaure l'objet dynamique. Gardez le projet natif pour le retrouver au-delà de la session d'historique.

Un objet dynamique diffère d'un matériau intelligent, assemblage de remplissages et masques de peinture 3D.

[Calques et conversion](doc:layers)
[Documents](doc:documents)
