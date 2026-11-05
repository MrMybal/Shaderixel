# Trim sheets et atlas de textures

Une trim sheet divise une texture en bandes/régions nommées pour les réutiliser sur des modèles. Le panneau Trim sheet conserve cette disposition séparément des pixels.

## Construire

Appliquez un modèle, entrez des hauteurs de bandes ou créez-les depuis les repères horizontaux. Les hauteurs vont du haut vers le bas ; zéro utilise l'espace restant selon la règle du panneau. Appliquer un modèle remplace les trims existants.

L'outil sélectionne et redimensionne les bords : pas normal de 8 px, Maj pour 1 px. Double-cliquer sélectionne les pixels du trim. Les propriétés exposent son rectangle et son nom.

## Traiter la texture

Découper en colonnes crée plusieurs tuiles horizontales. Rendre raccordable mélange le calque de pixels actif avec son contenu décalé dans le trim. Inspectez : le mélange peut changer le motif et ne recrée pas sa structure manquante.

Créez un calque ID pour les sélections/masques colorés et inspectez la répétition en 3D. Les marges limitent la lecture des régions voisines lors du filtrage.

## Exporter

Exporter la disposition écrit les noms, rectangles en pixels et UV selon origines haut-gauche et bas-gauche. Exportez l'image séparément et choisissez la même convention dans l'outil cible.

Atlas de sprites et nine-slice utilisent des idées proches, mais des fonctions différentes. Leurs gouttières/régions doivent correspondre à l'image réelle.

[Textures UI](#ui_textures)
[Repères](#layout)
