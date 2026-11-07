"""Check the distributable site, language coverage and unchanged CyDoc bundle."""
import argparse
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import sys
import unittest
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'.CyDocBuild'))
from build_manual import EXPORT, export_files, validate_generated


class Markup(HTMLParser):
    def __init__(self, source):
        super().__init__()
        self.nodes = []
        self.feed(source)

    def handle_starttag(self, tag, attrs):
        self.nodes.append((tag, dict(attrs)))


class SiteChecks(unittest.TestCase):
    def test_complete_languages_and_stable_documentation_targets(self):
        locales = json.loads((ROOT/'website/content.json').read_text(encoding='utf-8'))
        self.assertEqual(set(locales), {'en', 'fr'})
        self.assertEqual(set(locales['en']), set(locales['fr']))
        self.assertTrue(all(value.strip() for strings in locales.values() for value in strings.values()))
        ids = {re.search(r'^id: (.+)$', path.read_text(encoding='utf-8'), re.M)[1]
               for path in (ROOT/'.CyDocBuild/manual/en').glob('*.md')}
        for tag, attrs in self.markup.nodes:
            for attr in ('data-i18n', 'data-i18n-aria'):
                if attr in attrs:
                    self.assertIn(attrs[attr], locales['en'])
            if 'data-doc' in attrs:
                self.assertIn(attrs['data-doc'], ids)
                self.assertEqual(attrs['href'], 'Documentation.html#/en/' + attrs['data-doc'])
        self.assertIn('<html lang="en">', self.html)
        self.assertNotIn('{{', self.html)

    def test_local_links_media_and_anchors(self):
        ids = [attrs['id'] for tag, attrs in self.markup.nodes if 'id' in attrs]
        self.assertEqual(len(ids), len(set(ids)), 'Duplicate HTML IDs')
        for tag, attrs in self.markup.nodes:
            for name in ('href', 'src'):
                if name not in attrs:
                    continue
                url = urlsplit(attrs[name])
                if url.scheme:
                    self.assertEqual(url.scheme, 'https')
                    self.assertEqual(name, 'href', 'Runtime assets must be local')
                    continue
                if url.path:
                    target = (self.output / unquote(url.path)).resolve()
                    self.assertTrue(target.is_relative_to(self.output))
                    self.assertTrue(target.is_file(), str(target))
                    self.assertFalse(url.path.endswith('.md'), 'Links must open the visual reader')
                elif url.fragment:
                    self.assertIn(url.fragment, ids)
            if tag == 'img':
                self.assertIn('alt', attrs)
                self.assertGreater(int(attrs['width']), 0)
                self.assertGreater(int(attrs['height']), 0)
        for variants in self.manifest['media'].values():
            self.assertEqual(set(variants), {'en', 'fr'})
            for image in variants.values():
                blob = (self.output/image['src']).read_bytes()
                self.assertEqual(blob[:4], b'RIFF')
                self.assertEqual(blob[8:12], b'WEBP')

    def test_content_hashed_assets_and_no_extra_exported_data(self):
        expected = {self.output/path.relative_to(EXPORT) for path in export_files()}
        expected.update(self.output/name for name in ('index.html', '.nojekyll', 'website-build.json'))
        for name, sha in self.manifest['assets'].items():
            path = self.output/'assets'/name
            expected.add(path)
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), sha)
            self.assertIn('.' + sha[:12] + '.', name)
        actual = {path for path in self.output.rglob('*') if path.is_file()}
        self.assertEqual(actual, expected, 'Only the registered manual and showcase assets may be staged')
        stylesheet = next(self.output/'assets'/name for name in self.manifest['assets'] if name.endswith('.css'))
        self.assertNotIn('@import', stylesheet.read_text(encoding='utf-8'))

    def test_cydoc_reader_and_resources_are_unchanged(self):
        validate_generated()
        for source in export_files():
            copied = self.output/source.relative_to(EXPORT)
            self.assertEqual(hashlib.sha256(source.read_bytes()).digest(), hashlib.sha256(copied.read_bytes()).digest(), source.name)
        self.assertIn('Documentation.html', self.html)
        self.assertTrue((self.output/'DocData').is_dir())

    def test_downloads_match_the_declared_published_alpha(self):
        release = json.loads((ROOT/'website/release.json').read_text(encoding='utf-8'))
        self.assertEqual(release['version'], (ROOT/'VERSION').read_text().strip())
        self.assertEqual(release['status'], 'alpha')
        self.assertEqual(release['platform'], 'Windows x64')
        for asset in release['assets'].values():
            self.assertEqual(asset['url'], 'https://github.com/MrMybal/Shaderixel/releases/download/' + release['tag'] + '/' + asset['name'])
            self.assertGreater(asset['size'], 0)
            self.assertIn(asset['url'], self.html)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    SiteChecks.output = args.output.resolve()
    SiteChecks.html = (args.output/'index.html').read_text(encoding='utf-8')
    SiteChecks.markup = Markup(SiteChecks.html)
    SiteChecks.manifest = json.loads((args.output/'website-build.json').read_text(encoding='utf-8'))
    unittest.main(argv=[sys.argv[0]], verbosity=2)
