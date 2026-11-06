---
id: preset-format
title: Formats des fichiers de presets
description: Choisir les métadonnées de shader, broches typées et événements des presets réutilisables.
section: Développer / Effets
order: 100
parent: authoring
sourceRevision: 0d7117ea885e9734
---

## Choisir le format {#formats}

Un effet image, générateur Draw ou mode de fusion est un shader `.slang`. Une fonction du graphe principal est du JSON `.sxfn` avec `format: shaderixel-function`. Utilisez `kind: subgraph` pour le calcul direct et `kind: tool` pour les événements d’exécution. `applyMode: true` conserve les sorties après Appliquer ; false utilise le mode Direct.

Une fonction contenant des pixels conservés s’enregistre comme projet natif `.sxl` avec ses métadonnées, plutôt qu’en `.sxfn` sans pixels. Partez d’un fichier enregistré par l’éditeur pour conserver le schéma actuel.

## Métadonnées de shader {#metadata}

- `//@name` définit le libellé ; `//@category`, un chemin famille/sous-catégorie.
- `//@param id type ... "Libellé"` déclare une valeur exposée. `//@group` regroupe les contrôles suivants.
- `//@input` nomme les entrées image, jusqu’à quatre. Sans entrée explicite, un effet ordinaire reçoit Image. Les entrées de masque scalaire nommées ont des broches de canal.
- `//@generator` retire les entrées image ; `//@blend` fournit fond, calque et masque.
- `//@premultiplied` conserve les couleurs échantillonnées et renvoyées en alpha prémultiplié.
- `//@passes N` demande une à seize passes ; `ctx.pass` choisit la passe et `Prev` lit son résultat précédent.
- `//@halo 8`, `//@halo radius 3 2` et `//@halo full` déclarent un voisinage fixe, proportionnel à un paramètre, ou l’image entière. Déclarez le voisinage de chaque lecture de pixels voisins.
- `//@user` et `//@locked` sont les marqueurs de protection gérés par l’éditeur.

Le nom de fichier sans extension est l’identifiant stable du shader. Gardez minuscules, chiffres et soulignements. Les couleurs sont linéaires ; les effets ordinaires utilisent un alpha non prémultiplié sauf indication contraire. Les coordonnées du contexte couvrent le document entier, y compris lors du rendu par tuiles.

## Contrôles et fonctions de shader {#parameters}

Les scalaires sont `float`, `int` et `bool`. Les vecteurs sont `float2`, `float3`, `float4` ; `color` est du RVBA linéaire. `enum` contient un indice à partir de zéro et ses libellés. `curve` et `lut` utilisent leurs éditeurs dédiés. Le maximum est de 32 paramètres exposés.

```hlsl
//@param radius float 8 0 64 "Rayon"
//@param tint color 1 0.8 0.6 1 "Teinte"
//@param mode enum 0 "Doux|Dur" "Mode"
```

La signature d’entrée est `float4 Effect(EffectContext ctx)`. `ctx.px`, `ctx.uv`, `ctx.size`, `ctx.texel` et `ctx.pass` fournissent coordonnées, dimensions et état de passe. `Input(i, px)` lit un pixel ; `InputUV(i, uv)` échantillonne des UV normalisés ; `HasInput(i)` vérifie une connexion ; `Prev(px)` et `PrevUV(uv)` lisent la passe précédente. Conservez les valeurs hors de zéro à un lorsque l’opération en a besoin.

## Broches et connexions de fonction {#function-ports}

`graph.nodes` contient des identifiants entiers uniques, types, noms, positions et `inputs` ordonnés. Zéro signifie non relié. `subinput` et `suboutput` définissent l’interface publique par leurs indices `port` et valeurs initiales typées `value`. `graph.output` désigne la sortie affichée pendant l’édition.

Pour un nœud d’effet, `effect.effect` est son identifiant de shader et `effect.set` remplace ses valeurs initiales. `paramPinNames` ajoute les paramètres en broches après les images. Tous les effets référencés doivent exister.

`inputChannels[i]` choisit la sortie source de l’entrée i. La sortie k utilise `k * 8` ; R/V/B/A d’une séparation utilisent 1/2/3/4. Une liaison de données fournit une valeur ; une liaison Exec demande une exécution. Les nœuds `event` exposent des boutons ; les actions reliées peuvent calculer un résultat conservé, agrandir, appeler ComfyUI, exporter ou modifier des variables.

`graph.variables` contient les valeurs typées. `exposed: true` rend une variable réglable sur chaque instance ; `desc` explique le contrôle. Gardez les noms exposés stables lorsqu’ils sont utilisés par d’autres nœuds.

[Exemples de shader](doc:shader-authoring) · [Exécution du graphe](doc:automation)
