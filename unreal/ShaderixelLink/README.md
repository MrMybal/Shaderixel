# Shaderixel Link (Unreal Engine 4.27 – 5.8)

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
- Matériau sans texture : de nouvelles textures `T_<maillage>_BC / _N / _ORM` et une instance
  `MI_<maillage>` du matériau `M_ShaderixelPBR` (créé une fois dans `/Game/Shaderixel`) sont créées
  dans le dossier `Shaderixel` à côté du maillage, et assignées à l'emplacement.
- **Clic droit sur une Texture 2D > Éditer dans Shaderixel** : la texture s'ouvre comme image ;
  Ctrl+S la réimporte.
- **Tools > Shaderixel** : peindre l'acteur sélectionné, importer un export Shaderixel
  (`*.sxue.json`, écrit par « Exporter les textures » d'un projet Paint 3D en modèle Unreal ORM),
  ouvrir le dossier des sessions.

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
