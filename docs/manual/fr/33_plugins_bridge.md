# Plugins, scripts et pont local

## Gestionnaire de plugins

Plugins > Gestionnaire liste plugins et activation. Il installe un ZIP, recharge, recherche les nouveaux et ouvre le dossier utilisateur. Un ZIP remplace la version occupant la même destination.

Les dossiers sont plugins près de l'application et %LOCALAPPDATA%\Shaderixel\Plugins. Chaque plugin a un plugin.json. Un plugin natif tourne dans Shaderixel ; un script dans un autre processus. Installez des sources auxquelles vous voulez confier votre environnement d'édition.

Créer un plugin génère un squelette script/natif. Le rechargement de développement facilite les recompilations sans redémarrer. L'usage courant demande surtout installation/configuration, pas l'édition du SDK.

## Assistant et pont

Le panneau IA expose le pont local et la configuration des clients compatibles. Les opérations portent sur documents, calques, graphes et ressources. Une opération peut modifier le projet : vérifiez la cible et sauvegardez selon le travail.

Connexion d'assistant, fournisseur de génération d'images et serveur ComfyUI sont des configurations distinctes. Un pont démarré ne signifie pas qu'un modèle externe est installé.

## Références

La distribution inclut exemples SDK et documentation. docs/PLUGINS.md dans le dépôt décrit manifestes et API. Les outils de graphe peuvent automatiser des images sans plugin natif.

[IA et ComfyUI](#ai)
[Actions et outils](#automation)
