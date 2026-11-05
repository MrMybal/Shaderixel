"""Check actual reference coverage, links, offline assets and both staging paths."""
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import tempfile
import unittest
import base64
import hashlib
import struct
from manual_examples import files as practice_files
from manual_tools import catalog as tool_catalog
from manual_commands import catalog as command_catalog
from manual_brushes import catalog as brush_catalog
from manual_nodes import catalog as node_catalog, reference_id
from manual_panels import catalog as panel_catalog

from build_manual import ROOT, MANUAL, copy_manual, pages, validate_generated, languages, blocks, ui_strings


class Document(HTMLParser):
    def __init__(self, source):
        super().__init__()
        self.ids, self.links, self.remote = [], [], []
        self.articles, self.navigation, self.language = 0, 0, None
        self.images = []
        self.feed(source)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if 'id' in attrs:
            self.ids.append(attrs['id'])
        if tag == 'html':
            self.language = attrs.get('lang')
        if tag == 'article':
            self.articles += 1
        if tag == 'a':
            self.links.append(attrs.get('href', ''))
        if tag == 'img' and attrs.get('src'):
            self.images.append(attrs)
        for key in ('src', 'href'):
            if key in attrs and not attrs[key].startswith(('#', 'data:image/png;base64,', 'data:application/octet-stream;base64,')):
                self.remote.append(attrs[key])


class ManualTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pages = pages()
        cls.by_id = {p['id']: p for p in cls.pages}
        cls.editions = {l['code']: pages(l['code']) for l in languages()}

    def test_generated_files_are_current(self):
        self.assertEqual(self.pages, validate_generated())
        coverage=json.loads((MANUAL/'coverage.json').read_text(encoding='utf-8'))
        self.assertEqual(coverage['pagesPerLanguage'],len(self.pages))
        self.assertEqual(coverage['panelsAndDialogs'],len(panel_catalog()))
        self.assertEqual(coverage['catalogs']['Nodes'],168)
        for page in self.pages:
            if page['kind']=='Handbook': self.assertNotRegex(page['body'],r'(?m)^## [^\n]*\n\s*\n## ')

    def test_every_effect_and_parameter(self):
        actual = list((ROOT / 'shaders/effects').glob('*.slang'))
        self.assertEqual(len(actual), sum(p['kind'] == 'Effects' for p in self.pages))
        for path in actual:
            with self.subTest(effect=path.stem):
                source = path.read_text(encoding='utf-8-sig')
                page = self.by_id['effect:' + path.stem]['body']
                declared = re.findall(r'^\s*//@param\s+(\w+)', source, re.M)
                for param in declared:
                    self.assertIn('[' + param + ']', page)
                documented = re.findall(r'\[([a-zA-Z_0-9]+)\] \(', page)
                self.assertEqual(declared, documented)
                if re.search(r'^\s*//@generator\s*$', source, re.M):
                    self.assertIn('Draw / generator', page)
                if re.search(r'^\s*//@premultiplied\s*$', source, re.M):
                    self.assertIn('Alpha convention: premultiplied.', page)

    def test_every_programmable_brush(self):
        brushes=brush_catalog()
        self.assertEqual(len(brushes),53)
        for lang,edition in self.editions.items():
            references={p['id']:p for p in edition if p['kind']=='Brushes'}
            self.assertEqual(set(references),{b['id'] for b in brushes})
            index=next(p for p in edition if p['id']=='brushes_tablet')
            for brush in brushes:
                self.assertIn('(#'+brush['id']+')',index['body'])
                text=references[brush['id']]['body']
                self.assertEqual(re.findall(r'\[([a-zA-Z_0-9]+)\] \(',text),[p['name'] for p in brush['params']])
                for texture in brush['textures']: self.assertIn(texture['path'],text)
        self.assertIn('40.0 dabs/second',self.by_id['brush:airbrush']['body'])
        self.assertIn('No timed airbrush declared.',self.by_id['brush:cy_spray']['body'])
        self.assertIn('Self-flow:',self.by_id['brush:cy_texture']['body'])

    def test_every_function_and_variable(self):
        actual = list((ROOT / 'library/functions').glob('*.sxfn'))
        self.assertEqual(len(actual), sum(p['kind'] == 'Functions' for p in self.pages))
        for path in actual:
            data = json.loads(path.read_text(encoding='utf-8-sig'))
            self.assertEqual(self.by_id['function:' + path.stem]['sourceName'], data['name'])
            body = self.by_id['function:' + path.stem]['body']
            variables = [v for v in data['graph'].get('variables', []) if v.get('exposed')]
            self.assertEqual(len(variables), body.split('## Exposed variables')[1].split('## ')[0].count('\n\n- '))

    def test_reference_conventions(self):
        body = self.by_id['effect:ui_uv_nine_slice']['body']
        self.assertIn('[source_origin]', body)
        self.assertIn('Stretch | Tile | Mirror tile | Transparent', body)
        self.assertIn('No image input.', body)
        self.assertIn('8-bit PNG', self.by_id['pbr']['body'])
        self.assertIn('32-bit float', self.by_id['ui_textures']['body'])

    def test_language_coverage_and_identity(self):
        self.assertEqual(set(self.editions), {'en', 'fr'})
        english_ids = [p['id'] for p in self.pages]
        for language, edition in self.editions.items():
            self.assertEqual([p['id'] for p in edition], english_ids)
            self.assertEqual(sum(p['kind']=='Handbook' for p in edition), 44)
            for page in edition:
                for kind, text, target in blocks(page['body']):
                    if kind == 'link': self.assertIn(target, english_ids)
                    if kind == 'image': self.assertTrue((MANUAL / target).is_file())
        fr = {p['id']: p for p in self.editions['fr']}
        self.assertEqual(fr['getting_started']['title'], 'Prise en main')
        self.assertIn('métallicité', fr['pbr']['body'])
        self.assertIn('## Variables exposées', fr['function:UI UV Nine Slice Panel']['body'])
        self.assertIn('Plage : 0 à 32768.', fr['function:UI UV Nine Slice Panel']['body'])
        messages = ui_strings()
        self.assertEqual(set(messages['en']), set(messages['fr']))
        self.assertEqual(messages['fr']['Print handbook'], 'Imprimer le guide')
        # Stable shader IDs and parameter coverage are identical in both editions.
        for page in self.pages:
            if page['kind'] == 'Effects':
                self.assertEqual(re.findall(r'\[([a-zA-Z_0-9]+)\] \(',page['body']),
                                 re.findall(r'\[([a-zA-Z_0-9]+)\] \(',fr[page['id']]['body']))

    def test_embedded_illustrations(self):
        assets = json.loads((MANUAL / 'media/manifest.json').read_text(encoding='utf-8'))
        self.assertEqual(len(assets),42)
        self.assertEqual(len({a['resource'] for a in assets}),42)
        rc = (ROOT/'assets/manual_images.rc').read_text(encoding='utf-8')
        self.assertIn('#include "manual_images.rc"',(ROOT/'assets/shaderixel.rc').read_text(encoding='utf-8'))
        for asset in assets:
            self.assertEqual(hashlib.sha256((MANUAL/asset['path']).read_bytes()).hexdigest(),asset['sha256'])
            self.assertIn(str(asset['resource'])+' RCDATA',rc)
            self.assertGreater(asset['width'],0)
            self.assertGreater(asset['height'],0)

    def test_html_is_self_contained(self):
        source = (MANUAL / 'index.html').read_text(encoding='utf-8')
        doc = Document(source)
        self.assertEqual(doc.language, 'en')
        self.assertEqual(doc.articles, sum(map(len,self.editions.values())))
        self.assertEqual(len(doc.ids), len(set(doc.ids)))
        self.assertFalse(doc.remote, 'No remote/file assets or links should be needed')
        for link in doc.links:
            if link.startswith('#'): self.assertIn(link[1:].replace('/','--',1), doc.ids)
            else:
                self.assertTrue(link.startswith('data:application/octet-stream;base64,'))
                self.assertIn(base64.b64decode(link.split(',',1)[1]),practice_files().values())
        assets=json.loads((MANUAL/'media/manifest.json').read_text(encoding='utf-8'))
        expected={a['sha256'] for a in assets}
        self.assertEqual(len(doc.images),sum(typ=='image' for edition in self.editions.values() for page in edition for typ,_,_ in blocks(page['body'])))
        for image in doc.images:
            self.assertTrue(image['alt'])
            self.assertIn(hashlib.sha256(base64.b64decode(image['src'].split(',',1)[1])).hexdigest(),expected)
        self.assertIn("connect-src 'none'", source)
        self.assertNotIn('@@NAVIGATION@@', source)
        self.assertNotIn('@@ARTICLES@@', source)

    def test_practice_documents(self):
        practice=practice_files()
        self.assertEqual(len(practice),6)
        for lang,edition in self.editions.items():
            targets=[target for page in edition for kind,_,target in blocks(page['body']) if kind=='example']
            self.assertEqual(set(targets),set(practice),lang)
        for name,data in practice.items():
            self.assertEqual(data[:4],b'SXL1')
            version,size=struct.unpack('<IQ',data[4:16]);self.assertEqual(version,2)
            # No stored pixel blobs, external image paths or execution results.
            self.assertEqual(len(data),16+size)
            document=json.loads(data[16:]);self.assertEqual(document['precision'],'float32')
            self.assertNotIn('path',document)
            def graph_check(graph):
                ids={n['id'] for n in graph['nodes']}
                self.assertEqual(len(ids),len(graph['nodes']))
                self.assertIn(graph['output'],ids)
                for node in graph['nodes']:
                    self.assertTrue(all(i==0 or i in ids for i in node.get('inputs',[])))
                    self.assertNotIn('surface',node);self.assertNotIn('outSurf',node)
                    if node['kind']=='effect': self.assertTrue((ROOT/'shaders/effects'/(node['effect']['effect']+'.slang')).is_file())
                    if 'sub' in node: graph_check(node['sub'])
            graph_check(document['graph'])
            self.assertEqual(data,(MANUAL/'examples'/(name+'.sxl')).read_bytes())

    def test_every_toolbar_tool(self):
        tools=tool_catalog()
        self.assertEqual(len(tools),46)
        self.assertEqual(len({tool['enum'] for tool in tools}),46)
        for lang,edition in self.editions.items():
            references={page['id']:page for page in edition if page['kind']=='Tools'}
            self.assertEqual(set(references),{tool['id'] for tool in tools})
            index=next(page for page in edition if page['id']=='tools')
            for tool in tools:
                page=references[tool['id']]
                self.assertEqual(page['title'],tool[lang][0])
                self.assertIn('](#'+tool['id']+')',index['body'])
                self.assertIn('](#'+tool['chapter']+')',page['body'])
                self.assertTrue(all(text in page['body'] for text in tool[lang][1:]))
                self.assertIn(tool['enum'],page['sourceTool'])

    def test_every_action_command(self):
        commands=command_catalog();self.assertEqual(len(commands),20)
        for lang,edition in self.editions.items():
            references={page['id']:page for page in edition if page['kind']=='Commands'}
            self.assertEqual(set(references),{item['id'] for item in commands})
            index=next(page for page in edition if page['id']=='actions')
            for item in commands:
                page=references[item['id']]
                self.assertEqual(page['sourceCommand'],item['command'])
                self.assertIn('](#'+item['id']+')',index['body'])
                defaults=[text for kind,text,_ in blocks(page['body']) if kind=='code']
                self.assertEqual(json.loads(defaults[0]),item['defaults'])
        self.assertEqual(next(item['defaults'] for item in commands if item['command']=='filter'),{'effect':'black_white','destructive':True})
        self.assertEqual(next(item['defaults'] for item in commands if item['command']=='photo_raw'),{'settings':{},'auto':False,'lens':True})

    def test_staging_uses_identical_offline_content(self):
        for builder in ('package.py', 'build_installer.py'):
            source = (ROOT / 'tools' / builder).read_text(encoding='utf-8')
            self.assertIn('from build_manual import copy_manual', source)
            self.assertRegex(source, r'copy_manual\((dist|stage)\)')
        with tempfile.TemporaryDirectory(prefix='shaderixel-manual-') as folder:
            package, installer = Path(folder) / 'portable', Path(folder) / 'installer'
            copy_manual(package)
            copy_manual(installer)
            # Compare all copied locale sources and media, not only the top level.
            expected = {p.relative_to(package/'docs/manual').as_posix(): p.read_bytes()
                        for p in (package/'docs/manual').rglob('*') if p.is_file()}
            self.assertIn('fr/01_getting_started.md',expected)
            self.assertIn('media/nine_slice_fr.png',expected)
            self.assertIn('examples/ui_nine_slice.sxl',expected)
            for relative,data in expected.items():
                self.assertEqual(data,(MANUAL/relative).read_bytes())
            for destination in (package, installer):
                actual = {p.relative_to(destination/'docs/manual').as_posix(): p.read_bytes()
                          for p in (destination/'docs/manual').rglob('*') if p.is_file()}
                self.assertEqual(actual, expected)
            # A repeated staging call must not fail or erase unrelated assets.
            unrelated = package / 'docs/manual/user-note.txt'
            unrelated.write_text('preserve', encoding='utf-8')
            copy_manual(package)
            self.assertEqual(unrelated.read_text(), 'preserve')

    def test_all_builtin_nodes_and_panels(self):
        catalog=node_catalog();self.assertEqual({k:len(v) for k,v in catalog.items()},{'nodes':16,'math':45,'execution':11,'shader':96})
        for lang,edition in self.editions.items():
            by_id={p['id']:p for p in edition}
            refs={p['id'] for p in edition if p['kind']=='Nodes'}
            expected={reference_id(family,item['id']) for family,items in catalog.items() for item in items}
            self.assertEqual(refs,expected)
            for identifier in refs: self.assertIn('(#'+identifier+')',by_id['builtin_nodes']['body'])
            for name,(chapter,en,fr) in panel_catalog().items():
                self.assertIn(chapter,by_id);self.assertIn(fr if lang=='fr' else en,by_id['panels']['body'])
        self.assertIn('degrees',self.by_id['math:sin']['body'])
        self.assertIn('radians',self.by_id['shader:sin']['body'])
        self.assertIn('current UV',self.by_id['shader:sample']['body'])
        self.assertIn('UTF-8 byte',self.by_id['math:text_length']['body'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
