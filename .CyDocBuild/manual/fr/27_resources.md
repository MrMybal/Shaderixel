---
id: resources
title: Explorateur de ressources et éléments réutilisables
description: Fichier > Importer des ressources accepte ABR, PAT et ACO/ASE/GPL.
section: Manuel
order: 26
parent: libraries
sourceRevision: 7c2e9ef6d08a11f6
---

## Importer et vérifier une collection {#import-and-verify-a-resource-pack}

Fichier > Importer des ressources accepte ABR, PAT et ACO/ASE/GPL. Inspectez un élément avant une grosse collection : empreinte image, shader procédural et preset dynamique sont différents. Définir un motif/pinceau crée une ressource depuis le contenu courant ; vérifiez sélection et transparence.

Insérez une fonction par la bibliothèque du graphe, ouvrez un document par Fichier et créez depuis un modèle. Une ressource n’installe ni modèle IA ni plugin natif requis. Gardez dépendances/licences avec les collections partagées et les identifiants d’effets stables dans les fonctions.

## Parcourir et placer {#browse-and-place}

Ouvrez Ressources dans Fenêtre. Parcourez les dossiers, épinglez les emplacements et consultez les vignettes. Le menu contextuel propose ouvrir, placer comme objet dynamique incorporé/lié ou ajouter une image au graphe selon la ressource et le document.

La destination du dépôt compte. Un document s'ouvre comme projet ; une image placée dans une composition peut créer un calque ou nœud. Si le dépôt ne fait pas ce que vous attendez, utilisez la commande explicite.

## Familles {#resource-families}

- Pinceaux : .abr et pointes programmables.
- Motifs : .pat et images réutilisables.
- Nuanciers : .aco, .ase et .gpl.
- Fonctions : sous-graphes et outils.
- Modèles : documents de départ.
- Matériaux intelligents : piles de peinture 3D.
- Scènes : configurations de rendu.

Ces ressources ne sont pas interchangeables. Un .sxfn insère une fonction, sans scène peinte complète. Un modèle démarre un document plutôt qu'un calcul direct paramétrable.

## Transporter un projet {#portability}

Images liées, textures de pinceau, maillages et environnements peuvent dépendre de chemins externes. Incorporez le contenu compatible ou rassemblez les dépendances. Les ressources livrées sont près de l'application ; les ressources utilisateur dans les dossiers Shaderixel documentés.

[Documents et modèles](doc:documents)
[Objets dynamiques](doc:smart-objects)
