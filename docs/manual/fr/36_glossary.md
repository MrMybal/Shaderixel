# Glossaire et carte des workflows

- Alpha / couverture : part d'un pixel contribuant à la composition.
- Conversion en pixels : remplace le rendu d'un calque 2D par des pixels peignables.
- Bake de graphe : garde une image calculée par un outil.
- Cuisson de maillage : calcule des cartes depuis géométrie et UV.
- Draw : générateur shader sans image d'entrée.
- Graphe d'effet : calcul par pixel compilé en un shader.
- Graphe d'images : assemblage d'images, effets, valeurs et actions.
- Halo : voisinage nécessaire au rendu correct par tuiles.
- Direct / Live : recalcul lors d'un changement d'entrée.
- Résultat conservé : sortie stockée, régénérée sur exécution explicite.
- PBR : canaux destinés au rendu physiquement fondé.
- ORM : occlusion en rouge, rugosité en vert, métallicité en bleu.
- SDF : champ de distance signée, avec valeurs négatives.
- Champ UV : coordonnées d'échantillonnage, pas des couleurs ordinaires.
- Nine-slice : coins préservés, bords/centre étirés ou répétés.
- Eight-slice : bordure nine-slice au centre transparent.
- Objet dynamique : source 2D réutilisable au placement indépendant.
- Matériau intelligent : assemblage de remplissages/masques 3D.
- Modèle : document de départ ; fonction : nœud réutilisable.

## Couverture du manuel

Le guide traite les workflows utilisateur actuels et rejoint la référence générée des effets/fonctions livrés. Les valeurs initiales ne sont pas les réglages modifiés d'un document. Noms de presets et identifiants restent recherchables sous leur forme d'origine. Les broches et certains contrôles citent leur libellé actuel, parfois anglais, pour les retrouver dans la bibliothèque ; leurs explications et les notes de fonctionnement sont en français.

Les langues partagent les mêmes identités de chapitres : changer de langue conserve le sujet. Les langues d'interface non couvertes reviennent à l'anglais. Les presets utilisateur ne figurent pas dans la référence livrée.

Pour les schémas de fichiers/API, consultez les guides du dépôt. Pour une panne, utilisez Dépannage et un exemple minimal reproductible.

[Prise en main](#getting_started)
[Dépannage](#troubleshooting)
