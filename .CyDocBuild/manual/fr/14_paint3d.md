---
id: paint3d
title: Paint 3D, matériaux et cuisson
description: 1.
section: Créer / Peinture 3D
order: 13
sourceRevision: 4a383d092b35cc7d
---

```image
{"id": "paint3d-view", "src": "assets/manual/paint3d_view_fr.png", "alt": "Espace Paint 3D avec calques de matériau et modèle", "controls": "minimal"}
```

## Premier matériau jusqu’à la livraison {#first-material-from-start-to-delivery}

1. Créez le projet à une résolution adaptée avec primitive ou modèle importé. Inspectez damier et affectations avant peinture.
2. Choisissez matériau/jeu de textures. Ajoutez une couche de couleur et rugosité ; désactivez les canaux qu’elle ne doit pas modifier.
3. Ajoutez peinture ou masque, choisissez sa cible et testez une marque. Vérifiez les canaux actifs du matériau 3D.
4. Tournez le modèle et inspectez coutures, faces arrière et petits îlots. Une bonne vue à plat peut masquer des UV médiocres.
5. Ajoutez matériau intelligent ou masque géométrique après disponibilité des cartes requises. Inspectez le masque avant d’augmenter le contraste.
6. Enregistrez .sx3d, exportez les canaux et validez-les sur le modèle dans le moteur destinataire.

Commencez par une couche pleine puis localisez peinture/usure, saleté des creux et poussière directionnelle. Rugosité et métallicité sont des nombres, pas des teintes RVB décoratives. Un masque suivant une couche se place dessus, sur son bord, autour ou ailleurs ; préservez cette relation lors du réordonnancement.

Pour un remplissage image, contrôlez UV/triplanaire, échelle, rotation et décalages. Une photo éclairée peut sembler plausible en couleur mais donner des estimations de hauteur/rugosité incorrectes. Inspectez les canaux sous lumière neutre.

Créez un projet Paint 3D depuis Fichier. Importez un maillage compatible ou choisissez une primitive. Inspectez UV et résolution avant la peinture détaillée.

## Canaux et matériaux {#channels-and-materials}

Le pinceau propose douze canaux. Aux sept canaux de matière (couleur de base, rugosité, métal, normale, émissif, occlusion et hauteur) s’ajoutent l’opacité et quatre canaux utilisateur. Cochez les canaux à modifier dans la matière du pinceau. Les mêmes cibles s’appliquent sur le modèle et le canevas UV. La pipette peut aussi reprendre la couleur émissive.

Dans la vue 3D, sélectionnez le tampon et faites Ctrl+clic pour placer sa source sur le modèle. Choisissez le calque actif ou le résultat visible, puis peignez à destination. Le doigt étire la matière des canaux cochés. Pour protéger une zone, peignez-la avec le masque rapide. Ctrl+peindre la libère. Les commandes d’inversion et de levée du masque sont dans les options de cet outil.

Pour créer une couture ou une rangée de rivets, choisissez le tracé de surface. Cliquez sur le modèle pour poser les points et glissez un point pour déplacer le tracé. Choisissez le trait, le ruban à motif, la gomme ou le remplissage. Les points restent attachés au modèle quand la caméra tourne et restent modifiables après réouverture du projet.

Dans Matériau du pinceau, la taille en unités du modèle maintient la largeur du pinceau 3D quand la caméra zoome. Dans l’onglet Simulation, essayez « Rouille sous un rivet » sur un calque comportant couleur, rugosité et hauteur. Activez les particules et cliquez sous le rivet. Ajustez la durée de vie, la gravité ou l’intensité par canal, puis rejouez le geste avant de figer la peinture.

« Boue projetée » pose des impacts épais et des traînées terreuses. « Pluie sur la surface » baisse la rugosité et ajoute un léger relief en conservant la couleur du support. « Poussière déposée » ajoute un dépôt clair et mat sur les faces orientées vers le haut. Ouvrez « Matière à l’impact et sur la traînée » pour modifier et enregistrer ces réglages.

Choisissez une sélection UV ou un tracé existant comme source pour répartir l’émission sur une zone ou le long d’une ligne. L’option de dépôt humide transfère les impacts à la peinture liquide pour les laisser couler avant de les figer.

### Coder une brosse à particules {#coding-particles}

Dans Simulation, choisissez « Gouttes vagabondes (script) », puis « Éditer le script… ». « Appliquer le script » valide le code et l’applique à la prochaine simulation. Pour l’essayer sur un geste en cours d’aperçu, cliquez sur « Rejouer le geste ». Le script est conservé par « Enregistrer la brosse » et dans les gestes .sxstroke.

Dans **Réglages du script**, **Impact** démarre ouvert. Cliquez sur son titre pour le replier. « Rayon d’impact » multiplie le rayon de la goutte et « Force d’impact » dose le dépôt au contact. Décochez « Déposer aux impacts » pour supprimer ces dépôts tout en laissant les traînées actives. Choisissez « Rebondir » dans « Réaction à l’impact » pour afficher le groupe **Rebond**, replié au départ. Ouvrez-le pour régler la restitution entre 0 et 1 avec « Force du rebond ».

```text
group impact "Impact" open {
    slider impact_radius_scale "Rayon d’impact" = 2.1 [0, 4];
    slider impact_force "Force d’impact" = 1 [0, 2];
    checkbox paint_impacts "Déposer aux impacts" = true;
    choice impact_mode "Réaction à l’impact" = SLIDE [
        SLIDE "Glisser", BOUNCE "Rebondir", STOP "S’arrêter"
    ];
}

group bounce "Rebond" collapsed when impact_mode == BOUNCE {
    slider bounce_strength "Force du rebond" = 0.35 [0, 1];
}
```

Déclarez les réglages avant les blocs, avec un identifiant unique, un libellé et une valeur par défaut. Le script lit une case comme `true` ou `false`. Dans une liste, associez chaque libellé à une valeur numérique distincte ou à une constante comme `BOUNCE`. Le script lit la valeur de l’option choisie, par exemple avec `response = impact_mode`.

Déclarez les blocs `group` au niveau principal, avec un identifiant et un libellé. Après le libellé, indiquez `open` pour ouvrir le groupe au départ ou `collapsed` pour le replier. Sans option, le groupe démarre ouvert. Ajoutez ensuite `when` pour conditionner l’affichage du groupe. Les réglages individuels acceptent aussi `when`. La condition peut combiner les réglages déjà déclarés avec des constantes, des comparaisons et des opérateurs comme `&&`, `||` et `!`. Les valeurs restent disponibles pour le script et sont enregistrées avec la brosse et le geste, même si leur groupe est replié ou masqué.

Le bouton « Réinitialiser » près du titre d’un groupe restaure les valeurs et l’état ouvert ou replié définis dans le script pour ce groupe. Dans Gouttes vagabondes, **Impact** s’ouvre et revient à « Glisser », tandis que **Rebond** se replie.

« Tout réinitialiser », en haut de **Réglages du script**, restaure toutes les valeurs du script et l’état ouvert ou replié de chaque groupe, y compris celui des groupes masqués.

« Enregistrer la brosse » conserve les réglages et l’état ouvert ou replié de chaque groupe, même masqué. La brosse retrouve ces états au chargement. Un groupe conserve son état choisi quand son identifiant est conservé après une modification du script. Le fichier .sxstroke contient les valeurs utilisées pour peindre le geste. Après un ajustement, cliquez sur « Rejouer le geste » pour l’appliquer à l’aperçu avant de l’enregistrer. Un réglage conserve sa valeur si son identifiant et son type sont conservés. Un curseur est ramené dans ses nouvelles bornes si nécessaire. Une valeur de liste supprimée revient à celle définie par défaut.

- `emit` s’exécute une fois par particule à sa création. Il peut modifier la position `x/y/z`, la vitesse `vx/vy/vz`, `radius`, `life`, `alpha`, `alive` et `on_surface`.
- `update` s’exécute à chaque pas de 1/120 s. Il peut modifier la vitesse, l’accélération `ax/ay/az`, le facteur de gravité `gravity`, `radius`, `life`, `alpha` et `alive`.
- `impact` règle le rayon `impact_radius`, l’intensité `strength` et la réaction `response`. `NATIVE` reprend le comportement de la brosse, `SLIDE` longe la surface, `STOP` arrête la particule et `BOUNCE` la fait rebondir selon `restitution`.

Le langage accepte les expressions numériques, les variables locales déclarées avec `let` dans chaque bloc et les conditions `if/else`. `age`, `life`, `time` et `dt` sont en secondes. `nx/ny/nz` indique la normale et `speed` la vitesse. `random()` tire une valeur entre 0 et 1 à partir de la graine du geste. Les blocs peuvent utiliser `min`, `max`, `clamp`, `mix`, `abs`, `floor`, `sqrt`, `sin` et `cos`. Chaque particule programmée vit au maximum 15 secondes.

La vue 3D applique la matière au modèle. Éclairage/HDRI modifie l'aperçu, pas les pixels d'albédo exportés. Examinez les canaux pour diagnostiquer un éclairage inattendu.

Le dépliage automatique crée des UV s'il en manque. Vérifiez coutures, empaquetage et densité avant de vous fier au résultat.

## Matériaux intelligents {#smart-materials}

Ils assemblent remplissages et masques d'usure, creux, poussière, dégradés ou bruit. Certains utilisent des cartes cuites : occlusion, convexité, concavité ou épaisseur.

Choisissez un matériau dans les contrôles 3D et inspectez chaque masque. Les matériaux utilisateur vont dans %LOCALAPPDATA%\Shaderixel\SmartMaterials. Un matériau intelligent diffère d'un objet dynamique 2D.

## Cuisson et livraison {#baking-and-delivery}

Le menu Fichier ouvre le baker des projets Paint 3D. Sur un assemblage, appariez les pièces par nom pour éviter qu’une pièce voisine marque la projection. Par exemple, Porte_low et Porte_high forment une paire. Une cage personnalisée doit reprendre la topologie du modèle basse définition.

La cuisson de maillage dérive des données géométriques. Convertir en pixels rastérise un calque 2D. Bake dans un outil garde une image calculée : ce sont trois opérations différentes.

À la création d’un projet, la résolution par tuile fixe la densité native des UDIM. Dans Calques 3D, « + Pile masquée » limite les calques enfants au matériau, à la tuile ou à la sélection choisie. Dans « Canaux par jeu de textures », choisissez les canaux du matériau ou d’une tuile particulière. Dans un matériau projeté, ouvrez « Images par canal » pour affecter vos images ou importer un jeu tel que Bois_BaseColor.png et Bois_Roughness.png.

Dans « Exporter les textures », choisissez la taille native pour conserver les dimensions de chaque tuile. Le découpage « Atlas du projet » livre une seule image par carte pour un modèle utilisant les UV de l’atlas. Pour livrer une carte empaquetée sur mesure, dupliquez un modèle, choisissez la source de chaque composante RGBA et enregistrez-le. Il devient disponible dans les autres projets.

[PBR depuis une photo](doc:pbr)
[Export](doc:export)
