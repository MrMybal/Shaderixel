---
id: workspace
title: Espace de travail et navigation
description: Le canevas affiche le résultat du document.
section: Manuel
order: 1
parent: getting-started
sourceRevision: c59728de2eab8d0c
---

Le canevas affiche le résultat du document. Calques décrit la pile et Graphe les connexions d'images. Propriétés modifie le calque ou nœud sélectionné. Effets regroupe filtres et générateurs ; Fusion contient les réglages de composition. L'Éditeur d'effets travaille sur un autre graphe, interne à un effet.

## Retrouver et organiser les panneaux {#find-and-arrange-panels}

Le menu Fenêtre affiche les panneaux. La colonne de droite regroupe Général, Outils, Effets, Fusion, Peinture, 3D et IA. Alt+1 à Alt+7 sélectionne ces groupes. Hors saisie de texte, Tab masque/rétablit la zone inférieure et Maj+Tab la colonne droite.

Les panneaux s'ancrent, se déplacent dans des fenêtres supplémentaires et se sauvegardent avec les dispositions. F11 agrandit le panneau survolé dans sa propre fenêtre. Si un panneau manque, vérifiez Fenêtre et rétablissez la disposition avant de modifier le document.

## Examiner l'image {#inspect-the-image}

Ctrl+0 adapte l'image au canevas ; Ctrl+1 affiche à 100 %. Utilisez Main (H) ou maintenez Espace hors saisie pour déplacer la vue. Le zoom ne change pas les dimensions exportées.

Affichage > Mode de vue permet de voir la composition finale, le calque sélectionné seul ou le résultat jusqu'à ce calque. Revenez à Image finale pour juger toute la composition.

## Contrôles et langue {#controls-and-language}

De nombreux panneaux ont un filtre ; Ctrl+F le cible lorsqu'il est disponible. Effacez-le si des contrôles semblent avoir disparu.

Double-cliquez ou Ctrl+cliquez sur une valeur numérique pour la saisir. Entrée valide, Échap annule et Tab passe au contrôle suivant. Les décimales acceptent point ou virgule ; les paramètres entiers restent entiers. Un contrôle désactivé ne se modifie pas.

Affichage > Langue change l'interface. L’aide ouverte depuis l’application suit sa langue ; le sélecteur du lecteur permet de choisir indépendamment le français ou l’anglais.

F1 ouvre l'aide du panneau actif ou du nœud sélectionné. Propriétés > Aide sur la sélection et le menu contextuel d'un nœud ouvrent la même fiche. Aide > Documentation rouvre le manuel local ; le sommaire CyDoc mène aux sections et le zoom du navigateur agrandit l’article sans modifier l’échelle de l’éditeur.

Avec Canevas, Vue 3D, la barre ou Options actif, F1 ouvre la fiche de l'outil courant. Options > Aide de l'outil et Aide > Référence des outils donnent également un accès direct.

[Valeurs et dimensions](doc:dimensions)
[Raccourcis](doc:shortcuts)
[Référence des outils et commandes](doc:tools)


## Comparer les espaces de travail {#workspace-gallery}

Le graphe image assemble les calques ; le graphe d’effet définit un calcul par pixel. Sélectionnez une capture pour identifier la bonne surface.

```gallery
{"id": "workspace-gallery", "title": "Espaces de travail de Shaderixel", "controls": "minimal", "items": [{"type": "image", "id": "layers_view", "src": "assets/manual/layers_view_fr.png", "title": "Travail en calques", "alt": "Modifier la composition et les propriétés du calque sélectionné.", "description": "Modifier la composition et les propriétés du calque sélectionné.", "label": "Travail en calques", "controls": "minimal"}, {"type": "image", "id": "effect_graph", "src": "assets/manual/effect_graph_fr.png", "title": "Graphe visuel d’effet", "alt": "Construire le calcul interne d’un effet.", "description": "Construire le calcul interne d’un effet.", "label": "Graphe visuel d’effet", "controls": "minimal"}, {"type": "image", "id": "shader_editor", "src": "assets/manual/shader_editor_fr.png", "title": "Éditeur de shader", "alt": "Modifier le code du shader et ses paramètres exposés.", "description": "Modifier le code du shader et ses paramètres exposés.", "label": "Éditeur de shader", "controls": "minimal"}]}
```
