---
id: plugins-bridge
title: Plugins, scripts et pont local
description: Plugins > Gestionnaire liste plugins et activation.
section: Manuel
order: 32
parent: automation
sourceRevision: f874ad773198fa6d
---

## Gestionnaire de plugins {#plugin-manager}

Plugins > Gestionnaire liste plugins et activation. Il installe un ZIP, recharge, recherche les nouveaux et ouvre le dossier utilisateur. Un ZIP remplace la version occupant la même destination.

Les dossiers sont plugins près de l'application et %LOCALAPPDATA%\Shaderixel\Plugins. Chaque plugin a un plugin.json. Un plugin natif tourne dans Shaderixel ; un script dans un autre processus. Installez des sources auxquelles vous voulez confier votre environnement d'édition.

Créer un plugin génère un squelette script/natif. Le rechargement de développement facilite les recompilations sans redémarrer. L'usage courant demande surtout installation/configuration, pas l'édition du SDK.

## Assistant et pont {#assistant-and-bridge}

Le panneau IA expose le pont local et la configuration des clients compatibles. Les opérations portent sur documents, calques, graphes et ressources. Une opération peut modifier le projet : vérifiez la cible et sauvegardez selon le travail.

Connexion d'assistant, fournisseur de génération d'images et serveur ComfyUI sont des configurations distinctes. Un pont démarré ne signifie pas qu'un modèle externe est installé.

## Références {#developer-references}

Consultez les [manifestes de plugins](doc:plugin-manifests), l’[ABI native](doc:sdk-abi) et l’[interface Python](doc:script-api). Les outils de graphe peuvent automatiser des images sans plugin natif.

[IA et ComfyUI](doc:ai)
[Actions et outils](doc:automation)
