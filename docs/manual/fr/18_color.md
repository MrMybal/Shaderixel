# Couleur, précision et cartes de données

## Profils, épreuvage et inspection

Un profil décrit l’interprétation des couleurs ; assigner une interprétation et convertir les pixels sont deux opérations. Gardez un original connu. Épreuve et avertissement de gamut aident à inspecter une destination sans être l’export TIFF CMJN lui-même.

Choisissez la source d’analyse : image finale, calque, canal ou sélection peuvent donner des statistiques différentes. Fausses couleurs, écrêtage et netteté n’inscrivent pas ces surimpressions dans l’export. L’avant/après exige la bonne référence ; revenez à l’affichage normal pour juger le résultat.

Choisissez 8 bits pour web/UI si suffisant, 16 bits pour des dégradés plus fins si pris en charge et float pour HDR/données signées. Cartes numériques, alpha et RVB couleur n’ont pas une seule interprétation de transfert. Testez une valeur moyenne connue dans toute la chaîne si les octets exacts comptent.

Shaderixel stocke les couleurs en linéaire. Affichage et import/export d'images ordinaires appliquent une courbe de transfert. Une carte de données doit suivre la convention de sa chaîne.

## Alpha et masques

L'alpha est une couverture. L'échantillonnage prémultiplié évite les franges sombres lors du filtrage des bords.

Les nouveaux effets de masque stockent une couverture linéaire dans RVB avec alpha opaque. 0,5 représente une demi-couverture même si le gris affiché semble plus clair que l'octet 128. Masks Imported Matte to Coverage décode les PNG numériques ; l'alpha est lu directement.

## Cartes PBR numériques

La bibliothèque texture_* compense l'export PNG sRGB pour que les octets contiennent les valeurs attendues. Dans le moteur, désactivez sRGB pour hauteur, normales, rugosité, métallicité, occlusion et cartes empaquetées ; gardez-le pour l'albédo.

Un export flottant linéaire brut peut contenir les valeurs de stockage compensées. Vérifiez l'encodage avant de le considérer équivalent au PNG numérique.

## UV et distances

Un champ UI utilise rouge=U, vert=V, bleu=couverture, alpha=1. Une distance signée peut être négative. Utilisez 32 bits flottants, évitez réglages couleur et compression destructive intermédiaire, et inspectez avec le visualiseur de champ.

Connectez l'échantillonneur ou la conversion distance-couverture avant d'attendre une image habituelle.

## Aperçu

Inspectez à 100 % : une vignette cache l'aliasing. Vérifiez les octets exportés si les valeurs exactes comptent. Le flottant préserve une plage de calcul, mais ne recrée pas des détails écrêtés et ne garantit pas la plage du format de sortie.

[PBR](#pbr)
[Textures UI](#ui_textures)
