// Trusted DOM fixtures only. No browser, file URL navigation or resource loading.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { Window } from 'happy-dom';
import { mountViewer } from 'cydoc/viewer';
import { createDocumentation, createLink } from 'cydoc';
import image from 'cydoc/modules/image';
import gallery from 'cydoc/modules/gallery';
import graph from 'cydoc/modules/graph';
import { bindDownloads } from './resources.mjs';

const data=JSON.parse(await readFile(new URL('../Documentation/DocData/manifest.json',import.meta.url),'utf8'));
const resources=JSON.parse(await readFile(new URL('.cache/resources.json',import.meta.url),'utf8'));
const core=createDocumentation(data);
function fixture(t) {
  const window=new Window({settings:{disableJavaScriptFileLoading:true,enableJavaScriptEvaluation:false,enableImageFileLoading:false,enableCSSFileLoading:false}});
  const bindings={window,document:window.document,location:window.location,navigator:window.navigator,localStorage:window.localStorage,ResizeObserver:window.ResizeObserver,IntersectionObserver:window.IntersectionObserver,matchMedia:window.matchMedia.bind(window),requestAnimationFrame:window.requestAnimationFrame.bind(window),cancelAnimationFrame:window.cancelAnimationFrame.bind(window)};
  const previous=new Map();
  for(const [name,value] of Object.entries(bindings)){previous.set(name,Object.getOwnPropertyDescriptor(globalThis,name));Object.defineProperty(globalThis,name,{configurable:true,writable:true,value});}
  const host=window.document.createElement('div');window.document.body.append(host);
  const abort=new window.AbortController();
  const disposals=[];
  t.after(async()=>{for(const dispose of disposals.reverse())if(dispose)dispose();abort.abort();await window.happyDOM.abort();window.close();for(const [name,value] of previous){if(value)Object.defineProperty(globalThis,name,value);else delete globalThis[name];}});
  return {window,host,dispose:fn=>disposals.push(fn),context:{locale:'en',storageKey:'shaderixel-fixture',signal:abort.signal,assetUrl:src=>'DocData/'+src}};
}
const settle=()=>new Promise(resolve=>setImmediate(resolve));
function config(id,module,locale='en') {
  const page=core.resolve(id,locale).page;
  const match=page.html.match(new RegExp('data-cydoc-module="'+module+'"[^>]+data-config="([^"]+)"'));
  assert.ok(match,id);
  // CyDoc escapes its configuration as an HTML attribute; use a disabled DOM to decode it.
  const window=new Window({settings:{enableJavaScriptEvaluation:false,disableJavaScriptFileLoading:true,enableImageFileLoading:false}});
  window.document.body.innerHTML='<div data-config="'+match[1]+'"></div>';
  const result=JSON.parse(window.document.body.firstElementChild.dataset.config);window.close();return result;
}

test('all public routes resolve in English and French, including retired addresses',()=>{
  assert.deepEqual(core.availableLocales().map(l=>l.code),['en','fr']);
  for(const page of data.pages){assert.equal(core.resolve(page.id,page.locale).fallback,false);assert.ok(page.text.trim());}
  assert.equal(core.canonicalRoute({locale:'fr',pageId:'ui_textures'}).pageId,'ui-textures');
  assert.equal(core.canonicalRoute({locale:'fr',pageId:'reference-unreal-shaderixellink-readme'}).pageId,'unreal-link');
  assert.equal(core.canonicalRoute({locale:'en',pageId:'translation-report'}).pageId,'getting-started');
  assert.equal(createLink({locale:'fr',pageId:'ui-textures',anchor:'nine-slice-and-eight-slice',state:{media:'nine-slice'}},'Documentation/Documentation.html'),'Documentation/Documentation.html#/fr/ui-textures/nine-slice-and-eight-slice?media=nine-slice');
});

test('header search, Ctrl+K and the complete application navigation use the current viewer',async t=>{
  const {host,window,dispose}=fixture(t);const viewer=mountViewer(host,data,{homePage:'getting-started',initialTheme:'midnight',assetBase:'DocData/',storageId:'shaderixel-help'});
  dispose(()=>viewer.destroy());
  assert.equal(host.querySelectorAll('.side-nav [data-page]').length,data.pages.length/2);
  assert.equal(host.querySelector('.brand-logo').getAttribute('src'),'DocData/assets/branding/logo.png');
  assert.equal(host.querySelector('.sidebar .sidebar-note,.sidebar .mit-tag,.sidebar .offline-state'),null);
  const input=host.querySelector('[data-header-search]');assert.ok(input.closest('.topbar'));
  input.focus();input.value='pixels_read';input.dispatchEvent(new window.Event('input',{bubbles:true}));
  assert.equal(host.querySelector('.search-dialog').open,true);
  assert.match(host.querySelector('#search-results').textContent,/pixels_read/);
  window.document.dispatchEvent(new window.KeyboardEvent('keydown',{key:'k',ctrlKey:true,bubbles:true}));
  assert.equal(host.querySelector('.search-dialog').open,true);
  assert.ok(host.querySelectorAll('[data-set-theme]').length>=6);
  const select=host.querySelector('.locale-control select');select.value='fr';select.dispatchEvent(new window.Event('change',{bubbles:true}));
  window.dispatchEvent(new window.HashChangeEvent('hashchange'));await settle();
  assert.equal(host.querySelector('main h1').textContent,'Prise en main');
});

test('real page parents expose keyboard disclosures and full descriptions',async t=>{
  const {host,window,dispose}=fixture(t);const viewer=mountViewer(host,data,{homePage:'layers'});dispose(()=>viewer.destroy());
  const group=host.querySelector('[data-nav-key="page:layers"]');assert.ok(group.open);
  const summary=group.querySelector('summary');summary.focus();summary.dispatchEvent(new window.KeyboardEvent('keydown',{key:'ArrowLeft',bubbles:true}));assert.equal(group.open,false);
  summary.dispatchEvent(new window.KeyboardEvent('keydown',{key:'ArrowRight',bubbles:true}));assert.equal(group.open,true);
  const page=host.querySelector('[data-page="function-ui-uv-nine-slice-panel"]');page.focus();await settle();
  const tip=host.querySelector('.reader-tooltip');assert.equal(tip.hidden,false);
  assert.equal(tip.querySelector('.reader-tooltip-title').textContent,core.resolve('function-ui-uv-nine-slice-panel','en').page.title);
  assert.ok(tip.querySelector('.reader-tooltip-description').textContent.length>20);
});

test('images enlarge in the fixture window and Escape reduces them',t=>{
  const {host,context,window,dispose}=fixture(t);let nativeCalls=0;host.requestFullscreen=()=>{nativeCalls++;};
  const cleanup=image.mount(host,config('ui-textures','image','fr'),{...context,locale:'fr'});dispose(cleanup);
  assert.ok(host.querySelector('img').getAttribute('src').startsWith('DocData/assets/manual/'));
  host.querySelector('[data-action=expand]').click();assert.ok(host.classList.contains('media-expanded'));assert.equal(nativeCalls,0);
  window.document.dispatchEvent(new window.KeyboardEvent('keydown',{key:'Escape',bubbles:true}));assert.equal(host.classList.contains('media-expanded'),false);
});

test('the bilingual workspace gallery loads its selected image and labels only',async t=>{
  const {host,context,dispose}=fixture(t);const loads=[];
  const cleanup=gallery.mount(host,config('workspace','gallery','fr'),{...context,locale:'fr',loadModule:async name=>{loads.push(name);assert.equal(name,'image');return image;}});dispose(cleanup);await settle();
  const tabs=host.querySelectorAll('[role=tab]');assert.equal(tabs.length,3);
  assert.deepEqual([...tabs].map(t=>t.textContent),['Travail en calques','Graphe visuel d’effet','Éditeur de shader']);
  tabs[1].click();await settle();assert.equal(tabs[1].getAttribute('aria-selected'),'true');
  assert.ok(host.querySelector('.gallery-content img').getAttribute('src').endsWith('effect_graph_fr.png'));assert.deepEqual(loads,['image','image']);
});

test('the function graph preserves actual saved nodes and keyboard selection',t=>{
  const {host,context,window,dispose}=fixture(t);const cfg=config('function-ui-uv-nine-slice-panel','graph');
  const cleanup=graph.mount(host,cfg,context);dispose(cleanup);
  assert.equal(host.querySelectorAll('.graph-node').length,cfg.nodes.length);
  const node=host.querySelector('.graph-node');node.dispatchEvent(new window.KeyboardEvent('keydown',{key:'Enter',bubbles:true}));
  assert.equal(node.getAttribute('aria-pressed'),'true');assert.ok(cfg.links.length>0);
});

test('project downloads and legal files use reviewed local resource bindings',t=>{
  const {host}=fixture(t);host.innerHTML='<main><article>'+core.resolve('practice-ui-nine-slice','fr').page.html+'</article></main>';
  bindDownloads(host,resources['fr:practice-ui-nine-slice']);
  const link=host.querySelector('a[download]');assert.equal(link.getAttribute('href'),'DocData/assets/examples/ui_nine_slice.sxl');
  assert.equal(link.getAttribute('download'),'ui_nine_slice.sxl');
  host.querySelector('article').innerHTML=core.resolve('about','en').page.html;bindDownloads(host,resources['en:about']);
  assert.ok(host.querySelector('a[download="Shaderixel.txt"]'));assert.ok(host.querySelector('a[download="CyDoc.txt"]'));
});

test('public API search exposes behavior and signatures without source browsing',()=>{
  const results=core.search('pixels_read','en',20,{kind:'api'});assert.ok(results.some(r=>r.page.id==='sdk-sxhost-pixels-read-sxhost-pixels-read'));
  const page=core.resolve('sdk-sxhost-pixels-read-sxhost-pixels-read','fr').page;
  assert.ok(page.headings.some(h=>h.id==='parameters'));assert.match(page.text,/SxPixelsFn/);
  assert.equal(data.pages.some(p=>p.kind==='source'),false);
  assert.equal(data.pages.some(p=>p.id.startsWith('reference-claude')),false);
});
