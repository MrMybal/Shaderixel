"""Panel help registry, with coverage of the actual Window > Panels menu."""
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
LABELS={
 '###cameraraw':('Photo RAW','Photo RAW'),'###efbigcode':('Custom node code','Code de nœud personnalisé'),
 '##closeprompt':('Close unsaved documents','Fermer les documents modifiés'),
 '##colordlg':('Color management','Gestion de la couleur'),'##spotdlg':('Spot channel','Canal de ton direct'),
 '##jobmodal':('Operation progress','Progression des opérations'),'##imgdlg':('Image geometry','Géométrie de l’image'),
 '##seldialog':('Modify selection','Modifier la sélection')}


def catalog():
    profiles=json.loads((ROOT/'.CyDocBuild/locales/panels.json').read_text(encoding='utf-8'))
    source=(ROOT/'src/ui/Workspace.cpp').read_text(encoding='utf-8-sig')
    table=re.search(r'void App::DrawPanelsMenu\(\).*?static const char\* names\[\] = \{(.*?)\};',source,re.S)
    if not table: raise ValueError('Panel menu registry not found')
    names=re.findall(r'"([^"\n]+)"',table[1])
    if set(names)-set(profiles): raise ValueError('Undocumented panels: '+str(set(names)-set(profiles)))
    literal=set();modals=set()
    for path in (ROOT/'src/ui').glob('*.cpp'):
        if path.name=='Demo.cpp': continue
        text=path.read_text(encoding='utf-8-sig')
        literal.update(re.findall(r'ImGui::Begin\("([^"\n]+)"',text))
        modals.update(re.findall(r'ImGui::BeginPopupModal\("([^"\n]+)"',text))
        if path.name!='Documentation.cpp':
            for line in text.splitlines():
                if '###' in line and ('ImGui::Begin(' in line or 'std::string title =' in line):
                    modals.update('###'+key for key in re.findall(r'###([A-Za-z_0-9]+)"',line))
    literal={n for n in literal if not n.startswith('##')}|modals
    if literal-set(profiles): raise ValueError('Undocumented windows: '+str(literal-set(profiles)))
    for name,profile in profiles.items():
        if len(profile)!=3 or not all(profile): raise ValueError('Incomplete panel help: '+name)
    return profiles


def index(language,english):
    french=language=='fr';body=[]
    for name,(chapter,en,fr) in catalog().items():
        label=name[3:] if name.startswith('###') else name
        if name in LABELS: label=LABELS[name][int(french)]
        if name=='###photodlg': label='Photo operations' if not french else 'Opérations photo'
        if french and name=='###Software updates': label='Mises à jour du logiciel'
        if not french: label=english.get(label,label)
        body += ['## '+label,fr if french else en,
                 '['+('Open the workflow guide' if not french else 'Ouvrir le guide de cette fonction')+'](#'+chapter+')']
    return '\n\n'.join(body)
