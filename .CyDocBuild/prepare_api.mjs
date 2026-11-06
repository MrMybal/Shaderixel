import { readFile, writeFile } from 'node:fs/promises';
import { dirname, resolve, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import { extractComments } from 'cydoc/extract';
import { importApi } from 'cydoc/api-import';

const work=dirname(fileURLToPath(import.meta.url));
const locales=[{code:'en',label:'English'},{code:'fr',label:'Français'}];
const annotations=extractComments(await readFile(resolve(work,'../sdk/include/shaderixel_plugin.h'),'utf8'));
const imported=await importApi(locales.map(locale=>({format:'cydoc',path:`api/native-${locale.code}.json`,prefix:'sdk',locale:locale.code})),{base:work,locales});
const pages=[...annotations,...imported];
const originals=new Map(pages.filter(p=>p.locale==='en').map(p=>[p.id,p]));
for(const page of pages) {
  const original=originals.get(page.id);
  if(!original || !pages.some(p=>p.id===page.id&&p.locale==='fr'))throw Error('Incomplete public API translation: '+page.id);
  const revision=createHash('sha256').update(original.title+'\n'+original.markdown).digest('hex').slice(0,16);
  const parent=page.id==='sdk-abi'?'plugins-bridge':page.title.startsWith('SxUi::')?'sdk-ui':page.title.startsWith('Panel.')||page.title.startsWith('shaderixel_client.')?'script-api':'sdk-host';
  const description=page.markdown.split('\n').find(line=>line.trim()) || page.title;
  const fields={id:page.id,title:page.title,description,section:'API',parent,kind:'api',symbol:page.symbol||page.title,order:250,[page.locale==='en'?'revision':'sourceRevision']:revision};
  const text='---\n'+Object.entries(fields).map(([k,v])=>`${k}: ${String(v).replaceAll('\n',' ')}`).join('\n')+'\n---\n\n'+page.markdown+'\n';
  await writeFile(join(work,'manual',page.locale,`generated-${page.id}.md`),text);
}
console.log(`CyDoc public API: ${originals.size} bilingual pages; declarations and examples only`);
