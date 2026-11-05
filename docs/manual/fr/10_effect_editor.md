# Graphe d'effet et éditeur de shader

[Créer shader, fusion et pinceau pas à pas](#shader_authoring)
[Toutes les opérations de shader intégrées](#builtin_nodes)

L'Éditeur d'effets construit un effet avec des nœuds de calcul par pixel : mathématiques, échantillonnage, couleur et procédural. Il produit du code shader. Le graphe principal assemble des effets et images complets.

![Éditeur visuel d’effet : calculs reliés et aperçu direct.](media/effect_graph_fr.png)

## Modifier un effet

Sélectionnez l'effet puis Modifier une copie (graphe) lorsqu'un graphe éditable existe. Les effets livrés sont protégés ; la copie conserve l'original.

Reliez le calcul à la sortie d'effet, exposez des paramètres avec unités et valeurs initiales utiles, puis examinez aperçu et compilation. Une entrée manquante ou une erreur peut empêcher le résultat.

L'édition HLSL/Slang est également disponible. Ctrl+Entrée compile dans l'éditeur de code approprié. Fichier > Importer un effet HLSL accepte le code annoté.

## Familles d'effets

- Filtre : lit et traite une image.
- Draw / générateur : crée du contenu sans entrée image.
- Fusion : combine fond, calque et masque.

Le nom du fichier .slang est l'identifiant stable. Le renommer peut casser les références même si le libellé affiché reste identique.

## Échantillonnage et export

Toute lecture de voisins doit déclarer un halo fixe, dépendant d'un paramètre ou couvrant toute l'image. Sans cela, le rendu par tuiles peut différer de l'aperçu.

Employez la bonne convention alpha et inspectez les bords transparents à pleine échelle. Exporter un matériau Unreal diffère de sauvegarder un effet ou un document ; vérifiez les fonctionnalités prises en charge dans le moteur.

[Créer des presets](#authoring)
[Couleur et données](#color)
