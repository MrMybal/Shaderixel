---
id: sdk-types
title: Types du SDK natif et durée de vie
description: Interpréter événements d’outil, tampons de pixels, rappels et champs hôte versionnés.
section: Développer / Plugins
order: 100
parent: sdk-abi
sourceRevision: 30fe2201368a564a
---

## Tables et durée de vie {#tables}

`SxHost` est la table des fonctions hôte. `api_version` identifie l’ABI prise en charge, `app_version` la version de Shaderixel, et `ui` pointe vers `SxUi`. `imgui_version` décrit l’intégration ImGui directe facultative. `SxPlugin` contient un rappel `unload` facultatif et votre pointeur opaque `user`.

Conservez l’état du plugin jusqu’au déchargement ; libérez-le dans le rappel de nettoyage avant la DLL. Les chaînes hôte sont en UTF-8. L’hôte copie les chaînes reçues ; celles qu’il renvoie expirent au prochain appel hôte. Le pointeur `user` est opaque pour Shaderixel.

## Événement d’outil {#tool-event}

`SxToolEvent` est transmis à `SxToolFn` :

| Champ | Sens |
| --- | --- |
| `type` | PRESS=1, DRAG=2, RELEASE=3, HOVER=4, DRAW=5 avec `SX_TOOL_*`. |
| `x`, `y` | Position en pixels du document. |
| `pressure` | De zéro à un pour un stylet ; un pour une souris. |
| `modifiers` | Masque de bits : `SX_MOD_SHIFT=1`, `SX_MOD_CTRL=2`, `SX_MOD_ALT=4`. |
| `zoom` | Pixels écran par pixel du document. |
| `view` | API 2 : zéro pour le canevas, un pour la 3D. En 3D, x/y désignent le pixel de texture, ou -1 hors du modèle ; aucun aperçu d’outil. |

Dessinez les aperçus avec les fonctions hôte pendant `SX_TOOL_DRAW`. Ils ne peignent pas les pixels du document.

## Pixels et événements du document {#pixels-events}

`SX_PIXELS_RGBA8=0` correspond à quatre octets par pixel, en sRGB avec alpha non prémultiplié. `SX_PIXELS_RGBA32F=1` correspond à quatre réels par pixel, en linéaire avec alpha prémultiplié. Un tampon de `SxPixelsFn` reste valide uniquement pendant ce rappel ; copiez-le avant de conserver les données.

`SX_EVENT_DOCUMENT_ACTIVATED=1` comprend le changement de document ou la fermeture du dernier. `SX_EVENT_DOCUMENT_EDITED=2` signale une modification de pixels ou calques au plus une fois par image. `SX_EVENT_DOCUMENT_SAVED=3` fournit le nom du fichier enregistré dans le texte. Enregistrez le rappel avec `on_event` dans l’API 3.

## Signatures des rappels {#callbacks}

```c
void SxCommandFn(void* user);
void SxPanelFn(void* user);
void SxResultFn(void* user, int ok, const char* result_json);
const char* SxOperationFn(void* user, const char* params_json);
void SxToolFn(void* user, const SxToolEvent* e);
void SxPixelsFn(void* user, int ok, int width, int height, const void* pixels);
void SxEventFn(void* user, int event, const char* text);
```

Ce sont les signatures de fonctions de rappel ; les typedefs du SDK sont des pointeurs de fonction. `ok=1` signifie réussite. Un résultat d’opération en échec contient des informations d’erreur JSON. Les chaînes de résultat d’une opération de plugin appartiennent au plugin et restent valides jusqu’à son prochain appel d’opération.

[Fonctions hôte](doc:sdk-host) · [Contrôles des panneaux](doc:sdk-ui)
