# Animation par images et export vidéo

![Bande d’images, composition et commandes d’animation](media/animation_view_fr.png)

Les images référencent les calques sans copier tous les pixels. Peindre un calque partagé change le contenu des images qui l’utilisent. Pour des poses peintes, gardez des calques séparés et animez leur visibilité, ou des contenus d’objets dynamiques distincts.

Réglez la durée de chaque image. Une fois/trois fois/boucle contrôle la répétition. Les peaux d’oignon comparent les voisins ; revenez à une vue normale pour les couleurs finales. Enregistrez avant suppression ou réorganisation.

La vidéo échantillonne à 30 images/seconde : les durées s’inscrivent dans cette grille. GIF possède sa propre quantification. Aucun de ces formats ne préserve le projet modifiable. Turntable appartient séparément à la présentation 3D.

L'animation fonctionne en mode Calques. Chaque image mémorise état et durée : visibilité, opacité, fusion, filtres et placement des objets dynamiques. Ce n'est pas un système général de squelette ou d'animation de personnages.

## Créer une séquence

1. Ouvrez Animation puis Créer l'animation.
2. Composez la première image avec la pile de calques.
3. Dupliquez l'image puis modifiez les états pour la pose suivante.
4. Réglez durées et répétition.
5. Lancez la lecture et inspectez avant l'export.

La bande d'images choisit l'image active. Arrêtez la lecture pour les modifications précises. Supprimer l'unique image est désactivé.

## Interpoler

Interpoler ajoute des images entre la sélection et la suivante. Opacités et positions des objets dynamiques sont interpolées. Le logiciel ne déduit pas une trajectoire depuis des pixels arbitraires et ne crée pas de poses sémantiques.

## Exporter

Exporter GIF crée une animation ; Exporter vidéo propose MP4. Vérifiez durée, dimensions et couleur dans le lecteur final. La palette GIF limite les couleurs ; la vidéo ne conserve pas une composition transparente arbitraire.

Le nœud de flipbook échantillonne un atlas selon une entrée Frame explicite : il est indépendant de l'animation du document.

[Calques](#layers)
[Export](#export)
