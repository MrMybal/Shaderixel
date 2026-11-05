# Dépliage UV, jeux de textures et cartes de maillage

Les UV placent les triangles dans l'espace de texture. Avant de peindre, inspectez couverture et coutures avec un damier. Résolution et taille relative des îlots déterminent le détail disponible.

## Dépliage automatique

Ouvrez les outils UV, choisissez un preset de dépliage/empaquetage et comparez proposition et UV actuelles. Vérifiez séparation, marge et orientation des îlots puis appliquez. La peinture existante peut être transférée ; inspectez ensuite coutures et petits détails.

Changer les UV modifie la relation géométrie-pixels. Enregistrez une copie avant un gros réempaquetage. L'aperçu plat ne révèle pas tous les recouvrements indésirables.

## Plusieurs matériaux

Un projet multimatériau peut allouer tuiles/jeux par matériau. Sélectionnez le bon matériau pour peindre ou cuire. L'export produit un jeu correspondant ; vérifiez noms et affectations dans le moteur.

## Cuire les cartes

Le baker calcule l'occlusion et d'autres cartes géométriques. Vérifiez modèle, UV, résolution et réglages puis chaque sortie. Usure d'arêtes et saleté de creux peuvent dépendre de ces cartes.

### Procédure de cuisson complète

1. Enregistrez puis vérifiez UV de destination, superpositions, normales et jeux de matériaux.
2. Choisissez normale tangente, AO, courbure, épaisseur, hauteur, position, normale monde ou ID couleur. L’ID distingue pièces, matériaux ou îlots.
3. Pour une source haute définition, choisissez maillage et distances avant/arrière de cage relatives au modèle. Trop petite, elle manque le détail ; trop grande, elle capture un voisin.
4. Choisissez résolution (512–4096 dans ce dialogue), rayons/portée AO, largeur des arêtes et marge en texels. Davantage de rayons réduit le bruit et coûte plus.
5. Écrivez dans les canaux et/ou en PNG. La hauteur possède une option propre. Lancez puis inspectez chaque carte sous lumière neutre.

Annuler demande l’arrêt du calcul ; un aperçu partiel n’est pas un ensemble terminé. Gardez la source pour ajuster et recuire. La convention de tangentes/normales doit toujours être correcte dans le moteur destinataire.

La cuisson géométrique diffère de l'estimation depuis une photo et de la conversion d'un calque 2D. Choisissez l'opération qui correspond aux données disponibles.

[Paint 3D](#paint3d)
[Photo vers PBR](#pbr)
