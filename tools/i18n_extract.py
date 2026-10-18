"""Collects the French interface strings of Shaderixel into tools/i18n/strings_fr.json (sorted, unique).

Sources: string literals of src/ (C++), the metadata of the effects / brushes / blends (shaders/**/*.slang:
//@name, //@category, //@group, //@param labels and enum items), and the names of the library presets
(library/**/*.json, *.sxfn names). The visible part of an ImGui label is kept (before "##"); printf
numeric specifiers become {n} (the runtime text is matched with its numbers replaced the same way).

    python tools/i18n_extract.py
"""
import glob
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'tools', 'i18n', 'strings_fr.json')

LIT = re.compile(r'(?<![A-Za-z0-9_])(?:u8)?"((?:[^"\\\n]|\\.)*)"')
NUM_SPEC = re.compile(r'%[-+ #0]*\d*(?:\.\d+)?(?:hh|h|ll|l|z|j|t|L)?[diuxXfFeEgG]')
FRENCH_HINT = re.compile(r"[éèêàâùûîôçëïœÉÈÀÇ]|\b(le|la|les|des|du|de|un|une|et|ou|aux?|pour|par|sur|dans|avec|sans|calques?|couleurs?|"
                         r"fichiers?|ouvrir|enregistrer|exporter|importer|nouveau|nouvelle|annuler|fermer|choisir|taille|"
                         r"rayon|fond|pinceau|gomme|outils?|réglages?|filtre|fusion|masque|aucune?|tout|toutes?)\b", re.I)


def unescape(s):
    return s.encode('utf-8').decode('unicode_escape').encode('latin-1').decode('utf-8') if '\\' in s else s


def visible(s):
    i = s.find('##')
    return s[:i] if i >= 0 else s


def keep(s):
    s = s.strip('\n')
    if len(s) < 2 or not re.search(r'[A-Za-zÀ-ÿ]{2}', s):
        return False
    if re.fullmatch(r'[\w./\\:*?<>|-]+\.(png|jpg|json|slang|hlsl|exe|dll|txt|md|py|cpp|h|bat|ini|sxl|sx3d|obj|fbx|glb|gltf|hdr|zip)', s, re.I):
        return False
    if re.fullmatch(r'[a-z_][a-z0-9_.]*', s):  # identifiers, json keys
        return False
    if s.startswith(('http://', 'https://', '#', '//', '%s', 'shaderixel.')) and ' ' not in s:
        return False
    return bool(FRENCH_HINT.search(s)) or (re.fullmatch(r"[A-ZÀ-Ý][a-zà-ÿ'’ -]{2,}.*", s) is not None)


def norm(s):
    s = visible(s)
    s = NUM_SPEC.sub('{n}', s)
    return s


strings = set()
for f in glob.glob(os.path.join(ROOT, 'src', '**', '*.cpp'), recursive=True) + glob.glob(os.path.join(ROOT, 'src', '**', '*.h'), recursive=True):
    text = open(f, encoding='utf-8', errors='replace').read()
    for line in text.split('\n'):
        st = line.strip()
        if st.startswith('//') or st.startswith('#include') or 'Log(' in st or 'LOG(' in st:
            continue
        for m in LIT.finditer(line):
            try:
                s = unescape(m.group(1))
            except Exception:
                s = m.group(1)
            for part in s.split('\0'):
                part = norm(part)
                for sub in part.split('\n'):
                    if keep(sub.strip()):
                        strings.add(sub.strip())
                if keep(part.strip()) and '\n' in part:
                    strings.add(part.strip())

META = re.compile(r'^\s*//@(name|category|group)\s+(.+?)\s*$')
PARAM = re.compile(r'^\s*//@param\s+\S+\s+(\S+)(.*)$')
QUOTED = re.compile(r'"([^"]*)"')
for f in glob.glob(os.path.join(ROOT, 'shaders', '**', '*.slang'), recursive=True):
    for line in open(f, encoding='utf-8', errors='replace'):
        m = META.match(line)
        if m:
            strings.add(m.group(2))
            continue
        m = PARAM.match(line)
        if m:
            for q in QUOTED.findall(m.group(2)):
                for item in q.split('|'):
                    if len(item.strip()) >= 2:
                        strings.add(item.strip())

for f in glob.glob(os.path.join(ROOT, 'library', '**', '*.json'), recursive=True) + glob.glob(os.path.join(ROOT, 'library', '**', '*.sxfn'), recursive=True):
    try:
        j = json.load(open(f, encoding='utf-8'))
    except Exception:
        continue
    for key in ('name', 'category', 'description', 'help'):
        v = j.get(key) if isinstance(j, dict) else None
        if isinstance(v, str) and len(v) >= 2:
            strings.add(v)

strings = sorted(s for s in strings if s.strip())
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(strings, open(OUT, 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
print(len(strings), 'strings ->', OUT)
