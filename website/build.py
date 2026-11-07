"""Stage the static showcase and the unchanged complete CyDoc export.

No network access, runtime framework or application build is required.
"""
import argparse
import hashlib
import html
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT/'website'
sys.path.insert(0,str(ROOT/'.CyDocBuild'))
from stage_documentation import stage_documentation
from build_manual import export_files, EXPORT


def build(output):
    output=Path(output).resolve()
    if output==ROOT or output.is_relative_to(SOURCE):
        raise ValueError('Use a separate empty output directory, e.g. build/website-preview.')
    strings=json.loads((SOURCE/'content.json').read_text(encoding='utf-8'))
    media=json.loads((SOURCE/'media.json').read_text(encoding='utf-8'))
    release=json.loads((SOURCE/'release.json').read_text(encoding='utf-8'))
    if set(strings)!= {'en','fr'} or set(strings['en'])!=set(strings['fr']):
        raise ValueError('English and French must have identical complete translation keys.')
    if any(not value.strip() for locale in strings.values() for value in locale.values()):
        raise ValueError('Empty translation.')
    stage_documentation(output)  # Safe fresh destination; registered exports only.
    assets=output/'assets';assets.mkdir()
    hashes={}

    def asset(source):
        source=Path(source)
        if source.is_symlink() or not source.is_relative_to(SOURCE):raise ValueError('Invalid website asset.')
        data=source.read_bytes();sha=hashlib.sha256(data).hexdigest()[:12]
        name=source.stem+'.'+sha+source.suffix
        (assets/name).write_bytes(data);hashes[name]=hashlib.sha256(data).hexdigest()
        return 'assets/'+name

    for variants in media.values():
        for image in variants.values():
            image['src']=asset(SOURCE/'media'/image['file'])
    logo=asset(SOURCE/'media/logo.webp')
    stylesheet=asset(SOURCE/'site.css')
    script=asset(SOURCE/'site.js')
    data={'locales':strings,'media':media,'release':release}
    payload='window.SHADERIXEL_SITE='+json.dumps(data,ensure_ascii=False,separators=(',',':')).replace('<','\\u003c')+';\n'
    data_name='content.'+hashlib.sha256(payload.encode()).hexdigest()[:12]+'.js'
    (assets/data_name).write_bytes(payload.encode('utf-8'))
    hashes[data_name]=hashlib.sha256(payload.encode()).hexdigest()
    tr=strings['en']

    def text(key,tag='p',cls='',attrs=''):
        return f'<{tag} data-i18n="{key}"'+(f' class="{cls}"' if cls else '')+(f' {attrs}' if attrs else '')+'>'+html.escape(tr[key])+f'</{tag}>'

    def link(key,url,cls='text-link',attrs=''):
        return text(key,'a',cls,f'href="{html.escape(url,quote=True)}" {attrs}')

    def doc(key,page,cls='text-link'):
        return link(key,'Documentation.html#/en/'+page,cls,f'data-doc="{page}"')

    def figure(key,caption=None,cls='',eager=False,frame=None):
        im=media[key]['en'];alt=html.escape(tr['media.'+key],quote=True)
        return '<figure class="capture '+cls+'">'+(f'<div class="frame-label"><span class="window-dots" aria-hidden="true"><i></i><i></i><i></i></span>{text(frame,"span")}</div>' if frame else '')+f'<button class="image-button" type="button" data-media="{key}" aria-label="{html.escape(tr["zoom.open"]+": "+tr["media."+key],quote=True)}"><img src="{im["src"]}" width="{im["width"]}" height="{im["height"]}" alt="{alt}" loading="'+('eager' if eager else 'lazy')+'" decoding="async"'+(' fetchpriority="high" id="hero-image"' if eager else '')+'></button>'+ (text(caption,'figcaption') if caption else '')+'</figure>'

    def intro(prefix,page=None,cls=''):
        return '<div class="section-copy '+cls+'">'+text(prefix+'.eyebrow',cls='eyebrow')+text(prefix+'.title','h2')+text(prefix+'.body',cls='lead')+(text(prefix+'.detail',cls='secondary') if prefix+'.detail' in tr else '')+(doc(prefix+'.link',page) if page else '')+'</div>'

    sections=[]
    sections.append('<section id="workspaces" class="section container">'+intro('workspaces')+'<div class="workspace">'
       +'<div class="mode-tabs" role="tablist" aria-label="'+tr['chapters.workspaces']+'" data-i18n-aria="chapters.workspaces">'
       +''.join(text('mode.'+mode+'.label','button','mode-tab',f'type="button" role="tab" id="mode-{mode}" data-mode="{mode}" aria-controls="mode-panel" aria-selected="'+('true' if mode=='layers' else 'false')+'" tabindex="'+('0' if mode=='layers' else '-1')+'"') for mode in ('layers','graph','effect','paint'))+'</div>'
       +'<div id="mode-panel" role="tabpanel" aria-labelledby="mode-layers"><div class="mode-info">'+text('mode.layers.title','h3',attrs='id="mode-title"')+text('mode.layers.body',attrs='id="mode-description"')+text('mode.layers.features',cls='mode-features',attrs='id="mode-features"')+doc('mode.guide','layers')+'</div>'+figure('layers',cls='mode-capture')+'</div></div></section>')
    sections.append('<section id="layers" class="section container feature split">'+intro('layers','layers')+figure('shapes','shapes.caption')+'</section>'
       +'<div class="container detail-grid"><article class="detail-story"><span class="boolean-mark" aria-hidden="true">○ ∩ □</span>'+text('shapes.title','h3')+text('shapes.body')+'</article><article class="precision-story"><div>'+text('precision.title','h3')+text('precision.body')+'</div>'+figure('precision','precision.caption',cls='panel-crop')+'</article></div>')
    sections.append('<section id="graphs" class="section container"><div class="wide-heading">'+intro('graphs','image-graph')+'</div>'+figure('image-graph','graphs.caption',cls='wide-capture')+'</section>')
    sections.append('<section id="effect-lab" class="section lab-band"><div class="container">'+intro('lab','effect-editor')+'<div class="two-up"><article>'+figure('effect-lab')+text('lab.visual.title','h3')+text('lab.visual.body')+'</article><article>'+figure('shader')+text('lab.code.title','h3')+text('lab.code.body')+'</article></div></div></section>')
    sections.append('<section id="color" class="section container feature split reverse">'+intro('color','photo')+figure('curves','color.curves.caption')+'</section><div class="container raw-story">'+figure('raw','color.raw.caption')+'</div>')
    sections.append('<section id="materials" class="section material-band"><div class="container">'+intro('materials','paint3d')+figure('material','materials.caption',cls='wide-capture')+text('materials.channels',cls='channel-line')+'<div class="material-views"><div>'+text('materials.switch.body',cls='lead')+'<div class="view-tabs" role="group" data-i18n-aria="materials.switch.label" aria-label="'+tr['materials.switch.label']+'">'+text('materials.paint','button','view-tab',attrs='type="button" data-view="paint3d" aria-pressed="true"')+text('materials.render','button','view-tab',attrs='type="button" data-view="render" aria-pressed="false"')+'</div></div>'+figure('paint3d',cls='material-switch')+'</div></div></section>')
    sections.append('<section id="ui-textures" class="section container"><div class="feature split">'+intro('ui','ui-textures')+figure('ui-toolkit','ui.toolkit.caption',cls='output-capture')+'</div><div class="uv-story">'+figure('uv-library','ui.uv.caption',cls='output-capture')+'</div></section>')
    sections.append('<section id="animation" class="section container feature split reverse">'+intro('animation','animation')+figure('animation','animation.caption')+'</section>')
    sections.append('<section id="workflows" class="section container"><div class="feature split">'+intro('workflow','automation')+figure('actions','workflow.caption')+'</div><div class="workflow-notes"><article>'+text('ai.title','h3')+text('ai.body')+text('ai.note',cls='secondary')+doc('ai.link','ai')+'</article><article>'+text('delivery.title','h3')+text('delivery.body')+doc('delivery.link','export')+'</article><article>'+text('extend.title','h3')+text('extend.body')+doc('extend.link','plugins-bridge')+'</article></div></section>')
    sections.append('<section id="documentation" class="section docs-band"><div class="container docs-content"><span class="book-icon" aria-hidden="true">↗</span>'+intro('docs')+'<div class="button-row">'+doc('docs.open','getting-started','button primary')+doc('docs.practice','tutorials','button ghost')+'</div></div></section>')
    sections.append('<section id="questions" class="section container faq"><div>'+text('faq.eyebrow',cls='eyebrow')+text('faq.title','h2')+'</div><div class="faq-list">'+''.join('<details>'+text('faq.'+key+'.q','summary')+text('faq.'+key+'.a')+'</details>' for key in ('platform','alpha','ai','offline'))+'</div></section>')
    sections.append('<section id="download" class="section download-band"><div class="container">'+intro('download')+f'<div class="release-version">{html.escape(release["version"])}</div><div class="download-grid"><a class="download-option" href="{release["assets"]["installer"]["url"]}"><span class="download-icon" aria-hidden="true">↓</span><div>'+text('download.installer','strong')+text('download.installer.note','span')+'</div><span aria-hidden="true">↗</span></a>'
       +f'<a class="download-option" href="{release["assets"]["portable"]["url"]}"><span class="download-icon" aria-hidden="true">↓</span><div>'+text('download.portable','strong')+text('download.portable.note','span')+'</div><span aria-hidden="true">↗</span></a></div>'+text('download.requirements',cls='download-requirements')+link('download.notes',release['url'])+'</div></section>')
    nav=link('nav.features','#workspaces','nav-link')+doc('nav.docs','getting-started','nav-link')+link('nav.download','#download','nav-link')
    chapters=''.join(link('chapters.'+key,'#'+ident,'chapter-link',f'data-chapter="{ident}"') for key,ident in [('workspaces','workspaces'),('layers','layers'),('graphs','graphs'),('materials','materials'),('ui','ui-textures'),('workflows','workflows'),('download','download')])
    template=(SOURCE/'template.html').read_text(encoding='utf-8')
    values={'TITLE':html.escape(tr['meta.title']),'DESCRIPTION':html.escape(tr['meta.description'],quote=True),'CSS':stylesheet,'JS':script,'DATA':'assets/'+data_name,'LOGO':logo,
      'SKIP':text('skip','a','skip',attrs='href="#main"'),
      'HEADER_NAV':nav,'LANGUAGE':tr['language'],'NAV_LABEL':tr['nav.label'],'MENU_LABEL':tr['nav.menu'],
      'HERO':text('hero.status',cls='status')+'<h1 id="hero-title">Shaderixel</h1>'+text('hero.description',cls='hero-description')+text('hero.body',cls='hero-body')+'<div class="button-row">'+link('hero.download',release['assets']['installer']['url'],'button primary')+link('hero.explore','#workspaces','button ghost')+'</div>'+text('hero.note',cls='hero-note'),
      'HERO_IMAGE':figure('layers','hero.caption',cls='hero-capture',eager=True,frame='hero.frame'),
      'STRIP':text('strip.intro','span')+''.join(text('strip.'+k,'span') for k in ('layers','nodes','shader','pbr'))+link('strip.source','https://github.com/MrMybal/Shaderixel','source-link'),
      'CHAPTERS':chapters,'CHAPTER_LABEL':tr['chapters.label'],'SECTIONS':'\n'.join(sections),
      'FOOTER':text('footer.tagline',cls='footer-tagline')+'<nav>'+doc('nav.docs','getting-started')+link('footer.releases',release['url'])+link('footer.source','https://github.com/MrMybal/Shaderixel')+link('footer.license','https://github.com/MrMybal/Shaderixel/blob/main/LICENSE')+'</nav>'+text('footer.credit',cls='credit')+'<span class="copyright">© 2026 Shaderixel · GPL-3.0-or-later</span>',
      'DIALOG_TITLE':text('zoom.title','h2',attrs='id="lightbox-title"'),'DIALOG_HINT':text('zoom.hint','span'),'DIALOG_CLOSE':tr['zoom.close'],'DIALOG_PREVIOUS':tr['zoom.previous'],'DIALOG_NEXT':tr['zoom.next']}
    for key,value in values.items():template=template.replace('{{'+key+'}}',value)
    if '{{' in template:raise ValueError('Unresolved template variable.')
    (output/'index.html').write_text(template,encoding='utf-8')
    (output/'website-build.json').write_text(json.dumps({'release':release['tag'],'locales':['en','fr'],'media':media,'assets':hashes},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Built static showcase:',output/'index.html')
    print('CyDoc preserved:',len(export_files()),'files; media:',len(media),'bilingual views; version:',release['version'])
    return output


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True,help='A fresh, empty output directory.')
    build(parser.parse_args().output)
