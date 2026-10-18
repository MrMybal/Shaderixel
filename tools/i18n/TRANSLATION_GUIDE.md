# Shaderixel interface translation (French -> English)

Input: a JSON array of French interface strings (`chunk_NN_fr.json`).
Output: a JSON object `{ "<French string>": "<English string>" }` with exactly one entry per input string
(`chunk_NN_en.json`, UTF-8, `ensure_ascii=False`). Keys are copied byte for byte from the input.

## Rules

- Short, natural UI English, in the style of Photoshop / Substance Painter / Unreal Engine.
- Keep every placeholder exactly: `{n}`, `{nom}`, `{n3}`, `{l}`, `{h}`, `{date}`, `{dossier}`, `{ext}` (file name tokens: keep
  them as they are, they are typed by the user), `%s`, `%c`, `%%`, `\n` (real line breaks), leading / trailing spaces and
  punctuation such as `...`, ` : ` (French spacing before a colon becomes `: ` in English: "Taille : " -> "Size: ").
- Strings already in English, product names, file extensions, technical identifiers, code, URLs: copy them unchanged.
- Keys: Ctrl, Alt -> unchanged; Maj -> Shift; Suppr -> Del; Entrée -> Enter; Échap -> Esc; Retour arrière -> Backspace;
  Espace -> Space; Flèches -> Arrows; Clic -> Click; Clic droit -> Right-click; Molette -> Mouse wheel; Pavé numérique -> Numpad.
- Keep the casing pattern of the source (Title words stay capitalized only where the French is; sentence case otherwise).
- Do not translate names of the user's files or of brands: Shaderixel, Unreal, ComfyUI, lensfun, LibRaw, Photo RAW, Paint 3D,
  CyTexture, Marmoset, Substance, Blender, Godot, Unity, MCP, glTF, OBJ, FBX, PSD, HDR / HDRI, PBR, ORM, SDK.

## Glossary

calque -> layer; calque de pixels -> pixel layer; calque de réglage -> adjustment layer; calque de remplissage -> fill layer;
groupe / dossier -> group / folder; objet dynamique -> smart object; masque (de fusion) -> (layer) mask; masque d'écrêtage ->
clipping mask; mode de fusion / fusion -> blend mode / blend; opacité -> opacity; fond -> fill (layer fill) or background
(image background, by context); sélection -> selection; détourage -> cutout; recadrage -> crop; zone de travail -> canvas;
canevas -> canvas; document -> document; graphe -> graph; nœud -> node; broche -> pin; sortie -> output; entrée -> input;
pinceau -> brush; gomme -> eraser; pointe -> tip; dureté -> hardness; flux -> flow; espacement -> spacing; dispersion -> scatter;
pipette -> eyedropper; tampon de duplication -> clone stamp; correcteur -> healing brush; pièce -> patch; doigt -> smudge;
densité - / densité + -> dodge / burn; éponge -> sponge; fluidité -> liquify; dégradé -> gradient; pot de peinture -> paint bucket;
plume -> pen; tracé -> path; forme -> shape; texte -> text; baguette magique -> magic wand; lasso -> lasso;
sélection rapide -> quick selection; netteté -> sharpen / sharpness; flou -> blur; bruit -> noise; teinte -> hue (color)
or tint (white balance); saturation -> saturation; luminosité -> brightness / luminance (by context); contraste -> contrast;
exposition -> exposure; hautes lumières -> highlights; ombres -> shadows; blancs / noirs -> whites / blacks;
clarté -> clarity; correction du voile -> dehaze; vibrance -> vibrance; balance des blancs -> white balance; texture -> texture;
étalonnage des couleurs -> color grading; mélangeur de couleurs -> color mixer; courbe des tonalités -> tone curve;
retouches locales -> local adjustments; filtre gradué / radial -> graduated / radial filter; correction d'objectif -> lens correction;
réglage -> adjustment / setting (by context); paramètres -> parameters / settings; propriétés -> properties; préréglage -> preset;
historique -> history; instantané -> snapshot; action -> action; traitement par lots -> batch processing; lot -> batch;
bibliothèque -> library; ressources -> assets; modèle (3D) -> model; modèle (gabarit) -> template; maillage -> mesh;
matériau -> material; matériau intelligent -> smart material; masque intelligent -> smart mask; canal / canaux -> channel(s);
rugosité -> roughness; métal -> metallic; hauteur -> height; émissif -> emissive; occlusion -> occlusion; normale -> normal;
cuisson / cuire -> bake; dépliage UV -> UV unwrap; île UV -> UV island; rendu -> render / rendering; échantillons -> samples;
rebonds -> bounces; débruitage -> denoising; profondeur de champ -> depth of field; ouverture -> aperture; distance nette ->
focus distance; vue -> view; vue 3D -> 3D view; éclairage -> lighting; lumière -> light; ciel -> sky; sol -> ground;
caméra -> camera; projection -> projection; décalque -> decal; pochoir -> stencil; simulation -> simulation; particules -> particles;
coulures -> drips; réglages automatiques -> auto settings; plugin -> plugin; extension -> extension; IA -> AI;
générer / génération -> generate / generation; remplissage génératif -> generative fill; inpainting -> inpainting;
fichier -> file; dossier -> folder; ouvrir -> open; enregistrer -> save; enregistrer sous -> save as; exporter -> export;
importer -> import; fermer -> close; quitter -> quit / exit; annuler -> cancel (button) / undo (history action);
rétablir -> redo; appliquer -> apply; réinitialiser -> reset; supprimer -> delete; dupliquer -> duplicate; renommer -> rename;
aperçu -> preview; afficher -> show; masquer (verb) -> hide; activer -> enable; désactivé -> off / disabled;
aucun(e) -> none; tout -> all; filtrer -> filter; rechercher -> search; affichage -> view; fenêtre -> window; aide -> help;
édition -> edit; image -> image; calque (menu) -> layer; sélection (menu) -> select; filtre (menu) -> filter.
