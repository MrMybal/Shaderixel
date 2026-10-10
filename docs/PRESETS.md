# Créer des presets Shaderixel

Ce guide décrit tout ce qu'il faut pour écrire à la main (ou faire écrire par une IA) les presets de Shaderixel :

| Type de preset | Ce que c'est | Fichier | Où il apparaît |
|---|---|---|---|
| **Effet** (filtre) | une image entre, une image sort, calculé par un shader | `.slang` | Effets, menu Filtre, nœuds, calques de réglage, filtres de calque |
| **Calque Draw** (générateur) | un shader qui dessine sans entrée (couleur, motif, bruit…) | `.slang` avec `//@generator` | Effets > Générateurs, calques de remplissage, nœuds DRAW |
| **Mode de fusion** | mélange A (dessous) / B (calque) / masque | `.slang` avec `//@blend` | panneau Fusion, mode d'un calque, nœuds |
| **Sous-graphe** | une fonction de nœuds, calculée en direct | `.sxfn` (`"kind": "subgraph"`) | menu des nœuds > Bibliothèque : sous-graphes |
| **Outil** | une fonction avec des boutons d'exécution (bake, upscale, ComfyUI, export, boucles…) | `.sxfn` (`"kind": "tool"`) | menu des nœuds > Bibliothèque : outils |

Les effets, générateurs et fusions sont des **shaders** (HLSL / Slang avec quelques lignes de métadonnées).
Les sous-graphes et outils sont des **graphes** (JSON) qui assemblent des effets, des valeurs, des opérations et des actions.

Les deux sont **rechargés à chaud** : enregistrez le fichier, Shaderixel le reprend en quelques secondes (sans redémarrer).

---

## 1. Où mettre les fichiers

| Quoi | Dossier (dépôt) | Dossier (package installé) | Dossier utilisateur |
|---|---|---|---|
| Effets, générateurs, fusions | `shaders/effects/*.slang` | `shaders/effects/` à côté de l'exe | les effets enregistrés depuis l'éditeur y vont aussi (marqués `//@user`) |
| Sous-graphes, outils | `library/functions/*.sxfn` | `functions/` à côté de l'exe | `%LOCALAPPDATA%\Shaderixel\Functions\` |
| Pinceaux (2D et 3D) | `shaders/brushes/*.slang` | `shaders/brushes/` à côté de l'exe | |
| Matériaux intelligents (Paint 3D) | `library/smartmaterials/*.json` | `smartmaterials/` à côté de l'exe | `%LOCALAPPDATA%\Shaderixel\SmartMaterials\` |

- Le nom du fichier `.slang` est l'**identifiant** de l'effet (`sharpen.slang` → `sharpen`). Il doit rester stable : les documents et les fonctions le référencent. Minuscules, chiffres, `_`.
- Le nom du fichier `.sxfn` n'a pas d'importance (le nom affiché est dans le fichier).
- Une fonction qui contient des **pixels** (une image peinte, un résultat baké gardé) est enregistrée en `.sxl` + `.json` au lieu de `.sxfn`. Pour une bibliothèque de base, préférez des fonctions **sans pixels** (`.sxfn`).

---

## 2. Effets, calques Draw et fusions (`.slang`)

### 2.1 Structure

```hlsl
//@name Vignette
//@category Réglages
//@param amount float 0.5 0 1 "Intensité"
//@param radius float 0.75 0.1 1.5 "Rayon"
//@param color color 0 0 0 1 "Couleur du bord"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float d = length(ctx.uv - 0.5) * 1.41421;
    float k = smoothstep(radius, radius - 0.5, d);
    return float4(lerp(color.rgb, c.rgb, lerp(1.0, k, amount)), c.a);
}
```

- Les lignes `//@...` en tête sont les **métadonnées**.
- Le reste est du HLSL (compilé par Slang) qui doit définir `float4 Effect(EffectContext ctx)`.
- Chaque `//@param` devient une variable du même nom dans le code (`amount`, `radius`, `color`…).

### 2.2 Métadonnées

| Ligne | Effet |
|---|---|
| `//@name Nom affiché` | nom dans les menus et sur le nœud |
| `//@category Catégorie` | dossier du menu. Catégories existantes : Réglages, Traitements, Dessin, Masques, Flou, Netteté, Déformation, Pixellisation, Esthétiques, Canaux, Composition, Générateurs, Styles de calque, Fusions… (une nouvelle catégorie est créée automatiquement) | Le panneau Effets range les effets par familles ; `//@category Famille/Sous-catégorie` choisit la famille (ex. `Flou/Directionnel`).
| `//@param id type défaut… "Libellé"` | un paramètre (voir 2.3) |
| `//@group Nom` | les paramètres suivants sont rangés sous ce titre dans Propriétés |
| `//@input Nom` | une entrée image (jusqu'à 4). Sans `//@input`, l'effet a une entrée `Image`. Une entrée nommée `Masque`, `Mask`, `Alpha`, `R`, `V`, `G`, `B`, `A`, `Hauteur` ou `Height` est un **canal** (broche verte, une valeur par pixel) |
| `//@generator` | **calque Draw** : aucune entrée, le shader dessine seul |
| `//@blend` | **mode de fusion** : 3 entrées A (dessous), B (calque), Masque |
| `//@premultiplied` | les couleurs lues et écrites restent prémultipliées par l'alpha (flous, déformations : évite les franges) |
| `//@passes N` | N passes (1 à 16). `ctx.pass` donne la passe courante, `Prev(px)` lit le résultat de la passe précédente |
| `//@halo …` | voisinage lu autour de chaque pixel (rendu par tuiles des très grandes images) : `//@halo 8` (8 px), `//@halo radius 3 2` (3 × radius + 2), `//@halo full` (image entière). Sans `//@halo`, l'effet ne lit que le pixel courant (Input(0, ctx.px)) : **déclarez-le dès que vous lisez des voisins** |
| `//@user` / `//@locked` | ajoutés par l'éditeur pour les effets de l'utilisateur (un effet livré sans `//@user` ne peut pas être écrasé) |

### 2.3 Types de paramètres

| Type | Syntaxe | Variable HLSL | Exemple |
|---|---|---|---|
| `float` | `défaut min max` | `float` | `//@param radius float 4 0 250 "Rayon (px)"` |
| `int` | `défaut min max` | `float` (valeur entière) | `//@param levels int 4 2 64 "Niveaux"` |
| `bool` | `défaut` (0/1) | `float` (0 ou 1) | `//@param invert bool 0 "Inverser"` |
| `color` | `r g b a` | `float4` (linéaire) | `//@param tint color 1 0.5 0.2 1 "Teinte"` |
| `float2` | `x y min max` | `float2` | `//@param center float2 0.5 0.5 0 1 "Centre"` |
| `float3` | `x y z min max` | `float3` | |
| `float4` | `x y z w min max` | `float4` | |
| `enum` | `défaut "A|B|C"` | `float` (index) | `//@param mode enum 0 "Doux|Dur|Linéaire" "Mode"` |
| `curve` | — | courbe de tonalité (éditeur de courbe) | voir `curves.slang` |
| `lut` | — | LUT 3D `.cube` | voir `color_lookup.slang` |

Le dernier texte entre guillemets est le libellé. Maximum 32 paramètres.

Tous les paramètres `float`, `int`, `bool`, `color`, `float2/3/4` et `enum` peuvent devenir des **broches** dans le graphe (Propriétés du nœud > « Paramètres en broches », ou `paramPinNames` dans un `.sxfn`) : une fonction peut alors les piloter par une valeur, une variable ou une entrée.

### 2.4 Ce qu'on peut utiliser dans le code

```hlsl
struct EffectContext {
    int2   px;     // pixel du document (coordonnées absolues)
    float2 uv;     // 0..1 sur tout le document (centres des pixels)
    float2 size;   // taille du document en pixels
    float2 texel;  // 1 / size
    uint   pass;   // passe courante (//@passes)
};
float4 Input(uint i, int2 px);     // entrée i au pixel px
float4 InputUV(uint i, float2 uv); // entrée i en coordonnées 0..1 (bilinéaire)
bool   HasInput(uint i);           // l'entrée i est-elle reliée ?
float4 Prev(int2 px);              // passe précédente (passe 0 : l'entrée 0)
float4 PrevUV(float2 uv);
// common.slang
float  Luma(float3 c);             // luminance Rec.709
float4 Premultiply(float4 c);  float4 Unpremultiply(float4 c);
float3 SrgbToLinear(float3 c); float3 LinearToSrgb(float3 c);
float3 RGBToHSV(float3 c);     float3 HSVToRGB(float3 c);
float  Hash12(float2 p);       float2 Hash22(float2 p);   // bruit pseudo-aléatoire
```

Règles :
- Couleurs en **linéaire**, alpha **non prémultiplié** (sauf `//@premultiplied`). Retournez un `float4` de même convention.
- Les coordonnées sont toujours celles du **document entier**, même quand le rendu se fait par tuiles : utilisez `ctx.px` / `ctx.uv`, jamais un index de thread.
- Les valeurs peuvent dépasser 1 (images 16/32 bits flottants) : ne clampez que si c'est voulu.

### 2.5 Exemples

**Filtre** (un réglage) :

```hlsl
//@name Seuil doux
//@category Réglages
//@param level float 0.5 0 1 "Seuil"
//@param softness float 0.05 0 0.5 "Douceur"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    float y = LinearToSrgb1(Luma(c.rgb));
    float t = smoothstep(level - softness, level + softness, y);
    return float4(t.xxx, c.a);
}
```

**Calque Draw** (générateur, pas d'entrée) :

```hlsl
//@name Rayures
//@category Générateurs
//@generator
//@param width float 24 1 500 "Largeur (px)"
//@param angle float 45 -180 180 "Angle"
//@param colorA color 1 1 1 1 "Couleur A"
//@param colorB color 0 0 0 1 "Couleur B"
float4 Effect(EffectContext ctx)
{
    float a = radians(angle);
    float x = dot(float2(ctx.px), float2(cos(a), sin(a)));
    return frac(x / (2.0 * width)) < 0.5 ? colorA : colorB;
}
```

**Effet à voisinage** (déclarer le halo) :

```hlsl
//@name Contour
//@category Traitements
//@halo width 1 1
//@param width float 2 1 20 "Épaisseur (px)"
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    int r = (int)width;
    float m = 0;
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x) m = max(m, Input(0, ctx.px + int2(x, y)).a);
    return float4(lerp(float3(0, 0, 0), c.rgb, c.a), max(c.a, m));
}
```

**Mode de fusion** :

```hlsl
//@name Lueur douce
//@category Fusions
//@blend
//@input A (dessous)
//@input B (calque)
//@input Masque
//@param opacity float 1 0 1 "Opacité"
float4 Effect(EffectContext ctx)
{
    float4 A = Input(0, ctx.px), B = Input(1, ctx.px);
    float m = HasInput(2) ? Input(2, ctx.px).r : 1.0;
    float ab = saturate(B.a * m * opacity);
    float3 mixRgb = 1.0 - (1.0 - A.rgb) * (1.0 - B.rgb * 0.5);   // la formule du mode
    float a = ab + A.a * (1.0 - ab);
    float3 rgb = ab * (1.0 - A.a) * B.rgb + ab * A.a * mixRgb + (1.0 - ab) * A.a * A.rgb;
    return float4(a > 0.0 ? rgb / a : 0.0, a);
}
```

Les 5 exemples ci-dessus compilent tels quels. Plus de 300 effets livrés dans `shaders/effects/` servent de modèles (`cy_*.slang` : bibliothèque CyTexture).
L'éditeur d'effets de Shaderixel (onglet « Éditeur d'effets ») permet aussi de les écrire en nœuds ou en code, et « Fichier > Importer un effet HLSL » accepte du HLSL annoté (voir la compétence IA `shaderixel-hlsl-effect`).

---

## 3. Sous-graphes et outils (`.sxfn`)

### 3.1 Principe

Une **fonction** est un graphe de nœuds placé dans un seul nœud :
- ses **entrées** sont ses nœuds `subinput`, ses **sorties** ses nœuds `suboutput` (autant qu'on veut, de n'importe quel type) ;
- **sous-graphe** (`"kind": "subgraph"`) : calculé en direct, comme un effet ;
- **outil** (`"kind": "tool"`) : contient des nœuds `event` = des **boutons** (Propriétés du nœud, clic droit). Un bouton lance une **file d'exécution** (actions reliées par des liens exec blancs) : bake, upscale, ComfyUI, export, boucles, variables… Les images produites sont **gardées** : les nœuds avant ne sont plus recalculés (le bake est géré par l'outil lui-même) ;
- les **variables** exposées se règlent sur chaque nœud de la fonction (Propriétés), comme des paramètres.

Dans Shaderixel : sélectionnez des nœuds > clic droit > « Créer un sous-graphe / un outil depuis la sélection », double-clic pour éditer, « Enregistrer dans la bibliothèque » : le fichier `.sxfn` produit est exactement le format décrit ici. **Le plus simple pour démarrer est d'en enregistrer un depuis l'éditeur et de le modifier.**

### 3.2 Fichier

```json
{
 "format": "shaderixel-function",
 "name": "Netteté réglable",
 "kind": "subgraph",
 "applyMode": false,
 "description": "Texte affiché dans la bibliothèque.",
 "graph": {
  "output": 4,
  "variables": [],
  "nodes": [
   {"id": 1, "kind": "subinput", "name": "Image", "port": 0, "pos": [0, 0], "inputs": []},
   {"id": 2, "kind": "subinput", "name": "Force", "port": 1, "pos": [0, 200], "value": {"type": "Float", "f": 0.8}, "inputs": []},
   {"id": 3, "kind": "effect", "pos": [320, 0], "effect": {"effect": "sharpen", "set": {"radius": 2.0}},
    "paramPinNames": ["amount"], "inputs": [1, 2]},
   {"id": 4, "kind": "suboutput", "name": "Image", "port": 0, "pos": [640, 0], "inputs": [3]}
  ]
 }
}
```

| Champ | Rôle |
|---|---|
| `name` | nom dans la bibliothèque et du nœud inséré |
| `kind` | `subgraph` ou `tool` |
| `applyMode` | `true` : mode Apply (bouton « Appliquer » qui garde les sorties en pixels), `false` : Live |
| `description` | aide affichée |
| `graph.output` | id du `suboutput` montré quand on édite la fonction |
| `graph.variables` | variables (3.7) |
| `graph.nodes` | les nœuds (3.3) |

### 3.3 Nœuds

Champs communs :

| Champ | Rôle |
|---|---|
| `id` | entier unique dans le graphe (1, 2, 3… suffit) |
| `kind` | type de nœud (tableau ci-dessous) |
| `name` | nom affiché (optionnel pour `effect`, `math`, `action`, `varget`, `varset` : déduit) |
| `pos` | `[x, y]` position dans le graphe (≈ 320 px entre deux colonnes) |
| `inputs` | ids des nœuds reliés à chaque broche d'entrée, **dans l'ordre des broches** ; `0` = non relié |
| `inputChannels` | (optionnel) pour chaque entrée : quelle sortie du nœud source (3.4) |
| `pinValues` | (optionnel) valeur des entrées valeur **non reliées**, une par broche (les broches image / exec : `{"type": "Image"}`) |

| `kind` | Rôle | Entrées | Sorties |
|---|---|---|---|
| `subinput` | entrée de la fonction | — | 1 (type = `value.type`) |
| `suboutput` | sortie de la fonction | 1 | — |
| `effect` | un effet / générateur / fusion de la bibliothèque | images de l'effet, puis les paramètres en broches | `RGBA` |
| `blend` | fusion standard (`"blend": n°`, `"opacity"`) | Fond, Calque, Masque | `RGBA` |
| `split` | séparer R V B A | `RGBA` | R, V, B, A (canaux) |
| `value` | constante | — | `Valeur` |
| `math` | opération (`"op"`, 3.5) | selon l'opération | selon l'opération |
| `varget` | lire une variable (`"op"` = nom) | — | la valeur |
| `varset` | définir une variable (`"op"` = nom), exécution | exec, Valeur | exec, Valeur |
| `event` | bouton d'exécution (`name` = texte du bouton) | — | exec |
| `action` | étape d'exécution (`"op"`, 3.6) | exec, puis selon l'action | exec…, selon l'action |
| `tool` | outil intégré live Bake / Upscale / ComfyUI (`"tool"`, `"props"`) | Entrée | `RGBA` |
| `function` | une fonction dans la fonction (`"sub"`: graphe, `"funcKind"`) | ses entrées | ses sorties |
| `surface` | image peinte (vide dans un `.sxfn`) | — | `RGBA` |

**Nœud `effect`** :

```json
{"id": 7, "kind": "effect", "effect": {"effect": "hue_saturation", "set": {"saturation": 20}},
 "paramPinNames": ["hue"], "inputs": [1, 6]}
```

- `effect.effect` : identifiant de l'effet (nom du fichier `.slang`) ;
- `effect.set` : valeurs de paramètres par id (`{"radius": 4}`, couleur : `{"tint": [1, 0.5, 0.2, 1]}`), les autres gardent leur défaut ;
- `paramPinNames` : paramètres transformés en broches, **ajoutés après les entrées image** dans `inputs` (ici entrée 0 = image, entrée 1 = `hue`).

### 3.4 Liens et sorties multiples

`inputs[i]` donne le nœud source de la broche `i`. Si le nœud source a **plusieurs sorties**, `inputChannels[i]` dit laquelle :

| Source | `inputChannels[i]` |
|---|---|
| sortie 0 (la plupart des nœuds) | `0` (ou absent) |
| sortie k d'un nœud à plusieurs sorties | `k × 8` (sortie 1 → `8`, sortie 2 → `16`, sortie 3 → `24`) |
| nœud `split` : R, V, B, A | `1`, `2`, `3`, `4` |

Exemple : `"inputs": [4, 4], "inputChannels": [0, 8]` = broche 0 reliée à la sortie 0 du nœud 4 (son exec « Corps »), broche 1 à sa sortie 1 (« Élément »).

Les types doivent correspondre : image avec image, exec avec exec, valeur avec valeur (les nombres se convertissent entre eux, le texte se convertit en nombre et inversement ; une valeur simple peut alimenter un tableau). Une entrée exec peut recevoir **plusieurs flux** (les deux branches d'un `branch` qui continuent vers la même action, par exemple) : la première source est dans `inputs`, les suivantes dans `"execLinks": [[broche, source, inputChannel], ...]` (ex. `"execLinks": [[0, 7, 0]]` = la sortie exec 0 du nœud 7 rejoint aussi l'entrée 0). Une sortie exec reliée à plusieurs actions les lance dans l'ordre du graphe ; pour un ordre garanti, utilisez `sequence`.

### 3.5 Valeurs

Types de broches : `Image`, `Canal`, `Float`, `Int`, `Bool`, `Vec2`, `Couleur`, `Texte`, `Nom`, `Exec` (alias acceptés : `Color`, `String`, `Text`, `Name`, `FName`).

```json
{"type": "Float", "f": 0.5}
{"type": "Int", "f": 3}
{"type": "Bool", "f": 1}
{"type": "Vec2", "f": [0.5, 0.25]}
{"type": "Couleur", "f": [1, 0.5, 0.2, 1]}
{"type": "Texte", "s": "bonjour"}
{"type": "Nom", "s": "4x-UltraSharp.pth"}
{"type": "Float", "array": true, "items": [[0,0,0,0], [60,0,0,0], [120,0,0,0]]}
{"type": "Texte", "array": true, "items": ["a", "b", "c"]}
```

Les éléments d'un tableau de nombres sont des `[x, y, z, w]` (un Float utilise `x`, une couleur les quatre).

Un `subinput` de type valeur devient une entrée du nœud de la fonction : si rien n'y est relié, on règle sa valeur sur le nœud (Propriétés, et elle s'affiche sur la broche). `value` du `subinput` = valeur par défaut.

**Opérations (`"kind": "math"`, `"op"`)** — entrées dans l'ordre, puis sorties :

| `op` | Entrées | Sortie(s) |
|---|---|---|
| `add` `sub` `mul` `div` `min` `max` `pow` `mod` | A, B (Float) | Résultat (Float) |
| `abs` `neg` `floor` `ceil` `round` `sqrt` `sin` `cos` (degrés) | X | Résultat |
| `clamp` | X, Min, Max | Résultat |
| `lerp` | A, B, T | Résultat |
| `remap` | X, De min, De max, Vers min, Vers max | Résultat |
| `random` | Graine (Int), Min, Max | Résultat |
| `greater` `less` `equal` | A, B | Résultat (Bool) |
| `and` `or` | A, B (Bool) | Bool |
| `not` | A (Bool) | Bool |
| `select` | Condition (Bool), Si vrai, Si faux | Résultat |
| `vec2` | X, Y | Vec2 |
| `break_vec2` | Vec2 | X, Y |
| `color` | R, V, B, A | Couleur |
| `break_color` | Couleur | R, V, B, A |
| `hsv` | Teinte (0-1), Saturation, Valeur | Couleur |
| `mix_color` | A, B (Couleur), T | Couleur |
| `concat` | A, B (Texte) | Texte |
| `to_string` | X (Float) | Texte |
| `to_name` | Texte | Nom |
| `text_length` | Texte | Longueur (Int) |
| `to_int` / `to_float` | X | Int / Float |
| `array_length` | Tableau (Float[]) | Taille (Int) |
| `array_get` | Tableau (Float[]), Index | Élément (Float) |
| `array_get_color` | Tableau (Couleur[]), Index | Couleur |
| `array_get_text` | Tableau (Texte[]), Index | Texte |
| `range` | Début, Nombre (Int), Pas | Tableau (Float[]) |
| `array_sum` | Tableau (Float[]) | Somme |

### 3.6 Exécution (outils)

Un `event` est un bouton. Sa sortie exec (port 0) va dans l'entrée 0 d'une `action` ou d'un `varset`. Les actions s'enchaînent par leur sortie exec.

| `op` | Entrées (après l'exec) | Sorties |
|---|---|---|
| `sequence` | — | Puis 1, Puis 2, Puis 3 (exec, exécutées dans l'ordre) |
| `branch` | Condition (Bool) | Vrai, Faux (exec) |
| `for_range` | Début (Int), Fin (Int, incluse) | Corps (exec), Index (Int), Terminé (exec) |
| `foreach` | Tableau (Float[]) | Corps (exec), Élément (Float), Index (Int), Terminé (exec) |
| `foreach_text` | Tableau (Texte[]) | Corps, Élément (Texte), Index, Terminé |
| `bake` | Image | exec, Résultat (Image gardée) |
| `upscale` | Image, Facteur (Int), IA (Bool), Modèle (Nom) | exec, Résultat |
| `comfyui` | Image, Prompt (Texte), Denoise (Float), Workflow (Texte : chemin d'un workflow API, vide = image vers image intégré) | exec, Résultat |
| `export_png` | Image, Fichier (Texte : relatif = dossier du document, sinon `exports/` à côté de l'exe) | exec |
| `print` | Message (Texte) | exec (affiche le message) |
| `call` | Fonction (Nom d'un nœud de fonction du graphe du document), Événement (Nom ; vide = le premier) | exec (après la fin de l'événement appelé) |

Le **Résultat** d'une action image (`bake`, `upscale`, `comfyui`) montre l'image d'entrée en direct tant que l'action n'a pas tourné, puis l'image gardée (badge RÉSULTAT) : reliez-le à un `suboutput` pour que la fonction sorte le résultat. « Effacer les résultats » sur le nœud revient au direct.

Pendant une boucle, les sorties Élément / Index changent à chaque tour, et les images lues par les actions sont recalculées avec les valeurs du moment (variables comprises).

### 3.7 Variables

```json
"variables": [
 {"name": "Teinte", "value": {"type": "Float", "f": 0}, "exposed": true, "desc": "Décalage de teinte (degrés)"}
]
```

- `varget` (`"op": "Teinte"`, `"value": {"type": "Float"}` = son type) lit la variable dans les calculs ;
- `varset` la change pendant une exécution (entrée 1 = nouvelle valeur) ;
- `exposed: true` : réglable sur chaque nœud de la fonction (Propriétés > Variables). Chaque nœud garde ses propres valeurs.

### 3.8 Exemples livrés (`library/functions/`)

| Fichier | Montre |
|---|---|
| `Netteté réglable.sxfn` | entrée Float reliée à un paramètre d'effet (`paramPinNames`) |
| `Ombre colorée.sxfn` | plusieurs entrées de types différents (Image, Couleur, Float) |
| `Fondu AB.sxfn` | plusieurs images + masque + facteur |
| `Bake et export PNG.sxfn` | outil : event → bake → export, sortie = résultat gardé (`inputChannels` 8) |
| `Upscale IA x4.sxfn` | action avec `pinValues` (Facteur 4, IA vrai, modèle) |
| `Variantes de teinte.sxfn` | boucle `foreach` sur un tableau, `varset`/`varget`, texte construit, export de 6 fichiers |

### 3.9 Pièges fréquents

- `inputs` suit l'**ordre des broches** : pour un `effect`, d'abord ses entrées image (`//@input` ou `Image`), puis les `paramPinNames` dans l'ordre donné ; pour une `action`, l'entrée 0 est l'exec.
- Les ports des `subinput` / `suboutput` (`port`) donnent l'ordre des broches du nœud : 0, 1, 2…
- Une sortie autre que la première : `inputChannels` = port × 8.
- Le type d'un `varget` / `varset` doit être celui de la variable.
- Un `.sxfn` ne contient pas de pixels : une `surface` y arrive vide.
- Pour vérifier : insérez la fonction (menu des nœuds > Bibliothèque), regardez ses broches, double-cliquez pour voir son graphe. Un lien impossible (types incompatibles) n'est pas créé.

---

## 4. Modèles de documents

Un **modèle** est un document de départ (Nouveau document > Modèles) : calques, graphe, trims, repères, tailles.

| Fichier | Rôle |
|---|---|
| `Nom.sxl` (ou `.sx3d` pour Paint 3D) | le document lui-même |
| `Nom.json` | `{"name": ..., "category": ..., "description": ..., "width": ..., "height": ...}` |
| `Nom.png` | vignette (320 px max), optionnelle |

| Dossier | |
|---|---|
| `library/templates/` (dépôt), `templates/` à côté de l'exe (package) | modèles livrés |
| `%LOCALAPPDATA%\Shaderixel\Templates\` | modèles de l'utilisateur (supprimables depuis le dialogue) |

Le plus simple pour en créer : construire le document dans Shaderixel puis **Fichier > Enregistrer comme modèle...** (le `.json` et la vignette sont écrits), puis copier les trois fichiers dans `library/templates/` pour les livrer.
Les modèles livrés de base sont générés par l'application (`--demo maketemplates`, dossier de sortie dans la variable `SX_TPLDIR`, code dans `src/ui/TemplatesBuiltin.cpp`).

---

## 5. Matériaux intelligents (Paint 3D, `.json`)

Un matériau intelligent est une pile de matériaux de remplissage (du bas vers le haut) ; chacun a des valeurs PBR et un **masque intelligent** calculé à partir de la forme du modèle (cartes cuites : occlusion, convexité, concavité, épaisseur) et d'un bruit 3D sans raccord. Menu : Calques 3D > « + Intelligent ».

```json
{
 "name": "Laiton terni",
 "description": "Laiton, creux noircis, arêtes polies brillantes.",
 "layers": [
  {"name": "Laiton", "color": [0.78, 0.6, 0.28], "rough": 0.35, "metal": 1,
   "mask": {"type": 7, "amount": 1, "contrast": 4, "noise": 0, "scale": 6}},
  {"name": "Ternissure", "color": [0.25, 0.2, 0.1], "rough": 0.7, "metal": 0.7,
   "mask": {"type": 1, "amount": 0.5, "contrast": 3, "noise": 0.4, "scale": 7}},
  {"name": "Arêtes polies", "color": [0.9, 0.75, 0.4], "rough": 0.15, "metal": 1,
   "mask": {"type": 0, "amount": 0.2, "contrast": 5, "noise": 0.3, "scale": 10}}
 ]
}
```

| Clé de couche | |
|---|---|
| `color` | couleur de base sRGB 0..1 (absente : la couche ne touche pas la couleur) |
| `rough`, `metal` | 0..1 (absente : canal non touché) |
| `height` | 0..1, 0.5 = neutre (optionnelle) |
| `mask.type` | 0 usure des arêtes, 1 saleté des creux, 2 poussière (dessus), 3 dégradé, 4 occlusion, 5 bruit, 6 parties fines, 7 plein |
| `mask.amount` | part de la surface couverte (0..1) |
| `mask.contrast` | netteté du bord (0.5 doux .. 20 net) |
| `mask.noise`, `mask.scale` | bruit 3D qui casse le bord (0..1) et sa fréquence (le modèle fait environ 2 unités) |
| `mask.seed` | variante du bruit |
| `mask.invert` | `true` : inverse (ex. une peinture partout **sauf** sur les arêtes) |
| `mask.dir` | types 2 et 3 : 0 haut, 1 bas, 2 +X, 3 -X, 4 +Z, 5 -Z |

Conseils : une couche de base `type 7` (plein, `noise 0`) ; « peinture usée » = couche peinture avec `type 0` + `invert` au-dessus d'un métal ; la saleté et la rouille en `type 1`, la poussière en `type 2`, la mousse en `type 3` (`dir` 1, bas) plus `type 1`.

Les pinceaux de la catégorie `Texture 3D` (`shaders/brushes/sx3d_*.slang`) suivent le format des pinceaux (`Tip(Brush b)`, voir `shaders/brush_common.slang`) : leur forme est projetée sur le modèle en 3D (8 variantes par la graine `b.seed`).

---

## 6. Idées pour des bibliothèques de base

- **Réglages photo** : looks (teinte + courbes + vignette) avec Force / Couleur en entrées.
- **Styles de calque** : contour + ombre + lueur combinés, couleurs et tailles en entrées.
- **Matières** (calques Draw) : bois, métal brossé, béton, tissu (générateurs paramétrés).
- **Masques** : par luminance, par couleur, par hauteur, avec seuil / douceur en entrées.
- **Outils d'export** : variantes (tableau de valeurs + boucle + export), planches de contact, tailles multiples.
- **Outils IA** : upscale, image vers image avec prompt en entrée, variantes de prompts (`foreach_text`).
