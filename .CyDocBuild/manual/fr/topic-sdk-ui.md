---
id: sdk-ui
title: Interface des panneaux natifs
description: Dessiner les contrôles d’un plugin avec la table SxUi fournie par l’hôte.
section: Développer / Plugins
order: 100
parent: sdk-abi
sourceRevision: 9b176194d6026ce6
---

## Dessiner dans un rappel de panneau {#drawing}

`SxUi` est une table de contrôles en mode immédiat accessible par `host->ui`. Shaderixel crée la fenêtre ; votre rappel `SxPanelFn` dessine son contenu. Appelez ces fonctions uniquement depuis le rappel d’interface natif.

Les contrôles booléens et numériques modifient la valeur fournie et signalent ses changements. Associez `push_id` à `pop_id` pour les contrôles répétés. `same_line`, `separator` et `set_next_width` organisent les prochains contrôles ; `tooltip` décrit le précédent.

```c
static float strength = 0.5f;
host->ui->slider_float("Intensité", &strength, 0.0f, 1.0f, "%.2f");
```

## Version et valeurs {#versions}

Utilisez `SX_PLUGIN_API_VERSION` pour vérifier l’ABI hôte. Le bouton d’échantillon de couleur exige l’API 3. L’utilisation directe d’ImGui est facultative et exige la même version que l’hôte ; la table SxUi évite cette dépendance directe.

Chaque sous-page décrit la signature publique du rappel. Les tampons UTF-8 doivent conserver une place pour le caractère nul final.
