---
id: builtin-nodes
title: Référence complète des nœuds intégrés
description: Cet index couvre toutes les opérations enregistrées dans la version livrée.
section: Manuel
order: 39
parent: image-graph
sourceRevision: cbdf44d1624f2587
---

Cet index couvre toutes les opérations enregistrées dans la version livrée. Les sous-rubriques ci-dessous permettent de choisir le système de calcul. F1 sur un nœud du graphe principal ou une opération de shader ouvre sa fiche ; effets et fonctions de bibliothèque gardent leur propre référence.

## Choisir le graphe adapté {#choose-the-correct-graph}

```image
{"id": "node-types", "src": "assets/manual/node_types_fr.png", "alt": "Valeurs uniques, champs image et calculs de shader par pixel", "controls": "minimal"}
```

- Graphe image principal : images/canaux entiers, valeurs typées, variables, tableaux et exécution explicite. Sin/Cos Math utilisent des degrés.
- Éditeur d’effets : calculs scalaires/vectoriels par pixel, échantillonnage et sorties effet/fusion/pinceau. Trigonométrie et rotation UV utilisent des radians.
- Actions enregistrées : séquence de commandes gérée dans l’Éditeur d’actions, distincte des liens Exec du graphe.

Un canal image scalaire vert contient un nombre par pixel. Une broche Float contient une valeur typée pour l’ensemble du calcul. Les deux systèmes ne se convertissent pas implicitement. Les UV restent des données tant qu’une image n’est pas échantillonnée avec elles.

## Lire une fiche {#read-a-reference}

Chaque opération décrit identifiant stable, rôle, entrées ordonnées, valeurs initiales, sorties et limites. Les valeurs décrivent une opération nouvellement ajoutée. Les valeurs contextuelles de shader comme UV courant ou dureté sont nommées, plutôt qu’affichées comme des sentinelles négatives.

Les structures ont des interfaces dynamiques : un effet reçoit les broches de ses métadonnées, une fonction de ses ports internes, une variable de son type déclaré. Le panneau Propriétés de l’instance décrit sa configuration actuelle.

## Dépanner les connexions {#troubleshoot-connections}

1. Confirmez l’éditeur : effet image dans le graphe image, arithmétique interne d’un shader dans l’Éditeur d’effets.
2. Inspectez type et état tableau. Une référence Texture se relie uniquement à une broche Texture HLSL, pas à une valeur RVBA.
3. Choisissez la sortie voulue : RVBA/RVB et canaux isolés diffèrent ; les actions placent souvent Résultat après Exec.
4. Vérifiez les valeurs non reliées et exposez explicitement les réglages importants.
5. Reliez la sortie image pour voir le résultat ; lancez un événement seulement pour un calcul conservé ou une écriture voulue.

[Graphe image principal](doc:image-graph)
[Création de shaders](doc:shader-authoring)
[Actions enregistrées](doc:actions)

## Structure du graphe image {#image-graph-structure}

[Sortie du document](doc:node-output)

[Surface de pixels](doc:node-surface)

[Effet image / générateur](doc:node-effect)

[Fusion d’images](doc:node-blend)

[Séparation des canaux image](doc:node-split)

[Outil image classique](doc:node-tool)

[Instance de fonction image](doc:node-function)

[Entrée de fonction image](doc:node-subinput)

[Sortie de fonction image](doc:node-suboutput)

[Constante typée / tableau](doc:node-value)

[Opération de valeur typée](doc:node-math)

[Lire une variable](doc:node-varget)

[Définir une variable](doc:node-varset)

[Événement d’exécution](doc:node-event)

[Action d’exécution du graphe](doc:node-action)

[Reroutage du graphe](doc:node-reroute)

## Valeurs du graphe image {#image-graph-values}

[Additionner](doc:math-add)

[Soustraire](doc:math-sub)

[Multiplier](doc:math-mul)

[Diviser](doc:math-div)

[Minimum](doc:math-min)

[Maximum](doc:math-max)

[Puissance](doc:math-pow)

[Modulo](doc:math-mod)

[Valeur absolue](doc:math-abs)

[Opposé](doc:math-neg)

[Arrondi inférieur](doc:math-floor)

[Arrondi supérieur](doc:math-ceil)

[Arrondi](doc:math-round)

[Racine carrée](doc:math-sqrt)

[Sinus (degrés)](doc:math-sin)

[Cosinus (degrés)](doc:math-cos)

[Limiter](doc:math-clamp)

[Interpoler](doc:math-lerp)

[Remapper](doc:math-remap)

[Aléatoire (graine)](doc:math-random)

[Plus grand (A > B)](doc:math-greater)

[Plus petit (A < B)](doc:math-less)

[Égal (A = B)](doc:math-equal)

[Et](doc:math-and)

[Ou](doc:math-or)

[Non](doc:math-not)

[Choisir (si)](doc:math-select)

[Vec2 (X, Y)](doc:math-vec2)

[Séparer Vec2](doc:math-break-vec2)

[Couleur (R, V, B, A)](doc:math-color)

[Séparer couleur](doc:math-break-color)

[Couleur TSV (teinte 0-1)](doc:math-hsv)

[Mélanger couleurs](doc:math-mix-color)

[Concaténer](doc:math-concat)

[Nombre en texte](doc:math-to-string)

[Texte en nom](doc:math-to-name)

[Longueur du texte](doc:math-text-length)

[En entier](doc:math-to-int)

[En float](doc:math-to-float)

[Taille du tableau](doc:math-array-length)

[Élément du tableau](doc:math-array-get)

[Couleur du tableau](doc:math-array-get-color)

[Texte du tableau](doc:math-array-get-text)

[Suite de nombres](doc:math-range)

[Somme du tableau](doc:math-array-sum)

## Exécution du graphe image {#image-graph-execution}

[Séquence](doc:exec-sequence)

[Branche (si)](doc:exec-branch)

[Pour (de ... à)](doc:exec-for-range)

[Pour chaque (nombres)](doc:exec-foreach)

[Pour chaque (textes)](doc:exec-foreach-text)

[Bake](doc:exec-bake)

[Upscale](doc:exec-upscale)

[ComfyUI](doc:exec-comfyui)

[Exporter en PNG](doc:exec-export-png)

[Appeler une fonction](doc:exec-call)

[Message](doc:exec-print)

## Opérations de shader par pixel {#per-pixel-shader-operations}

[Image d'entrée](doc:shader-input)

[Échantillonner l'entrée (UV)](doc:shader-sample)

[Coordonnées UV](doc:shader-uv)

[Texture d'entrée](doc:shader-texture)

[Taille du canevas (float4)](doc:shader-canvassize)

[Position du pixel](doc:shader-pixel)

[Taille de l'image](doc:shader-size)

[Échantillonner décalé (pixels)](doc:shader-sampleoffset)

[Flou de l'entrée](doc:shader-blur)

[Contours de l'entrée (Sobel)](doc:shader-edges)

[Normale depuis la hauteur (entrée)](doc:shader-normalheight)

[Constante](doc:shader-const)

[Constante 2D](doc:shader-const2)

[Couleur](doc:shader-color)

[Pi](doc:shader-pi)

[Paramètre (réel)](doc:shader-param)

[Interrupteur (paramètre)](doc:shader-paramswitch)

[Paramètre (couleur)](doc:shader-paramcolor)

[Paramètre (2D)](doc:shader-param2)

[Ajouter](doc:shader-add)

[Soustraire](doc:shader-sub)

[Multiplier](doc:shader-mul)

[Diviser](doc:shader-div)

[Minimum](doc:shader-min)

[Maximum](doc:shader-max)

[Puissance](doc:shader-pow)

[Interpolation (lerp)](doc:shader-lerp)

[Limiter (clamp)](doc:shader-clamp)

[Smoothstep](doc:shader-smoothstep)

[Seuil (step)](doc:shader-step)

[1 - x](doc:shader-oneminus)

[Valeur absolue](doc:shader-abs)

[Opposé](doc:shader-negate)

[Saturer (0-1)](doc:shader-saturate)

[Partie fractionnaire](doc:shader-frac)

[Arrondi inférieur](doc:shader-floor)

[Arrondi supérieur](doc:shader-ceil)

[Racine carrée](doc:shader-sqrt)

[Sinus](doc:shader-sin)

[Cosinus](doc:shader-cos)

[Exp2](doc:shader-exp2)

[Log2](doc:shader-log2)

[Si (A > B)](doc:shader-if)

[Tangente](doc:shader-tan)

[Arc tangente (Y, X)](doc:shader-atan2)

[Modulo](doc:shader-fmod)

[Arrondi](doc:shader-round)

[Signe](doc:shader-sign)

[Remapper](doc:shader-remap)

[Postériser](doc:shader-posterize)

[Produit scalaire](doc:shader-dot)

[Longueur](doc:shader-length)

[Distance](doc:shader-distance)

[Normaliser](doc:shader-normalize)

[Produit vectoriel](doc:shader-cross)

[Séparer (R G B A)](doc:shader-split)

[Concaténer](doc:shader-append)

[Composer RGBA](doc:shader-make4)

[Composer RGB](doc:shader-make3)

[Composer vecteur 2](doc:shader-make2)

[Masque de composantes](doc:shader-mask)

[Luminance](doc:shader-luma)

[Désaturer](doc:shader-desaturate)

[RVB vers TSV](doc:shader-rgb2hsv)

[TSV vers RVB](doc:shader-hsv2rgb)

[Linéaire vers sRGB](doc:shader-tosrgb)

[sRGB vers linéaire](doc:shader-tolinear)

[Contraste](doc:shader-contrast)

[Rampe de couleurs](doc:shader-colorramp)

[Décaler la teinte](doc:shader-hueshift)

[Inverser les couleurs](doc:shader-invert)

[Bruit](doc:shader-noise)

[Damier](doc:shader-checker)

[Rotation UV](doc:shader-rotateuv)

[Décaler les UV](doc:shader-offsetuv)

[Tourbillon (UV)](doc:shader-twirl)

[Voronoï (cellules)](doc:shader-voronoi)

[Bruit fractal](doc:shader-fbm)

[Dégradé radial](doc:shader-radial)

[Dégradé linéaire](doc:shader-lineargrad)

[Coordonnées polaires](doc:shader-polar)

[Répéter les UV](doc:shader-tiling)

[Cercle](doc:shader-circle)

[Rectangle](doc:shader-rect)

[Opacité du calque](doc:shader-opacity)

[Composer la fusion](doc:shader-blendover)

[Touche du pinceau](doc:shader-brush)

[Texture du pinceau](doc:shader-brushtex)

[Pointe ronde](doc:shader-roundtip)

[Aléatoire par touche](doc:shader-random)

[HLSL personnalisé](doc:shader-custom)

[Reroutage](doc:shader-reroute)

[Entrée de fonction](doc:shader-funcin)

[Sortie de fonction](doc:shader-funcout)

[Fonction](doc:shader-function)

[Sortie](doc:shader-output)
