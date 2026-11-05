# Bibliothèques de pinceaux et tablettes

## Choisir et régler

Ouvrez Aide > Pinceaux pour cet index. Dans le panneau Pinceaux, sélectionnez un preset puis utilisez Aide du pinceau ou F1 pour sa fiche. La gomme possède une pointe indépendante. Rond standard ouvre ce guide ; les copies de code et de graphe personnalisées ouvrent le guide de création.

Le panneau Pinceaux classe les presets par catégorie et recherche. Un pinceau programmable définit pointe et placement des touches ; ses paramètres varient. Espacement, dispersion, variations de taille/angle et aérographe changent le trait.

Testez à l'échelle du document. Une grosse pointe, un espacement dense et un shader coûteux sollicitent davantage le GPU. Une cible masque reçoit la couverture ; une cible image la couleur.

## Importer et créer

Fichier > Importer des ressources accepte les pinceaux Photoshop .abr. Les ressources importées et les shaders procéduraux sont différents ; inspectez pointe et dynamique.

L'éditeur crée un pinceau depuis un modèle, modifie code/graphe et enregistre en bibliothèque. Ctrl+Entrée compile dans l'éditeur approprié. Une texture de pointe peut changer le rendu indépendamment de taille et espacement.

## Tablette

Les options proposent Windows Ink et Wintab. Pression, inclinaison, rotation et vitesse alimentent les dynamiques si le pinceau les utilise. Réglez la courbe de pression et essayez la zone de test avant le travail important.

Si la pression est constante, contrôlez système d'entrée, pilote et correspondance du pinceau. Wintab exige un pilote compatible ; la souris ne fournit pas tous les canaux. Un canal inutilisé par le pinceau ne change pas son rendu.

## Comprendre le trait

![Des échantillons du stylet à la couverture peinte](media/brush_pipeline_fr.png)

1. Choisissez un calque de pixels déverrouillé et sa vignette image ou masque. Convertissez un contenu procédural en pixels avant de peindre. En Paint 3D, vérifiez plutôt le canal du matériau actif.
2. Choisissez la pointe. Sa sélection applique l’espacement déclaré, en fraction du diamètre. Une valeur de 0,1 vaut un dixième du diamètre ; 1,2 sépare les centres de plus d’un diamètre.
3. Réglez taille, opacité, débit et pression prise en charge. Double-cliquez ou Ctrl+cliquez un champ numérique pour saisir une décimale ; Entrée valide, Échap annule.
4. Testez une courbe et un appui immobile. Seuls les presets avec temporisation d’aérographe ajoutent des touches à l’arrêt. Une forme pulvérisée ne suffit pas à activer cette peinture temporisée.
5. Comparez les superpositions à l’échelle finale du document. Une forte opacité ne remplace pas la couverture absente d’une pointe granuleuse. Un espacement serré remplit les petits trous et augmente le travail du GPU.

Tip renvoie couleur et couverture. Dynamics peut changer centre, angle, rayon ou couleur d’une touche. Extent réserve une zone de calcul plus large pour ces marques ; l’augmenter ne disperse pas le pinceau. Les presets à débit autonome appliquent débit et pression dans la pointe ; les autres passent par la multiplication d’opacité du moteur. Tous conservent les limites d’opacité globale et de sélection.

Longueur, largeur et bruit utilisent généralement le repère de la pointe plutôt que les pixels du document. Une couleur comprend des composantes RVBA ; Rotation, NibAngle et Slant utilisent des radians (environ 1,5708 pour un quart de tour). Les identifiants stables entre crochets permettent de reconnaître un réglage dans chaque langue.

## Dépannage et modification

- Rien ne se peint : vérifiez verrouillage, cible image/masque, sélection, opacité, débit et couleur. Comparez avec Rond standard pour distinguer un problème de cible d’une couverture procédurale peu dense.
- Texture absente : choisissez une image lisible dans le slot du preset. Une image sans alpha reçoit une couverture dérivée de la luminance ; un fond blanc peut donc être peint avec l’empreinte.
- Dureté sans effet : tous les shaders ne lisent pas la dureté commune. Certains définissent une douceur propre ou des seuils fixes ; consultez leur fiche.
- Rendu différent de la référence : celle-ci décrit les valeurs livrées. Code/graphe modifié ou texture remplacée changent le résultat. Resélectionner un preset livré réinitialise paramètres et remplacements de textures.
- Rouille, rivets et fissures sont des motifs de couverture/couleur. Ils ne déduisent pas de géométrie, ne détectent pas les arêtes et ne génèrent pas un matériau PBR complet. Peignez explicitement le canal voulu.

## Exemples rendus

![Douze pointes livrées rendues par le moteur de pinceaux](media/brush_atlas_fr.png)

Ces aperçus sont calculés sur le GPU avec une pression simulée, sans mesure de tablette. Tous utilisent le même bleu, une taille demandée de 110 px, une dureté de 0,7, un débit de 0,8 et une opacité pleine ; l’aperçu limite le diamètre effectif à sa hauteur. Espacement et paramètres du shader gardent les valeurs du preset. Les numéros identifient les pointes choisies dans le catalogue ci-dessous. L’apparence aquarelle n’exécute aucune simulation de fluide.

[Peinture](#painting)
[Création](#authoring)

## Catalogue des pinceaux livrés

### Exemples

[Aérographe granuleux](#brush:airbrush)

[Plume calligraphique](#brush:calligraphy)

[Fusain sur papier](#brush:charcoal)

[Dispersion d’étoiles](#brush:star_scatter)

### Matières

[Brosse à poils](#brush:cy_bristle)

[Craie](#brush:cy_chalk)

[Fusain directionnel](#brush:cy_charcoal)

[Médium sec](#brush:cy_dry_media)

[Encre diffuse](#brush:cy_ink_bleed)

[Râteau](#brush:cy_rake)

[Éponge](#brush:cy_sponge)

[Aquarelle](#brush:cy_watercolor)

### Formes

[Calligraphie](#brush:cy_calligraphy)

[Croix](#brush:cy_cross)

[Losange](#brush:cy_diamond)

[Ellipse](#brush:cy_ellipse)

[Trame de points](#brush:cy_halftone)

[Hexagone](#brush:cy_hexagon)

[Feuille](#brush:cy_leaf)

[Bruit](#brush:cy_noise)

[Anneau](#brush:cy_ring)

[Rond programmable](#brush:cy_round)

[Carré arrondi](#brush:cy_soft_square)

[Carré](#brush:cy_square)

[Étoile](#brush:cy_star)

[Empreinte de texture](#brush:cy_texture)

[Triangle](#brush:cy_triangle)

### Détails

[Chaîne](#brush:cy_chain)

[Fissure ramifiée](#brush:cy_crack)

[Empreinte digitale](#brush:cy_fingerprint)

[Herbe](#brush:cy_grass)

[Rayures](#brush:cy_scratch)

[Couture détaillée](#brush:cy_stitch)

### Particules

[Nuage](#brush:cy_cloud)

[Fourrure](#brush:cy_fur)

[Fumée](#brush:cy_smoke)

[Éclat](#brush:cy_sparkle)

[Pulvérisation](#brush:cy_spray)

### Texture 3D

[Coulures](#brush:sx3d_coulures)

[Points de fil](#brush:sx3d_couture)

[Écailles et pastilles](#brush:sx3d_ecailles)

[Éclaboussures](#brush:sx3d_eclaboussures)

[Fissures cellulaires](#brush:sx3d_fissures)

[Grain fin](#brush:sx3d_grain_bruit)

[Nuage doux](#brush:sx3d_nuage_doux)

[Poils secs](#brush:sx3d_pinceau_sec)

[Points réguliers](#brush:sx3d_points)

[Rayures fines](#brush:sx3d_rayures_fines)

[Rivets](#brush:sx3d_rivets)

[Saleté grunge](#brush:sx3d_salete)

[Taches de rouille](#brush:sx3d_taches_rouille)

[Trait net](#brush:sx3d_trait_net)

[Usure écaillée](#brush:sx3d_usure_ecaille)
