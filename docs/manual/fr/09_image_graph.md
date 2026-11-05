# Graphe d'images principal

[Référence de toutes les structures, valeurs et actions intégrées](#builtin_nodes)

Le graphe principal relie images complètes, canaux, valeurs et actions d'exécution. Le graphe d'effet décrit plutôt le calcul interne à un seul effet.

![Graphe d’images et graphe de calcul interne à un effet.](media/graphs_fr.png)

## Obtenir un résultat visible

1. Créez ou convertissez un document en mode Nodal.
2. Dans le vide du Graphe, clic droit puis source image ou générateur Draw.
3. Ajoutez un effet et reliez la source à son entrée Image.
4. Reliez l'image finale à Output/Sortie.
5. Sélectionnez les nœuds pour leurs Propriétés ; cadrez la vue si certains sont hors écran.

Un générateur n'a pas besoin d'image d'entrée, contrairement à la plupart des filtres. Sortie débranchée, surface vide ou source transparente peuvent produire un damier.

## Types et paramètres

Les broches image portent des images ; les canaux des champs scalaires. Les valeurs sont nombres, couleurs, vecteurs ou texte. Les liens d'exécution déclenchent les actions. Les types doivent être compatibles.

Paramètres en broches expose des réglages comme entrées. Une valeur reliée les pilote ; sinon leur valeur par défaut s'applique. Le comportement d'une entrée image vide dépend de l'effet.

Séparer extrait les canaux RVBA. Les mathématiques et variables pilotent les contrôles. Une fonction à sorties multiples exige la bonne broche de sortie.

## Bibliothèque et réutilisation

Clic droit > Bibliothèque insère une fonction .sxfn. Propriétés expose entrées et Variables ; double-clic ouvre son graphe. Les sous-graphes sont directs ; les outils ont des événements pour cuire, lancer l'IA, boucler ou exporter.

Un outil peut afficher l'entrée avant exécution : cela ne prouve pas que le calcul ou l'export a eu lieu. Sélectionnez des nœuds pour créer une fonction réutilisable et l'enregistrer dans la bibliothèque.

[Exécution et automatisation](#automation)
[Éditeur d'effets](#effect_editor)
