// A route can target a module whose script has not finished mounting yet.
// Focus that media once its real layout exists; later interactions retain scroll.
export function createRouteMediaFocus(root) {
  const view=root.ownerDocument.defaultView;
  let key='',pending=false,frame=0;
  function update(route) {
    const next=JSON.stringify([route.locale,route.pageId,route.anchor,route.state?.media]);
    if(next!==key) {
      if(frame)view.cancelAnimationFrame(frame);
      frame=0;key=next;pending=!!route.state?.media;
    }
    if(!pending||frame)return;
    const host=[...root.querySelectorAll('[data-cydoc-id]')].find(element=>element.dataset.cydocId===route.state.media);
    if(!host?.classList.contains('cy-media'))return;
    const requested=key;
    frame=view.requestAnimationFrame(()=>{
      frame=0;
      if(!pending||key!==requested||!root.contains(host))return;
      host.scrollIntoView({block:'center'});pending=false;
    });
  }
  return {update,destroy(){pending=false;if(frame)view.cancelAnimationFrame(frame);frame=0;}};
}
