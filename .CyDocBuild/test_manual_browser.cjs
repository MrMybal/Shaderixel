// Windows file:// tests with DOM events only: never uses system input devices.
const {chromium}=require('playwright');
const fs=require('node:fs');const path=require('node:path');const assert=require('node:assert/strict');
const {pathToFileURL}=require('node:url');
(async()=>{
 const root=path.resolve(__dirname,'..');const help=path.join(root,'Documentation');
 const out=path.join(root,'.CyDocBuild/internal/BrowserChecks');fs.mkdirSync(out,{recursive:true});
 const manifest=JSON.parse(fs.readFileSync(path.join(help,'DocData/manifest.json'),'utf8'));
 const browser=await chromium.launch({headless:true,...(process.env.SX_BROWSER_EXE?{executablePath:process.env.SX_BROWSER_EXE}:{})});
 const errors=[],remote=[];const results=[];
 try {
  const context=await browser.newContext({offline:true,viewport:{width:1440,height:1000},locale:'en-US'});
  const page=await context.newPage();page.on('pageerror',e=>errors.push(e.message));
  page.on('request',r=>{if(!/^(file:|data:|blob:)/.test(r.url()))remote.push(r.url())});
  const base=pathToFileURL(path.join(help,'Documentation.html')).href;
  async function open(route,title){await page.goto(base+route);await page.waitForFunction(t=>document.querySelector('main h1')?.textContent===t,title);}
  async function route(hash,title){await page.evaluate(h=>{location.hash=h},hash);await page.waitForFunction(t=>document.querySelector('main h1')?.textContent===t,title);}
  const click=selector=>page.evaluate(s=>{const e=document.querySelector(s);if(!e)throw Error('Missing '+s);e.click()},selector);
  await open('','Getting started');
  assert.equal(await page.locator('#app').getAttribute('data-theme'),'midnight');
  assert.equal(await page.locator('.side-nav [data-page]').count(),new Set(manifest.pages.map(p=>p.id)).size);
  await page.screenshot({path:path.join(out,'home-en.png')});results.push('English home and complete navigation');
  await open('#fr/ui_textures',"Textures d'interface et découpes");
  await page.waitForSelector('[data-cydoc-id="nine-slice"].cy-media');
  assert.equal(await page.locator('article').getAttribute('lang'),'fr');
  await page.evaluate(()=>{const select=document.querySelector('.locale-control select');select.value='en';select.dispatchEvent(new Event('change',{bubbles:true}))});
  await page.waitForFunction(()=>document.querySelector('main h1')?.textContent==='UI textures and slicing');
  assert.ok(await page.locator('article #nine-slice-and-eight-slice').count());results.push('legacy links and common anchors across locales');
  await route('#/en/ui-textures/nine-slice-and-eight-slice?media=nine-slice','UI textures and slicing');
  await page.waitForSelector('[data-cydoc-id="nine-slice"].cy-media');
  const picture='[data-cydoc-id="nine-slice"]';
  await click(picture+' [data-action="expand"]');
  assert.equal(await page.locator(picture+' [data-action="expand"]').getAttribute('aria-expanded'),'true');
  assert.equal(await page.evaluate(()=>document.fullscreenElement),null);
  await page.evaluate(()=>document.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true})));
  assert.equal(await page.locator(picture+' [data-action="expand"]').getAttribute('aria-expanded'),'false');
  const f1=await page.evaluate(()=>{const e=new KeyboardEvent('keydown',{key:'F1',bubbles:true,cancelable:true});dispatchEvent(e);return e.defaultPrevented});assert.ok(f1);
  await page.screenshot({path:path.join(out,'images-en.png')});results.push('image zoom stays in-window; Escape and F1');
  await route('#/en/function-ui-uv-nine-slice-panel/graph?media=function-graph','UI UV Nine Slice Panel');
  await page.waitForSelector('[data-cydoc-id="function-graph"] [data-node]');
  await page.waitForFunction(()=>{const r=document.querySelector('.cy-graph').getBoundingClientRect();return r.top>=0&&r.bottom<=innerHeight+2});
  const source=JSON.parse(fs.readFileSync(path.join(root,'library/functions/UI UV Nine Slice Panel.sxfn'),'utf8'));
  assert.equal(await page.locator('.graph-node').count(),source.graph.nodes.length);
  await page.evaluate(()=>{const n=document.querySelector('.graph-node');n.focus({preventScroll:true});n.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',bubbles:true}))});
  assert.equal(await page.locator('.graph-node[aria-pressed=true]').count(),1);
  const before=await page.locator('.graph-zoom').textContent();await click('[data-graph="in"]');
  assert.notEqual(await page.locator('.graph-zoom').textContent(),before);
  await page.screenshot({path:path.join(out,'function-graph.png')});results.push('actual function nodes, keyboard selection and graph zoom');
  await route('#/en/practice-ui-nine-slice/download','Nine-slice interface skin');await page.waitForSelector('a[download="ui_nine_slice.sxl"]');
  const file=await page.locator('a[download="ui_nine_slice.sxl"]').getAttribute('href');assert.ok(fs.existsSync(path.join(help,file)));
  assert.equal(await page.locator('a[download="ui_nine_slice.sxl"]').getAttribute('download'),'ui_nine_slice.sxl');results.push('local project download');
  await route('#/fr/sdk-abi/loading','ABI des plugins natifs');assert.ok(await page.locator('#loading').count());
  assert.ok((await page.locator('main').textContent()).includes('sx_plugin_load'));results.push('source-extracted C API and French anchor');
  const api=manifest.pages.find(p=>p.id==='sdk-sxhost-pixels-read-sxhost-pixels-read'&&p.locale==='en');
  await route('#/en/'+api.id+'/parameters',api.title);assert.ok(await page.locator('article #parameters').count());
  assert.ok((await page.locator('article').textContent()).includes('SxPixelsFn'));results.push('public API parameters without implementation listing');
  await route('#/fr/reference-docs-creative-libraries','Choisir une bibliothèque');
  assert.equal(await page.locator('article').getAttribute('lang'),'fr');results.push('retired appendix resolves to a complete French guide');
  await route('#/en/getting-started','Getting started');await page.evaluate(()=>document.dispatchEvent(new KeyboardEvent('keydown',{key:'k',ctrlKey:true,bubbles:true})));
  await page.evaluate(()=>{const input=document.querySelector('.search-dialog input');input.value='nine slice';input.dispatchEvent(new Event('input',{bubbles:true}))});
  await page.waitForSelector('#search-results a');assert.ok((await page.locator('#search-results').textContent()).toLowerCase().includes('nine'));
  await page.evaluate(()=>document.querySelector('.search-dialog').close());results.push('offline full text search');
  // Long-page navigation must preserve reading position on return.
  await route('#/en/tutorials','Guided projects');await page.evaluate(()=>scrollTo(0,650));
  await page.waitForTimeout(350);await route('#/en/getting-started','Getting started');
  await route('#/en/tutorials','Guided projects');await page.waitForTimeout(350);
  assert.ok(await page.evaluate(()=>scrollY>100));results.push('reading position survives navigation');
  const nativePath=path.join(root,'.CyDocBuild/internal/DocumentationNative/cydoc-results.json');
  if(fs.existsSync(nativePath)) {
    const native=JSON.parse(fs.readFileSync(nativePath,'utf8'));
    for(const name of ['home-fr','deep-image','deep-graph']) {
      const url=native.find(c=>c.case===name&&c.backend==='d3d12').urls[0];
      await page.goto(url);await page.waitForSelector('main article');
      assert.ok(await page.locator('main h1').textContent());
      if(name==='deep-graph') await page.waitForFunction(()=>{const r=document.querySelector('.cy-graph')?.getBoundingClientRect();return r&&r.top>=0&&r.bottom<=innerHeight+2});
    }
    results.push('exact native Windows URLs with accents, spaces and hash in the path');
    await page.goto(base+'#/en/getting-started');await page.waitForSelector('main article');
  }
  const images=[...new Set(manifest.pages.flatMap(p=>p.assets||[]))].filter(s=>/\.(png|svg)$/i.test(s));
  const decoded=await page.evaluate(async files=>Promise.all(files.map(src=>new Promise(resolve=>{const image=new Image();image.onload=()=>resolve(image.naturalWidth>0);image.onerror=()=>resolve(false);image.src="DocData/"+src}))),images);
  assert.ok(decoded.every(Boolean));results.push('all '+images.length+' local illustrations decode');
  await route('#/en/getting-started','Getting started');await click('[data-bookmark]');
  await page.reload();await page.waitForSelector('[data-bookmark]');assert.equal(await page.locator('[data-bookmark]').getAttribute('aria-pressed'),'true');results.push('bookmark persists across file-origin reload');
  await page.setViewportSize({width:390,height:844});await route('#/fr/getting-started','Prise en main');
  await click('[data-menu]');assert.equal(await page.locator('[data-menu]').getAttribute('aria-expanded'),'true');
  await page.waitForFunction(()=>Math.abs(document.querySelector('.sidebar').getBoundingClientRect().left)<.5);
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
  await page.screenshot({path:path.join(out,'mobile-fr.png')});results.push('narrow viewport navigation');
  await page.emulateMedia({media:'print'});assert.equal(await page.locator('article').isVisible(),true);results.push('printable article');
  assert.deepEqual(errors,[]);assert.deepEqual(remote,[]);
  fs.writeFileSync(path.join(out,'results.json'),JSON.stringify({passed:results,remoteRequests:remote,errors},null,2));
  console.log('PASS',results.length,'CyDoc browser scenarios; file://, offline, no OS input');
 } finally {await browser.close()}
})().catch(error=>{console.error(error);process.exitCode=1});
