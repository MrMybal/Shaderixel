---
id: tools
title: Outils et commandes d'édition
description: Les 46 outils de la barre possèdent une fiche dans la catégorie Outils.
section: Manuel
order: 36
parent: workspace
sourceRevision: 0b6a6c617e175f2c
---

Les 46 outils de la barre possèdent une fiche dans la catégorie Outils. Recherchez nom français/anglais, identifiant stable, opération ou prérequis. Avec le canevas, la barre ou Options actif, F1 ouvre la fiche de l'outil courant. Options propose aussi Aide de l'outil. Aide > Référence des outils ouvre cet index.

```image
{"id": "layers-view", "src": "assets/manual/layers_view_fr.png", "alt": "La barre choisit l'interaction ; Propriétés et Options modifient cible et réglages actuels.", "controls": "minimal"}
```

## Choisir la cible avant l'outil {#choose-the-target-before-the-tool}

La vignette image vise les pixels ; celle du masque vise la couverture. Un nœud sélectionné peut être un effet généré, une fonction directe ou un résultat conservé plutôt que des pixels peignables. La sélection limite les opérations de pixels indépendamment de la visibilité et des masques du calque.

Canevas et vue 3D conservent des choix d'outils selon le contexte. Certains n'apparaissent que dans le travail 3D. Le clic droit sur un groupe donne ses membres exacts. Les touches de l'infobulle peuvent être communes à un groupe ou dépendre de la vue ; le chapitre des raccourcis décrit cycles et exceptions.

Si un trait semble sans effet, vérifiez document actif, calque, vignette image/masque, verrouillage, opacité, sélection et canal PBR visible. F1 reste accessible pendant une saisie ; dans le lecteur il conserve l'article courant.

## Déplacer, cadrer et examiner {#move-frame-and-inspect}

[Déplacement](doc:tool-move)
[Transformation de calque](doc:tool-layer-transform)
[Recadrage](doc:tool-crop)
[Règle et redressement](doc:tool-ruler)
[Main](doc:tool-hand)
[Zoom](doc:tool-zoom)
[Pipette et points de mesure](doc:tool-picker)

## Sélectionner et construire la géométrie {#select-regions-and-build-geometry}

[Sélection rectangulaire](doc:tool-rect-select)
[Sélection elliptique](doc:tool-ellipse-select)
[Lasso](doc:tool-lasso)
[Lasso polygonal](doc:tool-poly-lasso)
[Lasso magnétique](doc:tool-magnetic-lasso)
[Baguette magique](doc:tool-wand)
[Sélection rapide](doc:tool-quick-select)
[Plume et tracés](doc:tool-pen)
[Texte](doc:tool-text)
[Forme](doc:tool-shape)
[Trim sheet](doc:tool-trim)

## Peindre, remplir et gommer {#paint-fill-and-erase}

[Pinceau](doc:tool-brush)
[Pinceau mélangeur](doc:tool-mixer)
[Remplacement de couleur](doc:tool-color-replace)
[Dégradé](doc:tool-gradient)
[Pot de peinture](doc:tool-bucket)
[Tampon de motif](doc:tool-pattern-stamp)
[Gomme](doc:tool-eraser)
[Gomme magique](doc:tool-magic-eraser)
[Gomme d'arrière-plan](doc:tool-background-eraser)

## Corriger et ajuster les détails {#repair-and-adjust-local-detail}

[Tampon de duplication](doc:tool-clone)
[Correcteur](doc:tool-heal)
[Correcteur localisé](doc:tool-spot-heal)
[Pièce](doc:tool-patch)
[Déplacement basé sur le contenu](doc:tool-content-move)
[Yeux rouges](doc:tool-red-eye)
[Forme d'historique](doc:tool-history-brush)
[Goutte — flou](doc:tool-blur)
[Pinceau de netteté](doc:tool-sharpen)
[Doigt](doc:tool-smudge)
[Densité moins](doc:tool-dodge)
[Densité plus](doc:tool-burn)
[Éponge](doc:tool-sponge)
[Fluidité](doc:tool-liquify)

## Peinture 3D, simulations et extensions {#paint-3d-simulations-and-extensions}

[Projection](doc:tool-projection)
[Remplissage par polygone](doc:tool-poly-fill)
[Particules de peinture](doc:tool-particles)
[Peinture liquide sur canevas ou modèle](doc:tool-fluid)
[Outil de plugin](doc:tool-plugin)

## Choisir la commande selon le résultat voulu {#choose-the-command-that-produces-the-intended-result}

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

[Opérations de calque et instantanés](doc:layers)
[Dimensions et sessions de transformation](doc:dimensions)
[Graphes directs, résultats et exécution](doc:automation)
[UV et cuisson de maillage](doc:uv-baker)
[Enregistrement et formats de livraison](doc:formats)
[Exercices guidés](doc:tutorials)
