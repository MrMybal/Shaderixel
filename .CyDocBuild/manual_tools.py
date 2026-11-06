"""Bilingual toolbar reference, checked against the actual Tool enum and toolbar."""
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def catalog():
    header=(ROOT/'src/ui/App.h').read_text(encoding='utf-8-sig')
    enum=re.search(r'enum class Tool\s*\{(.*?)\}',header,re.S)
    if not enum: raise ValueError('Tool enum not found')
    names=[part.strip() for part in enum[1].split(',') if part.strip()]
    if names.pop()!='Count' or not all(re.fullmatch(r'[A-Za-z]+',n) for n in names):
        raise ValueError('Unrecognized Tool enum format')
    source=(ROOT/'src/ui/Canvas.cpp').read_text(encoding='utf-8-sig')
    entries=re.findall(r'\{Tool::(\w+),\s*ui::\w+,\s*"((?:\\.|[^"\\])*)",\s*"([^"]*)"\}',source)
    toolbar={name:(json.loads('"'+label+'"'),key) for name,label,key in entries}
    profiles=json.loads((ROOT/'.CyDocBuild/locales/tools.json').read_text(encoding='utf-8'))
    if set(names)!=set(toolbar) or len(toolbar)!=len(entries):
        raise ValueError('Tool enum and toolbar coverage differ')
    if set(names)!=set(profiles):
        raise ValueError('Tool documentation coverage differs: '+str(set(names)^set(profiles)))
    result=[]
    for name in names:
        profile=profiles[name]
        if not all(len(profile.get(lang,[]))==5 and all(profile[lang]) for lang in ('en','fr')):
            raise ValueError('Incomplete bilingual tool profile: '+name)
        slug=re.sub(r'(?<!^)(?=[A-Z])','_',name).lower()
        result.append(dict(profile,enum=name,id='tool:'+slug,label=toolbar[name][0],key=toolbar[name][1]))
    return result


def pages(language,english):
    french=language=='fr'
    t=lambda en,fr:fr if french else en
    targets={
        'pixels':t('An unlocked, paintable pixel target. Check the active layer, image/mask thumbnail and any selection before the stroke.',
                   'Une cible de pixels déverrouillée et peignable. Vérifiez calque actif, vignette image/masque et sélection avant le trait.'),
        'layer':t('An active supported layer or selected pixel region. Verify what is selected before dragging.',
                  'Un calque compatible actif ou une région de pixels sélectionnée. Vérifiez la sélection avant de glisser.'),
        'selection':t('An open document with relevant image content. A selection limits later operations; it does not paint the image.',
                      'Un document ouvert avec le contenu pertinent. La sélection limite les opérations suivantes ; elle ne peint pas l’image.'),
        'document':t('An open document. This operation changes document content or retained geometry rather than just the view.',
                     'Un document ouvert. Cette opération modifie contenu ou géométrie conservée, et pas seulement la vue.'),
        'view':t('An open document and the intended working view. Verify the displayed layer/channel when inspecting content.',
                'Un document ouvert et la vue de travail voulue. Vérifiez calque/canal affiché pour examiner le contenu.'),
        '3d':t('A Paint 3D model with a suitable UV/material target and the 3D view active. Check the active material channels.',
               'Un modèle Paint 3D avec cible UV/matériau adaptée et vue 3D active. Vérifiez les canaux du matériau.'),
        'fluid':t('A suitable 2D paint target or Paint 3D material. Select the destination before starting the simulation.',
                  'Une cible de peinture 2D ou un matériau Paint 3D adapté. Choisissez la destination avant de démarrer.'),
        'plugin':t('A loaded plugin that provides a tool, plus the target required by that plugin.',
                   'Un plugin chargé proposant un outil, avec la cible exigée par ce plugin.')
    }
    result=[]
    for item in catalog():
        title,purpose,gesture,controls,limits=item[language]
        label=item['label'] if french else english.get(item['label'],title)
        hint=item['key'] if item['key'] not in ('','-') else t('No dedicated key shown','Aucune touche dédiée affichée')
        if not french: hint=hint.replace('Espace','Space')
        body=[purpose,
              t('Toolbar label: ','Libellé dans la barre : ')+label,
              t('Toolbar key hint (group/context dependent): ','Indication clavier (selon groupe/contexte) : ')+hint,
              t('Select the exact tool from its toolbar group; right-click opens the group flyout. A shared key hint does not guarantee direct selection of that member.',
                'Choisissez l’outil exact dans son groupe ; le clic droit ouvre les variantes. Une indication clavier partagée ne garantit pas la sélection directe de ce membre.'),
              t('## Prerequisites','## Prérequis'),targets[item['target']],
              t('## Use','## Utilisation'),gesture,
              t('## Controls','## Réglages'),controls,
              t('Double-click or Ctrl+click numeric controls to type; Enter confirms and Escape cancels. Decimal fields accept a dot or comma.',
                'Double-cliquez ou Ctrl+cliquez les contrôles numériques pour saisir ; Entrée valide et Échap annule. Les décimales acceptent point ou virgule.'),
              t('## Result and limits','## Résultat et limites'),limits,
              t('[Tool index and editing commands](#tools)','[Index des outils et commandes](#tools)'),
              t('[Related workflow](#','[Procédure associée](#')+item['chapter']+')',
              t('[Keyboard shortcuts and context](#shortcuts)','[Raccourcis et contexte](#shortcuts)')]
        result.append(dict(id=item['id'],title=title,kind='Tools',body='\n\n'.join(body),sourceTool=item['enum']))
    return result
