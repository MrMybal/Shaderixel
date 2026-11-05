// Optional headless browser check. Requires Playwright; never uses OS input.
const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

(async () => {
    const root = path.resolve(__dirname, '..');
    const handbookCount=fs.readdirSync(path.join(root,'docs/manual')).filter(n=>/^\d\d_.*\.md$/.test(n)).length;
    const toolCount=Object.keys(JSON.parse(fs.readFileSync(path.join(root,'docs/manual/locales/tools.json'),'utf8'))).length;
    const commandCount=Object.keys(JSON.parse(fs.readFileSync(path.join(root,'docs/manual/locales/commands.json'),'utf8'))).length;
    const brushCount=Object.keys(JSON.parse(fs.readFileSync(path.join(root,'docs/manual/locales/brushes.json'),'utf8'))).length;
    const nodeCount=Object.values(JSON.parse(fs.readFileSync(path.join(root,'docs/manual/locales/node_catalog.json'),'utf8')).catalog).reduce((n,entries)=>n+entries.length,0);
    const expected = handbookCount+toolCount+commandCount+brushCount+nodeCount
        + fs.readdirSync(path.join(root, 'shaders/effects')).filter(n => n.endsWith('.slang')).length
        + fs.readdirSync(path.join(root, 'library/functions')).filter(n => n.endsWith('.sxfn')).length;
    const output = path.join(root, 'build/DocumentationBrowser');
    fs.mkdirSync(output, { recursive: true });
    const browser = await chromium.launch({ headless: true,
        ...(process.env.SX_BROWSER_EXE ? { executablePath: process.env.SX_BROWSER_EXE } : {}) });
    try {
        const context = await browser.newContext({ offline: true, locale: 'en-US', viewport: { width: 1440, height: 1000 } });
        const page = await context.newPage();
        const errors = [], remote = [];
        page.on('pageerror', e => errors.push(e.message));
        page.on('request', r => { if (!r.url().startsWith('file:')&&!r.url().startsWith('data:')) remote.push(r.url()); });
        await page.goto(pathToFileURL(path.join(root, 'docs/manual/index.html')).href);
        assert.equal(await page.locator('article:not([hidden]) h1').textContent(), 'Getting started');
        assert.equal(await page.locator('article').count(), expected*2);
        assert.equal(await page.locator('nav a:not([hidden])').count(), expected);
        assert.ok(await page.locator('#sections option').count()>1);
        await page.evaluate(()=>{
            const size=document.getElementById('text-size');size.value='1.5';size.dispatchEvent(new Event('change'));
        });
        assert.equal(await page.locator('article:not([hidden])').evaluate(e=>getComputedStyle(e).fontSize),'24px');
        await page.evaluate(()=>{
            const sections=document.getElementById('sections');sections.value=sections.lastElementChild.value;sections.dispatchEvent(new Event('change'));
        });
        assert.ok(await page.locator('#content').evaluate(e=>e.scrollTop)>0);
        assert.equal(await page.evaluate(()=>location.hash),'');
        await page.evaluate(()=>{
            const size=document.getElementById('text-size');size.value='1';size.dispatchEvent(new Event('change'));document.getElementById('content').scrollTop=0;
        });
        await page.screenshot({ path: path.join(output, 'handbook.png') });

        // Dispatch browser-internal DOM events rather than touching system devices.
        await page.evaluate(() => {
            const input = document.getElementById('search');
            input.value = 'NINE SLICE'; input.dispatchEvent(new Event('input'));
            const select = document.getElementById('kind');
            select.value = 'Functions'; select.dispatchEvent(new Event('change'));
        });
        const found = await page.locator('nav a:not([hidden])').count();
        assert.ok(found > 0 && found < 162);
        await page.evaluate(() => {
            const a = Array.from(document.querySelectorAll('nav a')).find(a => !a.hidden && a.textContent.includes('UI UV Nine Slice Panel'));
            a.click();
        });
        await page.waitForFunction(() => document.querySelector('article:not([hidden])').dataset.page === 'function:UI UV Nine Slice Panel');
        assert.ok((await page.locator('article:not([hidden])').textContent()).includes('Source region origin'));
        await page.screenshot({ path: path.join(output, 'function.png') });
        await page.evaluate(() => {
            const a = Array.from(document.querySelectorAll('article:not([hidden]) a')).find(a => a.hash === '#en/effect:ui_uv_nine_slice');
            a.click();
        });
        await page.waitForFunction(() => document.querySelector('article:not([hidden])').dataset.page === 'effect:ui_uv_nine_slice');
        assert.ok((await page.locator('article:not([hidden])').textContent()).includes('[source_origin]'));
        await page.goBack();
        await page.waitForFunction(() => document.querySelector('article:not([hidden])').dataset.page === 'function:UI UV Nine Slice Panel');
        await page.evaluate(()=>{const l=document.getElementById('language');l.value='fr';l.dispatchEvent(new Event('change'));});
        await page.waitForFunction(()=>document.documentElement.lang==='fr');
        assert.equal(await page.locator('label[for=sections]').textContent(),'Dans ce chapitre');
        assert.equal(await page.locator('article:not([hidden])').getAttribute('data-page'),'function:UI UV Nine Slice Panel');
        assert.ok((await page.locator('article:not([hidden])').textContent()).includes('Variables exposées'));
        await page.screenshot({path:path.join(output,'function-fr.png')});
        await page.evaluate(()=>{
            const k=document.getElementById('kind');k.value='Handbook';k.dispatchEvent(new Event('change'));
            const s=document.getElementById('search');s.value='ECRÊTAGE';s.dispatchEvent(new Event('input'));
        });
        assert.ok(await page.locator('nav a:not([hidden])').count()>0);
        await page.evaluate(()=>location.hash='#fr/ui_textures');
        await page.waitForFunction(()=>document.querySelector('article:not([hidden])').dataset.page==='ui_textures');
        await page.waitForFunction(()=>document.querySelector('article:not([hidden]) img').naturalWidth>0);
        await page.evaluate(()=>document.querySelector('article:not([hidden]) button.figure').click());
        assert.equal(await page.locator('#zoom').isVisible(),true);
        assert.ok((await page.locator('#zoom-caption').textContent()).includes('préservent'));
        await page.screenshot({path:path.join(output,'diagram-zoom-fr.png')});
        await page.evaluate(()=>document.getElementById('close-zoom').click());
        assert.equal(await page.locator('#zoom').isVisible(),false);
        await page.evaluate(()=>location.hash='#fr/tutorials');
        await page.waitForFunction(()=>document.querySelector('article:not([hidden])').dataset.page==='tutorials');
        assert.equal(await page.locator('article:not([hidden]) a.example').count(),6);
        assert.equal(await page.locator('#sections option').count(),8);
        const downloadReady=page.waitForEvent('download');
        await page.evaluate(()=>document.querySelector('article:not([hidden]) a.example').click());
        const download=await downloadReady;
        assert.equal(download.suggestedFilename(),'ui_nine_slice.sxl');
        const downloaded=path.join(output,'ui_nine_slice.sxl');await download.saveAs(downloaded);
        assert.deepEqual(fs.readFileSync(downloaded),fs.readFileSync(path.join(root,'docs/manual/examples/ui_nine_slice.sxl')));
        await page.screenshot({path:path.join(output,'tutorials-fr.png')});
        await page.evaluate(() => {
            const input = document.getElementById('search');
            input.value = 'no_match_3849303'; input.dispatchEvent(new Event('input'));
        });
        assert.equal(await page.locator('nav a:not([hidden])').count(), 0);
        assert.equal(await page.locator('#empty').isVisible(), true);
        await page.setViewportSize({ width: 480, height: 900 });
        await page.evaluate(() => {
            document.getElementById('search').value = '';
            document.getElementById('search').dispatchEvent(new Event('input'));
            location.hash = '#fr/dimensions';
        });
        await page.waitForFunction(() => document.querySelector('article:not([hidden])').dataset.page === 'dimensions');
        await page.screenshot({ path: path.join(output, 'mobile.png'), fullPage: true });
        assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), true);
        await page.emulateMedia({ media: 'print' });
        assert.equal(await page.locator('article:visible').count(), 1);
        assert.equal(await page.locator('aside').isVisible(), false);
        await page.evaluate(()=>{window.__printReady=false;window.print=()=>{window.__printReady=true;};document.getElementById('print-all').click();});
        await page.waitForFunction(()=>window.__printReady===true);
        assert.equal(await page.locator('article:visible').count(),handbookCount);
        assert.equal(await page.locator('article[data-language=en]:visible').count(),0);
        assert.equal(await page.evaluate(()=>Array.from(document.querySelectorAll('article[data-print=true] img')).every(img=>img.complete&&img.naturalWidth>0)),true);
        await page.emulateMedia({media:'screen'});
        await page.evaluate(()=>{document.body.classList.remove('print-handbook');location.hash='#de/getting_started';});
        await page.waitForFunction(()=>document.documentElement.lang==='en');
        assert.equal(await page.locator('article:not([hidden]) h1').textContent(),'Getting started');
        await page.evaluate(()=>{
            const k=document.getElementById('kind');k.value='Tools';k.dispatchEvent(new Event('change'));
            const s=document.getElementById('search');s.value='';s.dispatchEvent(new Event('input'));
            location.hash='#fr/tool:clone';
        });
        await page.waitForFunction(()=>document.querySelector('article:not([hidden])').dataset.page==='tool:clone');
        assert.equal(await page.locator('nav a:not([hidden])').count(),toolCount);
        assert.equal(await page.locator('article:not([hidden]) h1').textContent(),'Tampon de duplication');
        assert.ok((await page.locator('article:not([hidden])').textContent()).includes('Alt+cliquez'));
        await page.setViewportSize({width:1440,height:1000});
        await page.screenshot({path:path.join(output,'tool-fr.png')});
        await page.evaluate(()=>{
            const a=Array.from(document.querySelectorAll('article:not([hidden]) a')).find(a=>a.hash==='#fr/tools');a.click();
        });
        await page.waitForFunction(()=>document.querySelector('article:not([hidden])').dataset.page==='tools');
        await page.evaluate(()=>{
            const k=document.getElementById('kind');k.value='Commands';k.dispatchEvent(new Event('change'));location.hash='#fr/command:export';
        });
        await page.waitForFunction(()=>document.querySelector('article:not([hidden])').dataset.page==='command:export');
        assert.equal(await page.locator('nav a:not([hidden])').count(),commandCount);
        assert.equal(await page.locator('article:not([hidden]) h1').textContent(),'Étape Exporter');
        assert.deepEqual(JSON.parse(await page.locator('article:not([hidden]) pre').textContent()),{format:'png',folder:'',suffix:'',quality:92});
        await page.screenshot({path:path.join(output,'command-fr.png')});
        await page.evaluate(()=>{
            const k=document.getElementById('kind');k.value='Brushes';k.dispatchEvent(new Event('change'));location.hash='#fr/brush:cy_texture';
        });
        await page.waitForFunction(()=>document.querySelector('article:not([hidden])').dataset.page==='brush:cy_texture');
        assert.equal(await page.locator('nav a:not([hidden])').count(),brushCount);
        assert.equal(await page.locator('article:not([hidden]) h1').textContent(),'Empreinte de texture');
        assert.ok((await page.locator('article:not([hidden])').textContent()).includes('textures/splatter.png'));
        await page.screenshot({path:path.join(output,'brush-fr.png')});
        await page.evaluate(()=>{
            const k=document.getElementById('kind');k.value='Nodes';k.dispatchEvent(new Event('change'));location.hash='#fr/shader:sin';
        });
        await page.waitForFunction(()=>document.querySelector('article:not([hidden])').dataset.page==='shader:sin');
        assert.equal(await page.locator('nav a:not([hidden])').count(),nodeCount);
        assert.ok((await page.locator('article:not([hidden])').textContent()).includes('radians'));
        await page.screenshot({path:path.join(output,'node-fr.png')});
        await page.evaluate(()=>location.hash='#fr/math:sin');
        await page.waitForFunction(()=>document.querySelector('article:not([hidden])').dataset.page==='math:sin');
        assert.ok((await page.locator('article:not([hidden])').textContent()).includes('degrés'));
        assert.deepEqual(errors, []);
        assert.deepEqual(remote, []);
        fs.writeFileSync(path.join(output, 'results.json'), JSON.stringify({ pages: expected, matchedFunctions: found,
            languages:['en','fr'],illustrations:42,practiceDocuments:6,tools:toolCount,commands:commandCount,brushes:brushCount,nodes:nodeCount,offline: true, navigation: true, mobile: true, print: true, handbookPrint:true,zoom:true,sections:true,textSize:true,errors, remote }, null, 2));
        console.log('Offline browser manual: all checks passed (' + expected + ' pages, search, history, mobile, print).');
    } finally { await browser.close(); }
})().catch(e => { console.error(e); process.exitCode = 1; });
