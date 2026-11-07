"""Merges tooltip description batches ({key: {"fr": ..., "en": ...}}) into the tooltip libraries read by the
interface (src/ui/AutoTooltips.cpp): library/lang/tooltips_fr.json and library/lang/tooltips_en.json.

    python tools/tooltips_merge.py <batch.json> [<batch.json> ...]

Keys are an item's label as written in the code (before "##"), or "Panel|label" for a label that means different
things in different panels. Existing entries are kept; the batches' entries win.
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LANG = os.path.join(ROOT, 'library', 'lang')


def load(path):
    return json.load(open(path, encoding='utf-8')) if os.path.exists(path) else {}


def main():
    libs = {lang: load(os.path.join(LANG, 'tooltips_%s.json' % lang)) for lang in ('fr', 'en')}
    added = 0
    for batch in sys.argv[1:]:
        for key, texts in load(batch).items():
            if not isinstance(texts, dict):
                continue
            for lang in libs:
                text = (texts.get(lang) or '').strip()
                if text and '%' not in text:
                    libs[lang][key] = text
            added += 1
    for lang, entries in libs.items():
        with open(os.path.join(LANG, 'tooltips_%s.json' % lang), 'w', encoding='utf-8', newline='\n') as f:
            json.dump(dict(sorted(entries.items())), f, ensure_ascii=False, indent=1)
            f.write('\n')
    print('%d entries merged -> %d (fr), %d (en)' % (added, len(libs['fr']), len(libs['en'])))


if __name__ == '__main__':
    main()
