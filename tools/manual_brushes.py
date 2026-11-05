"""Shipped brush reference: authored usage, source metadata and bilingual labels."""
import json
from pathlib import Path
import re
import shlex

ROOT=Path(__file__).resolve().parents[1]
# Labels absent from the interface catalog, kept here to translate the reference
# without changing the shipped brush presets or their stable parameter names.
FRENCH={
    'Arm Length':'Longueur des bras','Arm Width':'Largeur des bras','Aspect':'Rapport des axes',
    'Bias':'Biais','Blade Width':'Largeur des brins','Bleed Amount':'Quantité de diffusion',
    'Bleed Falloff':'Atténuation de diffusion','Bleed Width':'Largeur de diffusion','Branch Length':'Longueur des branches',
    'Breakup':'Discontinuité','Bristle Count':'Nombre de poils','Bristle Width':'Largeur des poils',
    'Cell Scale':'Fréquence des cellules','Cloud Scale':'Fréquence des nuages','Core Falloff':'Atténuation du cœur',
    'Corner Radius':'Rayon des coins','Dot Size':'Taille des points','Duty':'Fraction occupée',
    'Edge Breakup':'Irrégularité du bord','Edge Pooling':'Accumulation au bord','Edge Position':'Position du bord',
    'Edge Sharpness':'Netteté du bord','Edge Width':'Largeur du bord','Fiber Width':'Largeur des fibres',
    'Frequency':'Fréquence','Gap Amount':'Quantité de lacunes','Gap Scale':'Fréquence des lacunes',
    'Grain Amount':'Quantité de grain','Grain Cells':'Cellules du grain','Grain Frequency':'Fréquence du grain',
    'Grain Scale':'Échelle du grain','Grain Threshold':'Seuil du grain','Grain Warp':'Déformation du grain',
    'Halo Falloff':'Atténuation du halo','Halo Intensity':'Intensité du halo','Halo Radius':'Rayon du halo',
    'Halo Width':'Largeur du halo','Inner Radius':'Rayon intérieur','Knot Amount':'Quantité de nœuds',
    'Knot Radius':'Rayon des nœuds','Lean':'Inclinaison des brins','Line Width':'Largeur des lignes',
    'Link Length':'Longueur des maillons','Link Width':'Largeur des maillons','Links':'Maillons',
    'Mask Gamma':'Gamma du masque','Max Height':'Hauteur maximale','Min Height':'Hauteur minimale',
    'Noise Scale':'Échelle du bruit','Noise Threshold':'Seuil du bruit','Point Sharpness':'Netteté des branches',
    'Pore Size':'Taille des pores','Porosity':'Porosité','Radial Falloff':'Atténuation radiale',
    'Ray Falloff':'Atténuation des rayons','Ray Intensity':'Intensité des rayons','Ray Sharpness':'Netteté des rayons',
    'Rays':'Rayons','Ridge Count':'Nombre de crêtes','Ridge Width':'Largeur des crêtes',
    'Separation':'Séparation','Serration':'Dentelure','Serration Count':'Nombre de dents',
    'Spread':'Étendue','Stitches':'Points de couture','Taper':'Effilement','Thread Width':'Largeur du fil',
    'Tine Width':'Largeur des dents','Tines':'Dents','Use Luminance':'Utiliser la luminance',
    'Use Texture Color':'Utiliser la couleur de texture','Vein Color':'Couleur de nervure',
    'Vein Strength':'Intensité de nervure','Vein Width':'Largeur de nervure',
    'Whorl Amount':'Amplitude du tourbillon','Whorl Count':'Nombre de tours','Wobble':'Ondulation',
    'Brush Texture':'Texture de pointe','Formes':'Formes','Matières':'Matières',
    'Particules':'Particules','Détails':'Détails','Texture 3D':'Texture 3D','Exemples':'Exemples'
}


def catalog():
    profiles=json.loads((ROOT/'docs/manual/locales/brushes.json').read_text(encoding='utf-8'))
    paths=sorted((ROOT/'shaders/brushes').glob('*.slang'))
    if {p.stem for p in paths}!=set(profiles):
        raise ValueError('Brush documentation coverage differs: '+str({p.stem for p in paths}^set(profiles)))
    result=[]
    for path in paths:
        text=path.read_text(encoding='utf-8-sig')
        metadata=re.findall(r'^//@(\w+)[ \t]*(.*?)[ \t]*$',text,re.M)
        fields=dict(metadata)
        profile=profiles[path.stem]
        if len(profile)!=6 or not all(isinstance(s,str) and s for s in profile):
            raise ValueError('Incomplete bilingual brush profile: '+path.stem)
        params=[];textures=[]
        for key,val in metadata:
            if key=='param':
                tokens=shlex.split(val);name,typ=tokens[:2]
                size={'float':1,'int':1,'bool':1,'float2':2,'float3':3,'float4':4,'color':4}.get(typ)
                if not size: raise ValueError('Unsupported brush parameter: '+val)
                numbers=tokens[2:2+size]
                rest=tokens[2+size:];label=rest.pop() if rest and not re.fullmatch(r'[-+\d.eE]+',rest[-1]) else name
                if len(rest) not in (0,2): raise ValueError('Unexpected brush range: '+val)
                params.append(dict(name=name,type=typ,default=[float(n) for n in numbers],values=numbers,range=rest,label=label))
            if key=='texture':
                name,label,*file=shlex.split(val)
                source=' '.join(file)
                if source and not (path.parent/source).is_file(): raise ValueError('Missing brush texture: '+source)
                textures.append(dict(name=name,label=label,path=source))
        if len(textures)>4: raise ValueError('Brush texture limit exceeded: '+path.stem)
        result.append(dict(id='brush:'+path.stem,brush=path.stem,profile=profile,name=fields.get('name',path.stem),
            category=fields.get('category',''),spacing=float(fields.get('spacing','-1')),airbrush=float(fields.get('airbrush','0')),
            extent=max(1,min(16,float(fields.get('extent','1')))),selfFlow='selfflow' in fields,
            dynamics='Dynamics(' in text,params=params,textures=textures))
    return result


def pages(language,english):
    french=language=='fr';t=lambda en,fr:fr if french else en
    inverse={v:k for k,v in english.items()}
    def tr(s): return FRENCH.get(s,inverse.get(s,s)) if french else english.get(s,s)
    result=[]
    for item in catalog():
        title=item['profile'][int(french)]
        body=[item['profile'][2+int(french)],t('Library label: ','Libellé de bibliothèque : ')+tr(item['name']),
              t('Category: ','Catégorie : ')+tr(item['category']),t('Stable identifier: ','Identifiant stable : ')+item['brush'],
              'Source: shaders/brushes/'+item['brush']+'.slang',
              t('## Recommended controls','## Réglages conseillés'),item['profile'][4+int(french)],
              t('## Shipped parameters','## Paramètres livrés'),
              t('These are initial shader values, not current brush settings. Parameter identifiers stay unchanged in both languages.',
                'Ce sont les valeurs initiales du shader, pas les réglages actuels. Les identifiants de paramètres restent identiques dans les deux langues.')]
        for param in item['params']:
            spec=t('default ','valeur initiale ')+', '.join(param['values'])
            if param['range']: spec+=t('; range ',' ; plage ')+t(' to ',' à ').join(param['range'])
            body+=['- '+tr(param['label'])+' ['+param['name']+'] ('+param['type']+'): '+spec+'.']
        if not item['params']: body+=[t('No extra shader parameters.','Aucun paramètre de shader supplémentaire.')]
        body += [t('## Stroke placement','## Placement des touches'),
                 t('Spacing on selection: ','Espacement à la sélection : ')+str(item['spacing'])+t(' × brush diameter.',' × diamètre du pinceau.') if item['spacing']>0 else t('Selection retains your spacing.','La sélection conserve votre espacement.'),
                 t('Timed airbrush: ','Aérographe temporisé : ')+str(item['airbrush'])+t(' dabs/second while held.',' touches/seconde à l’arrêt.') if item['airbrush']>0 else t('No timed airbrush declared.','Aucune temporisation d’aérographe déclarée.'),
                 t('Render extent: ','Étendue de rendu : ')+str(item['extent'])+t(' brush radii. This reserves processing bounds; it does not itself scatter the tip.',' rayons de pinceau. Cela réserve la zone de calcul sans disperser la pointe.'),
                 t('A Dynamics callback changes dab placement. See the recommended controls above.','Une fonction Dynamics change le placement des touches. Voir les réglages conseillés ci-dessus.') if item['dynamics'] else t('No custom Dynamics callback; placement uses the shared brush settings.','Aucune fonction Dynamics personnalisée ; le placement utilise les réglages communs.'),
                 t('## Flow and pressure','## Débit et pression'),
                 t('Self-flow: the tip applies flow and pressure itself; the renderer does not multiply dab opacity again.',
                   'Débit autonome : la pointe applique elle-même débit et pression ; le moteur ne multiplie pas à nouveau l’opacité de la touche.') if item['selfFlow'] else
                 t('The renderer multiplies tip coverage by dab opacity from the stroke settings; the shader may also use pressure internally.',
                   'Le moteur multiplie la couverture par l’opacité de la touche issue des réglages du trait ; le shader peut aussi utiliser la pression en interne.'),
                 t('Global opacity and the active selection still limit the result. Size/pressure controls can change dab radius before the shader runs. Pen tilt, rotation and speed only affect brushes that use those channels.',
                   'L’opacité globale et la sélection active limitent toujours le résultat. Taille/pression peuvent changer le rayon avant le shader. Inclinaison, rotation et vitesse du stylet n’agissent que si le pinceau les utilise.'),
                 t('## Texture inputs','## Entrées de texture')]
        if item['textures']:
            body += ['- '+tr(tx['label'])+' ['+tx['name']+']: '+(tx['path'] or t('no default image','sans image initiale')) for tx in item['textures']]
            body += [t('Relative files are resolved from shaders/brushes. You can replace the image in the panel. RGB images without alpha receive a luminance-derived alpha on import. BrushTex uses wrapped bilinear sampling and returns white if the slot is unbound.',
                       'Les fichiers relatifs partent de shaders/brushes. Remplacez l’image depuis le panneau. Une image RVB sans alpha reçoit un alpha dérivé de la luminance. BrushTex échantillonne en bilinéaire répété et renvoie du blanc si le slot est vide.')]
        else: body += [t('No declared image texture slots.','Aucun slot de texture image déclaré.')]
        body += [t('[Brush index and tablet workflow](#brushes_tablet)','[Index des pinceaux et utilisation de la tablette](#brushes_tablet)'),
                 t('[Painting and retouching](#painting)','[Peinture et retouche](#painting)'),
                 t('[Code and graph authoring](#authoring)','[Création en code et en graphe](#authoring)')]
        result.append(dict(id=item['id'],title=title,kind='Brushes',body='\n\n'.join(body)))
    return result
