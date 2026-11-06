import { build } from 'esbuild';
import { readFile, writeFile, copyFile, mkdir } from 'node:fs/promises';
import { resolve, join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
const here = dirname(fileURLToPath(import.meta.url));
const project = resolve(process.argv[2]); const output = join(project, 'help');
const options = JSON.parse(await readFile(join(project, 'shaderixel-options.json'), 'utf8'));
await build({ entryPoints:[join(here,'reader.mjs')], bundle:true, format:'iife', minify:true,
  target:'es2020', outfile:join(output,'shaderixel-viewer.js'), legalComments:'eof' });
await copyFile(join(here,'reader.html'), join(output,'index.html'));
await writeFile(join(output,'shaderixel-options.js'), 'globalThis.SHADERIXEL_HELP='+JSON.stringify(options).replaceAll('<','\\u003c')+';\n');
await writeFile(join(output,'shaderixel.css'), '.shaderixel-download{display:inline-block;padding:.55rem .9rem;border:1px solid currentColor;border-radius:.5rem}.graph-stage{max-width:100%}\n');
await mkdir(join(output,'licenses'), {recursive:true});
await copyFile(resolve(here,'../../LICENSE'),join(output,'licenses/shaderixel.txt'));
for (const [pkg,file] of [['cydoc','LICENSE'],['marked','LICENSE.md'],['prismjs','LICENSE'],['esbuild','LICENSE.md']]) {
  const packageRoot = dirname(fileURLToPath(import.meta.resolve(pkg==='cydoc'?'cydoc/compiler':pkg)));
  // esbuild/prism/marked entry points are one or two levels below their package root.
  let directory=packageRoot, license;
  for(let i=0;i<4;i++) { try { license=await readFile(join(directory,file)); break; } catch { directory=dirname(directory); } }
  if(!license) throw new Error('Missing third-party notice: '+pkg);
  await writeFile(join(output,'licenses',pkg+'.txt'),license);
}
