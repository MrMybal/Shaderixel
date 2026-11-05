"""Complete built-in node reference, bound to a compiled registry snapshot."""
import json
from pathlib import Path
import re
from update_manual_catalog import source_hashes
from manual_node_notes import SHADER, MATH, EXEC, NODES

ROOT=Path(__file__).resolve().parents[1]
FAMILIES={'shader':SHADER,'math':MATH,'execution':EXEC,'nodes':NODES}
PREFIX={'shader':'shader','math':'math','execution':'exec','nodes':'node'}
EXTRA={
 'Réel':'Float','Décalage (tours)':'Offset (turns)','Si coché':'If checked','Sinon':'Otherwise',
 'A (dessous)':'A (background)','B (calque)':'B (layer)','Mélange RGB':'Blended RGB',
 'De min':'From min','De max':'From max','Vers min':'To min','Vers max':'To max',
 'A > B':'A > B','A <= B':'A <= B','Position locale':'Local position','Flux':'Flow',
 'Rotation stylet':'Pen rotation','Fonction (appel)':'Function (call)','Entrées':'Inputs',
 'Échantillonner l’entrée (UV)':'Sample input (UV)','Pour (de ... à)':'For range',
 'Valeur':'Value','Puis 1':'Then 1','Puis 2':'Then 2','Puis 3':'Then 3',
 'Vrai':'True','Faux':'False','Corps':'Body','Terminé':'Completed','Texte':'Text',
 'Graine':'Seed','Élément':'Element','Résultat':'Result','R':'R','V':'G',
 'Paramètre (réel)':'Parameter (float)','Paramètre (2D)':'Parameter (Vec2)',
 'Interrupteur (paramètre)':'Parameter switch','Normale depuis la hauteur (entrée)':'Normal from input height',
 'Couleur TSV (teinte 0-1)':'HSV color (hue 0–1)','Touche du pinceau':'Brush dab',
 'Concaténer':'Append / concatenate','Composer vecteur 2':'Make vector2',
 'Arc tangente (Y, X)':'Arctangent (Y, X)','Aléatoire (graine)':'Seeded random',
 'Image d’entrée':'Input image','Taille du canevas (float4)':'Canvas size (float4)',
 'Échantillonner décalé (pixels)':'Sample offset (pixels)','Flou de l’entrée':'Input blur',
 'Contours de l’entrée (Sobel)':'Input edges (Sobel)','Position du pixel':'Pixel position',
 'Constante 2D':'Constant 2D','Paramètre (couleur)':'Parameter (color)',
 'Séparer (R G B A)':'Split (R G B A)','Masque de composantes':'Component mask',
 'Linéaire vers sRGB':'Linear to sRGB','sRGB vers linéaire':'sRGB to linear',
 'Répéter les UV':'Tile UV','Coordonnées polaires':'Polar coordinates',
 'Texture du pinceau':'Brush texture','Aléatoire par touche':'Per-dab random',
 'HLSL personnalisé':'Custom HLSL','Entrée de fonction':'Function input',
 'Sortie de fonction':'Function output','Pour chaque (nombres)':'For each number',
 'Pour chaque (textes)':'For each text','Branche (si)':'Branch (if)',
 'Appeler une fonction':'Call function','Nombre en texte':'Number to text',
 'Texte en nom':'Text to name','Longueur du texte':'Text byte length',
 'En entier':'To integer','En float':'To float','Taille du tableau':'Array length',
 'Élément du tableau':'Array element','Couleur du tableau':'Array color element',
 'Texte du tableau':'Array text element','Suite de nombres':'Numeric range',
 'Somme du tableau':'Array sum','Si vrai':'If true','Si faux':'If false',
 'Image d\'entrée':'Input image','Échantillonner l\'entrée (UV)':'Sample input (UV)',
 'Flou de l\'entrée':'Input blur','Contours de l\'entrée (Sobel)':'Input edges (Sobel)',
 'Teinte':'Hue','Nom':'Name','Si (A > B)':'If (A > B)',
 'Bool':'Bool','Float':'Float','Int':'Int','Vec2':'Vec2','Color':'Color',
 'Canal':'Channel','Plus grand (A > B)':'Greater (A > B)','Plus petit (A < B)':'Less (A < B)',
 'Égal (A = B)':'Equal (A = B)','Séparer couleur':'Break color','Mélanger couleurs':'Mix colors',
 'Début':'Start','Fin':'End','Nombre':'Count','Pas':'Step','Tableau':'Array',
 'Distance (px)':'Distance (px)','Rayon (px)':'Radius (px)','Épaisseur (px)':'Thickness (px)'
}


def catalog():
    snapshot=json.loads((ROOT/'docs/manual/locales/node_catalog.json').read_text(encoding='utf-8'))
    if snapshot['sources']!=source_hashes(): raise ValueError('Compiled node catalog is stale; rebuild, export both backends and run tools/update_manual_catalog.py')
    catalog=snapshot['catalog']
    for family,notes in FAMILIES.items():
        ids=[n['id'] for n in catalog[family]]
        if len(set(ids))!=len(ids) or set(ids)!=set(notes): raise ValueError('Node reference coverage differs: '+family+' '+str(set(ids)^set(notes)))
        if any(len(note)!=(4 if family=='nodes' else 2) or not all(note) for note in notes.values()): raise ValueError('Incomplete node notes: '+family)
    # The snapshot is checked against source enums and registry IDs as well as hashes.
    header=(ROOT/'src/doc/Document.h').read_text(encoding='utf-8-sig')
    names=[n.strip() for n in re.search(r'enum class NodeKind[^\{]*\{([^}]+)',header)[1].split(',')]
    if names!=[n['id'] for n in catalog['nodes']]: raise ValueError('NodeKind order differs from snapshot')
    return catalog


def reference_id(family,identifier): return PREFIX[family]+':'+identifier.lower()


def pages(language,english):
    french=language=='fr';t=lambda en,fr:fr if french else en
    def tr(s): return s if french else EXTRA.get(s,english.get(s,s))
    catalog_data=catalog();result=[]
    sections={'shader':t('Shader operation','Opération de shader'),'math':t('Image-graph value operation','Opération de valeur du graphe image'),
              'execution':t('Image-graph execution action','Action d’exécution du graphe image'),'nodes':t('Image-graph node kind','Type de nœud du graphe image')}
    sentinel={-1:t('current UV','UV courant'),-2:t('layer opacity','opacité du calque'),
              -3:t('brush local position','position locale du pinceau'),-4:t('brush hardness','dureté du pinceau')}
    def value(v):
        if v.get('type')=='Image': return t('no image / execution value','sans valeur image / exécution')
        if v.get('array'): return json.dumps(v.get('items',[]),ensure_ascii=False)
        return json.dumps(v.get('s',v.get('f')),ensure_ascii=False)
    for family,entries in catalog_data.items():
        for item in entries:
            identifier=item['id'];note=FAMILIES[family][identifier]
            title=note[int(french)] if family=='nodes' else tr(item['label'])
            body=[sections[family]+'.',note[2+int(french)] if family=='nodes' else note[int(french)],
                  t('Stable operation: ','Opération stable : ')+identifier,
                  t('## Inputs and defaults','## Entrées et valeurs initiales')]
            if family=='shader':
                body += ['- '+str(i)+': '+tr(pin['name'] or t('Value','Valeur'))+' — '+sentinel.get(pin['default'],str(pin['default'])) for i,pin in enumerate(item['inputs'])]
            else:
                body += ['- '+str(i)+': '+tr(pin['name'] or 'Exec')+' ('+tr(pin['type'])+('[]' if pin['array'] else '')+')'+
                         (': '+value(item['defaults'][i]) if 'defaults' in item else '') for i,pin in enumerate(item['inputs'])]
            if not item['inputs']: body += [t('No fixed inputs. Dynamic nodes expose their configured interface.','Aucune entrée fixe. Les nœuds dynamiques exposent leur interface configurée.')]
            body += [t('## Outputs','## Sorties')]
            if not item['outputs']: body += [t('No fixed output pins. Structural/function interfaces depend on the configured node.','Aucune sortie fixe. Les interfaces de structure/fonction dépendent du nœud configuré.')]
            if family=='shader':
                body += ['- '+str(i)+': '+tr(name or t('Value','Valeur')) for i,name in enumerate(item['outputs'])]
                body += [t('Component signature: ','Signature des composantes : ')+({0:t('inferred per output / inputs','déduite par sortie / entrées'),5:t('texture handle','référence de texture')}.get(item['outputSize'],str(item['outputSize']))),
                         t('Output channel size can differ from the primary signature: RGBA=4, RGB=3, individual channels=1. Function/custom/brush nodes infer each output separately.',
                           'La taille d’un canal peut différer de la signature principale : RVBA=4, RVB=3, canal isolé=1. Fonction/HLSL/pinceau déduisent chaque sortie séparément.'),
                         t('## Availability and use','## Disponibilité et utilisation'),
                         ', '.join({0:t('Effect / Draw','Effet / Draw'),1:t('Blend','Fusion'),2:t('Brush','Pinceau')}[k] for k in item['kinds']),
                         t('Add this node in Effect Editor. Scalar inputs broadcast into vectors where supported. Shader trig/UV angles use radians; hue shifts use turns. Pin defaults shown as UV/opacity/brush values are contextual, not literal negative numbers.',
                           'Ajoutez ce nœud dans l’Éditeur d’effets. Les scalaires se répètent dans les vecteurs si compatible. Les angles de shader utilisent des radians ; la teinte utilise des tours. UV/opacité/valeurs du pinceau sont des valeurs contextuelles, pas des nombres négatifs littéraux.'),
                         t('[Shader authoring, types and troubleshooting](#shader_authoring)','[Création de shaders, types et dépannage](#shader_authoring)')]
            else:
                body += ['- '+str(i)+': '+tr(pin['name'] or 'Exec')+' ('+tr(pin['type'])+('[]' if pin['array'] else '')+')' for i,pin in enumerate(item['outputs'])]
                body += [t('## Use and limits','## Utilisation et limites'),
                         t('These nodes belong to the main image graph. Image/channel fields and single typed values are different. Connect an Exec flow to perform an action; a value link only supplies data.',
                           'Ces nœuds appartiennent au graphe principal. Champs image/canal et valeurs typées uniques sont différents. Reliez un flux Exec pour réaliser une action ; une valeur ne fournit que des données.'),
                         t('Defaults describe a new registered operation. Configured function, effect, variable and reroute interfaces can differ from this bare-node signature.',
                           'Les valeurs décrivent une opération nouvellement créée. Les interfaces de fonction, effet, variable ou reroutage configurées peuvent différer de cette signature de base.'),
                         t('[Image graphs and execution](#automation)','[Graphes image et exécution](#automation)')]
            body += [t('[Complete built-in node index](#builtin_nodes)','[Index complet des nœuds intégrés](#builtin_nodes)')]
            result.append(dict(id=reference_id(family,identifier),title=title,kind='Nodes',body='\n\n'.join(body),nodeFamily=family,operation=identifier))
    return result


def index(language,english):
    french=language=='fr';result=[]
    titles={'nodes':('Image graph structure','Structure du graphe image'),'math':('Image graph values','Valeurs du graphe image'),
            'execution':('Image graph execution','Exécution du graphe image'),'shader':('Per-pixel shader operations','Opérations de shader par pixel')}
    entries=pages(language,english)
    for family in ('nodes','math','execution','shader'):
        result+=['## '+titles[family][int(french)]]
        result += ['['+p['title']+'](#'+p['id']+')' for p in entries if p['nodeFamily']==family]
    return '\n\n'.join(result)
