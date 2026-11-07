/* Progressive enhancement. The initial HTML, images and links also work offline. */
(() => {
  'use strict';
  const data = window.SHADERIXEL_SITE;
  if (!data) return;
  // Keep the old Pages entry points opening the actual CyDoc reader.
  if (/^#\/?(?:en|fr)\//.test(location.hash)) {
    location.replace('Documentation.html' + location.hash);
    return;
  }
  const $ = (selector, root = document) => root.querySelector(selector);
  const all = (selector, root = document) => [...root.querySelectorAll(selector)];
  let language = new URLSearchParams(location.search).get('lang') === 'fr' ? 'fr' : 'en';
  let mode = 'layers';
  const modes = {layers: ['layers', 'layers'], graph: ['image-graph', 'image-graph'], effect: ['effect-lab', 'effect-editor'], paint: ['paint3d', 'paint3d']};
  const tr = key => data.locales[language][key];
  const dialog = $('.lightbox');
  let trigger = null;
  let enlarged = null;

  function translate(root = document) {
    all('[data-i18n]', root).forEach(el => { el.textContent = tr(el.dataset.i18n); });
    all('[data-i18n-aria]', root).forEach(el => { el.setAttribute('aria-label', tr(el.dataset.i18nAria)); });
    all('[data-doc]', root).forEach(el => { el.href = `Documentation.html#/${language}/${el.dataset.doc}`; });
  }
  function image(button) {
    const key = button.dataset.media;
    const source = data.media[key][language];
    const img = $('img', button);
    img.src = source.src;
    img.width = source.width;
    img.height = source.height;
    img.alt = tr('media.' + key);
    button.setAttribute('aria-label', tr('zoom.open') + ': ' + img.alt);
  }
  function setMode(next, focus = false) {
    mode = next;
    all('[data-mode]').forEach(button => {
      const selected = button.dataset.mode === mode;
      button.setAttribute('aria-selected', String(selected));
      button.tabIndex = selected ? 0 : -1;
      if (focus && selected) button.focus();
    });
    $('#mode-panel').setAttribute('aria-labelledby', 'mode-' + mode);
    [['title', '#mode-title'], ['body', '#mode-description'], ['features', '#mode-features']].forEach(([part, selector]) => {
      $(selector).dataset.i18n = `mode.${mode}.${part}`;
    });
    const button = $('.mode-capture .image-button');
    button.dataset.media = modes[mode][0];
    $('.mode-info [data-doc]').dataset.doc = modes[mode][1];
    translate($('#mode-panel'));
    image(button);
  }
  function showImage(key) {
    enlarged = key;
    const source = data.media[key][language];
    const img = $('.lightbox-stage img');
    img.src = source.src;
    img.width = source.width;
    img.height = source.height;
    img.alt = tr('media.' + key);
    $('.lightbox-caption p').textContent = img.alt;
  }
  function setLanguage(next, updateUrl = true) {
    language = next;
    document.documentElement.lang = next;
    document.title = tr('meta.title');
    $('meta[name=description]').content = tr('meta.description');
    $('meta[property="og:title"]').content = tr('meta.title');
    $('meta[property="og:description"]').content = tr('meta.description');
    translate();
    all('[data-lang]').forEach(button => button.setAttribute('aria-pressed', String(button.dataset.lang === next)));
    all('.image-button[data-media]').forEach(image);
    if (dialog.open) showImage(enlarged);
    if (updateUrl) {
      const url = new URL(location.href);
      url.searchParams.set('lang', next);
      history.replaceState(null, '', url);
    }
  }
  all('[data-lang]').forEach(button => button.addEventListener('click', () => setLanguage(button.dataset.lang)));
  all('[data-mode]').forEach(button => {
    button.addEventListener('click', () => setMode(button.dataset.mode));
    button.addEventListener('keydown', event => {
      const names = Object.keys(modes), index = names.indexOf(mode);
      const next = {ArrowRight: (index + 1) % names.length, ArrowLeft: (index + names.length - 1) % names.length, Home: 0, End: names.length - 1}[event.key];
      if (next !== undefined) { event.preventDefault(); setMode(names[next], true); }
    });
  });
  all('[data-view]').forEach(button => button.addEventListener('click', () => {
    all('[data-view]').forEach(item => item.setAttribute('aria-pressed', String(item === button)));
    const target = $('.material-switch .image-button');
    target.dataset.media = button.dataset.view;
    image(target);
  }));
  all('.image-button').forEach(button => button.addEventListener('click', () => {
    trigger = button;
    showImage(button.dataset.media);
    dialog.showModal();
    document.body.classList.add('modal-open');
    $('.lightbox-close').focus();
  }));
  const close = () => dialog.close();
  $('.lightbox-close').addEventListener('click', close);
  dialog.addEventListener('click', event => {
    if (event.target !== dialog) return;
    const rect = dialog.getBoundingClientRect();
    if (event.clientX < rect.left || event.clientX > rect.right || event.clientY < rect.top || event.clientY > rect.bottom) close();
  });
  dialog.addEventListener('close', () => {
    document.body.classList.remove('modal-open');
    if (trigger) trigger.focus({preventScroll: true});
  });
  function stepImage(step) {
    const keys = Object.keys(data.media);
    showImage(keys[(keys.indexOf(enlarged) + step + keys.length) % keys.length]);
  }
  all('[data-image-step]').forEach(button => button.addEventListener('click', () => stepImage(Number(button.dataset.imageStep))));
  dialog.addEventListener('keydown', event => {
    if (event.key === 'ArrowLeft' || event.key === 'ArrowRight') { event.preventDefault(); stepImage(event.key === 'ArrowLeft' ? -1 : 1); }
  });
  const menu = $('.menu-button');
  function closeMenu() { menu.setAttribute('aria-expanded', 'false'); $('#header-links').classList.remove('is-open'); }
  menu.addEventListener('click', () => {
    const open = menu.getAttribute('aria-expanded') !== 'true';
    menu.setAttribute('aria-expanded', String(open));
    $('#header-links').classList.toggle('is-open', open);
  });
  all('.header-links a').forEach(link => link.addEventListener('click', closeMenu));
  document.addEventListener('keydown', event => {
    if (event.key === 'Escape' && menu.getAttribute('aria-expanded') === 'true') { closeMenu(); menu.focus(); }
  });
  const chapters = all('[data-chapter]');
  // Keep a chapter selected through its subsections, up to the next chapter.
  // Measure the sticky bars so the chapter at the reading position wins even
  // when a few pixels of the preceding section remain visible beneath them.
  let queued = false;
  function updateChapter() {
    const edge = $('.chapter-nav').getBoundingClientRect().bottom + 24;
    const current = [...chapters].reverse().find(link => document.getElementById(link.dataset.chapter).getBoundingClientRect().top <= edge);
    chapters.forEach(link => { if (link === current) link.setAttribute('aria-current', 'location'); else link.removeAttribute('aria-current'); });
    queued = false;
  }
  window.addEventListener('scroll', () => {
    if (!queued) { queued = true; requestAnimationFrame(updateChapter); }
  }, {passive: true});
  window.addEventListener('resize', updateChapter);
  setLanguage(language, false);
  updateChapter();
})();
