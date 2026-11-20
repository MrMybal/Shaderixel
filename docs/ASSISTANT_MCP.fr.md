# Assistant, CLI et MCP dans Shaderixel

Le panneau **Assistant IA**, `ShaderixelCli.exe` et le serveur MCP utilisent les mêmes opérations. Les appels portant sur le document, ses calques ou le rendu s’exécutent sur le thread principal de l’instance ouverte.

## Chat intégré

Ouvre **Assistant IA**, choisis un fournisseur et un modèle, puis clique sur **Connecter ou reprendre**. **Détecter les fournisseurs** recherche les exécutables, variables de connexion et serveurs de modèles locaux disponibles. Tu peux aussi saisir l’adresse du fournisseur et le nom de la variable d’environnement qui contient sa clé. Les réglages d’assistant et de génération d’images sont indépendants.

Codex utilise son app-server et les outils dynamiques du SDK. Les connecteurs API OpenAI, Anthropic et OpenRouter, ainsi qu’Ollama, LM Studio, llama.cpp et les API compatibles OpenAI, reçoivent les outils via leur protocole de fonctions. Le modèle choisi doit prendre en charge les appels d’outils. Claude Code reçoit l’adresse MCP locale et utilise sa connexion native. Les fournisseurs et modèles externes nécessitent leur propre accès. Shaderixel compile le SDK sans gestionnaire de modèles ni téléchargement de runtime.

Les réponses arrivent progressivement. **Annuler la requête** interrompt la génération et les appels en attente. Les opérations déjà appliquées restent dans l’historique du document et peuvent être annulées avec les commandes habituelles. **Joindre un fichier** charge un fichier choisi explicitement, selon les formats acceptés par le fournisseur. Le contrôle **Fenêtre de contexte** limite les messages envoyés aux API qui laissent Shaderixel gérer l’historique. Le SDK conserve les instructions système, le tour courant et l’ordre des appels et de leurs résultats. Les agents CLI gèrent leur propre contexte.

Avec **Conserver l’historique local**, Shaderixel conserve les conversations au format privé du SDK dans `%LOCALAPPDATA%\Shaderixel\Mcp\conversations`. **Connecter ou reprendre** recharge la dernière conversation pour le fournisseur et l’adresse choisis. **Nouvelle conversation** ouvre un nouveau fil pour ce profil. Les anciennes conversations du compagnon Python n’étaient pas enregistrées par Shaderixel dans ce format et leurs fichiers externes sont préservés.

## Terminal et client MCP

Démarre le **Serveur MCP** dans **Assistant IA** ou lance `Shaderixel.exe --mcp` (ou `ShaderixelVk.exe --mcp`). L’option de test `--ai-test` démarre également le serveur en appliquant le profil configuré.

```powershell
.\ShaderixelCli.exe --help
.\ShaderixelCli.exe version
.\ShaderixelCli.exe instances
.\ShaderixelCli.exe list
.\ShaderixelCli.exe describe shaderixel.layer_add
'{"kind":"pixel","name":"Retouche"}' | .\ShaderixelCli.exe invoke shaderixel.layer_add -
.\ShaderixelCli.exe invoke shaderixel.document_info
```

Le CLI est un exécutable console. L’aide, la version et la recherche d’instances sont utilisables sans démarrer l’interface. Les opérations métier pilotent une instance équipée du moteur de rendu et n’ouvrent pas une nouvelle fenêtre. S’il existe plusieurs instances, ajoute `--instance PID` avant la commande. `--connection CHEMIN` permet de choisir un descripteur local précis. `invoke` accepte du JSON en argument, un fichier `@CHEMIN`, ou `-` pour lire stdin. Le résultat JSON va sur stdout et les erreurs JSON sur stderr. Le code `0` indique le succès, `2` une syntaxe ou un JSON invalide et `1` une autre erreur, par exemple un refus de droit ou une instance indisponible.

Le serveur écoute sur `127.0.0.1`. Il expose MCP Streamable HTTP sur `/mcp` et l’API JSON du CLI sur `/v1`. **Copier la configuration MCP** fournit l’URL et ajoute un en-tête `Authorization` quand le profil exige un jeton. Le descripteur privé `%LOCALAPPDATA%\Shaderixel\Mcp\instance-PID.json` permet au CLI de choisir l’instance et de lire son jeton quand il existe. `SHADERIXEL_MCP_STATE_DIR` permet aux intégrateurs et aux tests de choisir un autre dossier d’état absolu. Les requêtes comportant un en-tête `Origin` sont refusées.

| Profil MCP | Connexion | Opérations |
| --- | --- | --- |
| Free (par défaut) | Adresse seule | Toutes les opérations exposées |
| Local contrôlé | Adresse seule | Selon les autorisations du panneau |
| Jeton seul | Jeton Bearer | Toutes les opérations exposées |
| Protégé | Jeton Bearer | Selon les autorisations du panneau |

Les configurations sans profil explicite utilisent Free. Le serveur est activé depuis le panneau, au démarrage automatique si l’option est cochée, ou avec `--mcp`. En Free, tout client local qui connaît l’adresse peut appeler les opérations. Changer de profil redémarre le serveur s’il tourne. Recopie alors sa configuration dans les clients connectés.

### Opérations et autorisations

`list` et `tools/list` découvrent le catalogue complet. `describe NOM` montre les schémas et les annotations de lecture ou d’écriture. Les opérations couvrent les documents et leurs onglets, les calques, filtres et graphes, la sélection, le pinceau, les tracés, le rendu, les actions d’image et le Paint 3D. `shaderixel.actions_list` énumère les commandes de traitement d’image et les actions enregistrées utilisables par `shaderixel.action_run`. Les étapes d’export et les appels imbriqués s’appellent par leurs outils dédiés. Les plugins natifs chargés peuvent ajouter des commandes. Le client Python des plugins lancés par Shaderixel conserve son protocole et son jeton propre. La liaison Unreal Link reste le point d’entrée Unreal dédié.

Dans les profils Local contrôlé et Protégé, le chat, le CLI et MCP appliquent **Autoriser l’IA à modifier le document** aux opérations qui modifient un document, un projet ou l’interface. L’import, la sauvegarde, l’écriture dans les bibliothèques et le pilotage générique des menus ou des entrées appliquent aussi **Autoriser l’accès aux fichiers**. Les quatre profils valident les arguments. Pour `shaderixel.document_save`, utilise un chemin absolu et passe `overwrite: true` dans l’appel pour remplacer un fichier existant. L’appel attend la fin de l’écriture et en signale l’échec si nécessaire.

### Piloter l’interface

`shaderixel.ui_state` lit les panneaux et les contrôles visibles. `query` recherche aussi dans l’index des menus et des panneaux. Utilise `shaderixel.ui_activate` avec le `label`, le `kind` et le `path` d’un résultat pour lancer une commande de menu, révéler un panneau ou aller à un réglage. Les commandes disponibles dépendent du document et de l’état de l’interface.

`shaderixel.ui_input` accepte `click`, `drag`, `scroll`, `key` et `text`. Les coordonnées sont en pixels dans la fenêtre principale, avec l’origine en haut à gauche. Un clic sur un contrôle peut aussi utiliser `id` et `frame` de la dernière observation. `shaderixel.ui_capture` renvoie une ressource PNG privée de la fenêtre principale. Ces outils agissent sur Shaderixel. Les fenêtres des autres applications, les sélecteurs de fichiers Windows et les panneaux détachés restent hors de ces entrées. Les outils à chemin explicite permettent d’ouvrir et d’enregistrer directement depuis le terminal.

### Paint 3D et particules

- `paint3d_new`, `paint3d_info` et `paint3d_texture_set` créent un projet, décrivent ses matériaux et tuiles UDIM, et règlent les canaux par jeu de textures.
- `paint3d_camera` et `paint3d_environment` règlent la vue 3D. `paint3d_material` et `paint3d_brush` sélectionnent la matière et le pinceau. `paint3d_stroke` peint un trait multicanal annulable depuis la caméra, en utilisant des points normalisés dans la vue.
- `particles_info`, `particles_configure` et `particles_save` exposent les presets, les scripts d’émission, d’évolution et d’impact, leurs réglages conditionnels et les groupes. `particles_play` émet, rejoue, valide ou annule l’aperçu. `particles_gesture` récupère le geste et ses réglages pour le même modèle.
- `paint3d_bake` pilote la cuisson. `paint3d_export_presets` liste les modèles et `paint3d_export` attend l’écriture des cartes PBR en atlas ou par tuile. `document_save` et `document_open` gèrent les projets `.sx3d`.

```powershell
'{"mesh":"sphere","resolution":1024}' | .\ShaderixelCli.exe invoke shaderixel.paint3d_new -
'{"points":[[0.43,0.5],[0.57,0.5]]}' | .\ShaderixelCli.exe invoke shaderixel.paint3d_stroke -
```

### Recherche, gros résultats et mesures

La découverte à la demande du chat propose `cy_tools_search`, `cy_tools_describe` et `cy_tools_call`. Elle peut être désactivée indépendamment du serveur ou du CLI, qui conservent toujours le catalogue complet. `shaderixel.effects_list` accepte `category`, `query`, `offset` et `limit`. `resources`, `read URI` et `grep TEXTE [CURSOR]` exposent la documentation enregistrée dans le SDK. La recherche native porte sur ces textes, sans indexation de fichiers.

`shaderixel.render` renvoie `reference.uri` vers un PNG privé. `ShaderixelCli.exe download URI sortie.png` récupère tous les morceaux et écrit le fichier choisi. Les clients MCP peuvent lire `resources/read` et suivre `firstUri` puis `nextUri`. Le stockage temporaire conserve les résultats jusqu’à 15 minutes dans la limite de ses quotas. Il utilise jusqu’à 64 Mio en mémoire et, si activé, 512 Mio sur disque, avec bascule à partir de 1 Mio. Un URI expiré ne rejoue aucune opération.

Le cache de découverte a un TTL de 30 secondes. Le journal de changements du SDK relie les ajouts et retraits d’outils aux notifications sur les clients prenant en charge `subscriptions/listen`. Les clients sans abonnement peuvent redemander la liste. Les lectures du document et les résultats de mutation restent traités au moment de l’appel. Les mesures facultatives `performance-PID.jsonl` contiennent le nom de l’opération, la durée, les volumes et le statut. Elles excluent le texte des messages, les arguments, les résultats et les jetons. Les options de stockage, de cache, de notifications et de mesures prennent effet au prochain lancement de Shaderixel.

## Build et vérification locale

`build.bat Release` compile à partir du dépôt frère `CyMCPSDK`. Pour choisir un autre dossier, configure CMake avec `-DSX_CYMCP_SOURCE_DIR=CHEMIN`. Cette version embarque CyMCPSDK `0.7.2` au commit `e562c731a146d599e3404718ea8414bb5b96b66d`. CMake récupère la même révision sur GitHub si les sources locales sont absentes. `ShaderixelCli.exe version` indique la révision réellement compilée.

```powershell
python tools/mcp/test_shaderixel_mcp.py build/Release
python tools/mcp/test_shaderixel_mcp.py build/Release --backend vulkan
python tools/mcp/test_shaderixel_controls.py build/Release
python tools/mcp/test_shaderixel_controls.py build/Release --backend vulkan
```

Le test local lance des copies isolées de Shaderixel et un fournisseur HTTP simulé. Il vérifie les permissions, les opérations métier, le rendu par morceaux, le chat progressif, l’annulation et la restauration. Pour agir avec un outil d’une autre application, utilise le client MCP de cette application ou un client externe configuré. Le chat intégré ne charge pas de catalogues distants dans cette intégration.
