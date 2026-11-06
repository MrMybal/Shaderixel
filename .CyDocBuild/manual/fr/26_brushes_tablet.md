---
id: brushes-tablet
title: Bibliothèques de pinceaux et tablettes
description: Ouvrez Aide > Pinceaux pour cet index.
section: Manuel
order: 25
parent: painting
sourceRevision: 5ffd39fa291ebe8f
---

## Choisir et régler {#choose-and-configure-a-brush}

Ouvrez Aide > Pinceaux pour cet index. Dans le panneau Pinceaux, sélectionnez un preset puis utilisez Aide du pinceau ou F1 pour sa fiche. La gomme possède une pointe indépendante. Rond standard ouvre ce guide ; les copies de code et de graphe personnalisées ouvrent le guide de création.

Le panneau Pinceaux classe les presets par catégorie et recherche. Un pinceau programmable définit pointe et placement des touches ; ses paramètres varient. Espacement, dispersion, variations de taille/angle et aérographe changent le trait.

Testez à l'échelle du document. Une grosse pointe, un espacement dense et un shader coûteux sollicitent davantage le GPU. Une cible masque reçoit la couverture ; une cible image la couleur.

## Importer et créer {#import-and-create}

Fichier > Importer des ressources accepte les pinceaux Photoshop .abr. Les ressources importées et les shaders procéduraux sont différents ; inspectez pointe et dynamique.

L'éditeur crée un pinceau depuis un modèle, modifie code/graphe et enregistre en bibliothèque. Ctrl+Entrée compile dans l'éditeur approprié. Une texture de pointe peut changer le rendu indépendamment de taille et espacement.

## Tablette {#tablet-input}

Les options proposent Windows Ink et Wintab. Pression, inclinaison, rotation et vitesse alimentent les dynamiques si le pinceau les utilise. Réglez la courbe de pression et essayez la zone de test avant le travail important.

Si la pression est constante, contrôlez système d'entrée, pilote et correspondance du pinceau. Wintab exige un pilote compatible ; la souris ne fournit pas tous les canaux. Un canal inutilisé par le pinceau ne change pas son rendu.

## Comprendre le trait {#understand-a-stroke}

```image
{"id": "brush-pipeline", "src": "assets/manual/brush_pipeline_fr.png", "alt": "Des échantillons du stylet à la couverture peinte", "controls": "minimal"}
```

1. Choisissez un calque de pixels déverrouillé et sa vignette image ou masque. Convertissez un contenu procédural en pixels avant de peindre. En Paint 3D, vérifiez plutôt le canal du matériau actif.
2. Choisissez la pointe. Sa sélection applique l’espacement déclaré, en fraction du diamètre. Une valeur de 0,1 vaut un dixième du diamètre ; 1,2 sépare les centres de plus d’un diamètre.
3. Réglez taille, opacité, débit et pression prise en charge. Double-cliquez ou Ctrl+cliquez un champ numérique pour saisir une décimale ; Entrée valide, Échap annule.
4. Testez une courbe et un appui immobile. Seuls les presets avec temporisation d’aérographe ajoutent des touches à l’arrêt. Une forme pulvérisée ne suffit pas à activer cette peinture temporisée.
5. Comparez les superpositions à l’échelle finale du document. Une forte opacité ne remplace pas la couverture absente d’une pointe granuleuse. Un espacement serré remplit les petits trous et augmente le travail du GPU.

Tip renvoie couleur et couverture. Dynamics peut changer centre, angle, rayon ou couleur d’une touche. Extent réserve une zone de calcul plus large pour ces marques ; l’augmenter ne disperse pas le pinceau. Les presets à débit autonome appliquent débit et pression dans la pointe ; les autres passent par la multiplication d’opacité du moteur. Tous conservent les limites d’opacité globale et de sélection.

Longueur, largeur et bruit utilisent généralement le repère de la pointe plutôt que les pixels du document. Une couleur comprend des composantes RVBA ; Rotation, NibAngle et Slant utilisent des radians (environ 1,5708 pour un quart de tour). Les identifiants stables entre crochets permettent de reconnaître un réglage dans chaque langue.

## Dépannage et modification {#troubleshooting-and-safe-editing}

- Rien ne se peint : vérifiez verrouillage, cible image/masque, sélection, opacité, débit et couleur. Comparez avec Rond standard pour distinguer un problème de cible d’une couverture procédurale peu dense.
- Texture absente : choisissez une image lisible dans le slot du preset. Une image sans alpha reçoit une couverture dérivée de la luminance ; un fond blanc peut donc être peint avec l’empreinte.
- Dureté sans effet : tous les shaders ne lisent pas la dureté commune. Certains définissent une douceur propre ou des seuils fixes ; consultez leur fiche.
- Rendu différent de la référence : celle-ci décrit les valeurs livrées. Code/graphe modifié ou texture remplacée changent le résultat. Resélectionner un preset livré réinitialise paramètres et remplacements de textures.
- Rouille, rivets et fissures sont des motifs de couverture/couleur. Ils ne déduisent pas de géométrie, ne détectent pas les arêtes et ne génèrent pas un matériau PBR complet. Peignez explicitement le canal voulu.

## Exemples rendus {#rendered-examples}

```image
{"id": "brush-atlas", "src": "assets/manual/brush_atlas_fr.png", "alt": "Douze pointes livrées rendues par le moteur de pinceaux", "controls": "minimal"}
```

Ces aperçus sont calculés sur le GPU avec une pression simulée, sans mesure de tablette. Tous utilisent le même bleu, une taille demandée de 110 px, une dureté de 0,7, un débit de 0,8 et une opacité pleine ; l’aperçu limite le diamètre effectif à sa hauteur. Espacement et paramètres du shader gardent les valeurs du preset. Les numéros identifient les pointes choisies dans le catalogue ci-dessous. L’apparence aquarelle n’exécute aucune simulation de fluide.

[Peinture](doc:painting)
[Création](doc:authoring)

## Catalogue des pinceaux livrés {#shipped-brush-catalog}

### Exemples {#examples}

[Aérographe granuleux](doc:brush-airbrush)

[Plume calligraphique](doc:brush-calligraphy)

[Fusain sur papier](doc:brush-charcoal)

[Dispersion d’étoiles](doc:brush-star-scatter)

### Matières {#materials}

[Brosse à poils](doc:brush-cy-bristle)

[Craie](doc:brush-cy-chalk)

[Fusain directionnel](doc:brush-cy-charcoal)

[Médium sec](doc:brush-cy-dry-media)

[Encre diffuse](doc:brush-cy-ink-bleed)

[Râteau](doc:brush-cy-rake)

[Éponge](doc:brush-cy-sponge)

[Aquarelle](doc:brush-cy-watercolor)

### Formes {#shapes}

[Calligraphie](doc:brush-cy-calligraphy)

[Croix](doc:brush-cy-cross)

[Losange](doc:brush-cy-diamond)

[Ellipse](doc:brush-cy-ellipse)

[Trame de points](doc:brush-cy-halftone)

[Hexagone](doc:brush-cy-hexagon)

[Feuille](doc:brush-cy-leaf)

[Bruit](doc:brush-cy-noise)

[Anneau](doc:brush-cy-ring)

[Rond programmable](doc:brush-cy-round)

[Carré arrondi](doc:brush-cy-soft-square)

[Carré](doc:brush-cy-square)

[Étoile](doc:brush-cy-star)

[Empreinte de texture](doc:brush-cy-texture)

[Triangle](doc:brush-cy-triangle)

### Détails {#details}

[Chaîne](doc:brush-cy-chain)

[Fissure ramifiée](doc:brush-cy-crack)

[Empreinte digitale](doc:brush-cy-fingerprint)

[Herbe](doc:brush-cy-grass)

[Rayures](doc:brush-cy-scratch)

[Couture détaillée](doc:brush-cy-stitch)

### Particules {#particles}

[Nuage](doc:brush-cy-cloud)

[Fourrure](doc:brush-cy-fur)

[Fumée](doc:brush-cy-smoke)

[Éclat](doc:brush-cy-sparkle)

[Pulvérisation](doc:brush-cy-spray)

### Texture 3D {#3d-texture}

[Coulures](doc:brush-sx3d-coulures)

[Points de fil](doc:brush-sx3d-couture)

[Écailles et pastilles](doc:brush-sx3d-ecailles)

[Éclaboussures](doc:brush-sx3d-eclaboussures)

[Fissures cellulaires](doc:brush-sx3d-fissures)

[Grain fin](doc:brush-sx3d-grain-bruit)

[Nuage doux](doc:brush-sx3d-nuage-doux)

[Poils secs](doc:brush-sx3d-pinceau-sec)

[Points réguliers](doc:brush-sx3d-points)

[Rayures fines](doc:brush-sx3d-rayures-fines)

[Rivets](doc:brush-sx3d-rivets)

[Saleté grunge](doc:brush-sx3d-salete)

[Taches de rouille](doc:brush-sx3d-taches-rouille)

[Trait net](doc:brush-sx3d-trait-net)

[Usure écaillée](doc:brush-sx3d-usure-ecaille)
