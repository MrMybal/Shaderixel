"""Reviewed French labels for the public library reference.

Identifiers and actual preset filenames stay stable. Composite control labels
use an em dash between the translated scope and setting, rather than guessing
French agreement from English word order. Missing public labels fail the build.
"""
import json
from pathlib import Path
import re

WORK = Path(__file__).resolve().parent
ENGLISH = json.loads((WORK.parent/'library/lang/en.json').read_text(encoding='utf-8'))
INVERSE = {value:key for key,value in ENGLISH.items()}
LABELS = {}
for line in (WORK/'locales/fr_labels.txt').read_text(encoding='utf-8').splitlines():
    if not line.strip() or line.startswith('#'): continue
    original, translated = line.split(' = ', 1)
    if original in LABELS: raise ValueError('Duplicate French label: '+original)
    LABELS[original] = translated
FOLDED = {key.casefold():value for key,value in LABELS.items()}
APP_FOLDED = {key.casefold():value for key,value in INVERSE.items()}

PREFIXES = ('Creative Blend','Creative Draw','Creative Filter','Creative',
 'Graph Effects','Graph Layers','Graph','Game Art','Photo Essentials','UI Toolkit',
 'UI UV','UI','PBR','ComfyUI','Textures','Masks','Print','Scan','Logo','Advanced',
 'Layer 1','Layer 2','Layer 3','Source Ring','A','B','Base','Boundary','Combine',
 'Composite','Disabled','Displace','Distance','Dots','Export','Field','Fill','Fit',
 'Flow','Focus','Grid','Hover','Levels','Map','Modify','Noise','Optional','Output',
 'Panel','Parallax','Pattern','Playback','Posterize','Pressed','Quad','Refine',
 'Relief','Ring','Sample','Scroll','Shape','Slice','Snap','State','Stroke','Style',
 'Track','Transform','Transition','Wave','Widget','Atlas')
SUFFIXES = ('Variant','Opacity','Color','Strength','Amount','Scale','Radius','Width',
 'Softness','Sharpness','Threshold','Offset','Intensity','Falloff','Power','Angle',
 'Seed','Frequency','Period','Phase','Size','Count','Density','Spacing','Curve',
 'Channel','Channels','Mask','Mix','Preview','Image','Layer','Map','Contribution',
 'Position','Weight','Depth','Gain','Balance','Tint','Height','Minimum','Maximum',
 'Jitter','Mode','Range','Step','Variation','Direction','Speed','Repeat','Detail',
 'Value','UV','A','B','C','D','1','2','3','4','X','Y','X / Y')

ENGLISH_EXTRA = {
 'Bleu en sortie':'Output Blue','Calcul en sRGB':'Calculate in sRGB','Cases':'Cells',
 'Combler les zones unies':'Fill Solid Areas','Flou maximal':'Maximum Blur',
 'Fondu horizontal':'Horizontal Fade','Fondu vertical':'Vertical Fade',
 'Inverser la profondeur':'Invert Depth','Lamelles':'Blades',
 "Longueur d'onde":'Wavelength',"Rayon d'analyse":'Analysis Radius',
 'Rouge en sortie':'Output Red','Vert en sortie':'Output Green','Zone nette':'Sharp Zone',
 'Lumière et lueur':'Light and Glow','Fondu':'Crossfade','Variantes':'Variants',
 'Teintes':'Hues',
}


def english_label(text):
    return ENGLISH_EXTRA.get(text,ENGLISH.get(text,text))


def french_label(text, *, strict=True):
    if not text: return text
    if text in LABELS: return LABELS[text]
    if text in INVERSE and INVERSE[text] != text: return INVERSE[text]
    key=text.casefold()
    if key in FOLDED: return FOLDED[key]
    if key in APP_FOLDED: return APP_FOLDED[key]
    # Existing French labels (keys in the application's English dictionary).
    if text in ENGLISH: return text
    # Literal symbols, channel selectors, units and technical format names.
    if re.fullmatch(r'(?:[A-Z0-9_]+|[a-z]\d*|[A-Z] - [A-Z]|(?:min|max|lerp|chroma|size|type)|n\d+)',text): return text
    unit=re.fullmatch(r'(.+?) (\([^()]+\))',text)
    if unit:
        units={'degrees':'degrés','pixels':'pixels','short image side':'petit côté de l’image',
               'source pixels':'pixels source','document pixels':'pixels du document',
               'logical pixels':'pixels logiques','half size':'demi-taille',
               '0 = remaining':'0 = restant','0 or 1':'0 ou 1',
               '0 ORM, 1 RMA, 2 MRA':'0 ORM, 1 RMA, 2 MRA',
               '0 R, 1 G, 2 B, 3 A, 4 Luminance':'0 R, 1 G, 2 B, 3 A, 4 luminance',
               'Flip Green':'inverser le vert','Fraction':'fraction','Linear':'linéaire',
               'RGB':'RVB','linear RGBA':'RVBA linéaire','Stops':'IL',
               'cycles per second':'cycles par seconde','document UV':'UV du document',
               'local UV':'UV locales','normalized':'normalisé',
               'px per second':'px par seconde','px per side':'px par côté',
               'px; 0 = canvas':'px ; 0 = canevas','px; 0 = filled':'px ; 0 = rempli',
               'px; 0 = region texels':'px ; 0 = texels de la région',
               'seconds':'secondes','source px':'px source',
               'target pixels per source pixel':'pixels cibles par pixel source',
               'tiles':'tuiles','zero based':'à partir de zéro'}
        inner=unit[2][1:-1]
        return french_label(unit[1],strict=strict)+' ('+units.get(inner,inner)+')'
    for prefix in PREFIXES:
        if key.startswith(prefix.casefold()+' '):
            try: return french_label(prefix)+' — '+french_label(text[len(prefix)+1:])
            except ValueError: pass
    for suffix in SUFFIXES:
        if key.endswith(' '+suffix.casefold()):
            try: return french_label(text[:-len(suffix)-1])+' — '+french_label(suffix)
            except ValueError: pass
    # Remaining metadata labels are noun groups. Longest reviewed phrase first
    # keeps e.g. "black point" together; the dash marks a setting's scope.
    words=text.split(' ')
    for split in range(len(words)-1,0,-1):
        left=' '.join(words[:split]);right=' '.join(words[split:])
        if left.casefold() not in FOLDED and left.casefold() not in APP_FOLDED and not re.fullmatch(r'[A-Z0-9]+',left): continue
        try: return french_label(left)+' — '+french_label(right)
        except ValueError: pass
    if strict: raise ValueError('Missing French library label: '+text)
    return text
