# Shaderixel — Architecture

Éditeur d'images natif (C++20), entièrement calculé sur GPU. Il est le successeur
autonome de l'éditeur Document de CyTexture (plugin Unreal).

## Principes

1. **Le graphe nodal est la seule vérité.** Un document en mode *Calques* génère
   son graphe à partir de la pile (`Document::RebuildFromLayers`). Un document en
   mode *Nodal* édite le graphe directement. Calques → nœuds est toujours possible ;
   nœuds → calques ne l'est que si le graphe est encore une pile linéaire
   (`ExtractLayers`), sinon la conversion est refusée avec la raison.
2. **Tout est un compute shader.** Fusion, effets, pinceau, dégradé, affichage,
   miniatures, import et export passent tous par des dispatchs compute.
3. **Rien n'est recalculé sans raison.** Chaque nœud a une signature de contenu
   (paramètres + signatures des entrées + versions des surfaces). Une image
   immobile coûte 0 dispatch, et un trait de pinceau ne recalcule que l'aval.
4. **Jamais de blocage de l'UI.** Slang compile sur un thread dédié, avec un cache
   DXIL sur disque. Les lectures GPU→CPU (pipette, export) sont asynchrones.

## Arborescence

| Dossier | Rôle |
|---|---|
| `src/gpu/Device.*` | Couche D3D12 : swapchain à faible latence, tas de descripteurs, ring d'upload, root signature universelle, readbacks |
| `src/gpu/ShaderSystem.*` | Slang → DXIL asynchrone, `#include` maison, rechargement à chaud, cache disque |
| `src/doc/Document.*` | Calques, graphe, surfaces, conversions de mode |
| `src/doc/Effects.*` | Format d'effet (métadonnées `//@`), bibliothèque, génération du code |
| `src/doc/History.*` | Annuler/rétablir : instantanés d'état + régions de pixels sur GPU |
| `src/render/Renderer.*` | Évaluation du graphe avec cache, affichage, pinceau, dégradé, import/export |
| `src/ui/*` | Interface Dear ImGui (docking) : canevas, calques, graphe, propriétés... |
| `src/platform/*` | Fenêtre Win32, entrée tablette (pression + historique haute fréquence), dialogues |
| `shaders/` | Noyaux intégrés (`composite`, `brush`, `display`...) |
| `shaders/effects/` | Bibliothèque d'effets : un fichier = un effet, rechargés à chaud |

## Données

- Les couleurs internes sont **linéaires, en alpha prémultiplié**, en RGBA16F
  (« 16 bits ») ou RGBA32F (« 32 bits »).
- Les modes de fusion travaillent sur les couleurs non prémultipliées, puis
  appliquent la composition W3C. L'option « fusion en gamma » reproduit le rendu
  de Photoshop ; sinon, la fusion est physiquement linéaire.
- Une `Surface` est une texture GPU plus une version. Calques et nœuds la
  référencent par identifiant. Les instantanés d'annulation copient l'état, pas
  les pixels.

## Écrire un effet

```hlsl
//@name Mon effet
//@category Personnalisé
//@param amount float 0.5 0 1 "Quantité"
//@param tint color 1 0.5 0.2 1 "Teinte"
//@input Image            (facultatif : 1 entrée par défaut, //@generator = 0)
//@passes 2               (facultatif : multi-passes, Prev() lit la passe précédente)
//@premultiplied          (facultatif : flous, déformations)
float4 Effect(EffectContext ctx)
{
    float4 c = Input(0, ctx.px);
    return float4(lerp(c.rgb, c.rgb * tint.rgb, amount), c.a);
}
```

Ce même format sert à trois choses :

- les fichiers de `shaders/effects/` ;
- les nœuds « HLSL personnalisé » stockés dans le document ;
- plus tard, l'éditeur d'effets visuel (façon éditeur de matériaux d'Unreal), qui
  générera ce code.

Pour tester toute la bibliothèque : `Shaderixel.exe --test-effects`.

## Automatisation

```
Shaderixel.exe --test-effects               compile tous les effets, résultat dans shaderixel.log
Shaderixel.exe --demo [nodes] --capture out.png [--frames N]
Shaderixel.exe image.png                    ouvre une image
```
