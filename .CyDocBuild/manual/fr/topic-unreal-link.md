---
id: unreal-link
title: Connecteur Unreal Engine
description: Peindre des maillages et modifier des textures avec réimport automatique dans Unreal Engine.
section: Livrer / Intégration moteur
order: 100
parent: formats
sourceRevision: 3a3eca338cf543b7
---

Le connecteur d’éditeur Shaderixel Link permet de peindre un maillage statique ou de modifier une texture dans Shaderixel, puis de réimporter le résultat dans Unreal Engine.

## Installation {#installation}

Copiez le dossier `ShaderixelLink` compilé pour votre version du moteur depuis le paquet Shaderixel (`Unreal/<version>/ShaderixelLink`) vers le dossier `Plugins` de votre projet, ou vers `Engine/Plugins/Marketplace` du moteur. Activez **Shaderixel Link** dans **Edit > Plugins**.

Si le paquet ne contient que les sources, copiez `Unreal/Sources/ShaderixelLink` dans le dossier `Plugins` d’un projet Unreal C++, puis compilez la cible Editor de ce projet pour votre moteur. Les chemins conditionnels du connecteur couvrent Unreal Engine 4.25 à 5.8 ; utilisez un paquet compilé pour votre version exacte.

Le plugin retrouve automatiquement le dernier exécutable Shaderixel lancé sur l’ordinateur. Pour le choisir manuellement, ouvrez **Project Settings > Plugins > Shaderixel Link > Executable Path**.

## Utilisation {#usage}

### Peindre un maillage {#paint-mesh}

Dans le navigateur de contenu d’Unreal, faites un clic droit sur un maillage statique puis choisissez **Peindre dans Shaderixel**, et l’emplacement de matériau. Le connecteur ouvre un projet Paint 3D avec le maillage, le canal UV sélectionné et les textures de ce matériau. Les rôles BaseColor, Normal, ORM, Roughness, Metallic, AO, Emissive et Height sont reconnus à partir des noms de paramètres ou de textures.

Les textures d’origine sont placées dans un calque de source Unreal. Peignez au-dessus, puis utilisez **Ctrl+S** ou **Envoyer vers Unreal** pour réimporter les textures dans les mêmes ressources en conservant leurs réglages. Le mode **Direct** dans la barre d’état de Shaderixel renvoie automatiquement les changements pendant l’édition.

Pour un matériau sans texture, le connecteur crée `T_<mesh>_BC`, `T_<mesh>_N`, `T_<mesh>_ORM` et une instance `MI_<mesh>`. Le parent `M_ShaderixelPBR2` est créé une fois dans `/Game/Shaderixel`. Les nouvelles ressources se trouvent dans un sous-dossier Shaderixel près du maillage et sont affectées à son emplacement de matériau.

### Modifier une texture {#edit-texture}

Faites un clic droit sur une texture 2D puis choisissez **Éditer dans Shaderixel**. Elle s’ouvre comme une image ; **Ctrl+S** réimporte le résultat.

### Importer un projet ou un modèle {#import-model}

**Tools > Shaderixel > Importer un modèle Shaderixel...** accepte :

- un projet Paint 3D `.sx3d`, exporté en arrière-plan par Shaderixel avec `--export-unreal` ;
- une description `.sxmat.json` produite par **Exporter vers Unreal**, près du modèle OBJ et de ses textures.

L’import crée un maillage `SM_<name>`, avec unités et position d’origine, une échelle réglable et des UV d’éclairage ; des textures `T_<name>_BaseColor`, `_Normal`, `_ORM` et `_Emissive` avec les réglages de compression et sRGB appropriés ; puis une instance `MI_<name>` affectée au maillage.

Le parent est `M_ShaderixelPBR2`, ou celui choisi dans Shaderixel. BaseColor, Normal, ORM et Emissive sont pris en charge ; Emissive est multiplié par EmissiveStrength. Nom, dossier de sortie, échelle, parent, double face et intensité émissive sont enregistrés dans le projet `.sx3d`. Le même menu accepte les descriptions de textures seules `.sxue.json`.

Un `.sxmat.json` situé près d’un FBX ou OBJ et portant le même nom de base s’applique aussi lors de son import normal, par glisser-déposer ou par la commande Importer, avec textures et instance de matériau.

Les commandes de console `Shaderixel.Import`, `Shaderixel.Paint`, `Shaderixel.EditTexture` et `Shaderixel.Poll` sont également disponibles. **Tools > Shaderixel** permet d’ouvrir le dossier des sessions.

Les projets restent dans `%LOCALAPPDATA%\Shaderixel\Link\sessions`. Rouvrir le même maillage restaure ses calques tant que sa géométrie n’a pas changé.

## Format d’échange {#exchange-format}

Le plugin écrit un `session.json` comprenant :

- `kind` : `mesh` ou `texture` ;
- `mesh` : fichier OBJ, axe Y vers le haut, en mètres ;
- `textures` : rôles, fichiers et ressources d’entrée ;
- `outputs` : rôles, fichiers de destination et ressources cibles ;
- `project` : projet Shaderixel à rouvrir.

Après les fichiers de sortie, Shaderixel écrit `result.json` avec la révision et les sorties. Le plugin le surveille et réimporte les résultats. Une instance Shaderixel déjà ouverte reçoit les sessions par `Link/inbox`.

## Compiler le connecteur {#build-and-test}

Depuis le dépôt source de Shaderixel, cette commande compile le connecteur pour les moteurs installés sélectionnés :

```text
python tools/build_unreal_plugin.py [versions]
```

Formes supplémentaires des commandes de console :

```text
Shaderixel.Import <file> [folder]
Shaderixel.Paint <mesh asset path> [material slot]
Shaderixel.EditTexture <texture asset path>
Shaderixel.Poll
```

## Licence {#license}

Le connecteur est sous [licence MIT](assets/legal/ShaderixelLink.txt). Unreal Engine reste soumis aux conditions d’Epic et n’est pas couvert par cette licence. L’application Shaderixel séparée est sous GPL-3.0-or-later ; consultez la [licence de l’application](doc:about#license).
