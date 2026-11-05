# Paint 3D, matériaux et cuisson

![Espace Paint 3D avec calques de matériau et modèle](media/paint3d_view_fr.png)

## Premier matériau jusqu’à la livraison

1. Créez le projet à une résolution adaptée avec primitive ou modèle importé. Inspectez damier et affectations avant peinture.
2. Choisissez matériau/jeu de textures. Ajoutez une couche de couleur et rugosité ; désactivez les canaux qu’elle ne doit pas modifier.
3. Ajoutez peinture ou masque, choisissez sa cible et testez une marque. Vérifiez les canaux actifs du matériau 3D.
4. Tournez le modèle et inspectez coutures, faces arrière et petits îlots. Une bonne vue à plat peut masquer des UV médiocres.
5. Ajoutez matériau intelligent ou masque géométrique après disponibilité des cartes requises. Inspectez le masque avant d’augmenter le contraste.
6. Enregistrez .sx3d, exportez les canaux et validez-les sur le modèle dans le moteur destinataire.

Commencez par une couche pleine puis localisez peinture/usure, saleté des creux et poussière directionnelle. Rugosité et métallicité sont des nombres, pas des teintes RVB décoratives. Un masque suivant une couche se place dessus, sur son bord, autour ou ailleurs ; préservez cette relation lors du réordonnancement.

Pour un remplissage image, contrôlez UV/triplanaire, échelle, rotation et décalages. Une photo éclairée peut sembler plausible en couleur mais donner des estimations de hauteur/rugosité incorrectes. Inspectez les canaux sous lumière neutre.

Créez un projet Paint 3D depuis Fichier. Importez un maillage compatible ou choisissez une primitive. Inspectez UV et résolution avant la peinture détaillée.

## Canaux et matériaux

Les canaux sont couleur de base, rugosité, métallicité, normale, émissif, occlusion et hauteur. Choisissez les canaux affectés par une couche et peignez son masque/contenu avec l'outil actif.

La vue 3D applique la matière au modèle. Éclairage/HDRI modifie l'aperçu, pas les pixels d'albédo exportés. Examinez les canaux pour diagnostiquer un éclairage inattendu.

Le dépliage automatique crée des UV s'il en manque. Vérifiez coutures, empaquetage et densité avant de vous fier au résultat.

## Matériaux intelligents

Ils assemblent remplissages et masques d'usure, creux, poussière, dégradés ou bruit. Certains utilisent des cartes cuites : occlusion, convexité, concavité ou épaisseur.

Choisissez un matériau dans les contrôles 3D et inspectez chaque masque. Les matériaux utilisateur vont dans %LOCALAPPDATA%\Shaderixel\SmartMaterials. Un matériau intelligent diffère d'un objet dynamique 2D.

## Cuisson et livraison

Le menu Fichier ouvre le baker pour les projets Paint 3D. Vérifiez modèle, UV, résolution et réglages puis les cartes produites.

La cuisson de maillage dérive des données géométriques. Convertir en pixels rastérise un calque 2D. Bake dans un outil garde une image calculée : ce sont trois opérations différentes.

Exporter les textures du projet écrit les canaux. L'export Unreal et le connecteur aident à la livraison ; contrôlez convention des normales, empaquetage et sRGB dans le moteur.

[PBR depuis une photo](#pbr)
[Export](#export)
