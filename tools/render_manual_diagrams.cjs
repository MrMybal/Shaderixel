// Rasterize authored SVG diagrams for the GPU manual viewer. No OS input.
const {chromium}=require('playwright');
const fs=require('node:fs'),path=require('node:path');
const {pathToFileURL}=require('node:url');
(async()=>{
 const dir=path.resolve(__dirname,'../docs/manual/media');
 const browser=await chromium.launch({headless:true,...(process.env.SX_BROWSER_EXE?{executablePath:process.env.SX_BROWSER_EXE}:{})});
 try{
  const page=await browser.newPage({viewport:{width:1200,height:640},deviceScaleFactor:1});
  for(const file of fs.readdirSync(dir).filter(f=>f.endsWith('.svg')).sort()){
   await page.goto(pathToFileURL(path.join(dir,file)).href);
   await page.evaluate(()=>window.scrollTo(0,0));
   await page.locator('svg').screenshot({path:path.join(dir,file.replace(/\.svg$/,'.png')),omitBackground:true});
  }
 }finally{await browser.close();}
 console.log('Rasterized bilingual diagrams.');
})().catch(e=>{console.error(e);process.exitCode=1;});
