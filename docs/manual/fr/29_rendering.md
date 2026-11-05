# Scènes de rendu et présentation

![Rendu de présentation et réglages de scène](media/render_view_fr.png)

## Premier rendu de produit

1. Ouvrez un projet Paint 3D avec modèle/matériau valides. Le rendu n’est pas disponible sur un simple document 2D.
2. Ouvrez Rendu et Scène de rendu. Actualisez Textures du projet après peinture ; copiez éventuellement la caméra 3D.
3. Choisissez un environnement simple et une lumière. Fixez cadre et dimensions avant les échantillons.
4. Inspectez à exposition neutre puis réglez lumière, fond et courbe. Corrigez modèle/scène avant débruitage.
5. Accumulez, mettez en pause si nécessaire et exportez image/passes. Rouvrez pour vérifier dimensions et canaux.

La scène propose lumières ponctuelles, spots, surfaciques et soleil. Position/orientation, cône et taille ont des effets différents. La visibilité de l’environnement contrôle le fond séparément du HDRI. L’orthographique diffère du champ perspectif ; mise au point et distance de cadrage sont distinctes. La transmission utilise un indice de réfraction et peut demander assez de rebonds.

Changer caméra/lumière/matériau/scène redémarre l’accumulation concernée. Le débruitage peut masquer les détails et biaiser un aperçu pauvre en échantillons ; comparez avec un rendu suffisamment convergé. GPU/CPU concernent la présentation, pas tous les effets 2D.

L'aperçu de matériau 3D et la vue Rendu ont des rôles différents. Rendu construit une scène de présentation et accumule des échantillons. Gardez le projet de matière éditable comme source.

## Préparer la scène

Ouvrez Rendu et Scène de rendu dans l'espace 3D. Choisissez/importez des objets, réglez transformations et matériaux, ajoutez lumières/environnement et cadrez la caméra. Le panneau de scène contrôle ces objets ; les calques contrôlent les textures peintes.

Une grande lumière de surface adoucit les ombres. Rotation/luminosité HDRI change l'éclairage. Projection, champ de vision et mise au point changent la présentation sans redimensionner les cartes.

## Qualité et apparence

Choisissez GPU ou CPU, nombre d'échantillons et qualité. Mettez en pause pendant les réglages. Accumuler réduit le bruit ; le débruitage aide l'aperçu, sans corriger une mauvaise géométrie ou matière.

PBR Neutral et AgX proposent des courbes d'affichage différentes. Exposition, vernis et transmission changent l'aspect final. Préférez un éclairage neutre pour comparer une matière extraite à sa photo.

## Passes

Exportez l'image finale ou les passes de composition. L'opération prévue produit albédo, normales, profondeur et masque avec des suffixes. La profondeur de rendu n'est pas du microrelief mesuré.

Actualiser les canaux peints met à jour les entrées ; un preset de scène réutilise la configuration. Gardez maillages et HDRI externes pour une édition transportable.

[Paint 3D](#paint3d)
[Couleur et données](#color)
