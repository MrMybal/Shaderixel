"""Refresh the reference snapshot from a compiled, isolated native fixture.

Run with a folder containing matching D3D12/Vulkan manual-runtime-catalog.json
exports (SX_MANUAL_CATALOG_EXPORT=1 with a --demo scenario).
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SOURCES=['src/doc/EffectGraph.cpp','src/doc/EffectGraph.h','src/doc/GraphTypes.cpp','src/doc/GraphTypes.h',
         'src/doc/Document.h','src/ui/ManualCatalog.cpp']


def source_hashes():
    # Canonical text avoids a false stale result after Git LF/CRLF conversion.
    return {name:hashlib.sha256((ROOT/name).read_text(encoding='utf-8-sig').encode('utf-8')).hexdigest() for name in SOURCES}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('run',type=Path)
    args=parser.parse_args()
    paths=list(args.run.glob('*/manual-runtime-catalog.json'))
    if len(paths)!=2 or not any(p.parent.name.startswith('d3d12-') for p in paths) or not any(p.parent.name.startswith('vulkan-') for p in paths):
        raise ValueError('Exactly two backend exports are required')
    catalogs=[json.loads(p.read_text(encoding='utf-8')) for p in paths]
    if catalogs[0]!=catalogs[1]: raise ValueError('Compiled backend catalogs differ')
    target=ROOT/'docs/manual/locales/node_catalog.json'
    target.write_text(json.dumps(dict(sources=source_hashes(),catalog=catalogs[0]),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Updated compiled catalog:',{k:len(v) for k,v in catalogs[0].items()})


if __name__=='__main__': main()
