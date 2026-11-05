# Prise en main

Le manuel couvre les procédures et références des effets/fonctions livrés, outils, pinceaux, commandes enregistrées et opérations de nœuds intégrées. Les ressources ajoutées par l’utilisateur peuvent en différer.

## Trouver une procédure complète

Les projets guidés proposent sept exercices et six documents ouvrables. L’index des nœuds distingue graphes image, shaders par pixel et exécution. Le répertoire explique chaque panneau/dialogue enregistré. Réglages couvre interface, tablette, mémoire, IA et mises à jour ; Livraison et Récupération décrivent contrôles finaux et dépannage.

[Tous les nœuds intégrés](#builtin_nodes)
[Panneaux et dialogues](#panels)
[Réglages et mises à jour](#settings)
[Livraison des assets](#delivery)
[Diagnostic et récupération](#recovery)

Shaderixel est un éditeur d'images sur GPU. Vous pouvez composer avec une pile de calques ou un graphe d'images. Le graphe d'effet, distinct de ce dernier, décrit le calcul effectué à l'intérieur d'un seul effet.

![Travail en calques : canevas, graphe généré et propriétés éditables.](media/layers_view_fr.png)

Aide > Documentation ou F1 ouvre ce manuel sans connexion Internet. La recherche couvre le guide et la référence des effets et fonctions livrés. Les presets ajoutés par l'utilisateur ne figurent pas automatiquement dans cette référence.

F1 tient compte du panneau actif : un effet sélectionné ouvre sa fiche, tandis qu'un outil de peinture ou un éditeur ouvre son chapitre. Propriétés propose aussi Aide sur la sélection, comme le menu contextuel d'un nœud du graphe. Une fonction inconnue ou renommée renvoie au chapitre des bibliothèques ; un effet personnalisé renvoie à la création d'effets. Dans le manuel, F1 conserve l'article courant. Aide > Documentation reprend le dernier article.

Dans ce chapitre permet d'atteindre une section ; Taille du texte propose une lecture de 85 à 150 %. Ces réglages existent dans le lecteur intégré et le navigateur. La taille et le choix de langue du manuel restent valables pour la session.

## Installer et démarrer

Utilisez l'installateur Windows x64 ou extrayez le ZIP portable dans un dossier accessible en écriture. Shaderixel.exe exige Windows 10 version 2004 ou ultérieure et un GPU D3D12 ; ShaderixelVk.exe utilise un pilote Vulkan compatible. L'installation par utilisateur contient les bibliothèques nécessaires et n'exige pas de droits administrateur.

Aide > Rechercher des mises à jour consulte le canal GitHub public. Recherche au démarrage et préversions se règlent dans ce panneau. Gardez les projets éditables séparés des images de livraison et rassemblez leurs fichiers liés.

## Une première image en cinq minutes

1. Choisissez Fichier > Nouveau, réglez largeur, hauteur et précision, puis le mode Calques. Vous pouvez aussi ouvrir une image avec Ctrl+O.
2. Sélectionnez un calque de pixels, choisissez Pinceau (B), une couleur de premier plan, puis peignez.
3. Appliquez un effet comme filtre de calque. Modifiez ses paramètres dans Propriétés et désactivez-le pour comparer.
4. Ajoutez un masque pour cacher une partie du calque. Cliquez de nouveau sur la vignette de l'image avant de peindre en couleur.
5. Enregistrez le document natif avec Ctrl+S. Utilisez Fichier > Exporter ou Exporter sous pour livrer une image.

Le damier représente la transparence. Un calque de pixels vide doit recevoir de la peinture ou un remplissage pour devenir visible.

## Choisir son point de départ

- Photo : ouvrez l'image et essayez Photo Essentials Develop.
- Matière : reliez une image à PBR Photo to Maps dans le graphe principal.
- Interface : utilisez un document 32 bits flottants et UI UV Nine Slice Panel.
- Peinture de modèle : créez un projet Paint 3D et choisissez ou importez un maillage.

Les noms anglais des presets sont conservés pour les retrouver dans la bibliothèque. Les explications du manuel suivent la langue choisie.

[Espace de travail](#workspace)
[Graphe d'images](#image_graph)
[Cartes PBR depuis une photo](#pbr)
[Textures d'interface](#ui_textures)
