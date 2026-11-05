"""Application-owned bilingual handbook and source-derived reference content.

Rendering, navigation and media presentation belong to CyDoc.
"""
import hashlib
import struct
import unicodedata
import json
from pathlib import Path
import re
import shlex
from manual_examples import files as practice_files
from manual_tools import pages as tool_pages, catalog as tool_catalog
from manual_commands import pages as command_pages
from manual_brushes import pages as brush_pages
from manual_nodes import pages as node_pages, index as node_index
from manual_panels import catalog as panel_catalog, index as panel_index

ROOT = Path(__file__).resolve().parents[1]
MANUAL = ROOT / 'docs/manual'
TRANSLATIONS = ROOT / 'library/lang/en.json'


def languages():
    return json.loads((MANUAL / 'locales/languages.json').read_text(encoding='utf-8'))


def fold(text):
    text = text.casefold().replace('œ', 'oe').replace('æ', 'ae')
    return ''.join(c for c in unicodedata.normalize('NFKD', text) if not unicodedata.combining(c))


def pages(language='en'):
    config = next((l for l in languages() if l['code'] == language), languages()[0])
    language = config['code']
    french = language == 'fr'
    english = json.loads(TRANSLATIONS.read_text(encoding='utf-8'))
    inverse = {v: k for k, v in english.items()}
    descriptions = json.loads((MANUAL / 'locales/fr_functions.json').read_text(encoding='utf-8'))
    notes = json.loads((MANUAL / 'locales/fr_notes.json').read_text(encoding='utf-8'))
    def say(en, fr):
        return fr if french else en
    def tr(text):
        # Actual library labels are preserved when no interface translation exists.
        return inverse.get(text, text) if french else english.get(text, text)
    def note(text):
        if not text:
            return ''
        if not french:
            return english.get(text, 'Applied hue offset (degrees)' if text=='Décalage de teinte appliqué (degrés)' else text)
        if text not in notes:
            raise ValueError('Missing French variable note: ' + text)
        return notes[text]
    result = []
    sources = sorted(MANUAL.glob('[0-9][0-9]_*.md'))
    translated = sorted((MANUAL / config['directory']).glob('[0-9][0-9]_*.md'))
    if {p.name for p in sources} != {p.name for p in translated}:
        raise ValueError(f'{language}: handbook chapter coverage differs from English')
    for path in translated:
        body = path.read_text(encoding='utf-8').strip()
        if not body.startswith('# '):
            raise ValueError(f'{path.name}: first line must be a title')
        title, _, content = body.partition('\n')
        if path.stem[3:]=='builtin_nodes': content+='\n\n'+node_index(language,english)
        if path.stem[3:]=='panels': content+='\n\n'+panel_index(language,english)
        result.append(dict(id=path.stem[3:], title=title[2:], kind='Handbook', body=content.strip()))

    effects = {}
    for path in sorted((ROOT / 'shaders/effects').glob('*.slang')):
        metadata = re.findall(r'^[ \t]*//@(\w+)[ \t]*(.*?)[ \t]*$', path.read_text(encoding='utf-8-sig'), re.M)
        fields = dict(metadata)
        name = tr(fields.get('name', path.stem))
        category = '/'.join(tr(v) for v in fields.get('category', 'Effects').split('/'))
        kind = say('Blend', 'Fusion') if 'blend' in fields else say('Draw / generator', 'Draw / générateur') if 'generator' in fields else say('Image effect', 'Effet image')
        body = [say(f'Kind: {kind}. Category: {category}.', f'Type : {kind}. Catégorie : {category}.'),
                say('Stable identifier: ', 'Identifiant stable : ') + path.stem,
                'Source: shaders/effects/' + path.name,
                say('These are shipped metadata defaults, not the current settings of a document.',
                    'Valeurs initiales des métadonnées livrées, et non réglages actuels du document. Les libellés techniques reprennent ceux de la bibliothèque.')]
        inputs = [tr(v) for k, v in metadata if k == 'input']
        if not inputs:
            inputs = [] if 'generator' in fields else [say('Background','Fond'), say('Layer','Calque'), say('Mask','Masque')] if 'blend' in fields else ['Image']
        body += [say('## Image inputs','## Entrées image')] + ([f'- {i}: {v}' for i,v in enumerate(inputs)] or [say('No image input.','Aucune entrée image.')])
        body += [say('## Parameters','## Paramètres')]
        count = 0
        for key, val in metadata:
            if key == 'group':
                body.append('### ' + tr(val))
            if key != 'param':
                continue
            args = shlex.split(val)
            if len(args) < 2:
                raise ValueError(f'Invalid parameter in {path.name}: {val}')
            ident, typ = args[:2]
            tail = args[2:]
            label = tr(tail[-1]) if tail and not re.fullmatch(r'[-+\d.eE]+',tail[-1]) else ident
            values = tail[:-1] if label != ident or (tail and '"' in val) else tail
            n = {'float':1,'int':1,'bool':1,'float2':2,'float3':3,'float4':4,'color':4}.get(typ)
            if typ == 'enum' and len(values) >= 2:
                spec = say('default index ','indice initial ') + values[0] + say('; options (zero-based): ',' ; choix (à partir de zéro) : ') + ' | '.join(tr(v) for v in values[1].split('|'))
            elif n and values:
                spec = say('default ','valeur initiale ') + ', '.join(values[:n])
                if len(values) >= n+2:
                    spec += say('; range ',' ; plage ') + values[n] + say(' to ',' à ') + values[n+1]
            elif values:
                spec = ' '.join(tr(v) for v in values)
            else:
                spec = say('edited with the dedicated control','édité avec le contrôle dédié')
            body.append(f'- {label} [{ident}] ({typ}): {spec}.')
            count += 1
        if not count:
            body.append(say('No exposed parameters.','Aucun paramètre exposé.'))
        body += [say('## Rendering metadata','## Métadonnées de rendu'),
                 say('Neighborhood (halo): ','Voisinage (halo) : ') + fields.get('halo',say('current pixel only','pixel courant seulement')) + '.',
                 say('Passes: ','Passes : ') + fields.get('passes','1') + '.',
                 say('Alpha convention: ','Convention alpha : ') + (say('premultiplied.','prémultipliée.') if 'premultiplied' in fields else say('standard effect convention.','convention standard des effets.')),
                 say('[Using the effect editor](#effect_editor)','[Utiliser l’éditeur d’effets](#effect_editor)'),
                 say('[Authoring presets](#authoring)','[Créer des presets](#authoring)')]
        effects[path.stem] = name
        result.append(dict(id='effect:'+path.stem,title=name,kind='Effects',body='\n\n'.join(body)))

    def value(v):
        if not v:
            return 'Image'
        typ = tr(v.get('type','Image'))
        data = v.get('items') if v.get('array') else v.get('s',v.get('f'))
        return typ + ('[]' if v.get('array') else '') + (': '+json.dumps(data,ensure_ascii=False) if data is not None else '')

    def nodes(graph):
        for node in graph.get('nodes',[]):
            yield node
            if isinstance(node.get('sub'),dict):
                yield from nodes(node['sub'])
    for path in sorted((ROOT/'library/functions').glob('*.sxfn')):
        data = json.loads(path.read_text(encoding='utf-8-sig'))
        graph = data['graph']
        desc = data.get('description','')
        if french:
            item = descriptions.get(path.stem)
            if not item or item['source'] != desc:
                raise ValueError('Missing/stale French function description: '+path.name)
            desc = item['text']
        else:
            desc = tr(desc)
        body = [desc, say('Kind: ','Type : ') + (say('Tool (explicit execution)','Outil (exécution explicite)') if data.get('kind')=='tool' else say('Live subgraph','Sous-graphe direct')) + '.',
                say('Initial evaluation: ','Évaluation initiale : ') + (say('Apply mode.','mode Appliquer.') if data.get('applyMode') else say('Live mode.','mode Direct.')),
                'Source: functions/'+path.name]
        for typ,title in (('subinput',say('Inputs','Entrées')),('suboutput',say('Outputs','Sorties'))):
            body += ['## '+title]
            ports = sorted((n for n in graph['nodes'] if n['kind']==typ),key=lambda n:n.get('port',0))
            body += [f'- {n.get("port",0)}: {tr(n.get("name",title))} ({value(n.get("value"))}).' for n in ports] or [say('None.','Aucune.')]
        body += [say('## Exposed variables','## Variables exposées')]
        variables = [v for v in graph.get('variables',[]) if v.get('exposed')]
        body += [f'- {tr(v["name"])} ({value(v.get("value"))}). {note(v.get("desc",""))}' for v in variables] or [say('None.','Aucune.')]
        events = [n for n in graph['nodes'] if n['kind']=='event']
        if events:
            body += [say('## Execution buttons','## Boutons d’exécution')] + ['- '+tr(n.get('name','Run')) for n in events]
            body += [say('Run an event to compute retained results or write files. The pre-execution image can be a live input preview.',
                         'Lancez un événement pour calculer les résultats conservés ou écrire les fichiers. Avant exécution, l’image peut être l’aperçu direct de l’entrée.')]
        refs = sorted({n['effect']['effect'] for n in nodes(graph) if n.get('kind')=='effect' and isinstance(n.get('effect'),dict)})
        if refs:
            body += [say('## Referenced effects','## Effets référencés')]
            for ref in refs:
                if ref not in effects:
                    raise ValueError(f'{path.name}: missing effect {ref}')
                body += [f'[{effects[ref]}](#effect:{ref})']
        body += [say('[Working with functions](#automation)','[Utiliser les fonctions](#automation)')]
        result.append(dict(id='function:'+path.stem,title=tr(data['name']),kind='Functions',body='\n\n'.join(body),sourceName=data['name']))
    result.extend(tool_pages(language,english))
    result.extend(command_pages(language))
    result.extend(brush_pages(language,english))
    result.extend(node_pages(language,english))
    ids = [p['id'] for p in result]
    if len(set(ids)) != len(ids):
        raise ValueError('Duplicate documentation identifiers')
    practice_ids=set(practice_files())
    for page in result:
        for target in re.findall(r'\]\(#([^\)]+)\)',page['body']):
            if target not in ids:
                raise ValueError(f'{page["id"]}: broken chapter link {target}')
        for target in re.findall(r'\]\(example:([^\)]+)\)',page['body']):
            if target not in practice_ids:
                raise ValueError(f'{page["id"]}: unknown practice example {target}')
    return result


def blocks(body):
    """Small intentionally limited Markdown subset, shared by both renderers."""
    paragraph = []
    code = None
    for line in body.splitlines() + ['']:
        if line.startswith('```'):
            if code is not None:
                yield 'code', '\n'.join(code), ''
                code = None
            else:
                if paragraph:
                    yield 'text', ' '.join(paragraph), ''
                    paragraph = []
                code = []
            continue
        if code is not None:
            code.append(line)
            continue
        link = re.fullmatch(r'\[([^\]]+)\]\(#([^\)]+)\)', line)
        example = re.fullmatch(r'\[([^\]]+)\]\(example:([a-z_]+)\)', line)
        picture = re.fullmatch(r'!\[([^\]]+)\]\((media/[^\)]+)\)', line)
        special = not line.strip() or line.startswith('#') or line.startswith('- ') or re.match(r'\d+\. ', line) or link or picture or example
        if special and paragraph:
            yield 'text', ' '.join(paragraph), ''
            paragraph = []
        if not line.strip():
            continue
        if picture:
            yield 'image', picture[1], picture[2]
        elif example:
            yield 'example', example[1], example[2]
        elif link:
            yield 'link', link[1], link[2]
        elif line.startswith('#'):
            yield 'heading', line.lstrip('# ').strip(), ''
        elif line.startswith('- '):
            yield 'bullet', line[2:], ''
        elif re.match(r'\d+\. ', line):
            yield 'step', line, ''
        else:
            paragraph.append(line)
    if code is not None:
        raise ValueError('Unclosed code fence')


def media_assets(editions):
    paths = sorted({target for items in editions.values() for page in items
                    for kind, _, target in blocks(page['body']) if kind == 'image'})
    result = []
    for i, relative in enumerate(paths):
        path = (MANUAL / relative).resolve()
        if not path.is_relative_to((MANUAL / 'media').resolve()) or path.suffix != '.png':
            raise ValueError('Unsafe manual image path: ' + relative)
        data = path.read_bytes()
        if not data.startswith(b'\x89PNG\r\n\x1a\n'):
            raise ValueError('Expected PNG: ' + relative)
        width, height = struct.unpack('>II', data[16:24])
        if not 0 < width <= 16384 or not 0 < height <= 16384:
            raise ValueError('Invalid manual image dimensions')
        result.append(dict(path=relative, resource=1000+i, width=width, height=height,
                           sha256=hashlib.sha256(data).hexdigest()))
    return result


def ui_strings():
    data = json.loads((MANUAL / 'locales/ui.json').read_text(encoding='utf-8'))
    for language in languages():
        if set(data[language['code']]) != set(data['en']):
            raise ValueError('UI translation coverage differs: '+language['code'])
    return data
