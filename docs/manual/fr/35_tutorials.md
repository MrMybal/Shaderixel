# Projets guidés

Ces exercices utilisent les effets et fonctions livrés. Six documents contiennent des sources procédurales et leurs graphes : ils s'ouvrent sans photo, ressource téléchargée ni serveur IA. Remplacez ensuite la source d'entraînement par votre image. Ouvrir un exemple ne déclenche ni export ni outil IA.

Aide > Projets d’exercice ouvre une copie non enregistrée dans Shaderixel ; utilisez Enregistrer sous pour conserver votre travail. La page CyDoc télécharge un fichier .sxl local à ouvrir dans Shaderixel. Conservez le dossier docs/manual complet près de l’exécutable. Aide > Projets guidés ramène ici ; le sommaire de la page permet de choisir l’exercice.

## 1. Redimensionner un skin sans étirer ses coins

Objectif : un panneau de 375,5 × 75,25 pixels centré dans un canevas transparent de 512 × 256, avec un centre répété. Les dimensions décimales décrivent la couverture dans le canevas ; les dimensions de l'image exportée restent entières.

[Ouvrir l'exercice nine-slice](example:ui_nine_slice)

![Coins conservés et modes indépendants pour le centre et les bords.](media/nine_slice_fr.png)

1. Sélectionnez UI UV Nine Slice Panel dans Graphe. Skin Image reçoit déjà UI Draw Slice Test Card et la sortie Image est reliée à Output.
2. Dans Propriétés, saisissez Widget Size (px) = 375,5 ; 75,25 et Widget Position (UV) = 0,5 ; 0,5. Double-cliquez ou Ctrl+cliquez sur les contrôles numériques pour saisir les décimales.
3. Dans Variables, gardez Logical source size (px; 0 = region texels) = 128 ; 128. Laissez les quatre marges à 0,25 et Source margin units = 0 (Fraction). Chaque coin occupe une zone logique de 32 × 32 pixels.
4. Choisissez Center mode = 1 (Tile), Edge mode = 0 (Stretch), Border scale = 1 et Tile scale (target pixels per source pixel) = 1 ; 1. L'exemple règle déjà le centre sur Tile.
5. Passez Center mode à 3 (Transparent) pour une bordure eight-slice, puis revenez à 1. Comparez l'alpha central tout en gardant les proportions des coins.
6. Reliez temporairement Coverage à Output pour examiner la silhouette, puis remettez Image. Exportez le PNG final et vérifiez ses dimensions de 512 × 256 et sa transparence extérieure.

Résultat attendu : quatre coins de couleurs différentes, des bords fins et un centre quadrillé. Augmenter la largeur ajoute des répétitions sans élargir le dessin des coins. Cette mire sert au diagnostic ; elle ne constitue pas un bouton graphique terminé.

Si des coordonnées rouges/vertes apparaissent, reliez Image plutôt que UV Map. Pour des coins trop grands, vérifiez ensemble taille logique et unités de marge. Avec un skin dans un atlas, réglez d'abord la région source. Pour un export ajusté, choisissez des tailles entières correspondantes pour canevas et widget : une couverture décimale ne crée pas un PNG de largeur fractionnaire.

[Broches et variables nine-slice](#function:UI UV Nine Slice Panel)
[Échantillonnage et coordonnées UI](#ui_textures)

## 2. Convertir un calque procédural en pixels peignables

Objectif : peindre sur un fond généré tout en gardant un original récupérable et une composition de calque éditable.

[Ouvrir l'exercice de fond peignable](example:bake_background)

![Le contenu devient des pixels ; les réglages de composition restent éditables.](media/layers_fr.png)

1. Sélectionnez Editable gradient dans Calques. Modifiez couleurs ou angle dans Propriétés et enregistrez une copie de travail .sxl.
2. Ajoutez éventuellement un filtre de calque. Il fera partie du contenu intégré par la conversion.
3. Choisissez Calque > Convertir en calque de pixels, la commande du menu contextuel ou le bouton de Propriétés. Attendez la fin de la conversion.
4. Choisissez Pinceau, une couleur visible et tracez. Si vous éditiez un masque, sélectionnez la vignette image ; retirez une sélection de pixels involontaire.
5. Modifiez opacité ou mode de fusion pour vérifier qu'ils agissent toujours sur le calque converti.
6. Annulez le trait puis la conversion. Les paramètres du dégradé redeviennent éditables. Enregistrez une version procédurale distincte si vous en aurez besoin après fermeture de la session d'annulation.

Résultat attendu : le calque sélectionné devient un calque de pixels et accepte la peinture. Contenu rendu, transformation et filtres sont intégrés ; masque, opacité et fusion restent éditables. Les calques voisins ne sont pas fusionnés. L'action Bake du graphe conserve un résultat image et correspond à une autre commande.

Si la peinture semble sans effet, vérifiez document actif, verrouillage, vignette masque/image, sélection et opacité du pinceau. Un canevas transparent indique souvent une entrée vide, un calque masqué ou un masque nul plutôt qu'un échec de conversion.

[Conversion et annulation](#layers)
[Contrôles de peinture](#painting)
[Référence du générateur de dégradé](#effect:gradient_fill)

## 3. Examiner les cartes PBR avant export

Objectif : comprendre les sept sorties, puis remplacer la mire par une photo uniformément éclairée. Le bruit procédural de l'exemple sert à apprendre les connexions ; ce n'est ni une matière mesurée ni une extraction IA.

[Ouvrir l'exercice d'inspection PBR](example:pbr_maps)

![Distinguer couleur et données numériques lors de la livraison.](media/pbr_fr.png)

1. Ouvrez l'exemple de 256 × 256. PBR Photo to Maps reçoit le bruit d'entraînement ; Output affiche initialement Normals.
2. Reliez successivement Albedo, Height, Normals, Occlusion, Roughness, Metalness et ORM à Output. Notez la carte examinée.
3. Commencez avec Detail Height = 1,5, Normal Strength = 4 et Base Roughness = 0,6. Réduisez Normal Strength si les détails sont trop forts. Essayez Invert Height pour comprendre l'interprétation du relief.
4. Gardez Uniform Metalness = 0 pour un diélectrique, et choisissez 1 seulement pour un métal. Réglez DirectX Normals selon le moteur cible ; OpenGL est la convention initiale.
5. Remplacez le bruit par votre photo. Préférez un éclairage uniforme et une surface de face ; inspectez les raccords avant de juger les normales. Lighting Correction et Lighting Radius estiment l'ombrage photographié : réglez-les sans considérer chaque zone sombre comme un creux.
6. Pour les fichiers automatiques, insérez PBR Photo to Maps and PNG Export, reconnectez la photo et recopiez vos réglages. Saisissez File Prefix dans un dossier accessible en écriture, puis lancez Extract and Export. L'aperçu seul n'écrit aucun fichier.
7. Dans le moteur, activez sRGB pour l'albédo et désactivez-le pour les données. ORM contient occlusion en R, rugosité en G et métallicité en B. Examinez la matière sous un éclairage neutre.

Résultat attendu : sept sorties distinctes. L'outil écrit prefix_albedo.png, prefix_height.png, prefix_normal.png, prefix_occlusion.png, prefix_roughness.png, prefix_metal.png et prefix_orm.png. Réutiliser le préfixe remplace ces fichiers. Cet export PNG est en 8 bits ; choisissez un export de précision adaptée pour un displacement exigeant davantage de profondeur.

Si une bosse semble inversée, examinez séparément inversion de hauteur et convention des normales. Pour des données délavées dans le moteur, vérifiez l'espace colorimétrique d'import. Une estimation de profondeur décrit la distance dans la scène, pas le même phénomène que le microrelief de surface.

[Broches et réglages d'extraction](#function:PBR Photo to Maps)
[Outil d'export PBR](#function:PBR Photo to Maps and PNG Export)
[Matière et espaces colorimétriques](#pbr)

## 4. Retirer un fond coloré et examiner le matte

Objectif : séparer un sujet d'un fond vert uni et identifier la bonne sortie à exporter.

[Ouvrir l'exercice de détourage couleur](example:color_key)

![Retrait de couleur, couverture et alpha final sont des étapes distinctes.](media/matte_fr.png)

1. L'exemple compose un panneau rouge arrondi sur un vert opaque, puis transmet l'image à Masks Color Key and Despill. Output affiche Foreground.
2. Gardez Background Color = RGBA linéaire 0 ; 1 ; 0 ; 1 et Color Tolerance = 0,12. Le fond synthétique est exactement vert ; pour une photo, prélevez la vraie couleur du fond.
3. Reliez Coverage Mask à Output. Blanc conserve le sujet, noir retire le fond et gris indique une couverture partielle. Rétablissez Foreground ensuite.
4. Ajustez Selection Softness et Feather Radius juste assez pour adoucir la limite. Commencez avec Contract Radius = 0 ; trop de contraction supprime les détails fins.
5. Comparez Despill Strength = 0 et 0,6. Le despill modifie la couleur des bords ; les réglages de couverture modifient l'alpha. Checker Preview sert uniquement à l'inspection.
6. Exportez Foreground en PNG. Rouvrez-le ou placez-le sur un fond contrasté et examinez les bords à 100 %.

Résultat attendu : panneau opaque et extérieur transparent. Checker Preview contient son fond de diagnostic ; l'exporter intègre ce fond à l'image. Foreground contient déjà l'alpha du détourage. Ne le multipliez pas une seconde fois par le même Coverage Mask, sinon les bords adoucis deviennent trop transparents. Pour une composition avec un matte distinct, partez d'un sujet non masqué adapté et utilisez Masks Composite with Matte.

Si du vert subsiste, séparez nettoyage de couverture et despill. Si le sujet possède le même vert que le fond, la couleur seule ne permet pas de les distinguer : complétez par sélection, masque ou retouche.

[Référence du détourage](#function:Masks Color Key and Despill)
[Composition avec matte séparé](#function:Masks Composite with Matte)
[Sélections et masques](#masks)

## 5. Agrandir localement puis comparer la voie IA facultative

Objectif : exporter une entrée de 256 × 128 en 512 × 256 par la voie locale, sans ComfyUI.

[Ouvrir l'exercice d'agrandissement local](example:upscale)

![L'export conservé en pleine taille et l'aperçu adapté au document ont des rôles différents.](media/upscale_fr.png)

1. Sélectionnez Textures Upscale and Export. Texture reçoit une mire procédurale, Scale Factor vaut 2 et Use ComfyUI est désactivé.
2. Dans PNG File, saisissez un chemin absolu vers un fichier dans un dossier accessible en écriture. Enregistrez également votre projet d'entraînement.
3. Lancez Upscale and Export dans les contrôles d'exécution et attendez la fin.
4. Ouvrez le PNG écrit et vérifiez ses dimensions de 512 × 256. Le graphe adapte toujours Upscaled Texture au document de 256 × 128 : un aperçu de même taille ne signifie pas un échec d'export.
5. Modifiez la mire source. Le résultat conservé ne change qu'à la réexécution. Effacer les résultats rétablit l'aperçu de l'entrée directe.
6. Pour la voie IA facultative, configurez d'abord un serveur ComfyUI en marche. Activez Use ComfyUI et entrez le nom exact d'Upscale Model installé, puis relancez. Le preset n'installe pas le modèle.

Résultat attendu : la voie locale produit un PNG agrandi par interpolation bicubique. La voie IA dépend du serveur et du modèle et peut inventer du détail. Comparez textes fins, contours et raccords à échelle d'affichage égale ; conservez les cartes UV, normales et données compactées dans un traitement adapté à leurs valeurs.

Si aucun fichier n'apparaît, vérifiez l'exécution, le statut et la destination. Si le fichier garde sa taille initiale, distinguez l'export de l'outil d'un export ordinaire de l'aperçu du document.

[Broches et variables de l'outil](#function:Textures Upscale and Export)
[Configuration IA](#ai)

## 6. Développer une image et exporter des alternatives d'exposition

Objectif : appliquer une correction mesurée puis comparer trois fichiers plutôt que remplacer sans cesse le même export.

[Ouvrir l'exercice de variantes photo](example:photo_variants)

1. La mire alimente Photo Essentials Develop, dont Developed Photo alimente Photo Essentials Exposure Bracket Export. Remplacez la mire par une photo pour un véritable développement.
2. Sélectionnez Develop et comparez Exposure (Stops) = 0 et 0,5. Gardez Strength = 1 pour la correction complète. L'entrée Mask facultative peut limiter l'effet ; laissez-la vide pour toute l'image.
3. Ajustez Temperature, Shadows, Highlights et Contrast par petites quantités. Examinez Sharpening à 100 % et les contours lumineux avant d'augmenter Local Contrast.
4. Sélectionnez l'outil d'export. Gardez Exposure Offsets = [-1, 0, 1], réglez File Prefix dans un dossier accessible en écriture et lancez Export Exposure Bracket.
5. Comparez prefix_exposure_0.png, prefix_exposure_1.png et prefix_exposure_2.png. Les suffixes sont les indices du tableau ; ils ne représentent pas -1, 0 et +1 dans les noms.
6. La sortie conserve la dernière image exportée et la variable Exposure Offset garde la dernière valeur de boucle. Réinitialisez ou relancez volontairement avant d'autres comparaisons ; conservez le projet source éditable.

Résultat attendu : trois alternatives à un stop sous l'entrée développée, à exposition égale et à un stop au-dessus. Dans l'exercice, Develop ajoute déjà 0,5 stop avant les décalages du bracket. Plus clair n'est pas forcément meilleur : examinez hautes lumières écrêtées et bruit des ombres.

[Réglages de développement](#function:Photo Essentials Develop)
[Événement et tableau d'expositions](#function:Photo Essentials Exposure Bracket Export)
[Traitement photo](#photo)

## 7. Exécuter ComfyUI en distinguant aperçu et résultat

Objectif : passer d'une entrée locale vérifiée à un résultat IA conservé. Cet exercice exige votre serveur ComfyUI et ses modèles installés ; aucun modèle spécialisé de profondeur n'est fourni avec les exemples.

![Les liens de données fournissent les images ; les événements déclenchent le travail et conservent les résultats.](media/execution_fr.png)

1. Vérifiez d'abord l'exercice d'agrandissement local. Confirmez entrée et export ordinaire avant d'ajouter un serveur.
2. Pour image-vers-image, insérez ComfyUI Texture Image to Image, connectez Texture et réglez Texture Prompt. Laissez Optional API Workflow vide pour utiliser le pipeline intégré avec le checkpoint configuré ; commencez AI Denoise à 0,2.
3. Lancez Process Texture, attendez puis examinez Processed Texture. L'image visible avant exécution est seulement l'entrée. Seam Inspection sert à examiner la répétition ; c'est une sortie de diagnostic.
4. Pour la profondeur IA, utilisez une instance distincte de ComfyUI Depth to Surface Maps. Fournissez API Workflow Path pour un workflow réalisant effectivement l'estimation, exporté au format API de ComfyUI, avec modèles et nœuds personnalisés installés.
5. Utilisez une entrée LoadImage et une seule sortie SaveImage pour le résultat voulu. Shaderixel récupère la première image générée ; des workflows et instances distincts clarifient les cartes indépendantes. Lancez Estimate Depth puis examinez AI Depth avant Height, Normals, Occlusion et Curvature dérivées.
6. Après modification d'entrée ou de réglages, relancez l'événement. Effacez les résultats conservés pour revenir à l'entrée directe. N'exportez qu'après vérification des dimensions et de la signification du résultat.

Résultat attendu : une image conservée après exécution réussie du serveur, avec retour de statut. Un chemin spécialisé vide affiche un message au lieu d'inventer une chaîne de profondeur. Un workflow API personnalisé peut conserver sa propre valeur de denoise ; ne supposez pas que tous les réglages visibles du preset la remplacent.

En cas d'échec, vérifiez endpoint, checkpoint/modèle, JSON au format API, nœuds manquants et workflow dans ComfyUI lui-même. Une connexion au bridge et un serveur de génération d'images sont deux services distincts.

[Outil image-vers-image](#function:ComfyUI Texture Image to Image)
[Outil de profondeur](#function:ComfyUI Depth to Surface Maps)
[Configuration et limites IA](#ai)
