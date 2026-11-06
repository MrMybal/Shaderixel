---
id: pbr
title: Cartes PBR depuis une photo
description: Une photo mélange couleur de matière et éclairage.
section: Créer / Matériaux
order: 11
sourceRevision: f3a2b1d625fa36cc
---

Une photo mélange couleur de matière et éclairage. PBR Photo to Maps estime albédo, hauteur, normales, occlusion et rugosité ; il ne les mesure pas. La métallicité se choisit explicitement.

```image
{"id": "pbr", "src": "assets/manual/pbr_fr.png", "alt": "Traitement de la photo et choix sRGB des cartes exportées.", "controls": "minimal"}
```

```image
{"id": "pbr-view", "src": "assets/manual/pbr_view_fr.png", "alt": "Inspection du canal de rugosité sur une sphère 3D.", "controls": "minimal"}
```

## Extraction {#extraction-workflow}

1. Choisissez une vue de face, uniformément éclairée et peu déformée par la perspective.
2. Nettoyez avec Textures Cleanup and Albedo et inspectez les raccords avant les normales.
3. Connectez la photo à PBR Photo to Maps.
4. Ajustez détails de hauteur, force des normales et rugosité de base. Les Variables exposent rayon d'éclairage, inversion et occlusion.
5. Choisissez la métallicité : 0 pour un diélectrique, 1 pour un métal.
6. Inspectez chaque carte et la matière sur un modèle.
7. Dans PBR Photo to Maps and PNG Export, définissez un préfixe puis Extract and Export.

Le préfixe material produit material_albedo.png, material_height.png, material_normal.png, material_occlusion.png, material_roughness.png, material_metal.png et material_orm.png. Réutiliser le préfixe remplace ces fichiers.

## Conventions {#conventions}

La chaîne texture_* compense l'export PNG sRGB pour préserver les valeurs numériques des octets. Dans le moteur, activez sRGB pour l'albédo et désactivez-le pour normales, hauteur, rugosité, occlusion, métallicité et cartes empaquetées.

Les normales sont OpenGL par défaut. DirectX Normals inverse le vert. Une normale plate exportée est proche de 128,128,255. Le mélange attend la même convention pour les deux entrées.

ORM signifie rouge=occlusion, vert=rugosité, bleu=métallicité. PBR Channel Packing offre aussi RMA/MRA. La hauteur en alpha est optionnelle et peut rendre l'aperçu transparent.

## Qualité {#quality}

L'outil écrit du PNG 8 bits. Un déplacement précis demande une haute précision et un encodage adaptés. La profondeur d'une scène n'est pas son microrelief.

Le raccord à pleine force fait coïncider les valeurs des bords opposés, mais motifs et dérivées des normales peuvent encore nécessiter des retouches. L'aperçu répété sert à les repérer.

[Couleur et données](doc:color)
[Paint 3D](doc:paint3d)
