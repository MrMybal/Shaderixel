# Créer et partager effets, fusions et pinceaux

L’Éditeur d’effets construit le shader interne d’un effet. Une fonction image .sxfn assemble des effets entiers ; une fonction de shader vit dans le graphe d’effet. Ces formats diffèrent par leurs ports et leur exécution.

## Premier générateur

1. Créez un calque de remplissage à effet visuel ou modifiez une copie dans l’Éditeur d’effets.
2. Choisissez un graphe Effet avec zéro entrée image pour générer seul.
3. Ajoutez un paramètre Couleur et un masque Cercle. Reliez la couleur à Sortie RVB et le masque à Sortie Alpha. RVB/Alpha reliés remplacent leurs composantes même si RVBA est relié.
4. Ajoutez un paramètre Vec2 pour le centre et un Float pour le rayon. Définissez valeurs et plages utiles.
5. Compilez, inspectez les bords transparents et modifiez les réglages dans le document parent. Enregistrez une copie utilisateur avec nom/catégorie stables.

## Première déformation image

Reliez UV à Rotation UV, puis à UV d’Échantillonner l’entrée, et sa sortie RVBA à Sortie RVBA. Choisissez le bon indice d’image. Angle utilise des radians ; pi/2 vaut un quart de tour. Placez le centre à (0,5 ; 0,5). Modifier seulement les UV sans échantillonner affiche des coordonnées plutôt que l’image tournée.

Le support de l’image entière est généré pour l’échantillonnage arbitraire, le HLSL et les nœuds lisant les voisins. En code manuel, déclarez //@halo pour toute lecture voisine : rayon fixe, dépendant d’un paramètre ou full. Le rendu par tuiles des grandes images doit correspondre au rendu normal.

## Première fusion

Choisissez le type Fusion, qui fournit fond A, calque B et masque. Construisez le mélange RVB voulu puis fournissez-le à Composer la fusion avec A/B, masque et opacité du calque. Renvoyez son RVBA. Une formule de couleur seule ne compose pas correctement la couverture transparente.

## Graphe de pinceau

Choisissez Nouveau (graphe) dans Pinceaux. Touche du pinceau fournit coordonnées locales, couleur, pression et canaux du stylet. Utilisez Pointe ronde pour la couverture et la couleur du pinceau pour RVB. Ces graphes n’ont pas d’entrée image ordinaire ; Texture du pinceau fournit jusqu’à quatre slots. Réglez espacement, aérographe temporisé et variations de dispersion/taille/angle/opacité dans les réglages du graphe.

Suivi, angle/taille par inclinaison, rotation du stylet et taille par vitesse agissent seulement si configurés et fournis. Les aperçus simulent les canaux ; validez séparément le matériel. L’étendue doit couvrir les touches dispersées ou agrandies.

## HLSL et broches exposées

Le HLSL historique a quatre entrées A/B/C/D float4 et renvoie float4. Les broches déclarées acceptent float, float2/3/4, texture, int, bool et couleur. Une sortie utilise return ; plusieurs sorties utilisent des paramètres out nommés. Placez les fonctions auxiliaires dans les déclarations du nœud plutôt que dans un autre nœud.

Une entrée déclarée non texture non reliée devient un paramètre avec valeur, plage, libellé et groupe. Une texture non reliée se lit en blanc. Reliez les références Texture seulement aux broches Texture ; utilisez Texture2DSample dans le code. Une texture n’est une couleur qu’après lecture à des UV.

## Contrat du code manuel

```hlsl
//@name Adjustable tint
//@category User/Color
//@halo 0
//@param tint color 1 0.8 0.6 1 "Tint"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    return float4(c.rgb * tint.rgb, c.a);
}
```

Le nom de fichier .slang sans extension est l’identifiant stable ; utilisez minuscules, chiffres et soulignements. Les couleurs sont linéaires, normalement à alpha droit. Un effet déclarant //@premultiplied doit lire et renvoyer la convention correspondante. Les coordonnées restent celles du document entier en rendu par tuiles. Ne bornez pas HDR, distances signées ou champs UV sans intention.

Les paramètres acceptent float, int, bool, color, float2/3/4, enum, courbe et LUT. Un shader a jusqu’à quatre images et 32 paramètres. //@passes demande 1–16 passes ; ctx.pass indique la passe et Prev/PrevUV lisent le résultat précédent. Un générateur porte //@generator ; une fusion porte //@blend et ses entrées fond/calque/masque.

## Réutilisation et vérification

Dans un shader, regroupez une sélection en fonction pour créer des ports nommés ; les appels imbriqués sont développés à la compilation. Dans le graphe image, créez un sous-graphe/outil, exposez les variables importantes et enregistrez en bibliothèque. Gardez les .sxfn sans pixels, ne référencez que des effets installés et préservez les identifiants des paramètres exposés.

Compilez dans l’éditeur (Ctrl+Entrée dans la vue code adaptée), inspectez Erreurs de shaders et le journal, puis testez extrêmes, alpha et taille finale. Dans le dépôt, Shaderixel.exe --test-effects compile les effets livrés et écrit [test-effects] dans shaderixel.log. L’export Unreal possède ses propres limites ; compiler localement ne prouve pas l’équivalence dans le moteur.

[Tous les nœuds intégrés](#builtin_nodes)
[Création d’effets et presets](#authoring)
[Pinceaux](#brushes_tablet)
