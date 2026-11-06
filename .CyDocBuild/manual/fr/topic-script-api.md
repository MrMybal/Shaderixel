---
id: script-api
title: Interface Python des scripts
description: Appeler les opérations de l’éditeur et décrire des panneaux sans DLL native.
section: Développer / Plugins
order: 100
parent: plugins-bridge
sourceRevision: 959e2a6275002fc8
---

## Lancer depuis Shaderixel {#launch}

Créez un plugin script dans le Gestionnaire de plugins. L’application le lance dans son dossier et fournit `SHADERIXEL_URL`, `SHADERIXEL_TOKEN` et `SHADERIXEL_WORKSPACE` dans son environnement. Ne placez pas ces valeurs dans les manifestes ou fichiers partagés.

Importez `shaderixel_client` et `shaderixel_panel` depuis les utilitaires du SDK fourni. Ils utilisent la bibliothèque standard de Python. `ShaderixelError` signale un pont absent, une erreur d’opération ou un appel sans réponse.

## Lire le document actif {#example}

```python
from shaderixel_client import call
info = call("shaderixel.document_info")
print(info)
```

`describe()` renvoie les identifiants, descriptions et schémas d’entrée des opérations. `call(operation, parameters, workspace)` appelle une opération sur le thread principal de l’éditeur. Son résultat est la valeur JSON de l’opération ; consultez son schéma avant d’utiliser ses champs.

Pour regrouper plusieurs modifications en une annulation, encadrez les appels avec `shaderixel.history_group` et `action: begin`, puis `action: end`. Fermez le groupe même lorsqu’une opération échoue.

## Panneaux de script {#panels}

`Panel` construit des dictionnaires de contrôles et envoie un message JSON par ligne de sortie. Une entrée `panels` de `plugin.json` donne au script une fenêtre ancrable dessinée par l’application.

- `text`, `note`, `separator` et `same_line` organisent les explications.
- `button`, `slider`, `checkbox`, `input`, `color` et `combo` décrivent des contrôles à identifiants stables.
- `on_click` reçoit un identifiant ; `on_change` reçoit un identifiant et une valeur. Ils s’utilisent aussi comme décorateurs.
- `run(ui)` envoie l’interface initiale et traite les événements jusqu’à la fermeture. La fonction `ui` renvoie la liste actuelle des contrôles.
- `set_ui`, `status` et `log` envoient des mises à jour et messages. Réservez `send` à un dictionnaire du protocole, pas à du texte libre.

Shaderixel envoie les événements `open`, `click`, `change` et `close` sur l’entrée standard. Le script envoie les messages `ui`, `status` ou `log` sur la sortie standard. Réservez cette sortie au protocole tant que le panneau s’exécute.

Les sous-pages décrivent les signatures actuelles des utilitaires et leurs paramètres.
