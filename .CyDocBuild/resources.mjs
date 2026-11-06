export function bindDownloads(root, entries) {
  const article=root.querySelector('main article');if(!article)return;
  for(const entry of entries) {
    let found=false;
    for(const link of article.querySelectorAll('a'))if(link.textContent===entry.label) {
      if(link.getAttribute('href')!==entry.src) {
        link.setAttribute('href',entry.src);link.setAttribute('download',entry.filename);
      }
      found=true;break;
    }
    if(found)continue;
    // CyDoc's safe Markdown renderer preserves the label of unsupported local
    // links as text. Promote that exact reviewed label without inserting HTML.
    const document=root.ownerDocument,walker=document.createTreeWalker(article,4);
    for(let node=walker.nextNode();node;node=walker.nextNode()) {
      if(node.parentElement?.closest('a,code,pre,button'))continue;
      const index=node.textContent.indexOf(entry.label);if(index<0)continue;
      const link=document.createElement('a');link.textContent=entry.label;
      link.setAttribute('href',entry.src);link.setAttribute('download',entry.filename);
      node.replaceWith(document.createTextNode(node.textContent.slice(0,index)),link,document.createTextNode(node.textContent.slice(index+entry.label.length)));
      break;
    }
  }
}
