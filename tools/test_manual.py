"""Check actual reference coverage, links, offline assets and both staging paths."""
import json
from pathlib import Path
import re
import tempfile
import unittest
import hashlib
import struct
from manual_examples import files as practice_files
from manual_tools import catalog as tool_catalog
from manual_commands import catalog as command_catalog
from manual_brushes import catalog as brush_catalog
from manual_nodes import catalog as node_catalog, reference_id
from manual_panels import catalog as panel_catalog

from build_manual import ROOT, MANUAL, copy_manual, pages, validate_generated, languages, blocks, ui_strings


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
        self.assertTrue(messages['fr']['Tool help'])
        # Stable shader IDs and parameter coverage are identical in both editions.
        for page in self.pages:
            if page['kind'] == 'Effects':
                self.assertEqual(re.findall(r'\[([a-zA-Z_0-9]+)\] \(',page['body']),
                                 re.findall(r'\[([a-zA-Z_0-9]+)\] \(',fr[page['id']]['body']))

    def test_translation_acknowledgements_do_not_advance_automatically(self):
        from cydoc_content import translation_revisions
        registry=json.loads((MANUAL/'locales/revisions.json').read_text(encoding='utf-8'))
        en=self.editions['en'][0];fr=self.editions['fr'][0]
        source,reviewed=translation_revisions(en,fr,registry)
        self.assertEqual(source,reviewed)
        changed={**en,'body':en['body']+' English meaning changed.'}
        new,previous=translation_revisions(changed,fr,registry)
        self.assertNotEqual(new,previous)
        self.assertEqual(previous,reviewed)
        changed_fr={**fr,'body':fr['body']+' Texte modifié sans validation.'}
        self.assertIsNone(translation_revisions(en,changed_fr,registry)[1])

    def test_cydoc_export_and_assets(self):
        from build_manual import EXPORT
        manifest=json.loads((EXPORT/'manifest.json').read_text(encoding='utf-8'))
        self.assertEqual(manifest['defaultLocale'],'en')
        self.assertEqual({item['code'] for item in manifest['locales']},{'en','fr'})
        self.assertGreater(len(manifest['pages']),1884)
        self.assertTrue((EXPORT/'media-image.js').is_file())
        self.assertTrue((EXPORT/'media-graph.js').is_file())
        for unused in ('mesh','gif','video','gallery'):
            self.assertFalse((EXPORT/('media-'+unused+'.js')).exists())
        self.assertFalse((ROOT/'tools/cydoc/node_modules/three').exists())
        for edition in self.editions.values():
            for page in edition:
                for kind,_,target in blocks(page['body']):
                    if kind=='image':
                        self.assertEqual((MANUAL/target).read_bytes(),(EXPORT/'assets/docs/manual'/target).read_bytes())
        for old in self.by_id:
            canonical=manifest.get('aliases',{}).get(old,old)
            self.assertTrue(any(p['id']==canonical for p in manifest['pages']),old)

    def test_common_cydoc_anchors(self):
        from build_manual import EXPORT
        manifest=json.loads((EXPORT/'manifest.json').read_text(encoding='utf-8'))
        records={(p['id'],p['locale']):p for p in manifest['pages']}
        for old in self.by_id:
            ident=manifest.get('aliases',{}).get(old,old)
            en,fr=records[ident,'en'],records[ident,'fr']
            self.assertEqual({h['id'] for h in en['headings']},{h['id'] for h in fr['headings']},old)
        self.assertIn('sdk-abi',{p['id'] for p in manifest['pages']})
        self.assertTrue(any(p['kind']=='source' for p in manifest['pages']))

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
            self.assertIn('data.js',expected)
            self.assertIn('assets/docs/manual/media/nine_slice_fr.png',expected)
            self.assertIn('examples/ui_nine_slice.sxl',expected)
            for relative,data in expected.items():
                from build_manual import EXPORT
                self.assertEqual(data,(EXPORT/relative).read_bytes())
            for destination in (package, installer):
                actual = {p.relative_to(destination/'docs/manual').as_posix(): p.read_bytes()
                          for p in (destination/'docs/manual').rglob('*') if p.is_file()}
                self.assertEqual(actual, expected)
            # A repeated staging call must not fail or erase unrelated assets.
            unrelated = package / 'docs/manual/user-note.txt'
            unrelated.write_text('preserve', encoding='utf-8')
            copy_manual(package)
            self.assertEqual(unrelated.read_text(), 'preserve')

    def test_pages_stages_only_registered_export(self):
        from stage_documentation import stage_documentation
        from build_manual import EXPORT, STAMP
        state=json.loads(STAMP.read_text(encoding='utf-8'))
        with tempfile.TemporaryDirectory(prefix='shaderixel-pages-') as folder:
            destination=(Path(folder)/'site').resolve()
            copied=stage_documentation(destination)
            expected={str((ROOT/relative).relative_to(EXPORT)):ROOT/relative
                      for relative in state['outputs'] if (ROOT/relative).is_relative_to(EXPORT)}
            self.assertEqual({str(p.relative_to(destination)) for p in copied},set(expected))
            for relative,source in expected.items():
                self.assertEqual((destination/relative).read_bytes(),source.read_bytes())
            self.assertTrue((destination/'.nojekyll').is_file())
            self.assertFalse((destination/'docs').exists())
            self.assertFalse((destination/'Guide.html').exists())
            with self.assertRaises(ValueError):stage_documentation(destination)

    def test_readme_links_target_visual_reader_routes(self):
        from build_manual import EXPORT
        manifest=json.loads((EXPORT/'manifest.json').read_text(encoding='utf-8'))
        known={(p['locale'],p['id']) for p in manifest['pages']}
        for name in ('README.md','unreal/ShaderixelLink/README.md'):
            text=(ROOT/name).read_text(encoding='utf-8')
            links=re.findall(r'https://mrmybal.github.io/Shaderixel/#/(en|fr)/([a-z0-9-]+)',text)
            self.assertTrue(links,name)
            for route in links:self.assertIn(route,known)

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
