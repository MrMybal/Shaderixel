"""Action-command reference with defaults read from the application's command factory."""
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]


def catalog():
    source=(ROOT/'src/ui/Actions.cpp').read_text(encoding='utf-8-sig')
    table=re.search(r'const char\* kCmds\[\]\[2\]\s*=\s*\{(.*?)\n\};',source,re.S)
    if not table: raise ValueError('Action command table not found')
    entries=re.findall(r'\{"([a-z_]+)",\s*"([^"]+)"\}',table[1])
    profiles=json.loads((ROOT/'docs/manual/locales/commands.json').read_text(encoding='utf-8'))
    ids=[identifier for identifier,_ in entries]
    if len(set(ids))!=len(ids) or set(ids)!=set(profiles):
        raise ValueError('Action documentation coverage differs: '+str(set(ids)^set(profiles)))
    factory=re.search(r'Json ActionDefaultParams\([^)]*\)\s*\{(.*?)\n\}',source,re.S)
    if not factory: raise ValueError('Action default factory not found')
    defaults={identifier:{} for identifier in ids}
    for identifier,body in re.findall(r'(?:else\s+)?if\s*\(c == "([a-z_]+)"\)\s*(\{[^{}]*\}|[^;\n]+;)',factory[1]):
        if identifier not in defaults: raise ValueError('Unregistered command default: '+identifier)
        for key,value in re.findall(r'p\["([a-z_]+)"\]\s*=\s*([^;]+);',body):
            value=value.strip()
            if value=='Json::Object()': parsed={}
            else:
                if re.fullmatch(r'[-+0-9.eE]+f',value): value=value[:-1]
                try: parsed=json.loads(value)
                except json.JSONDecodeError as error: raise ValueError('Unsupported command default: '+value) from error
            defaults[identifier][key]=parsed
    result=[]
    for identifier,label in entries:
        profile=profiles[identifier]
        for lang in ('en','fr'):
            if len(profile.get(lang,[]))!=5 or not all(profile[lang]): raise ValueError('Incomplete command profile: '+identifier)
        documented={name.strip() for group in profile['parameters'] for name in group.split(',')}
        if set(defaults[identifier])-documented: raise ValueError('Undocumented defaults: '+identifier)
        if not all(len(text)==2 and all(text) for text in profile['parameters'].values()): raise ValueError('Untranslated command parameters: '+identifier)
        result.append(dict(profile,id='command:'+identifier,command=identifier,label=label,defaults=defaults[identifier]))
    return result


def pages(language):
    french=language=='fr';t=lambda en,fr:fr if french else en
    result=[]
    for item in catalog():
        title,purpose,target,expected,limits=item[language]
        body=[purpose,t('Stable action command: ','Commande d’action stable : ')+item['command'],
              t('## Target and use','## Cible et utilisation'),target,
              t('## Parameters','## Paramètres')]
        body += ['- '+name+': '+text[1 if french else 0] for name,text in item['parameters'].items()] or [t('No command-specific parameters.','Aucun paramètre spécifique.')]
        body += [t('## New-step defaults','## Valeurs d’une nouvelle étape'),
                 t('These are created by Add Step. An imported action containing an empty params object is not automatically merged with these defaults.',
                   'Ces valeurs sont créées par Ajouter une étape. Un params vide importé n’est pas automatiquement complété avec ces valeurs.'),
                 '```json\n'+json.dumps(item['defaults'],ensure_ascii=False,indent=2)+'\n```',
                 t('## Expected result','## Résultat attendu'),expected,
                 t('## Limits and replay','## Limites et réexécution'),limits,
                 t('Whole-action runs skip disabled steps. Run This Step explicitly executes the chosen step even when it is disabled. Completed edits are not rolled back if a later step fails.',
                   'Une action entière ignore les étapes désactivées. Exécuter cette étape la lance même désactivée. Les modifications terminées ne sont pas annulées si une étape ultérieure échoue.'),
                 t('[Action recording and batch guide](#actions)','[Guide des actions et du traitement par lots](#actions)'),
                 t('[Related workflow](#','[Procédure associée](#')+item['chapter']+')']
        result.append(dict(id=item['id'],title=title,kind='Commands',body='\n\n'.join(body),sourceCommand=item['command']))
    return result
