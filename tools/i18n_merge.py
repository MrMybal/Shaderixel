"""Merges the translated chunks (tools/i18n/chunk_NN_<lang>.json) into library/lang/<lang>.json.

    python tools/i18n_merge.py [lang]      (default: en)

Entries whose translation equals the French text are left out (nothing to translate). An existing dictionary
is kept and updated (entries of the chunks win), so a new extraction only needs its new strings translated.
"""
import glob
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
lang = sys.argv[1] if len(sys.argv) > 1 else 'en'
out = os.path.join(ROOT, 'library', 'lang', lang + '.json')
merged = {}
if os.path.exists(out):
    merged.update(json.load(open(out, encoding='utf-8')))
n = 0
for f in sorted(glob.glob(os.path.join(ROOT, 'tools', 'i18n', 'chunk_*_%s.json' % lang))):
    for k, v in json.load(open(f, encoding='utf-8')).items():
        if isinstance(v, str) and v and v != k:
            merged[k] = v
            n += 1
# Manual corrections (context the chunk translators did not have) win.
ov = os.path.join(ROOT, 'tools', 'i18n', 'overrides_%s.json' % lang)
if os.path.exists(ov):
    merged.update(json.load(open(ov, encoding='utf-8')))
os.makedirs(os.path.dirname(out), exist_ok=True)
json.dump(dict(sorted(merged.items())), open(out, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print(len(merged), 'entries ->', out, '(%d from the chunks)' % n)
