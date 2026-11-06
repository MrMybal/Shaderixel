---
id: image-graph
title: Graphe d'images principal
description: Le graphe principal relie images complètes, canaux, valeurs et actions d'exécution.
section: Créer / Graphes
order: 8
sourceRevision: fec167db155fe163
---

[Référence de toutes les structures, valeurs et actions intégrées](doc:builtin-nodes)

Le graphe principal relie images complètes, canaux, valeurs et actions d'exécution. Le graphe d'effet décrit plutôt le calcul interne à un seul effet.

```image
{"id": "graphs", "src": "assets/manual/graphs_fr.png", "alt": "Graphe d’images et graphe de calcul interne à un effet.", "controls": "minimal"}
```

## Obtenir un résultat visible {#build-a-visible-result}

1. Créez ou convertissez un document en mode Nodal.
2. Dans le vide du Graphe, clic droit puis source image ou générateur Draw.
3. Ajoutez un effet et reliez la source à son entrée Image.
4. Reliez l'image finale à Output/Sortie.
5. Sélectionnez les nœuds pour leurs Propriétés ; cadrez la vue si certains sont hors écran.

Un générateur n'a pas besoin d'image d'entrée, contrairement à la plupart des filtres. Sortie débranchée, surface vide ou source transparente peuvent produire un damier.

## Types et paramètres {#types-and-parameters}

Les broches image portent des images ; les canaux des champs scalaires. Les valeurs sont nombres, couleurs, vecteurs ou texte. Les liens d'exécution déclenchent les actions. Les types doivent être compatibles.

Paramètres en broches expose des réglages comme entrées. Une valeur reliée les pilote ; sinon leur valeur par défaut s'applique. Le comportement d'une entrée image vide dépend de l'effet.

Séparer extrait les canaux RVBA. Les mathématiques et variables pilotent les contrôles. Une fonction à sorties multiples exige la bonne broche de sortie.

## Bibliothèque et réutilisation {#library-and-reuse}

Clic droit > Bibliothèque insère une fonction .sxfn. Propriétés expose entrées et Variables ; double-clic ouvre son graphe. Les sous-graphes sont directs ; les outils ont des événements pour cuire, lancer l'IA, boucler ou exporter.

Un outil peut afficher l'entrée avant exécution : cela ne prouve pas que le calcul ou l'export a eu lieu. Sélectionnez des nœuds pour créer une fonction réutilisable et l'enregistrer dans la bibliothèque.

[Exécution et automatisation](doc:automation)
[Éditeur d'effets](doc:effect-editor)
