---
id: automation
title: Fonctions, exécution et automatisation
description: Les sous-graphes calculent en direct ; les outils exécutent des événements nommés.
section: Manuel
order: 16
parent: image-graph
sourceRevision: adb6ec6d49bacb8f
---

Les sous-graphes calculent en direct ; les outils exécutent des événements nommés. Une sortie peut être une entrée directe ou un résultat conservé : vérifiez son état.

```image
{"id": "execution", "src": "assets/manual/execution_fr.png", "alt": "Distinguez les images directes de l’exécution explicite.", "controls": "minimal"}
```

## Événements et résultats {#events-and-results}

Sélectionnez l'outil et déclenchez son événement dans Propriétés. Les liens blancs ordonnent les actions ; les images fournissent les données. Une connexion image seule ne lance ni IA ni export.

Bake garde une image calculée ; Upscale et ComfyUI gardent leur résultat après exécution. Effacer les résultats restaure l'aperçu de l'entrée. Les changements en amont ne recalculent pas automatiquement un résultat conservé.

Sequence fixe l'ordre, Branch choisit un chemin et les boucles changent les valeurs entre actions. Les variables exposées appartiennent à chaque instance.

## Réutiliser et enregistrer des actions {#reuse-and-actions}

Sélectionnez des nœuds, créez un sous-graphe/outil, nommez broches et variables, testez valeurs initiales et masques optionnels, puis enregistrez. Un .sxfn est sans pixels ; du contenu image conservé peut demander un document associé.

Actions enregistre les commandes prises en charge ; l'Éditeur d'actions modifie leur séquence. Fichier > Automatisation > Traitement par lots les applique à plusieurs fichiers. La conversion en pixels est enregistrable. Tous les gestes ou résultats externes ne le sont pas ; testez sur une copie avant un lot.

Le guide dédié explique enregistrement, essais partiels, import JSON, lots de dossiers/documents ouverts et conflits de sortie. La catégorie Commandes décrit les 20 types. Dans l'Éditeur d'actions, sélectionnez une étape puis F1 ou Aide de la commande pour paramètres et valeurs initiales.

## Variantes {#variants}

Brackets d'exposition et flashs de sprites utilisent tableaux/boucles. Le suffixe peut être l'indice plutôt que la valeur du paramètre. La dernière variante et les dernières valeurs peuvent rester dans l'outil.

Vérifiez préfixe et destination avant de lancer l'événement.

[Graphe principal](doc:image-graph)
[Créer des presets](doc:authoring)
[Guide des actions et du traitement par lots](doc:actions)
