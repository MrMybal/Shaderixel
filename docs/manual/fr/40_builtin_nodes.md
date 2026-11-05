# Référence complète des nœuds intégrés

Cet index couvre toutes les opérations enregistrées dans la version livrée. La catégorie Nœuds limite la recherche. F1 sur un nœud du graphe principal ou une opération de shader ouvre sa fiche ; effets et fonctions de bibliothèque gardent leur propre référence.

## Choisir le graphe adapté

![Valeurs uniques, champs image et calculs de shader par pixel](media/node_types_fr.png)

- Graphe image principal : images/canaux entiers, valeurs typées, variables, tableaux et exécution explicite. Sin/Cos Math utilisent des degrés.
- Éditeur d’effets : calculs scalaires/vectoriels par pixel, échantillonnage et sorties effet/fusion/pinceau. Trigonométrie et rotation UV utilisent des radians.
- Actions enregistrées : séquence de commandes gérée dans l’Éditeur d’actions, distincte des liens Exec du graphe.

Un canal image scalaire vert contient un nombre par pixel. Une broche Float contient une valeur typée pour l’ensemble du calcul. Les deux systèmes ne se convertissent pas implicitement. Les UV restent des données tant qu’une image n’est pas échantillonnée avec elles.

## Lire une fiche

Chaque opération décrit identifiant stable, rôle, entrées ordonnées, valeurs initiales, sorties et limites. Les données sont exportées des registres compilés sur les deux moteurs GPU et liées aux versions des sources. Les valeurs contextuelles de shader comme UV courant ou dureté sont nommées, plutôt qu’affichées comme des sentinelles négatives.

Les structures ont des interfaces dynamiques : un effet reçoit les broches de ses métadonnées, une fonction de ses ports internes, une variable de son type déclaré. Le panneau Propriétés de l’instance décrit sa configuration actuelle.

## Dépanner les connexions

1. Confirmez l’éditeur : effet image dans le graphe image, arithmétique interne d’un shader dans l’Éditeur d’effets.
2. Inspectez type et état tableau. Une référence Texture se relie uniquement à une broche Texture HLSL, pas à une valeur RVBA.
3. Choisissez la sortie voulue : RVBA/RVB et canaux isolés diffèrent ; les actions placent souvent Résultat après Exec.
4. Vérifiez les valeurs non reliées et exposez explicitement les réglages importants.
5. Reliez la sortie image pour voir le résultat ; lancez un événement seulement pour un calcul conservé ou une écriture voulue.

[Graphe image principal](#image_graph)
[Création de shaders](#shader_authoring)
[Actions enregistrées](#actions)
