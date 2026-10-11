# Shaderixel Link (Unreal Engine 4.25 – 5.8)

Plugin d'éditeur : peindre un maillage statique ou retoucher une texture dans Shaderixel, puis
réimporter le résultat automatiquement dans Unreal.

## Installation

Copier le dossier `ShaderixelLink` compilé pour votre version (dans le package Shaderixel :
`Unreal/<version>/ShaderixelLink`) dans le dossier `Plugins` du projet (ou
`Engine/Plugins/Marketplace` du moteur), puis activer **Shaderixel Link** (Edit > Plugins).

Shaderixel est trouvé automatiquement (le dernier lancé sur l'ordinateur). Sinon :
Project Settings > Plugins > Shaderixel Link > Executable Path.

## Utilisation

- **Content Browser, clic droit sur un Static Mesh > Peindre dans Shaderixel > (emplacement)** :
  Shaderixel ouvre un projet Paint 3D avec le maillage (UV du canal choisi dans les réglages) et
  les textures du matériau de cet emplacement (BaseColor, Normal, ORM, Roughness, Metallic, AO,
  Emissive, Height, reconnues d'après le nom du paramètre ou de la texture), sous forme d'un calque
  « Unreal (textures d'origine) ». Peignez par-dessus, puis **Ctrl+S** (ou « Envoyer vers Unreal ») :
  les textures sont réimportées en place (mêmes assets, mêmes réglages).
  Case **Direct** (barre d'état de Shaderixel) : les textures partent toutes seules vers Unreal un
  instant après chaque modification ; le matériau se met à jour dans l'éditeur sans rien faire.
- Matériau sans texture : de nouvelles textures `T_<maillage>_BC / _N / _ORM` et une instance
  `MI_<maillage>` du matériau `M_ShaderixelPBR2` (créé une fois dans `/Game/Shaderixel`) sont créées
  dans le dossier `Shaderixel` à côté du maillage, et assignées à l'emplacement.
- **Clic droit sur une Texture 2D > Éditer dans Shaderixel** : la texture s'ouvre comme image ;
  Ctrl+S la réimporte.
- **Tools > Shaderixel > Importer un modèle Shaderixel...** :
  - un projet Paint 3D (`*.sx3d`) : Shaderixel l'exporte en arrière-plan (`--export-unreal`) ;
  - ou la description écrite par « Exporter pour Unreal » dans Shaderixel (`*.sxmat.json`, à côté
    du modèle OBJ et des textures).
  Résultat dans le dossier courant du Content Browser : le maillage statique `SM_<nom>` (unités et
  position d'origine, échelle réglable, UV de lightmap générées), les textures `T_<nom>_BaseColor /
  _Normal / _ORM / _Emissive` avec leurs réglages (sRGB, compression), et l'instance `MI_<nom>` du
  matériau `M_ShaderixelPBR2` (BaseColor, Normal, ORM, Emissive x EmissiveStrength) ou du matériau
  parent choisi dans Shaderixel, déjà assignée au maillage. Les réglages (nom, dossier, échelle,
  parent, double face, intensité émissive) sont enregistrés dans le `.sx3d`.
  Le même menu importe les textures seules (`*.sxue.json`).
- Un `.sxmat.json` posé à côté d'un FBX / OBJ **du même nom** est appliqué automatiquement quand ce
  modèle est importé normalement (glisser-déposer, Import) : textures + instance de matériau.
- Console : `Shaderixel.Import <fichier> [dossier]`, `Shaderixel.Paint`, `Shaderixel.EditTexture`,
  `Shaderixel.Poll`. Tools > Shaderixel ouvre aussi le dossier des sessions.

Les projets Shaderixel restent dans la session (`%LOCALAPPDATA%\Shaderixel\Link\sessions\...`) :
rouvrir le même maillage reprend tous les calques tant que la géométrie n'a pas changé.

## Échange (pour référence)

`session.json` (écrit par le plugin) : `kind` mesh / texture, `mesh` (OBJ, Y vers le haut, mètres),
`textures` (entrées : rôle, fichier, asset), `outputs` (rôle, fichier à écrire, asset cible),
`project` (projet Shaderixel à rouvrir). Shaderixel écrit `result.json` (révision, sorties) une
fois les fichiers écrits ; le plugin le surveille et réimporte. Une instance de Shaderixel déjà
ouverte reçoit les sessions par `Link\inbox`.

## Compilation

`python tools/build_unreal_plugin.py [versions]` (dans le dépôt Shaderixel) compile pour chaque
moteur installé ; `python tools/test_unreal_link.py [versions]` fait un test complet sans fenêtre.

Commandes console : `Shaderixel.Paint <chemin du maillage> [emplacement]`,
`Shaderixel.EditTexture <chemin de la texture>`, `Shaderixel.Poll`.
