# Outils et commandes d'édition

Les 46 outils de la barre possèdent une fiche dans la catégorie Outils. Recherchez nom français/anglais, identifiant stable, opération ou prérequis. Avec le canevas, la barre ou Options actif, F1 ouvre la fiche de l'outil courant. Options propose aussi Aide de l'outil. Aide > Référence des outils ouvre cet index.

![La barre choisit l'interaction ; Propriétés et Options modifient cible et réglages actuels.](media/layers_view_fr.png)

## Choisir la cible avant l'outil

La vignette image vise les pixels ; celle du masque vise la couverture. Un nœud sélectionné peut être un effet généré, une fonction directe ou un résultat conservé plutôt que des pixels peignables. La sélection limite les opérations de pixels indépendamment de la visibilité et des masques du calque.

Canevas et vue 3D conservent des choix d'outils selon le contexte. Certains n'apparaissent que dans le travail 3D. Le clic droit sur un groupe donne ses membres exacts. Les touches de l'infobulle peuvent être communes à un groupe ou dépendre de la vue ; le chapitre des raccourcis décrit cycles et exceptions.

Si un trait semble sans effet, vérifiez document actif, calque, vignette image/masque, verrouillage, opacité, sélection et canal PBR visible. F1 reste accessible pendant une saisie ; dans le lecteur il conserve l'article courant.

## Déplacer, cadrer et examiner

[Déplacement](#tool:move)
[Transformation de calque](#tool:layer_transform)
[Recadrage](#tool:crop)
[Règle et redressement](#tool:ruler)
[Main](#tool:hand)
[Zoom](#tool:zoom)
[Pipette et points de mesure](#tool:picker)

## Sélectionner et construire la géométrie

[Sélection rectangulaire](#tool:rect_select)
[Sélection elliptique](#tool:ellipse_select)
[Lasso](#tool:lasso)
[Lasso polygonal](#tool:poly_lasso)
[Lasso magnétique](#tool:magnetic_lasso)
[Baguette magique](#tool:wand)
[Sélection rapide](#tool:quick_select)
[Plume et tracés](#tool:pen)
[Texte](#tool:text)
[Forme](#tool:shape)
[Trim sheet](#tool:trim)

## Peindre, remplir et gommer

[Pinceau](#tool:brush)
[Pinceau mélangeur](#tool:mixer)
[Remplacement de couleur](#tool:color_replace)
[Dégradé](#tool:gradient)
[Pot de peinture](#tool:bucket)
[Tampon de motif](#tool:pattern_stamp)
[Gomme](#tool:eraser)
[Gomme magique](#tool:magic_eraser)
[Gomme d'arrière-plan](#tool:background_eraser)

## Corriger et ajuster les détails

[Tampon de duplication](#tool:clone)
[Correcteur](#tool:heal)
[Correcteur localisé](#tool:spot_heal)
[Pièce](#tool:patch)
[Déplacement basé sur le contenu](#tool:content_move)
[Yeux rouges](#tool:red_eye)
[Forme d'historique](#tool:history_brush)
[Goutte — flou](#tool:blur)
[Pinceau de netteté](#tool:sharpen)
[Doigt](#tool:smudge)
[Densité moins](#tool:dodge)
[Densité plus](#tool:burn)
[Éponge](#tool:sponge)
[Fluidité](#tool:liquify)

## Peinture 3D, simulations et extensions

[Projection](#tool:projection)
[Remplissage par polygone](#tool:poly_fill)
[Particules de peinture](#tool:particles)
[Peinture liquide sur canevas ou modèle](#tool:fluid)
[Outil de plugin](#tool:plugin)

## Choisir la commande selon le résultat voulu

- Convertir en calque de pixels : remplacer le contenu rendu d'un seul calque par des pixels indépendants et peignables. Transformation/filtres sont intégrés ; masque, opacité et fusion restent éditables. Utilisez cette commande pour peindre sur un générateur, texte, forme ou objet dynamique. Annuler restaure l'original tant que l'historique correspondant est disponible.
- Fusionner vers le bas : combiner le calque actif et le calque compatible du dessous dans le même groupe. Les groupes ne sont pas pris en charge et le calque inférieur ne peut pas être un réglage. Le résultat commun diffère de la conversion d'un seul calque.
- Fusionner les calques visibles : remplacer la composition visible par un calque de pixels en conservant les calques masqués. Les enfants masqués d'un groupe fusionné peuvent passer à la racine ; les groupes devenus vides disparaissent. La transparence est conservée.
- Aplatir l'image : remplacer la pile par un calque de pixels, supprimer les calques masqués et composer les régions transparentes sur blanc. Gardez une copie native si vous aurez besoin des calques ou d'une livraison transparente.
- Bake du graphe ou Appliquer : conserver un résultat calculé. Les modifications en amont ne le régénèrent pas automatiquement. Relancez, réappliquez ou effacez les résultats selon le cas. Cela ne garantit pas un calque peignable dans la pile.
- Cuisson de maillage : calculer occlusion, courbure ou autres cartes depuis géométrie et UV. Elle demande un modèle et des UV adaptés ; elle diffère du gel de graphe et de la conversion 2D.
- Transformation manuelle : démarrer une session sur une cible compatible ; Entrée valide, Échap annule. Transformation de calque est un autre outil avec modes clavier et propriétés numériques.
- Taille de l'image : changer les dimensions de pixels et rééchantillonner selon les réglages. Zoom agit sur la vue ; Taille du canevas agit sur cadrage/étendue. Ce sont des opérations distinctes.
- Désélectionner : Ctrl+D retire la restriction d'édition, pas les pixels sélectionnés. Un masque de calque peut toujours cacher le contenu ensuite.
- Enregistrer/Enregistrer sous : conserver le document natif éditable. Exporter produit l'image livrée et ne remplace pas la structure de calques/graphe du projet.
- Exécuter un événement : déclencher volontairement export, agrandissement ou IA. Connecter une image ou ouvrir un exercice ne lance pas l'événement.

Consultez disponibilité des commandes et statut si la cible n'est pas compatible. Gardez une copie native de travail avant de réduire la structure éditable.

[Opérations de calque et instantanés](#layers)
[Dimensions et sessions de transformation](#dimensions)
[Graphes directs, résultats et exécution](#automation)
[UV et cuisson de maillage](#uv_baker)
[Enregistrement et formats de livraison](#formats)
[Exercices guidés](#tutorials)
