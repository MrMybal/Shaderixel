---
id: plugin-manifests
title: Installation et manifestes de plugins
description: Installer ou créer des extensions natives et script avec le Gestionnaire de plugins.
section: Développer / Plugins
order: 100
parent: plugins-bridge
sourceRevision: 868939c73daf9697
---

## Installer ou recharger un plugin {#install}

1. Ouvrez Plugins > Gestionnaire de plugins.
2. Installez un ZIP contenant un dossier de plugin et `plugin.json`, ou copiez ce dossier dans `%LOCALAPPDATA%\Shaderixel\Plugins`.
3. Recherchez les nouveaux plugins, activez le plugin et vérifiez ses commandes ou panneaux.
4. Rechargez-le après modification. Le rechargement automatique de développement surveille les DLL natives recompilées.

L’application recherche aussi le dossier `plugins` près de l’exécutable. Installer un ZIP remplace la version située à la même destination. Un plugin natif s’exécute dans Shaderixel sur le thread principal ; un script s’exécute dans un processus séparé. Installez des extensions auxquelles vous confiez votre environnement d’édition.

## Champs du manifeste {#manifest}

```json
{
  "id": "studio.example",
  "name": "Outils d’exemple",
  "version": "1.0",
  "dll": "example.dll"
}
```

- `id`, `name`, `version`, `author` et `description` identifient l’extension.
- `dll` nomme une bibliothèque native relativement au dossier du plugin.
- `commands` est une liste d’objets avec `menu`, `run` et éventuellement `description`. La barre oblique sépare les sous-menus Plugins ; `run` s’exécute dans le dossier du plugin.
- `panels` est une liste avec `title` et `run` ; le processus décrit un panneau dessiné par l’application.

Les entrées natives et script peuvent coexister. Utilisez Créer un plugin dans le Gestionnaire pour obtenir un squelette au format actuel. Suivez la [référence ABI](doc:sdk-abi) pour les rappels natifs et l’[interface Python](doc:script-api) pour les scripts.
