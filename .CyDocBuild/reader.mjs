// Public CyDoc embedding: preserve Shaderixel reading data and local project downloads.
import { mountViewer } from 'cydoc/viewer';
import { parseLink } from 'cydoc';
import { bindDownloads } from './resources.mjs';

const options=globalThis.CYDOC_OPTIONS;
const pending=new Map();
function loadAsset(src) {
  if(pending.has(src))return pending.get(src);
  const file=options.assets?.[src];
  if(!file)return Promise.reject(Error('Unpackaged local media: '+src));
  const promise=new Promise((resolve,reject)=>{
    const script=document.createElement('script');script.src='DocData/'+file;
    script.onload=()=>{const data=globalThis.CYDOC_ASSETS?.[src];delete globalThis.CYDOC_ASSETS?.[src];script.remove();pending.delete(src);data?resolve(data):reject(Error('Invalid packed media'));};
    script.onerror=()=>{script.remove();pending.delete(src);reject(Error('Local media could not load'));};
    document.head.append(script);
  });pending.set(src,promise);return promise;
}
const modules=Object.fromEntries(options.modules.map(name=>[name,()=>new Promise((resolve,reject)=>{
  const script=document.createElement('script');script.src=`DocData/media-${name}.js`;
  script.onload=()=>resolve(globalThis[`CyDocMedia_${name}`].default);
  script.onerror=()=>reject(Error('Local reader could not load: '+name));document.head.append(script);
})]));
let theme;try{theme=localStorage.getItem('cydoc:theme');}catch{}
if(location.hash && !location.hash.includes('/')) {
  const old=decodeURIComponent(location.hash.slice(1));
  if(globalThis.CYDOC_DATA.pages.some(p=>p.id===old)||globalThis.CYDOC_DATA.aliases?.[old])history.replaceState(null,'','#/en/'+encodeURIComponent(old));
}
const root=document.querySelector('#app');
const reader=mountViewer(root,globalThis.CYDOC_DATA,{homePage:options.homePage,initialTheme:theme||'midnight',storageId:'shaderixel-help',assetBase:'DocData/',modules,loadAsset});
function localDownloads() {
  const route=reader.canonicalRoute(parseLink(location.hash,'en',options.homePage));
  const entries=globalThis.SHADERIXEL_RESOURCES[route.locale+':'+route.pageId]||[];
  bindDownloads(root,entries);
}
const observer=new MutationObserver(localDownloads);observer.observe(root,{childList:true,subtree:true});localDownloads();
addEventListener('hashchange',localDownloads);
addEventListener('keydown',event=>{if(event.key==='F1')event.preventDefault();});
addEventListener('pagehide',event=>{if(!event.persisted){observer.disconnect();reader.destroy();}});
