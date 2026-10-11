# Plugins Shaderixel

Un plugin ajoute à Shaderixel des **commandes de menu**, des **fenêtres** (ancrables comme les autres panneaux), des **outils** pour le canevas et des **opérations** utilisables par l'assistant IA, l'éditeur d'actions et les autres plugins — sans toucher au code de Shaderixel.

Deux sortes de plugins :

| | Natif (DLL C/C++) | Script (Python ou autre programme) |
|---|---|---|
| Ajoute | commandes, fenêtres, outils, opérations | commandes de menu |
| Tourne | dans Shaderixel (thread principal) | dans son propre processus |
| Pilote Shaderixel | `host->call(opération, json)` | pont HTTP local (`shaderixel_client.py`) |
| Compilation | oui (n'importe quel compilateur Windows x64) | non |

## Emplacement

Un dossier par plugin, avec un `plugin.json` :

- `<Shaderixel>\plugins\<Nom>\` (livrés avec l'application) ;
- `%LOCALAPPDATA%\Shaderixel\Plugins\<Nom>\` (les vôtres).

Menu **Plugins > Gestionnaire de plugins...** : liste, activer / désactiver (mémorisé), **Recharger** (une DLL recompilée est rechargée sans redémarrer : Shaderixel charge une copie, l'original reste modifiable), rechercher les nouveaux, ouvrir le dossier utilisateur.

## plugin.json

```json
{
  "id": "studio.mon-plugin",
  "name": "Mon plugin",
  "version": "1.0",
  "author": "Moi",
  "description": "Ce que fait le plugin.",
  "dll": "mon_plugin.dll",
  "commands": [
    {"menu": "Mon plugin/Faire quelque chose", "run": "python faire.py --fort", "description": "..."}
  ]
}
```

- `dll` : plugin natif (optionnel).
- `commands` : commandes de script (optionnel). `run` est une ligne de commande lancée dans le dossier du plugin ; `menu` est le chemin dans le menu Plugins (`/` = sous-menu).

Un plugin peut avoir les deux.

## Plugin natif

En-tête unique : `SDK\include\shaderixel_plugin.h` (API C, versionnée par `SX_PLUGIN_API_VERSION`). La DLL exporte :

```c
#include "shaderixel_plugin.h"

static const SxHost* H;

static void Bonjour(void* user) { H->status("Bonjour !"); }

static void Fenetre(void* user) {
    static float force = 0.5f;
    H->ui->slider_float("Force", &force, 0, 1, "%.2f");
    if (H->ui->button("Flou")) H->call("shaderixel.filter_add", "{\"layer\": 2, \"effect\": \"gaussian_blur\"}", NULL, NULL);
}

SX_PLUGIN_EXPORT int sx_plugin_load(const SxHost* host, SxPlugin* plugin) {
    if (host->api_version < SX_PLUGIN_API_VERSION) return 0;
    H = host;
    host->add_command("Mon plugin/Bonjour", Bonjour, NULL);
    host->add_panel("Mon plugin", Fenetre, NULL);
    return 1;
}
```

### Ce que l'hôte fournit (`SxHost`)

| Fonction | Rôle |
|---|---|
| `add_command(chemin, fn, user)` | commande du menu Plugins |
| `add_panel(titre, fn, user)` | fenêtre ancrable (Plugins > Fenêtres) ; `fn` dessine son contenu avec `host->ui` |
| `add_tool(nom, aide, fn, user)` | outil du canevas (barre d'outils et Plugins > Outils) : événements `SX_TOOL_PRESS / DRAG / RELEASE / HOVER` en pixels du document, et `SX_TOOL_DRAW` pour dessiner un aperçu (`draw_line`, `draw_circle`, `draw_rect`, `draw_text`) |
| `add_operation(id, description, schéma JSON, écrit, fn, user)` | opération appelable par l'assistant IA (MCP), les actions, les autres plugins ; `fn` reçoit les paramètres en JSON et rend un JSON (`{"error": "..."}` pour échouer) |
| `call(opération, paramètres JSON, done, user)` | lance une opération de Shaderixel ou d'un plugin ; le résultat arrive dans `done` (tout de suite ou quelques images plus tard) |
| `operations_json()` | liste des opérations (id, description, schéma des paramètres) |
| `document_size`, `workspace_dir`, `open_panel`, `select_tool`, `log`, `status` | utilitaires |
| `active_layer()` (API 2) | id du calque actif |
| `pixels_read(calque, x, y, l, h, format, done, user)` (API 2) | lit un rectangle d'un calque de pixels (calque 0 : la cible de peinture active ; l ou h ≤ 0 : tout le calque) ; le résultat arrive dans `done` à l'image suivante |
| `pixels_write(calque, x, y, l, h, format, pixels, nom)` (API 2) | écrit un rectangle (une étape d'annulation `nom`) |
| `ui` | widgets : texte, bouton, case à cocher, curseurs, saisie de texte, couleur, liste, séparateur, info-bulle, en-tête repliable... |
| `imgui_context()` | avancé : le contexte Dear ImGui de Shaderixel pour un plugin compilé avec la même version d'ImGui (`imgui_version`) |

Formats de pixels : `SX_PIXELS_RGBA8` (4 octets, sRGB, alpha droit, comme un PNG) ou `SX_PIXELS_RGBA32F` (4 flottants, linéaire, alpha prémultiplié, comme les calques sont stockés).

Les **outils** marchent aussi dans la **Vue 3D** des projets Paint 3D (API 2) : l'événement porte `view = 1` et `x, y` = le pixel de la texture sous le curseur (`-1` hors du modèle) ; un plugin qui peint en (x, y) peint donc sur le modèle. Pas d'aperçu dessiné dans la vue 3D.

Compatibilité : `SX_PLUGIN_API_VERSION` augmente quand l'hôte ajoute des fonctions (toujours à la fin des structures) ; un plugin vérifie `host->api_version` avant d'utiliser une fonction récente. Un plugin compilé pour une version plus ancienne continue de fonctionner.

Tout est appelé sur le thread principal, entre deux images : un plugin ne doit pas bloquer (travail long : un thread à lui, puis `call` depuis une commande / une fenêtre).

### Opérations de Shaderixel

Les mêmes que l'assistant IA : `shaderixel.document_info`, `document_new`, `document_save`, `layer_add`, `layer_update`, `layer_delete`, `filter_add`, `fill`, `paint_stroke`, `select`, `image_import`, `render` (image en PNG), `undo`, `redo`, `effects_list`, `effect_from_hlsl`, `graph_info`, `node_add`, `node_link`... Leur description et leurs paramètres : `host->operations_json()`.

### Compiler

`SDK\examples\example_plugin` : un exemple complet (commandes, fenêtre, outil Étoile avec aperçu, opération `exemple.etoile`, inversion des couleurs du calque par `pixels_read` / `pixels_write`) avec son `CMakeLists.txt` :

```
cmake -S SDK\examples\example_plugin -B build -A x64
cmake --build build --config Release
```

puis copier `example_plugin.dll` et `plugin.json` dans `%LOCALAPPDATA%\Shaderixel\Plugins\Exemple\` et **Plugins > Gestionnaire > Rechercher de nouveaux plugins** (ou **Recharger** après une modification).

## Plugin script

Les commandes du `plugin.json` lancent un programme avec, dans l'environnement :

- `SHADERIXEL_URL` : adresse du pont local de Shaderixel ;
- `SHADERIXEL_TOKEN` : jeton d'accès (les scripts lancés depuis le menu peuvent modifier le document) ;
- `SHADERIXEL_WORKSPACE` : dossier pour les fichiers temporaires.

Avec Python, `SDK\examples\script_plugin\shaderixel_client.py` (bibliothèque standard seulement) :

```python
from shaderixel_client import call
info = call("shaderixel.document_info")
calque = call("shaderixel.layer_add", {"kind": "pixel", "name": "Mon calque"})
call("shaderixel.fill", {"color": [1, 0.5, 0, 1], "layer": calque["id"]})
print("Terminé")   # la dernière ligne affichée va dans la barre d'état
```

La sortie du script est gardée dans `%TEMP%\Shaderixel\plugins\<id>\run_*.log`.
