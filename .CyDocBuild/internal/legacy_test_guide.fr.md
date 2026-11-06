# Ancien guide des exemples de test

Archive du guide de test antérieur à CyDoc. Consultez le manuel actuel pour les commandes et réglages. Les captures temporaires générées par l’ancien paquet ne sont pas les illustrations du manuel actuel. Les noms de projets ci-dessous concernent les exemples du paquet de test.

# Shaderixel · version de test

Éditeur d'images entièrement GPU : calques façon Photoshop, mode nodal, effets HLSL/Slang codables, peinture 3D PBR et pont IA (MCP). Version du (version historique).

## Démarrer

### Lancer

- Décompressez l'archive où vous voulez (le dossier est autonome).
- Lancez `Shaderixel.exe `(Direct3D 12).
- Variante Vulkan : `Lancer Shaderixel (Vulkan).bat `ou `ShaderixelVk.exe `.
- Ouvrez un exemple : Ctrl + O → dossier `Exemples `, ou glissez un fichier sur la fenêtre.

### Configuration requise

- Windows 10 / 11 64 bits.
- Carte graphique Direct3D 12 (niveau 12_0) ; Vulkan 1.3 pour la variante Vulkan.
- Premier lancement : les shaders se compilent (quelques secondes), ensuite ils sont en cache.
- Réglages et disposition sont enregistrés à côté de l'exécutable.

## Exemples

Chaque document du dossier `Exemples `illustre une fonctionnalité. L'image est une capture de l'application ; le texte dit quoi essayer.

## Autres fichiers d'exemple

### Calques_Photoshop.psd

PSD à calques exporté par Shaderixel (groupes, masque, écrêtage, Blend If). Ouvrez-le dans Shaderixel ou dans Photoshop. Export : Fichier > Exporter en PSD (8 ou 16 bits, PSB au-delà de 30 000 px). Les calques de réglage Luminosité / Contraste, Niveaux, Courbes, Teinte / Saturation, Négatif, Isohélie, Seuil et Exposition deviennent de vrais calques de réglage Photoshop, modifiables ; les autres réglages et les filtres sont rastérisés. À l'ouverture, les PSD 16 et 32 bits de Photoshop gardent leurs calques.

### 16_Tasse_Paint3D.sx3d

Projet Paint 3D : une tasse (Tasse.obj, sans UV) dépliée automatiquement par xatlas, un matériau Or dont le masque dessine le bord, un calque de peinture « Vagues » (couleur, rugosité et relief) et un logo posé avec l'outil Projection. Ouvrez-le, tournez le modèle (clic droit), essayez « UV automatiques... », peignez, puis « Exporter les textures... ». Tasse.obj seul : Fichier > Nouveau projet Paint 3D > Fichier.

### 17_Tonneau_Paint3D.sx3d

Tonneau (Tonneau.obj, sans UV, déplié par xatlas) : calque de peinture « Bois » (douelles, veinures, rainures en relief, fonds en planches), matériau « Fer brut » pour les cerclages et « Rouille » par-dessus, chacun avec son masque. La scène de rendu ajoute un second tonneau couché (copie du modèle) : ouvrez l'onglet Rendu.

### 18_Vase_grec_Paint3D.sx3d

Amphore (Vase.obj) : terre cuite mate et vernis noir brillant (rugosité différente dans le même calque), frise de méandres, vague, points et rayons, lèvre dorée (matériau « Or » masqué). Rendu sur fond sombre avec un spot en contre-jour.

### 19_Coffre_Paint3D.sx3d

Coffre au trésor (Coffre.obj) : planches avec rainures et clous en relief, ferrures et serrure en « Or » (masque), trou de serrure peint. La scène de rendu ajoute des perles d'or devant le coffre.

### Tore.glb

Maillage glTF avec coordonnées UV pour la peinture 3D : Vue 3D > « Ouvrir un maillage... ». Les fichiers OBJ et glTF/GLB sont acceptés.

### Ciel.hdr

Environnement HDRI : Vue 3D > « HDRI... ». Réglez intensité et rotation ; les matériaux brillants reflètent le ciel.

### LUT_Sarcelle_Orange.cube

LUT d'étalonnage : Calque > nouveau calque de réglage « Correspondance de couleur », puis « Charger un fichier .cube » dans Propriétés.

## Parcours conseillé (15 minutes)

### 1. Retouche classique

- Fichier > Nouveau ( Ctrl + N ), pinceau ( B ), taille avec [ ] .
- Nouveau calque ( Ctrl + Maj + N ), sélection rectangulaire ( M ), remplir ( Alt + Retour ).
- Transformation ( Ctrl + T ), Entrée pour valider ; annuler Ctrl + Z .
- Filtre > Flou gaussien : le filtre reste modifiable dans Propriétés.

### 2. Mode nodal et effets

- Ouvrez `05_Mode_nodal.sxl `, panneau Graphe en bas.
- Clic droit dans le graphe : ajoutez un effet, reliez-le.
- Tirez un lien depuis une broche et lâchez-le dans le vide : le nœud choisi est relié par sa première broche du bon type. Mini-carte en bas à droite (clic ou glisser pour s'y rendre, M pour la masquer). Relais : double-clic sur un lien pour y poser un point de passage (images, valeurs ou exécution), à déplacer pour ranger les liens ; aussi dans le menu des nœuds (Organisation).
- Sélectionnez le nœud HLSL et modifiez son code, Ctrl + Entrée .
- Ouvrez `06_Graphe_d_effet.sxl `: onglet Éditeur d'effets.
- Effets de la bibliothèque faits en graphe : panneau Effets , entrée « Effets en graphe » (badge violet sur les vignettes), clic droit > « Ouvrir son graphe ». Un filtre de ce type posé sur le calque s'affiche directement en graphe dans l'Éditeur d'effets (protégé : la première modification en fait une copie dans le document, « Voir le code » pour le source). Dans la page Bibliothèque de l'éditeur, case « Graphes seulement ».
- Éditeur d'effets : à droite du graphe, la colonne Aperçu montre la sortie de l'effet (ou du nœud sélectionné ) sur des entrées simulées au choix : logo sur fond, mire de couleurs, dégradé, damier, blanc, noir, calque actif, image entière ou une image de votre disque ; les réglages de l'effet sont juste en dessous. La page Bibliothèque montre aussi l'entrée et la sortie de l'élément choisi.
- Ajouter un nœud : clic droit sur le graphe, menu en arbre par catégories (Maths en sous-groupes), recherche avec Entrée sur le premier résultat, description de chaque nœud au survol. Lâcher un lien dans le vide ouvre le menu pour ce lien (« Contextuel » : seulement les nœuds compatibles, reliés à la création).
- Code HLSL coloré, numéros de ligne, bouton « Mettre en forme » (une instruction par ligne) et « Agrandir » (grande fenêtre de code). Une erreur de compilation marque sa ligne en rouge dans le code du nœud (et le nœud), message au survol ; de même dans les effets écrits en code.
- Graphe : aperçus sur les nœuds (résultat de chaque nœud de calcul sur l'entrée simulée, recalculé après une courte pause dans les modifications), sélection multiple ( Ctrl / Maj +clic, cadre tiré dans le vide), Ctrl + C / X / V (collé sous la souris) / D (dupliquer), Ctrl + A , F cadre la sélection ou tout le graphe, Suppr . Un graphe ouvert est cadré automatiquement ; « Écarter les nœuds » pousse vers le bas ceux qui se chevauchent.
- Entrées non reliées : leur valeur se règle par nœud (section « Entrées non reliées » des détails, ou double-clic sur l'entrée dans le nœud). C entoure la sélection d'un cadre de commentaire (titre : double-clic pour renommer, clic droit pour la couleur ou supprimer ; le déplacer emmène les nœuds qu'il contient, coin en bas à droite pour le redimensionner). Double-clic sur un lien : nœud de reroutage pour ranger les liens.
- Nœuds ajoutés : entrée décalée en pixels, flou et contours (Sobel) de l'entrée, Pi, tangente, arc tangente, modulo, arrondi, signe, remapper, postériser, produit vectoriel, contraste, décaler la teinte, inverser, Voronoï, bruit fractal, dégradés radial et linéaire, coordonnées polaires, répéter les UV, cercle, rectangle (aussi exportables vers Unreal).

### 3. Peinture 3D (façon Substance)

- Nouveau document carré (1024 × 1024), onglet 3D à droite, bouton « Espace peinture 3D » : vue 3D à gauche, texture à droite.
- Cliquez un matériau de l'étagère (Or, Rouille...) : un calque de matériau est créé, masque noir.
- Pinceau ( B ) sur le modèle : le masque se peint en blanc et applique couleur, rugosité, métal et hauteur en même temps.
- Réglez les canaux du matériau dans « Propriétés du matériau » ; HDRI... → Ciel.hdr, puis « Exporter les textures PBR... ».

### Espace de travail

- Colonne de droite en onglets : Général, Outils, Effets, Fusion, Peinture, 3D, IA ( Alt + 1 ... 7 ).
- Tab masque la zone du graphe, Maj + Tab la colonne de droite (boutons aussi dans la barre d'état).
- Fenêtre > Nouvelle fenêtre : une fenêtre à placer sur un autre écran, glissez-y des panneaux par leur onglet ; tout panneau peut aussi sortir de la fenêtre principale.
- Fenêtre > Dispositions : enregistrer / recharger vos dispositions.

### Transformer un calque (outil Transformation, A )

- Déplacer / Rotation / Échelle / Pivot ( W E R P dans l'outil) : glissez dans le cadre, sur les flèches ou les poignées. Maj plus rapide, Ctrl plus lent.
- Barre en haut à droite du canevas : grilles d'aimantation pour chaque mode (préréglages, valeur libre, ou « libre » sans grille), repère Monde / Local, flèches, aperçu du dépassement, ratio verrouillé.
- Sans perte : l'image entière est conservée (calque pixel converti en objet dynamique), ce qui dépasse du document reste visible pendant la manipulation. « Rogner au document » si vous voulez couper.
- Calques Draw (remplissage, texte, forme) : un filtre Transform non destructif est ajouté. En mode nodal : un nœud Transform après le nœud actif, pour transformer une entrée avant sa fusion.
- Propriétés d'un calque image : transformation en valeurs (position, échelle, rotation, pivot, miroirs, centrer, ajuster), filtrage (bicubique, bilinéaire, plus proche) et « Réglages de l'image » (exposition, contraste, gamma, saturation, teinte, température...).

### Types de nœuds et sous-graphes

- Chaque nœud du graphe porte son type : IMAGE (pixels), DRAW (dessiné par un shader), BLEND (fusion), EFFECT (entrée → sortie), TOOL (outils du graphe : Transform, d'autres suivront), SUBGRAPH (graphe à l'intérieur : groupe, document), CANAUX, SORTIE.
- SubGraph : glissez un .sxl ou .sx3d sur le graphe, le canevas ou les calques (ou Calque > Objet dynamique depuis un fichier) : la sortie de son graphe devient l'image d'un objet dynamique, transformable. Lié : suit le fichier (« Mettre à jour ») ; incorporé : « Modifier le contenu » ouvre son graphe.

### Outils du graphe : Bake, Upscale, ComfyUI

- Clic droit dans le graphe > Outils. Bake fige son entrée en pixels : les nœuds avant sont grisés et ne sont plus calculés (graphe plus léger).
- PassLive (case du nœud ou clic droit) : l'entrée repasse en direct pour la modifier, le bake est gardé ; puis « Rebaker ».
- Upscale : agrandit l'entrée (x2 à x8) en bicubique sur le GPU ou avec un modèle IA via ComfyUI (4x-UltraSharp, RealESRGAN...). Le résultat garde sa pleine résolution : « Exporter l'image... ».
- ComfyUI : envoie l'entrée à un workflow (image vers image intégré, ou votre workflow « Save (API format) ») avec un prompt, et utilise le résultat dans le graphe. Serveur réglé dans le panneau IA.
- Clic droit > Convertir en image : le nœud devient une image (pixels) du graphe, modifiable au pinceau.

### Fonctions : sous-graphes et outils personnalisés

- Sélectionnez des nœuds, clic droit > « Créer un sous-graphe / un outil depuis la sélection » : ils sont remplacés par un nœud Fonction ; les liens qui entraient ou sortaient deviennent ses entrées et sorties (autant que nécessaire). Ou clic droit > Fonctions > nouveau sous-graphe / outil.
- Double-clic sur le nœud : son graphe s'ouvre dans un onglet (avec l'image reçue en entrée pour l'aperçu, ou l'image de référence si rien n'est relié). Le bandeau du graphe indique où l'on est (Document › Fonction) et « < Retour » revient au graphe parent. Chaque modification met à jour le nœud. Ajoutez des nœuds « Entrée de fonction » / « Sortie de fonction » pour ses broches (champ Ordre pour les trier).
- Deux modes : Live (calculé en direct) ou Apply (« Appliquer » garde les sorties en pixels, les nœuds avant sont figés ; « Réappliquer » après un changement).
- « Enregistrer dans la bibliothèque » : la fonction apparaît dans le menu des nœuds > Bibliothèque (sous-graphes / outils), réutilisable dans tous les documents.

### Valeurs, variables et exécution

- Broches typées : Image, Canal, Float, Int, Bool, Vec2, Couleur, Texte, Nom, tableaux (carrés), exécution (flèches blanches). Seuls les types compatibles se relient.
- Clic droit > Valeurs / Maths / Logique / Vecteurs / Texte / Tableaux : constantes et opérations. Une entrée valeur non reliée a son champ sur le nœud, comme dans Unreal : glisser horizontalement un nombre (Maj plus vite, Ctrl plus lent), clic pour le taper, clic sur une case à cocher, clic sur une couleur pour le nuancier, clic sur un texte pour l'écrire.
- Propriétés d'un effet > « Paramètres en broches » : un paramètre devient une entrée, pilotée par une valeur, une variable ou une entrée de fonction.
- Variables (Propriétés sans sélection, ou d'un nœud Variable) : les variables exposées d'une fonction se règlent sur chaque nœud de la fonction.
- Exécution : un nœud Événement est un bouton ; il lance les actions reliées (Séquence, Branche, Pour / Pour chaque, Définir une variable, Bake, Upscale, ComfyUI, Exporter en PNG, Message). Les images produites sont gardées : les nœuds avant ne sont plus calculés.
- Créer ses propres effets, calques Draw, fusions, sous-graphes et outils : voir PRESETS.md (à côté de Shaderixel.exe).

### Trim sheets

- Panneau « Trim sheet » (onglet Outils) : un modèle de disposition (architecture, détails, bandes égales, bandes + tuiles), des hauteurs sur mesure (ex. 256, 128, 64, 0) ou une bande entre chaque repère horizontal ; marge entre les bandes.
- Outil Trim (avec le recadrage) : glisser un bord pour redimensionner (pas de 8 px, Maj : 1 px, la bande voisine suit), clic pour choisir, double-clic pour sélectionner ses pixels. Chaque trim : nom, rectangle, raccord H / V, découpage en colonnes (tuiles).
- « Rendre raccordable » : mélange les bords du trim avec son contenu décalé, sur le calque actif, pour une répétition sans couture ; l'aperçu de répétition (avec décalage) montre le raccord. Effet « Raccord de bande (trim) » aussi disponible en filtre.
- Sorties : calque ID (une couleur par trim, masqué), aperçu 3D (chaque bande répétée trois fois dans la Vue 3D), disposition exportée en JSON (rectangles et UV en haut/bas à gauche) pour les moteurs et logiciels 3D. La disposition est enregistrée dans le .sxl.

### Modèles de documents

- Fichier > Nouveau (Ctrl+N) ou « Depuis un modèle... » sur l'écran d'accueil : onglet Modèles, par catégorie (Textures, Trim sheets, Graphisme, Game art, Photo, Nodal) avec recherche et vignettes ; double-clic pour créer. L'onglet « Document vide » garde les tailles prédéfinies.
- Un document créé depuis un modèle est nouveau et sans nom : l'enregistrer demande un emplacement, le modèle ne change pas.
- Fichier > « Enregistrer comme modèle... » : nom, catégorie, description ; le document entier est gardé (calques, graphe, trims, repères) avec une vignette. Vos modèles (catégorie « Mes modèles » par défaut) se suppriment depuis le dialogue ; « Ouvrir le dossier de mes modèles » pour les copier ou les partager.

### Organiser un grand graphe

- Cadres de commentaire : sélectionnez des nœuds puis C (ou clic droit > « Commentaire autour de la sélection », ou « + Commentaire ici » dans le menu d'ajout). Glisser le titre déplace le cadre et les nœuds qu'il contient, le coin le redimensionne ; double-clic ou clic droit sur le titre : renommer, couleur, ajuster aux nœuds, supprimer (les nœuds restent).
- Ctrl + F dans le graphe : chercher un nœud ou un commentaire (nom, type, opération, effet) et y aller.
- Clic droit sur une sélection > Aligner : à gauche, en haut, en colonne, répartir horizontalement ou verticalement. F cadre la sélection (ou tout).

### Panneau Ressources (zone du bas)

- Onglets Images et Maillages : explorateur de fichiers avec vignettes (les maillages en petit rendu 3D, avec leur nombre de triangles), raccourcis (Bureau, Images, Documents, Téléchargements, Exemples, disques) et la liste « Gardés » : l'étoile d'un fichier ou du dossier courant le garde à portée.
- Double-clic : ouvrir (un maillage s'ajoute à la scène de rendu si un projet 3D est actif). Glisser sur le canevas ou les calques : objet dynamique ; sur le graphe : nœud image ; sur la vue Rendu : ajouté à la scène. Clic droit : placer, lier, ajouter au graphe, afficher dans l'Explorateur Windows.
- Onglet Lumières et objets : boîte à lumière, débouchage, contre-jour, projecteur, ampoule, lumière du dessus, soleil, néon ; sol, fond cyclo, cube, sphère ; scènes complètes. Clic ou glisser sur la vue Rendu pour composer la scène du projet 3D actif.

### Panneau Effets

- Les effets sont rangés par familles (Couleur et tons, Flou, Netteté, Déformation, Lumière et glow, Stylisation, Bruit et grain, Glitch et rétro, Textures et PBR, Masques et canaux, Styles de calque, Générateurs, Dessin et formes, Game art) avec leur nombre, plus Favoris (étoile) et Récents.
- Filtre de source (Shaderixel / CyTexture) ; « Variantes » montre les variantes et copies de test masquées par défaut. Le menu d'ajout de nœuds suit les mêmes familles. Dans un effet, `//@category Famille/Sous-catégorie `le range où vous voulez.

### Effets, fusions, pinceaux en graphe

- Glissez un effet du panneau Effets sur un nœud de calque (filtre), sur un lien (inséré), dans le vide (nouveau nœud), sur un calque ou sur l'image.
- Vignettes du panneau Effets : « Aperçu : calque actif », « image entière » ou « référence » (logo sur fond, mire de couleurs, ou votre image avec « Choisir une image... ») pour juger un effet sans dépendre du document.
- Onglet Fusion : modes standard et fusions de bibliothèque avec aperçu sur le calque actif ; « Nouvelle (graphe) » ouvre l'éditeur visuel (A dessous, B calque, masque).
- Effet en HLSL : Fichier > Importer un effet HLSL (ou déposez un .hlsl, ou Éditeur d'effets > « Importer du HLSL... ») ; entrées et sorties créées depuis la fonction ou les annotations `//@in ``//@out `. Pour une IA : skill `IA/skills/shaderixel-hlsl-effect `et outil MCP `shaderixel.effect_from_hlsl `.
- Tablette graphique : Options > Tablette (Windows Ink ou Wintab, courbe de pression). Les pinceaux reçoivent la pression, l'inclinaison, la rotation du stylet et la vitesse ; réglez les réactions dans l'éditeur de pinceau et essayez-les dans sa zone d'essai. Le pinceau « Texture » (Formes) utilise une image : choisissez-la dans ses réglages.
- Onglet Peinture : pinceaux en code ou « Nouveau (graphe) » ; l'éditeur visuel règle aussi le placement (espacement, aérographe, suivre le trait, dispersion, aléas).

### Mode Paint 3D (texturer un modèle)

- Fichier > Nouveau projet Paint 3D : maillage (sphère, cube, plan ou OBJ / glTF avec UV), résolution, canaux, modèle d'export, matériau de base. Le projet (.sx3d) a son propre espace de travail.
- Onglet 3D : Calques 3D (« + Peinture » peint plusieurs canaux d'un coup, « + Matériau » ajoute un matériau à masque) et Matériau du pinceau (canaux cochés et leurs valeurs, préréglages).
- Outil Projection : choisissez une image, puis cliquez sur le modèle ( Ctrl +molette : taille, Maj +molette : rotation). Symétrie X, Y ou Z dans Calques 3D.
- Vue 3D > « Exporter les textures... » : séparées, Unreal (BaseColor, Normal, ORM) ou Unity HDRP (BaseMap, Normal, MaskMap).

### Paint 3D : pinceaux et outils

- Matériau du pinceau > Pinceau 3D : la pointe de n'importe quel pinceau de la bibliothèque est projetée sur le modèle (avec ses variantes), dont 15 pinceaux « Texture 3D » : usure écaillée, éclaboussures, rouille, coulures, fissures, saleté, rivets, couture, pinceau sec, rayures...
- Dynamique : angle, suivre le trait, variations d'angle / de taille / de flux, dispersion. Trait : lissage au fil ( lazy mouse ), angle limite (faces de biais), Maj +clic pour une ligne droite depuis la fin du trait précédent.
- Outil Remplissage par polygone (clic droit sur l'outil Projection) : remplit le triangle, la face plate, l'île UV, la pièce ou tout le modèle sous le curseur (surligné au survol) avec le matériau du pinceau ; Ctrl +clic efface.

### Rechercher partout

- Champ « Rechercher partout » dans la barre de menus ( Ctrl + Maj + F ) : tous les menus et sous-menus, toutes les fenêtres et onglets, et les propriétés et boutons des panneaux. Plusieurs mots, sans accents nécessaires (« pression taille », « opacite »).
- Résultats groupés (menus, fenêtres et onglets, propriétés et boutons) avec leur emplacement ; ↑ ↓ puis Entrée , ou un clic : un menu est exécuté (ses sous-menus s'ouvrent seuls), une fenêtre passe devant (son onglet de colonne s'affiche, un panneau fermé se rouvre), une propriété est amenée à l'écran et clignote.
- Les contenus aussi : effets (ajoutés en filtre au calque actif), pinceaux (choisis), matériaux intelligents (appliqués au projet 3D), fonctions et outils de la bibliothèque (ajoutés au graphe). Champ vide : les résultats récents .
- « Surligner dans les panneaux » : pendant la saisie, les propriétés visibles qui correspondent sont entourées dans toutes les fenêtres.
- L'index se construit une fois après chaque mise à jour de Shaderixel (quelques secondes au démarrage : les menus s'ouvrent invisibles, les onglets défilent) puis se complète tout seul ; il est gardé d'une session à l'autre.

### Plugins

- Menu Plugins : les commandes, fenêtres (Plugins > Fenêtres) et outils (Plugins > Outils, et en bas de la barre d'outils) ajoutés par les plugins.
- Livrés en exemple : Exemple (C++) (Plugins > Exemple > Dire bonjour, fenêtre « Exemple de plugin », outil Étoile : un clic peint une étoile avec aperçu au survol, aussi sur le modèle dans la Vue 3D ; Plugins > Exemple > Inverser les couleurs du calque, par l'accès aux pixels) et Exemple (script Python) (lister les calques, ajouter un calque bleu, et la fenêtre Grille écrite en Python : Plugins > Fenêtres > Grille).
- Palette (plugin livré) : Plugins > Fenêtres > Palette, « Extraire » : les couleurs dominantes du calque actif ; clic : couleur du pinceau, clic droit : arrière-plan ; « Auto » suit les modifications.
- Gestionnaire : « Installer depuis un .zip... » (ou glisser le .zip sur la fenêtre), « Créer un plugin... » (squelette Python fenêtre / commande ou C++), « Rechargement automatique » pour les développeurs.
- Plugins > Gestionnaire de plugins... : activer / désactiver, recharger une DLL recompilée sans redémarrer, rechercher les nouveaux, ouvrir le dossier utilisateur (%LOCALAPPDATA%\Shaderixel\Plugins).
- Créer un plugin : dossier `SDK `(en-tête C `shaderixel_plugin.h `, exemples, `LISEZMOI_Plugins.md `). Un plugin natif ajoute commandes, fenêtres, outils et opérations (utilisables par l'assistant IA et les actions) ; un plugin script lance un programme qui pilote Shaderixel.

### Paint 3D : masques et matériaux intelligents

- Calques 3D > « + Intelligent » : matériaux en plusieurs couches qui suivent la forme du modèle (métal peint usé, fer rouillé, plastique sale, bois verni usé, cuivre oxydé, chrome poussiéreux, caoutchouc usé, pierre moussue, peinture militaire, or patiné). Ajoutez les vôtres en JSON dans `library/smartmaterials `.
- Dossiers : « + Dossier » range le calque 3D sélectionné dans un dossier (visibilité, opacité, fusion et masque communs, masque intelligent possible) ; « Entrer dans le dossier au-dessus », « Sortir du dossier », « Dégrouper ».
- « + Intelligent » ouvre la grille des matériaux intelligents avec leur aperçu ; un clic l'applique. En bas de la grille, « Enregistrer » range les calques de matériau actuels (valeurs, masques intelligents, liens « suit un calque ») comme nouveau matériau intelligent de votre bibliothèque, avec l'aperçu de la vue 3D ; il s'applique ensuite à n'importe quel modèle.
- Calque de matériau sélectionné > Masque intelligent : usure des arêtes, saleté des creux, poussière (direction), dégradé, occlusion, bruit, parties fines, suit un calque (dessus, bord, autour ou ailleurs que la peinture d'un autre calque 3D, mis à jour quand on y peint, comme les points d'ancrage de Substance) ; couverture, contraste, bruit 3D (sans raccord), échelle, graine, inverser. « Peindre des retouches » : le pinceau ajoute le matériau, la gomme l'enlève, et ces retouches restent quand le masque est recalculé (« Effacer les retouches » rend la main au masque). Les réglages sont gardés dans le .sx3d.
- Les cartes du maillage (occlusion, convexité, concavité, épaisseur) sont cuites en arrière-plan (moins d'une seconde pour un modèle courant) ; Vue 3D > liste des canaux > « Carte : ... » pour les voir.
- Vue 3D : exposition, fil de fer.

### Paint 3D : cuisson, simulation et rendu (à la Marmoset)

- Cuisson des cartes (Vue 3D > « Cuire... » ou Fichier) : depuis le modèle du projet, ou depuis un modèle haute définition (OBJ / FBX / glTF superposé) projeté sur ses UV avec une cage (au-dessus / en dessous) : normale en espace tangent (convention DirectX / Unreal), occlusion ambiante, courbure, épaisseur, hauteur, position, normale monde ; 512 à 4096 px, rayons d'occlusion, marge. Les calques « Normale cuite » et « Occlusion cuite » vont dans les canaux, les masques intelligents suivent le détail cuit, export PNG possible.
- Carte d'ID (Cuisson, « ID couleur ») : une couleur unie par pièce, par matériau ou par île UV (du modèle haute définition s'il y en a un) dans un calque caché « ID cuit » et en PNG, pour sélectionner ces zones (baguette magique, plage de couleurs).
- Matériau projeté (Calques 3D > + Matériau > Matériau projeté) : une image qui se répète (fichier ou motif de la bibliothèque) projetée sur le modèle selon les trois axes, sans raccord d'UV : couleur, rugosité et relief tirés de l'image ; échelle, rotation, netteté des raccords, décalage. Il garde un masque : peignez-le ou ajoutez un masque intelligent.
- Pochoir (Matériau du pinceau > Projection > « Pochoir ») : l'image reste à l'écran et le pinceau ou la gomme ne peignent qu'à travers elle (transparence ou luminosité avec seuil et contraste pour un pochoir franc, inversion) ; S + glisser la déplace, Ctrl / Maj + molette : taille / rotation.
- Filtres d'un calque 3D (Calques 3D > calque sélectionné > « + Filtre ») : teinte / saturation, luminosité / contraste, niveaux, courbes, balance des couleurs, flou, netteté, bruit, inverser, sur un canal ou tous, non destructifs et réglables.
- Débruitage du lancer de rayons (Vue 3D et onglet Rendu) : image propre en quelques échantillons ; il s'appuie sur la couleur, la normale et la distance des surfaces vues, donc les détails des textures et les contours restent nets ; il s'efface à mesure que l'image s'affine.
- Export des textures (Vue 3D > « Exporter les textures... ») : modèles Unreal (ORM), Unity HDRP, Unity URP, glTF / Blender, Godot, textures séparées, avec le contenu de chaque canal affiché ; PNG 8 ou 16 bits, TGA, taille, convention de normale (DirectX / OpenGL), marge autour des îles UV, nom des fichiers.
- Outils selon la vue : quand vous travaillez dans la Vue 3D, la barre d'outils propose les outils de peinture 3D (pinceau, gomme, projection, remplissage par polygone, simulation...) ; dans le Canevas, tous les outils d'image, même dans un projet 3D. Chaque vue garde son dernier outil. Si les deux vues sont visibles côte à côte, c'est la dernière cliquée qui compte.
- Simulation de peinture (bouton « gouttes » de la barre d'outils, à côté de Projection et Remplissage par polygone, dans les projets 3D et les documents avec un modèle dans la Vue 3D ; choisir l'outil affiche ses réglages dans Matériau du pinceau) : en maintenant le clic, des particules partent du pinceau et peignent le matériau du pinceau là où elles passent, même derrière le modèle : coulures, ruissellement épais, poussière qui tombe, éclaboussures, propagation de taches, suie qui monte ; réglages dans Matériau du pinceau (débit, taille, gravité, adhérence, durée de vie).
- Modèles à plusieurs matériaux : à la création du projet (Nouveau projet Paint 3D > Matériaux multiples), chaque matériau dont les UV recouvrent celles d'un autre (ou sortent de 0..1) reçoit sa propre tuile carrée de la texture ; l'export (textures séparées, Unreal ORM ou Unity) écrit alors un jeu de textures par matériau, découpé dans sa tuile (Modèle_Matériau_BaseColor.png...), avec un script / manifeste Unreal chacun. Les tuiles et les matériaux sont enregistrés avec le projet. Sur un projet existant : Vue 3D > « Une tuile par matériau » (la peinture déjà faite suit). Matériau du pinceau > « Limiter au matériau sous le pinceau » : un trait ne peint que le matériau sur lequel il commence.
- Recadrage en perspective (Image > Recadrage en perspective, ou case « Perspective » de l'outil Recadrage) : tracez le cadre, placez ses quatre coins sur les bords d'un plan (façade, document, tableau photographié de biais), puis Entrée : l'image est redressée en rectangle (taille tirée des côtés du cadre).
- Coller en perspective (Édition > Coller en perspective, façon Point de fuite) : le contenu du calque actif est posé sur un plan de l'image ; glissez les quatre coins sur les bords d'un mur, d'un sol, d'un écran (grille de perspective affichée), glissez l'intérieur pour déplacer le plan. Option Mosaïque : le motif est répété sur tout le plan (carrelage, papier peint), taille réglable. Résultat sur un nouveau calque, Entrée pour appliquer, Échap pour annuler.
- Appliquer une image (Image) : un calque (ou l'image fusionnée) d'un document ouvert de même taille, en RVB ou une seule couche (rouge, vert, bleu, transparence), inversée ou non, fondue dans le calque de pixels actif avec un mode et une opacité, dans la sélection s'il y en a une, en conservant la transparence si demandé ; une étape d'annulation.
- Planche contact (Fichier > Automatisation) : les images d'un dossier (sous-dossiers en option) en vignettes sur une grille (colonnes, lignes, marge, espacement), sur autant de pages que nécessaire (un document par page : A4, A4 paysage, écran ou taille libre) ; les vignettes sur un calque, les noms de fichier dessous sur un autre (deux calques par page, quel que soit le nombre d'images) ; pages aplaties sur demande. JPEG, PNG, PSD, TIFF, WebP et RAW ; un fichier illisible est passé sans laisser de case vide. Progression et annulation dans la fenêtre de travail.
- Recadrer et redresser les photos (Fichier > Automatisation) : plusieurs photos numérisées ensemble sur un fond uni (vitre du scanner) sont trouvées, chacune redressée (rotation corrigée) et recadrée légèrement à l'intérieur de ses bords, puis ouverte dans son propre document (Photo 1, Photo 2... dans l'ordre de lecture). Le fond est la couleur dominante du bord de l'image.
- Calculs (Image) : deux couches (gris, rouge, vert, bleu ou transparence) de calques ou d'images fusionnées de documents ouverts de même taille, chacune inversée ou non, fondues avec un mode et une opacité (la source 1 sur la source 2) ; le résultat devient une sélection, un nouveau calque gris ou un nouveau document. Pratique pour construire une sélection à partir des couches (cheveux, ciel).
- Instantanés du document (panneau Historique) : « Créer un instantané » mémorise tout le document (calques, réglages et une copie des pixels) ; un clic le rétablit en une étape d'annulation, même quand l'historique a oublié les étapes qui y menaient ; double-clic pour renommer, clic droit pour supprimer (mémoire utilisée affichée).
- Définir une forme prédéfinie de pinceau (Édition) : la sélection, ou l'image entière, devient une pointe de pinceau échantillonnée (le sombre peint, le blanc et le transparent non), rognée sur son contenu ; elle rejoint la catégorie « Mes pinceaux » et devient la pointe du Pinceau, à sa taille d'origine (rotation aléatoire et suivi du trait réglables).
- Aligner (Calque > Aligner) : le contenu du calque actif aligné sur la sélection, ou sur la zone de travail sans sélection : bords gauches, centres horizontaux, bords droits, bords supérieurs, centres verticaux, bords inférieurs (calques de pixels, texte, formes, objets dynamiques ; une étape d'annulation). Avec plusieurs calques choisis et sans sélection, ils s'alignent entre eux ; à partir de trois, « Répartir les centres » horizontalement ou verticalement les espace régulièrement (les deux extrêmes restent en place).
- Choisir plusieurs calques (panneau Calques) : Ctrl +clic sur une ligne ajoute ou retire le calque du choix, Maj +clic choisit la plage depuis le calque actif ; Ctrl +clic sur la vignette charge toujours ses pixels comme sélection. Les calques choisis se déplacent ensemble avec l'outil Déplacement ou les flèches (leurs cadres suivent pendant le glisser, une seule étape d'annulation ; avec une sélection de pixels, seule elle se déplace), se groupent ensemble ( Ctrl + G , les calques écrêtés suivent leur base), se suppriment ensemble (corbeille, Calque > Supprimer les calques) et s'alignent ou se répartissent ensemble.
- Navigateur (onglet Général, ou Fenêtre > Panneaux) : toute l'image en petit, la partie visible dans le canevas encadrée de rouge ; cliquer ou glisser dedans déplace la vue, la molette zoome ; curseur de zoom (1 à 6400 %), boutons - / + et Ajuster.
- Lier les calques (Calque > Lier les calques, ou le bouton chaîne en bas du panneau Calques) : les calques choisis restent liés (icône de chaîne, en couleur pour ceux liés au calque actif) ; déplacer l'un d'eux, à la souris ou aux flèches, emporte les autres même quand il est choisi seul. Calque > Délier les calques, Sélectionner les calques liés. Les liens sont enregistrés avec le document.
- Repères commentés (Affichage > Repères commentés, avec le Magnétisme) : en déplaçant un calque, ses bords et son centre s'alignent sur ceux des autres calques et de la zone de travail, avec des lignes magenta ; Ctrl pendant le glisser pour s'en affranchir.
- Éditeur de dégradé (options de l'outil Dégradé) : barre d'arrêts (clic pour ajouter, glisser, clic droit pour supprimer, jusqu'à 16), couleur et opacité de chaque arrêt ou couleur de premier plan / d'arrière-plan suivie, préréglages (premier plan vers transparent, arc-en-ciel, coucher de soleil, chrome, cuivre...), inverser, tramage, interpolation perceptuelle (sRGB) ou linéaire.
- Symétrie de peinture (options du Pinceau et de la Gomme) : axe vertical, horizontal, deux axes, radiale (N segments) ou mandala (radiale + miroir) ; centre et angle réglables, axes affichés sur le canevas. Chaque touche est répétée, orientation de la pointe comprise.
- Exporter sous... (Fichier) : plusieurs tailles d'un coup avec leur suffixe (1x, @2x, @3x...), PNG / JPEG (qualité) / TIFF 16 bits / TGA / BMP, transparence ou couleur de fond, bords transparents rognés ; l'image entière, le rectangle de la sélection ou chaque calque séparément (un fichier par calque, nommé d'après lui).
- Déformation du texte (Propriétés d'un calque de texte) : Arc, Arc inférieur, Arc supérieur, Arche, Renflement, Drapeau, Onde, Poisson, Montée, Gonflement, Compression, Torsion ; inflexion, distorsions horizontale et verticale, orientation verticale. Le texte reste modifiable (la déformation est refaite à chaque changement) et elle est enregistrée avec le document.
- Remplacement de couleur (Maj+B depuis le Pinceau) : peint la couleur de premier plan sur les tons proches de celui sous le centre du pinceau (tolérance), en gardant la luminosité ; modes Teinte, Saturation, Couleur, Luminosité.
- Effets d'éclairage (filtre, catégorie Lumière et lueur) : projecteur, point ou lumière infinie (+ seconde lumière ponctuelle), couleur, intensité, portée, ouverture, hauteur, lumière ambiante, relief tiré de la luminosité ou de l'alpha, brillance ; positions placées sur l'image (bouton « + »).
- Correcteur localisé (J, avec le Correcteur) : peignez sur un défaut (poussière, bouton, fil électrique, petit objet) ; au relâchement, la zone peinte est reconstruite d'après ce qui l'entoure (même moteur que Remplissage d'après le contenu), sans source à choisir. [ et ] : taille.
- Pinceau mélangeur (avec le Pinceau dans la barre d'outils) : le pinceau porte une réserve de peinture qui se dépose puis reprend la couleur de la toile à chaque touche ; Humidité (combien de toile il reprend), Charge (combien de couleur de premier plan il recharge), Débit, nettoyage après chaque trait, préréglages Sec / Humide / Très humide / Mélange seul.
- Règle (avec la Pipette) : glissez pour mesurer une longueur et un angle (Maj : multiples de 45°), ajustez les extrémités ; « Redresser l'image » tourne l'image pour rendre la ligne horizontale (ou verticale), en gardant la taille, en recadrant les coins vides ou en agrandissant. Image > Rotation de l'image > Paramétrée... : n'importe quel angle, mêmes options (aussi en étape d'action).
- Zone de netteté (Sélection > Zone de netteté...) : sélectionne les parties nettes d'une photo (le sujet mis au point devant un fond flou), d'après le détail local ; seuil, transition, rayon d'analyse, « Combler les zones unies » pour inclure les surfaces sans texture entourées de contours nets, inversion pour prendre le flou. Résultat en direct, OK pour garder.
- Rogner (Image > Rogner...) : retire les bords faits de pixels transparents, ou de la couleur du pixel en haut à gauche / en bas à droite, sur les côtés choisis.
- Galerie de flous (effets, catégorie Flou) : Flou de diaphragme (net dans une ellipse, flou progressif autour), Inclinaison-décalage (bande nette, effet maquette), Flou de l'objectif (profondeur) (le flou suit une carte de profondeur branchée sur l'entrée Profondeur : mise au point, profondeur de champ, sujets nets gardés nets devant un fond flou). Tous ont un bokeh : hautes lumières qui s'épanouissent en disques ou en polygones (lamelles du diaphragme). Les positions (centre du flou, de la bande nette...) se placent directement sur l'image : bouton « + » à côté du réglage dans Propriétés, puis clic ou glisser sur le canevas (Échap pour finir).
- Séparation de fréquences (Calque > Séparation de fréquences) : deux copies du calque, basses fréquences (couleurs et volumes, flou gaussien) et hautes fréquences (texture, en Lumière linéaire) qui recomposent exactement l'image ; retouchez les teintes sur l'une, les détails sur l'autre.
- Modèles UDIM (Paint 3D) : un modèle dont les UV couvrent plusieurs tuiles (1001, 1002, 1011...) est reconnu à la création du projet ; chaque tuile devient une case de la texture, on peint normalement (option « Limiter à la tuile UDIM sous le pinceau »), et l'export écrit un jeu de textures par tuile, nommé comme l'attendent les outils 3D : `nom_BaseColor.1001.png `, `nom_ORM.1002.png `... L'export du maillage (OBJ) remet les UV dans l'espace UDIM.
- Animation image par image (Fenêtre > Panneaux > Animation, à côté du Graphe) : « Créer l'animation » mémorise l'état des calques comme première image ; chaque image retient la visibilité, l'opacité, le mode de fusion, les filtres activés et la position des objets dynamiques, et l'image sélectionnée suit les changements faits aux calques. Dupliquer l'image, Interpoler (images intermédiaires : fondus d'opacité et déplacements des objets dynamiques), glisser une vignette sur une autre pour la déplacer, durée par image (menu sous la vignette, ou « Toutes les images à cette durée »), lecture (une fois, trois fois, en boucle). Pelures d'oignon : l'image précédente (teinte rouge) et la suivante (teinte verte) en transparence sur le canevas (clic droit sur la case : image suivante, opacité). Export en GIF animé (durées et répétitions conservées) ou en vidéo MP4 (30 images/s).
- Mise à l'échelle selon le contenu (Image > Mise à l'échelle selon le contenu...) : change la largeur et / ou la hauteur en retirant (ou en doublant) les chemins de pixels les moins visibles, si bien que les sujets gardent leur forme pendant que le fond se resserre ou s'étire. Réduction jusqu'à 10 %, agrandissement jusqu'au double. Une sélection peut être protégée (gardée intacte) ou supprimée (les chemins passent d'abord par elle). Le calcul se fait en arrière-plan (barre de progression, Interrompre) puis s'applique à tous les calques et masques en une seule étape d'annulation.
- Nouveaux nœuds de l'éditeur d'effets : Rampe de couleurs (barre de dégradé : clic pour ajouter un arrêt, glisser pour le déplacer, clic droit pour le supprimer, couleur de l'arrêt choisi, inverser), Normale depuis la hauteur (carte de normales tirée de la luminosité de l'image), Interrupteur (une case à cocher dans Propriétés qui choisit une branche ou l'autre), Décaler les UV, Tourbillon (UV).
- Fonctions dans l'éditeur d'effets (comme les Material Functions d'Unreal) : sélectionnez des nœuds puis Ctrl+G (ou « Créer une fonction ») : ils deviennent un seul nœud, les liens qui entraient deviennent ses entrées et ceux qui sortaient ses sorties ; double-clic pour l'ouvrir (chemin « Graphe > Fonction » et « Remonter au graphe parent »), nœuds « Entrée de fonction » / « Sortie de fonction » (nom, ordre, valeur par défaut) pour changer son interface ; « Dégrouper » remet les nœuds à plat ; « Enregistrer dans la bibliothèque » la rend disponible dans tous les effets (menu d'ajout > Fonctions de la bibliothèque). Le code généré et l'export Unreal sont identiques à ceux des nœuds d'origine.
- Peinture liquide (bouton « coulures » de la barre d'outils, en 2D comme en 3D) : le pinceau pose un film de peinture simulé sur la carte graphique qui coule avec la gravité et transporte tout le matériau à la fois : couleur, rugosité, métal, émissif, et son épaisseur donne le relief (canal Hauteur). Dans un projet 3D, la pente vient du modèle : la peinture descend le long des faces, contourne le relief existant (« Suit le relief »), se détache sous les surfaces tournées vers le bas ; la gravité peut être le bas du monde, le bas de la vue (tournez la caméra pour choisir où elle coule, même pendant qu'elle coule) ou une direction personnalisée (un plan posé à plat devient un mur incliné). Sur une image, le support est un plan dont on règle l'inclinaison (à plat, 45°, mur, plafond) et le sens de la pente ; une sélection retient la peinture comme un ruban de masquage. Réglages : charge (au-dessus de 1, l'excédent coule en petites coulures tout le long), débit (un pinceau immobile fait une flaque qui coule en grosse goutte), viscosité, film retenu, étalement, cohésion, prise, irrégularités, tache le support, couvrance, brillance humide (plus brillante et plus sombre tant qu'elle est humide). Préréglages : peinture qui coule, empâtement, encre, huile / goudron, rouille, eau (seulement la rugosité), métal liquide, cire, sang, lave émissive. Sous les surfaces tournées vers le bas, la peinture épaisse se détache en gouttes qui tombent (visibles dans la Vue 3D) et retombent plus bas sur le modèle, où elles coulent à leur tour. Sur un modèle, la peinture passe les coutures UV : elle continue sur l'île voisine (celle qui touche ce bord sur le modèle), par exemple du dessus d'un cube vers sa face avant. « Traces de poils » : les poils du pinceau creusent des sillons le long du trait, qui restent en relief avec une peinture épaisse (préréglage Empâtement). « Faire couler le calque actif » : la peinture déjà présente dans le calque (tous ses canaux dans un projet 3D) devient liquide et coule. « Reprise de couleur » : en coulant sur la peinture du calque, le liquide prend de sa couleur (frais sur frais). Ctrl : enlève la peinture humide ; Pause ; Entrée fige, Échap annule. La peinture se fige seule après le temps de prise (ou en changeant d'outil, en annulant) : une seule étape d'historique pour tous les canaux.
- Vue orthographique : case « Ortho » de la Vue 3D (pavé numérique 5 ), menu « Vues » : face, arrière, gauche, droite, dessus, dessous (pavé numérique 1 3 7 , avec Ctrl la vue opposée). La molette règle la taille de la vue ; la peinture, le sol et le path tracing suivent la projection. En mode Rendu : Caméra > « Orthographique ».
- Path tracing dans la Vue 3D (liste de qualité de la Vue 3D, « Path tracing ») : le moteur du mode Rendu, progressif comme dans Marmoset (reflets, ombres douces, lumière indirecte, sol pour les ombres portées), avec l'HDRI de la vue ou le studio, la lumière principale en soleil et les effets de caméra. L'image s'affine quand rien ne bouge et suit la peinture en direct ; échantillons, rebonds et sol dans « Rendu et caméra... ».
- Pipette 3D : outil Pipette, clic sur le modèle : sa couleur, sa rugosité, son métal et sa hauteur à cet endroit passent dans le matériau du pinceau.
- Maj + clic droit glissé dans la Vue 3D : l'éclairage (HDRI et lumières) tourne autour du modèle, la caméra ne bouge pas. Dans Calques 3D, Alt + clic sur la case de visibilité affiche le calque seul (encore : tous).
- Environnements intégrés (bouton HDRI de la Vue 3D, et onglet Rendu) : studio photo, ciel couvert, plein soleil, coucher de soleil, néons, ou un fichier HDR ; l'environnement de la vue est enregistré avec le projet.
- Éclairage (« Rendu et caméra... ») : préréglages Simple , Studio 3 points , Contre-jour , Doux , Dramatique , Coucher de soleil , et jusqu'à 4 lumières supplémentaires (direction, hauteur, couleur, intensité, attachées à la caméra ou fixes), dans la vue rapide, qualité et en path tracing.
- Profondeur de champ (path tracing, « Rendu et caméra... ») : ouverture et distance nette ; « Choisir sur le modèle » puis un clic sur le point qui doit être net.
- Caméras : « + Enregistrer la vue » garde l'angle, le cadrage et la focale ; un clic sur une caméra y revient en douceur. Réglage de la focale (champ). Enregistrées avec le projet, comme les lumières.
- Turntable... (barre de la Vue 3D) : le modèle fait un tour complet, exporté en vidéo MP4 , en GIF animé (en boucle, 720 px de large au plus) et / ou en images PNG (nombre d'images, images par seconde, taille jusqu'à 4K, échantillons par image en path tracing). « Éclairage fixe » : lumières et HDRI tournent avec la caméra, comme un plateau tournant.
- Rendu : « Rendu qualité » ajoute l'ombre de la lumière principale, l'occlusion cuite l' ombre au sol (ombre portée et ombre de contact sur un sol invisible, case « Ombre au sol ») et l' occlusion d'écran (SSAO : creux et contacts assombris en temps réel, sans cuisson) ; « Rendu et caméra... » règle la lumière (suit la caméra ou fixe, direction, hauteur, soleil avec un HDRI) et les effets de caméra : exposition, courbe (ACES, AgX, PBR Neutral...), bloom, netteté, aberration chromatique, température / teinte, contraste, saturation, vignettage, grain, avec des préréglages (studio produit, cinéma, jeu vidéo, vintage). Les mêmes effets s'appliquent au rendu par lancer de rayons (onglet Rendu, réglages) et à son export PNG ; ils sont enregistrés avec le projet.

### Fichiers Unreal (.uasset)

- Textures (.uasset de projets UE 4.x à 5.8, éditeur ou cookées, BC1 à BC7) : Fichier > Ouvrir, glisser-déposer sur le canevas / le graphe (objet dynamique ou nœud image), comme une image. Oodle est chargé depuis votre installation d'Unreal si besoin.
- Maillages (StaticMesh, SkeletalMesh) : glissez-les dans l'onglet Rendu, ouvrez-les (nouveau projet Paint 3D) ou choisissez-les dans « + Modèle... ». La version d'Unreal du projet les exporte en arrière-plan (sans fenêtre, le Content est monté en lecture seule), avec la texture de couleur et la rugosité / le métal de leurs matériaux ; le résultat est gardé en cache.
- Le .uasset doit rester dans le dossier Content de son projet (à côté du .uproject) ; la première conversion d'une version d'Unreal prend un peu plus de temps.

### Plugin Unreal Engine (Shaderixel Link)

- Copiez `Unreal/<version>/ShaderixelLink `(4.25 à 5.8) dans le dossier `Plugins `de votre projet, puis activez « Shaderixel Link » (Edit > Plugins).
- Content Browser, clic droit sur un Static Mesh > Peindre dans Shaderixel > emplacement de matériau : un projet Paint 3D s'ouvre avec le maillage (ses UV) et les textures du matériau dans un calque « Unreal (textures d'origine) ».
- Peignez, puis Ctrl + S (ou « Envoyer vers Unreal » dans la barre d'état) : les textures sont réimportées en place. Case Direct à côté : envoi automatique un instant après chaque modification, le matériau se met à jour en direct dans Unreal. Sans texture au départ : nouvelles BaseColor / Normal / ORM et une instance de matériau assignée.
- Clic droit sur une Texture 2D > Éditer dans Shaderixel .
- Modèle + matériau prêts : dans Shaderixel, Vue 3D > « Unreal... » (ou Fichier > Exporter pour Unreal) règle le nom, le dossier, l'échelle (100 pour un modèle en mètres), le matériau parent, la double face et l'intensité émissive ; ces réglages sont enregistrés dans le .sx3d. L'export écrit le modèle (OBJ), les textures et une description `.sxmat.json `.
- Dans Unreal : Tools > Shaderixel > Importer un modèle Shaderixel... avec le `.sxmat.json `, ou directement le projet `.sx3d `(Shaderixel l'exporte en arrière-plan) : maillage statique SM_, textures T_ réglées (sRGB, compression), instance MI_ du matériau M_ShaderixelPBR2 (ou de votre matériau parent) déjà assignée. Commande console : `Shaderixel.Import <fichier> [dossier] `.
- Un `.sxmat.json `posé à côté d'un FBX / OBJ du même nom est appliqué automatiquement quand vous importez ce modèle normalement dans Unreal. Les textures seules ( `.sxue.json `) s'importent par le même menu.

### Mode Rendu (path tracing)

- Onglet « Rendu » (à côté de la Vue 3D d'un projet .sx3d) : le modèle peint est rendu en path tracing progressif ; l'image s'affine au fil des échantillons. Clic gauche / droit : orbite, molette : zoom, clic molette : déplacer.
- Calcul GPU par défaut (toute carte graphique : NVIDIA, AMD, Intel) ou CPU (tous les cœurs, même résultat). Pause, Recommencer, « Exporter l'image... » en PNG, JPEG ou HDR (Radiance, valeurs linéaires). « Exporter les passes... » : l'image et ses passes pour le compositing (albédo, normales, profondeur, masque de couverture).
- Glissez-déposez dans l'onglet Rendu un modèle (.obj avec son .mtl, .fbx, .gltf / .glb) ou un projet Paint 3D (.sx3d) : il rejoint la scène, posé au sol, avec ses matériaux (un par partie, texture de couleur comprise) ou ses textures peintes. Aussi par « + Modèle... ».
- Panneau « Scène de rendu » (onglet 3D) : objets (sol, fond cyclo, cube, sphère, maillages OBJ / FBX / glTF importés, projets .sx3d, copies du modèle) avec position, rotation, échelle et matériau (préréglages, verre, métal, émission) ; lumières ponctuelles, spots, surfaciques et soleil ; environnement studio intégré ou fichier HDRI (intensité, rotation, visible en fond).
- Caméra : champ de vision, profondeur de champ (ouverture, mise au point) ; réglages : échantillons, rebonds, taille de la vue ou résolution fixe (jusqu'à 8K), exposition, courbe ACES / Reinhard. La scène est enregistrée dans le .sx3d.

### Scènes de rendu

- La scène (objets, matériaux, lumières, HDRI, réglages, caméra) est enregistrée avec le projet .sx3d ; les fichiers dans le dossier du projet sont notés en chemins relatifs.
- Panneau « Scène de rendu » > Scène : préréglages (Studio doux, Studio trois lumières, Produit sur fond noir, Extérieur ensoleillé, Contre-jour dramatique, Sol réfléchissant) appliqués en entier, pour l'éclairage et l'environnement seulement, ou pour ajouter leurs objets ; les positions suivent le sol du projet.
- « Enregistrer la scène... » / « Charger une scène... » : un fichier .sxscene réutilisable dans d'autres projets ; « Enregistrer comme préréglage » l'ajoute à la liste.
- « Rassembler les fichiers » : les modèles importés (avec .mtl, textures, .bin ; un maillage Unreal sous sa forme convertie) et le HDRI sont copiés dans « <projet>_fichiers » à côté du projet, qui reste complet si les originaux bougent ou si vous l'envoyez. Les fichiers introuvables sont signalés en rouge.

### Éditeur d'effets et graphes

- « ◀ Bibliothèque » : effets, fusions et pinceaux avec recherche ; « Nouveau à partir de ce modèle » (copie) ou « Éditer l'original » (après déverrouillage). « Annuler les modifications » revient à l'état et au panneau d'avant.
- Palette de nœuds (onglet Effets) : glissez un nœud sur le graphe des calques ou sur le graphe d'effet, ou cliquez pour l'ajouter au centre.
- Glissez une image depuis l'explorateur sur le graphe, le canevas ou les calques : en mode calques, elle arrive à sa taille réelle comme objet dynamique (Ctrl+T la transforme sans perte) ; en mode nodal, un nœud image suivi d'un nœud Transformation (position, échelle, rotation, pivot), fusionné au résultat. Ailleurs, elle s'ouvre comme document.
- Langue de l'interface : anglais par défaut ; Affichage > Langue / Language pour passer en français (ou dans une autre langue dont le dictionnaire est dans le dossier `lang `), sans redémarrer. Les filtres et la recherche acceptent les mots des deux langues.
- Filtres partout (comme dans Unreal) : un champ « Filtrer » en haut des panneaux (Propriétés, Options, Couleur, Matériau du pinceau, Scène de rendu, Photo RAW, réglages de la Vue 3D, dialogues d'export, cuisson, lots, plugins...) ne garde que les réglages dont le nom contient les mots tapés (sans tenir compte des majuscules ni des accents) ; le titre d'une section qui correspond garde toute la section. Dans les listes (Calques, Calques 3D, Historique, Actions), le champ filtre les lignes. Chaque menu principal a aussi son champ, prêt à taper dès l'ouverture : les sous-menus s'affichent à plat. Ctrl + F place le curseur dans le filtre du panneau, Échap l'efface.
- Cliquez un nœud dans le graphe (mode calques) : Propriétés affiche d'abord ce nœud (filtre : ses paramètres ; fusion : mode, opacité, écrêtage ; masque ; remplissage / réglage ; sortie : l'image finale et le document), les autres réglages du calque étant repliés dans « Calque : ... ». Aperçu du calque montre le résultat de ce nœud (« Voir le calque » pour revenir). Cliquer le calque dans Calques revient à ses propriétés.
- Barre d'état : Vue finale / calque seul / jusqu'au calque ( Ctrl + Alt + 1 2 3 ).

### Retouche et déformation

- Sélectionnez un objet puis Édition > Remplir d'après le contenu ( Maj + F5 ) : la zone est reconstruite depuis le reste de l'image, sans IA.
- Groupe du correcteur ( J , Maj + J pour changer) : Pièce, Déplacement basé sur le contenu, Yeux rouges. Gommes ( Maj + E ) : gomme d'arrière-plan, gomme magique. Forme d'historique ( Y ) : instantané dans le panneau Historique.
- Historique : les dernières retouches restent sur la carte graphique (annulation instantanée) ; les plus anciennes sont compressées en mémoire système, ce qui libère la VRAM sur les grands documents, et rechargées quand on les annule. Le panneau Historique affiche la mémoire utilisée (GPU / système) ; « Réglages mémoire... » règle la VRAM gardée pour l'annulation instantanée, la VRAM maximale et la mémoire système maximale (Auto : selon la carte graphique et la mémoire du PC, par exemple environ 3 Go sur une carte de 24 Go).
- Édition > Marionnette (épingles) et Déformation de la perspective (plans, puis « Déformer »).

### Photo

- Ouvrez un RAW : tous les formats d'appareils photo sont décodés par LibRaw (CR2, CR3, CRW, NEF, NRW, ARW, RAF, ORF, RW2, PEF, DNG, X3F, 3FR, IIQ...), en 32 bits linéaire, balance des blancs de l'appareil, sans courbe ajoutée. La fenêtre Photo RAW s'ouvre aussitôt.
- Filtre > Photo RAW... ( Ctrl + Maj + A ) sur n'importe quel calque : grand aperçu (molette ou double-clic pour zoomer, glisser pour se déplacer), histogramme, Avant ( P ). Sections : Base (balance des blancs, exposition, contraste, hautes lumières, ombres, blancs, noirs, texture, clarté, correction du voile, vibrance, saturation), Courbe des tonalités, Détail (netteté, réduction du bruit de luminance et de couleur), Mélangeur de couleurs (teinte / saturation / luminance par couleur), Étalonnage des couleurs (ombres, tons moyens, hautes lumières, équilibre), Optique, Effets (vignettage, grain). OK = une seule étape d'historique ; Annuler revient à l'état d'avant. Les réglages restent modifiables : c'est le filtre Photo RAW du calque (bouton « Ouvrir dans Photo RAW... » dans Propriétés).
- Photo RAW : Pipette ( W ) puis clic sur une zone neutre pour la balance des blancs ; « Balance auto », « Tel quel » (balance de l'appareil), « Auto » (balance, exposition, hautes lumières, ombres, blancs et noirs calculés sur l'image). Préréglages : Vif, Paysage, Portrait doux, Noir et blanc, Film délavé, Tons chauds / froids, Teal & orange, Haute / basse clé... ; « Enregistrer... » crée les vôtres (avec ou sans balance et exposition, clic droit pour supprimer). « Copier » / « Coller » reporte les réglages d'une photo sur une autre.
- Géométrie (section Optique de Photo RAW, ou Filtre > Déformation > Géométrie) : perspective verticale (verticales des bâtiments qui convergent quand l'appareil est incliné), perspective horizontale, rotation pour redresser l'horizon, aspect, échelle, décalage ; « Remplir le cadre » zoome juste assez pour ne pas laisser de bords vides, « Afficher une grille » aide à aligner. « Avant » ( P ) coupe aussi la géométrie et les retouches locales.
- Retouches locales (section de Photo RAW) : « + Filtre gradué » (effet plein au point A, nul au point B : ciel, premier plan) et « + Filtre radial » (ellipse : visage, sujet ; « Inverser » pour agir autour). Déplacez les poignées sur l'aperçu (point rouge : A / centre ; poignée du milieu : tout le dégradé ; poignées de l'ellipse : rayons), réglez température, teinte, exposition, contraste, hautes lumières, ombres, blancs, noirs, saturation, voile et une couleur ajoutée. « Afficher le masque » montre la zone en rouge. Chaque retouche est un filtre « Retouche locale » du calque, modifiable ensuite.
- Développer par lots : dans Photo RAW, « Lot... » > « Traiter un dossier de RAW avec ces réglages... » crée une action « Photo RAW - <photo> » (étape Photo RAW (développement) : vos réglages, la correction d'objectif d'après les métadonnées de chaque photo, et en option la balance et les tons automatiques par image) et ouvre le Traitement par lots en JPG : choisissez le dossier, Démarrer. L'étape s'ajoute aussi à vos actions depuis l'éditeur d'actions.
- Export TIFF 16 bits (Fichier > Exporter..., type « TIFF 16 bits », étape Exporter des actions, format de sortie du traitement par lots) : 16 bits par couche, compression sans perte (deflate), transparence gardée si l'image en a ; idéal après un développement RAW.
- Panneau Ressources : les RAW ont une vignette (aperçu intégré au fichier), avec leurs dimensions et le boîtier.
- Correction d'objectif (lensfun) : téléchargez la base de profils lensfun (fichiers XML du dossier data/db du projet lensfun, bouton « Page de téléchargement ») et placez-les dans `%LOCALAPPDATA%\Shaderixel\lensfun `(« Ouvrir le dossier », puis « Recharger » ; ou « Choisir un autre dossier... » ; une installation darktable / RawTherapee est détectée). Dans Optique, cochez « Activer les corrections de profil » : l'objectif est retrouvé d'après les métadonnées de la photo (sinon cherchez-le dans la liste), focale, ouverture, distance et crop du boîtier ajustables ; distorsion, aberrations chromatiques et vignetage corrigés, « Remplir le cadre » recadre les bords courbés.
- Simuler un objectif : Filtre > Déformation > Correction d'objectif sur n'importe quel calque, mode « Simuler l'objectif », choisissez l'objectif dans la liste lensfun des Propriétés : sa distorsion, ses aberrations chromatiques et son vignetage sont appliqués à l'image (par exemple un 10 mm grand-angle sur un rendu 3D). Les paramètres restent réglables à la main (modèles poly3 / poly5 / ptlens) et l'intensité de chaque défaut est dosable.
- Fichier > Automatisation > Photomerge (panorama) ou Fusion HDR. Édition > Alignement / Fusion automatique des calques (panorama ou empilement de mise au point).
- Couleur : Affichage > Épreuvage ( Ctrl + Alt + Y ), Image > Mode (RVB, CMJN, Lab), Édition > Attribuer / Convertir en profil, Fichier > Exporter en TIFF CMJN.

### Productivité et ressources

- Onglet Outils > Actions : « Enregistrer », utilisez les menus (taille, filtres, rotation...), « Arrêter », puis « Exécuter » ou « Traitement par lots... » sur un dossier.
- Fichier > Automatisation > Éditeur d'actions (ou « Éditeur... » dans le panneau Actions) : chaque étape est expliquée en clair ; cochez / décochez, glissez pour réordonner, clic droit pour exécuter une étape, jusqu'ici ou à partir d'ici ; réglages complets à droite (JSON en avancé). « Actions simples » : conversions (PNG, JPG, TGA, BMP, PSD, SXL), tailles, carré centré, 16:9, rotations, noir et blanc, vignettes ; « Copier dans mes actions » pour les modifier. Import / export en .sxactions.
- Ouvrir en grand : l'icône (deux fenêtres) en haut à droite des panneaux Effets, Fusions, Calques, Graphe, Ressources (textures et modèles 3D), Palette de nœuds, Matériaux, Pinceaux, Motifs, Trim sheet et des éditeurs ouvre le panneau dans une grande fenêtre séparée (sur le 2e écran s'il y en a un) ; la même icône le rattache. Aussi F11 sur le panneau sous la souris, ou Fenêtre > Ouvrir en grand.
- Traitement par lots : enchaînez plusieurs actions, source = dossier (sous-dossiers, filtre d'extensions) ou documents ouverts, format de sortie (ou même format que l'original), qualité JPG, noms avec {nom} {n3} {l} {h} {date}, sous-dossiers recréés, fichiers existants remplacés / ignorés / numérotés. Aperçu des noms avant de lancer, journal cliquable ensuite.
- Compositions de calques, plans de travail (Calque > Nouveau plan de travail ; Fichier > Exporter les plans de travail), styles de texte (caractère et paragraphe), texte sur tracé (cliquez sur un tracé avec l'outil Texte).
- Déposez un .abr (pinceaux), .pat (motifs), .aco / .ase / .gpl (nuanciers) sur la fenêtre. Panneaux Nuancier et Motifs dans l'onglet Peinture ; outil Tampon de motif avec le tampon ( S ).
- Analyse d'image (onglet Général, ou Fenêtre > Panneaux) : histogramme (RVB, luminance, canaux, échelle log, % écrêtés), waveform (luminance, RVB superposés, parade RVB), vectorscope (cibles R Yl G Cy B Mg, ligne des tons chair), spectre (FFT 2D de la luminance), fausses couleurs (exposition : noirs et blancs écrêtés, gris moyen, tons chair), statistiques par canal, couleurs dominantes (clic : couleur de premier plan) et valeur exacte du pixel sous la souris. Source : image entière ou calque / nœud actif ; « 4 vues » montre quatre scopes ensemble ; curseur d'intensité des scopes. Onglets Netteté et bruit (carte de netteté, score, bruit estimé) et Comparer (avec un autre calque : écart moyen et max, PSNR, % de pixels différents, carte des différences). Case Sélection : analyse limitée à la sélection. Points de mesure : pipette + Maj +clic sur le canevas (4 au plus, numérotés), valeurs lues en continu dans l'onglet Pixel.
- Formes personnalisées : outil Forme ( U ), type « Personnalisée » : 20 formes fournies (cœur, flèches, bulles, éclair, coche, croix, nuage, goutte, anneau, lune, bouclier, étiquette, pastille, maison, note, ruban...), remplissage, contour et rotation comme les autres formes. « Depuis le tracé actif » enregistre un tracé à la plume comme forme réutilisable ; « Importer un SVG... » lit les <path> et <polygon> d'un fichier SVG. Vos formes sont dans %LOCALAPPDATA%\Shaderixel\shapes (clic droit sur une vignette : supprimer).
- Avant / après (Affichage) : « Prendre un instantané » fige l'image ; continuez à retoucher puis comparez : partage glissant (tirez la ligne sur le canevas), avant seulement, ou différence amplifiée.
- Analyse sur le canevas (Affichage, ou le panneau) : zébras au-dessus d'un seuil, écrêtage (bleu / rouge), fausses couleurs, focus peaking (zones nettes en rouge), luminance seule — en pleine résolution, sans toucher à l'image.
- Formes : avec l'outil Forme sur un calque de forme, Maj unit, Alt soustrait, Maj + Alt intersecte (liste dans Propriétés).

### 4. Grands formats

- Nouveau document 16384 × 16384.
- Le rendu passe en tuiles (barre de progression en bas) et les calques n'occupent la mémoire que là où vous peignez. Un nouveau très grand document reçoit un fond de couleur (calque de remplissage, sans mémoire) et un calque vide « Calque 1 » pour peindre. Les nœuds du graphe montrent alors un détail de 512 px au centre de l'image (marqué « détail »).
- La barre d'état montre des jauges colorées (vert, orange à partir de 60 %, rouge à partir de 85 %) : utilisation du GPU (moteur le plus chargé de la carte), du CPU , la VRAM utilisée par toutes les applications sur la mémoire de la carte, et la VRAM libre . L'infobulle de la VRAM détaille la part de Shaderixel, les textures creuses et le budget accordé par le système. Mesures des compteurs Windows, une fois par seconde.

## Raccourcis utiles

Tab / Maj + Tab Replier / déplier la zone du graphe (aussi « ▼ Replier » sur la zone ; repliée, une barre en bas du canevas la rouvre et change de calque) / la colonne de droite A , puis W E R P Transformation du calque : déplacer, rotation, échelle, pivot V M L W C Déplacement, sélection rectangle/ellipse, lasso, sélection rapide/baguette, recadrage B E S J R O Pinceau, gomme, tampon, correcteur, goutte/netteté/doigt, densité −/+ et éponge G T U P I Dégradé, texte, forme, plume, pipette X D Q Permuter / réinitialiser les couleurs, mode masque Ctrl + J / E / G Dupliquer, fusionner vers le bas, grouper ( Ctrl + Maj + E : fusionner les visibles) Ctrl + D / Maj + Ctrl + I Désélectionner, intervertir ; glisser dans une sélection pour la déplacer Ctrl + R / ; / ' Règles, repères, grille (tirez un repère depuis une règle) Ctrl + 0 / 1 , Espace Adapter à l'écran, 100 %, main Glisser un curseur Mouvement relatif, curseur masqué ; Maj pendant le glisser : rapide, Ctrl : précis ( Ctrl +clic : saisir une valeur) Ctrl + S / O Enregistrer (.sxl), ouvrir (images, .sxl, .psd/.psb)

## IA optionnel

### Inpainting IA

- Panneau « Inpainting IA » (onglet IA) : cochez « Peindre le masque » ; le pinceau peint la zone à régénérer (teinte rose sur le canevas), la gomme la retire. Ou « Depuis la sélection », « Inverser », « Effacer ». Le masque est gardé avec le document.
- Écrivez le prompt, choisissez le fournisseur : ComfyUI (graphe d'inpainting intégré avec le modèle des réglages IA, Denoise pour une retouche plus légère, ou votre workflow API), OpenAI, Gemini, ou « Test local » pour essayer sans réseau.
- « Générer » : la zone et son contexte partent avec le masque ; le résultat arrive sur un nouveau calque dont le masque est la zone agrandie et adoucie (réglages Agrandir / Adoucir) : il se fond dans l'image, rien n'est écrasé. Avec plusieurs variations, cliquez sur leurs vignettes pour les comparer sur le calque.

### Génération et remplissage génératif

Panneau Génération IA : définissez la variable d'environnement de clé ( `OPENAI_API_KEY `ou `GEMINI_API_KEY `) avant de lancer l'application. Le remplissage génératif remplace la zone sélectionnée sur un nouveau calque masqué.

Sans clé, testez la chaîne avec `set SX_AI_MOCK=1 `avant de lancer : un résultat factice local est utilisé.

ComfyUI (local) : choisissez ce fournisseur, indiquez l'adresse du serveur (http://127.0.0.1:8188 par défaut) et soit un checkpoint pour le graphe intégré, soit un workflow exporté avec « Save (API format) » : le prompt, la graine, la taille et l'image actuelle (option img2img) y sont injectés.

### Pont MCP (CyToolsCore)

Panneau Assistant IA : le pont local est actif au lancement ; « Autoriser l'IA à modifier le document » pour les opérations d'écriture. Un agent externe peut piloter Shaderixel (calques, effets, export).

## Limites connues

- Les appels réels aux fournisseurs IA n'ont pas pu être testés sans clé ; les scripts Unreal générés n'ont pas été exécutés dans Unreal.
- Les interactions stylet/souris des outils récents (plume, sélection rapide, repères) ont été vérifiées par scripts ; vos retours sont précieux.
- Le fichier `shaderixel.log `à côté de l'exécutable aide à diagnostiquer un problème.

## Projets générés du paquet de test

Ces fichiers concernent le paquet d’exemples généré, distinct des six projets d’exercice du manuel. Les consignes historiques sont conservées ci-dessous.

### Calques et effets non destructifs

`Exemples/01_Calques_et_effets.sxl`

Dégradé, peinture, copie floutée en mode Écran (lueur), calque de réglage Vibrance, bruit en Incrustation avec masque. Masquez/affichez des calques, changez les modes de fusion, modifiez le rayon du flou dans Propriétés.

### Groupes et masques d'écrêtage

`Exemples/02_Groupes_et_ecretage.sxl`

Un dégradé écrêté sur la peinture, le tout dans un groupe à 85 %. Essayez Ctrl+G / Ctrl+Maj+G, Ctrl+Alt+G (écrêtage).

### Formes vectorielles et styles de calque

`Exemples/03_Formes_et_styles.sxl`

Rectangle, ellipse, étoile, polygone, ligne avec ombre portée, biseau, lueur, contour. Outil Forme (U) : déplacez/redimensionnez ; Propriétés : type, couleurs.

### Calques de texte

`Exemples/04_Texte.sxl`

Texte éditable avec dégradé écrêté. Outil Texte (T) : cliquez sur un texte pour le modifier (zone de texte dans Propriétés), ou ailleurs pour en créer un ; police, taille, interligne dans Propriétés.

### Mode nodal et nœud HLSL personnalisé

`Exemples/05_Mode_nodal.sxl`

Le document est un graphe. Panneau Graphe : clic droit pour ajouter des nœuds, reliez les sorties aux entrées ; le nœud personnalisé contient du code HLSL modifiable (Ctrl+Entrée).

### Éditeur d'effets visuel

`Exemples/06_Graphe_d_effet.sxl`

Un effet construit avec des nœuds (onglet Éditeur d'effets), façon éditeur de matériaux Unreal. Bouton « Exporter vers un matériau Unreal ».

### Calque de réglage Courbes

`Exemples/07_Courbes.sxl`

Éditeur de courbes avec l'histogramme en fond (Propriétés). Clic pour ajouter un point, clic droit pour supprimer ; panneau Histogramme.

### Options de fusion (Blend If)

`Exemples/08_Blend_If.sxl`

La lueur ne s'applique que sur les zones sombres. Propriétés > Options de fusion : déplacez les poignées, Alt pour les séparer ; Gris / Rouge / Vert / Bleu pour conditionner par canal.

### Fluidité

`Exemples/09_Fluidite.sxl`

Déformation avant, tourbillon et dilatation déjà appliqués. Outil Fluidité (groupe Goutte, clic droit) : essayez les modes dans Options.

### Outil Plume et tracés

`Exemples/10_Trace_plume.sxl`

Un tracé en cœur rempli et contouré. Outil Plume (P), panneau Tracés : Sélection, Remplir, Contour ; Ctrl+glisser pour éditer les points.

### Sélection rapide

`Exemples/11_Selection_rapide.sxl`

Deux traits sélectionnés par l'outil Sélection rapide (W). Peignez sur une zone pour l'ajouter, Alt pour retirer ; glissez dans la sélection pour la déplacer.

### Correspondance de couleur (LUT 3D)

`Exemples/12_LUT_etalonnage.sxl`

Un calque Correspondance de couleur avec une LUT .cube embarquée. Chargez LUT_Sarcelle_Orange.cube (ou la vôtre) depuis Propriétés.

### Matériau PBR pour la peinture 3D

`Exemples/13_Materiau_PBR.sxl`

Document organisé en canaux (couleur, rugosité, métal, normale, émissif, occlusion, hauteur). Onglet Vue 3D : Sphère ou « Ouvrir un maillage... » (Tore.glb), peignez avec le pinceau ; menu d'affichage pour voir un canal ; HDRI... (Ciel.hdr) ; Exporter les textures PBR + script Unreal.

### Déformation (grille)

`Exemples/14_Deformation.sxl`

Les traits de peinture courbés par une déformation. Édition > Déformation : glissez les coins, les poignées ou directement la surface ; Entrée pour valider. Aussi depuis Ctrl+T, bouton « Passer en déformation ».

### Tracé depuis une sélection

`Exemples/15_Trace_depuis_selection.sxl`

Une ellipse percée d'un rectangle et un disque transformés en tracé. Sélection > Créer un tracé depuis la sélection (ou panneau Tracés, réglage de la tolérance) ; les tracés sont conservés dans les PSD exportés.
