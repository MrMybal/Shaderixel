---
id: plugins-bridge
title: Plugins, scripts et pont local
description: Plugins > Gestionnaire liste plugins et activation.
section: Manuel
order: 32
parent: automation
sourceRevision: 716c20052d4e2b8f
---

## Gestionnaire de plugins {#plugin-manager}

Plugins > Gestionnaire liste plugins et activation. Il installe un ZIP, recharge, recherche les nouveaux et ouvre le dossier utilisateur. Un ZIP remplace la version occupant la même destination.

Les dossiers sont plugins près de l'application et %LOCALAPPDATA%\Shaderixel\Plugins. Chaque plugin a un plugin.json. Un plugin natif tourne dans Shaderixel ; un script dans un autre processus. Installez des sources auxquelles vous voulez confier votre environnement d'édition.

Créer un plugin génère un squelette script/natif. Le rechargement de développement facilite les recompilations sans redémarrer. L'usage courant demande surtout installation/configuration, pas l'édition du SDK.

## Assistant et pont {#assistant-and-bridge}

Dans Assistant IA, démarrez le serveur MCP local et copiez sa configuration pour un client HTTP. Le profil Free permet d’appeler toutes les opérations exposées avec l’adresse locale seule. Les [réglages](doc:settings) décrivent les autres profils. Le chat et le serveur utilisent les mêmes opérations pour les documents, calques, graphes et ressources. `ShaderixelCli.exe list` affiche le catalogue de l’instance ouverte.

Les opérations Paint 3D peuvent créer un projet, régler la caméra et les matières, peindre un trait, modifier un script de particules et rejouer une émission, cuire des cartes et exporter des jeux de textures. `ShaderixelCli.exe describe shaderixel.paint3d_stroke` détaille les coordonnées normalisées dans la vue et les réglages du pinceau.

`shaderixel.ui_state` recherche les contrôles visibles et les menus indexés. `shaderixel.ui_activate` navigue vers un résultat. `shaderixel.ui_input` envoie des clics, glissements, défilements, touches ou du texte dans la fenêtre principale. `shaderixel.ui_capture` renvoie une image privée de cette fenêtre. Les profils contrôlés demandent les autorisations de modification et d’accès aux fichiers pour piloter les menus et les entrées. Les dialogues natifs Windows et les panneaux détachés sont hors du champ de la capture et de la saisie.

Connexion d'assistant, fournisseur de génération d'images et serveur ComfyUI sont des configurations distinctes. Un pont démarré ne signifie pas qu'un modèle externe est installé.

## Références {#developer-references}

Consultez les [manifestes de plugins](doc:plugin-manifests), l’[ABI native](doc:sdk-abi) et l’[interface Python](doc:script-api). Les outils de graphe peuvent automatiser des images sans plugin natif.

[IA et ComfyUI](doc:ai)
[Actions et outils](doc:automation)
