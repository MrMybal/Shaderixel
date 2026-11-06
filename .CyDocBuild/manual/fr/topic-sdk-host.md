---
id: sdk-host
title: API de l’hôte natif
description: Enregistrer des extensions et accéder aux opérations, pixels, réglages et événements.
section: Développer / Plugins
order: 100
parent: sdk-abi
sourceRevision: 0722495e85e948ef
---

## Utiliser la table hôte {#host-table}

Incluez `shaderixel_plugin.h` et conservez le pointeur `SxHost` uniquement pendant le chargement du plugin. Tous les rappels des plugins natifs s’exécutent sur le thread principal. Vérifiez `host->api_version` avant d’accéder aux champs ajoutés dans l’API 2 ou 3.

L’hôte copie les chaînes reçues. Les chaînes renvoyées restent valides jusqu’au prochain appel hôte. Les tampons de lecture de pixels ne restent valides que pendant leur rappel. Copiez-les avant de conserver leur contenu.

## Choisir un service {#services}

- Enregistrez commandes, panneaux, outils ou opérations avec `add_command`, `add_panel`, `add_tool` et `add_operation`.
- Obtenez les identifiants d’opérations et schémas d’entrée avec `operations_json`, puis appelez une opération avec `call`. Son résultat peut être différé.
- Utilisez `active_layer`, `pixels_read` et `pixels_write` pour une cible de pixels peignable. Choisissez l’espace colorimétrique et la convention alpha adaptés.
- Enregistrez les préférences du plugin avec `setting_get` et `setting_set` ; réagissez aux changements du document avec `on_event`.
- Dessinez les aperçus d’outils uniquement pendant `SX_TOOL_DRAW`. Les coordonnées sont en pixels du document et les couleurs utilisent `0xAABBGGRR`.

Sélectionnez une fonction ci-dessous pour sa signature publique réelle, ses paramètres et son comportement.

[ABI native et formats](doc:sdk-abi) · [Interface des panneaux](doc:sdk-ui)
