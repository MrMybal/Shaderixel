// Shaderixel host for the public CyDoc viewer. All files and readers are local.
import { mountViewer } from 'cydoc/viewer';
import { parseLink } from 'cydoc';

const options = globalThis.SHADERIXEL_HELP;
const generated = globalThis.CYDOC_OPTIONS;
const pending = new Map();
// CyDoc's generated classic-script providers avoid file:// fetch/CORS rules.
function loadAsset(src) {
  if (pending.has(src)) return pending.get(src);
  const file = generated.assets?.[src];
  if (!file) return Promise.reject(new Error(`Local asset is not packaged: ${src}`));
  const promise = new Promise((resolve, reject) => {
    const script = document.createElement('script'); script.src = file;
    const clean = () => { pending.delete(src); script.remove(); };
    script.onload = () => {
      const asset = globalThis.CYDOC_ASSETS?.[src];
      if (globalThis.CYDOC_ASSETS) delete globalThis.CYDOC_ASSETS[src];
      clean(); asset ? resolve(asset) : reject(new Error(`Invalid local asset: ${src}`));
    };
    script.onerror = () => { clean(); reject(new Error(`Cannot load local asset: ${src}`)); };
    document.head.append(script);
  });
  pending.set(src, promise); return promise;
}
const modules = Object.fromEntries(generated.modules.map(name => [name, () => new Promise((resolve, reject) => {
  const script = document.createElement('script'); script.src = `media-${name}.js`;
  script.onload = () => resolve(globalThis[`CyDocMedia_${name}`].default);
  script.onerror = () => { script.remove(); reject(new Error(`Cannot load media-${name}.js`)); };
  document.head.append(script);
})]));

// Previous releases also accepted a bare #chapter ID. CyDoc itself handles
// #en/old_id and aliases, including encoded effect/function names.
if (location.hash && !location.hash.includes('/')) {
  const old = decodeURIComponent(location.hash.slice(1));
  if (options.aliases[old] || globalThis.CYDOC_DATA.pages.some(page => page.id === old))
    history.replaceState(null, '', '#/en/'+encodeURIComponent(old));
}
let savedTheme;
try { savedTheme = localStorage.getItem('cydoc:theme'); } catch { /* CyDoc reports unavailable reading storage. */ }
const root = document.querySelector('#app');
const reader = mountViewer(root, globalThis.CYDOC_DATA, {
  homePage: generated.homePage, initialTheme: savedTheme || options.theme,
  storageId: options.storageId, modules, loadAsset
});
// In the previous manual F1 retained the current article. Prevent the browser's
// own online-help shortcut while keeping this fully local reader in place.
addEventListener('keydown', event => { if (event.key === 'F1') event.preventDefault(); });

const initializedNavigation=new Set();
const navigationPage=new Map();
let focusedMediaRoute='', cancelledMediaRoute='', currentMediaRoute='';
for(const type of ['wheel','pointerdown']) addEventListener(type,()=>{cancelledMediaRoute=location.hash},{passive:true});
addEventListener('keydown',event=>{if(['ArrowUp','ArrowDown','PageUp','PageDown','Home','End',' '].includes(event.key))cancelledMediaRoute=location.hash});
function downloads() {
  const route = reader.canonicalRoute(parseLink(location.hash, document.documentElement.lang || 'en', generated.homePage));
  const activeGroup=root.querySelector('.nav-group [aria-current="page"]')?.closest('.nav-group');
  if(!initializedNavigation.has(route.locale)) {
    for(const group of root.querySelectorAll('.nav-group')) {
      const label=group.querySelector('summary')?.textContent;
      group.open=activeGroup ? group===activeGroup : ['Handbook','Manuel'].includes(label);
    }
    initializedNavigation.add(route.locale);
  }
  if(navigationPage.get(route.locale)!==route.pageId && activeGroup) activeGroup.open=true;
  navigationPage.set(route.locale,route.pageId);
  // A late module mount can enlarge a short end-of-page placeholder after the
  // initial anchor scroll was clamped. Apply this explicit media route once the
  // reader has its actual size; ordinary reading positions remain CyDoc-owned.
  const key=location.hash;
  if(currentMediaRoute!==key) {currentMediaRoute=key;focusedMediaRoute='';cancelledMediaRoute='';}
  if(route.state?.media && focusedMediaRoute!==key && cancelledMediaRoute!==key) {
    const media=[...root.querySelectorAll('[data-cydoc-id]')].find(node=>node.dataset.cydocId===route.state.media);
    if(media?.classList.contains('cy-media')) {
      focusedMediaRoute=key;
      requestAnimationFrame(()=>{
        if(location.hash!==key || cancelledMediaRoute===key || !media.isConnected)return;
        const bounds=media.getBoundingClientRect();
        if(bounds.top<76 || bounds.bottom>innerHeight)media.scrollIntoView({block:'center',behavior:'instant'});
      });
    }
  }
    const legal = root.querySelector('.footer > div > span');
    if (legal && !legal.hasAttribute('data-shaderixel-legal')) {
      legal.dataset.shaderixelLegal = '';
      const license=document.createElement('a');license.href='licenses/shaderixel.txt';
      license.textContent='Shaderixel · GPL-3.0';legal.replaceChildren(license);
    }
    const badge=root.querySelector('.mit-tag');
    if(badge && badge.textContent!=='CyDoc · MIT') badge.textContent='CyDoc · MIT';
  const item = options.downloads[route.pageId];
  const section = root.querySelector('article #download, article #source-file');
  if (!item || !section || section.parentElement.querySelector('[data-shaderixel-download]')) return;
  const link = document.createElement('a');
  link.dataset.shaderixelDownload = ''; link.className = 'shaderixel-download';
  link.href = item.src; link.download = item.filename;
  link.textContent = `${route.locale.startsWith('fr') ? 'Télécharger' : 'Download'} ${item.filename}`;
  const p = document.createElement('p'); p.append(link); section.after(p);
}
const observer = new MutationObserver(downloads);
observer.observe(root, { childList: true, subtree: true }); downloads();
addEventListener('hashchange',downloads);
addEventListener('pagehide', event => { if(!event.persisted) {observer.disconnect();reader.destroy();} });
