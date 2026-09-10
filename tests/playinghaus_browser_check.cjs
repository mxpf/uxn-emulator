/* Looping carousel, responsive layout and homepage-to-game navigation. */
const assert = require('node:assert/strict');
const {execFileSync} = require('node:child_process');
const {checkTypography} = require('./viewport_fit_check.cjs');
const url = process.argv[2] || 'http://127.0.0.1:8765/';
const call = (...args) => execFileSync('agent-browser', ['--session','playinghaus-check',...args], {encoding:'utf8'});
const evaluate = source => {
  const response = JSON.parse(call('eval', source, '--json'));
  assert.equal(response.success, true, JSON.stringify(response.error));
  return response.data.result;
};
const current = '[data-current]';
const selected = () => evaluate('document.querySelector("[data-current]").getAttribute("href")');
const settle = () => call('wait','--fn','!document.querySelector(".carousel").hasAttribute("data-moving")');
const choose = route => {
  for(let i=0;i<3 && selected()!==route;i++) {call('click','.next');settle();}
  assert.equal(selected(),route);
};
const trackX = () => evaluate('new DOMMatrix(getComputedStyle(document.querySelector(".cartridge-track")).transform).m41');
try {
  call('open', url);
  call('wait','--load','networkidle');
  assert.match(evaluate('document.title'), /Playinghaus/);
  call('snapshot','-i');
  checkTypography(evaluate);
  assert.equal(evaluate('document.querySelector("nav, footer, .console, .about, .box-cover, .game-copy, .start")'), null);
  assert.equal(evaluate('getComputedStyle(document.documentElement).backgroundColor'), 'rgb(255, 255, 255)');
  assert.deepEqual(evaluate('Array.from(document.querySelectorAll(".cartridge-link:not([data-clone])"), a=>a.getAttribute("href"))'), ['tiny-neighbors/','no-escape/','sketchpad/']);
  call('wait','--fn','[...document.images].every(i=>i.complete && i.naturalWidth>0)');
  assert.deepEqual(evaluate('Array.from(document.querySelectorAll(".brand img, .cartridge-link:not([data-clone]) img"), i=>[i.getAttribute("src"),i.naturalWidth,i.naturalHeight])'), [['playinghaus-chrome-arc.png',1942,809],['tiny-neighbors-cartridge.png',1122,1402],['no-escape-cartridge.png',1122,1402],['sketchpad-cartridge.png',1122,1402]]);
  // Desktop drift stops exactly on hover and focus, then resumes on exit.
  call('set','viewport','1440','900');call('mouse','move','10','10');
  evaluate('new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)))');
  let x=trackX();call('wait','--fn',`new DOMMatrix(getComputedStyle(document.querySelector('.cartridge-track')).transform).m41 < ${x-8}`);
  call('hover',current);x=trackX();call('wait','500');assert.equal(trackX(),x,'hover must stop drift');
  call('mouse','move','10','10');x=trackX();call('wait','--fn',`new DOMMatrix(getComputedStyle(document.querySelector('.cartridge-track')).transform).m41 < ${x-8}`);
  call('focus',current);x=trackX();call('wait','500');assert.equal(trackX(),x,'keyboard focus stops drift');
  call('click','.rotation');call('focus','.brand');call('mouse','move','10','10');
  x=trackX();call('wait','500');assert.equal(trackX(),x,'explicit pause persists outside carousel');
  for(const width of [320,393,540,768,1440]) {
    call('set','viewport',String(width),'900');
    assert.equal(evaluate('document.documentElement.scrollWidth <= innerWidth'), true, `overflow at ${width}`);
    assert.equal(evaluate('document.querySelector("[data-current]").getBoundingClientRect().height >= 44'),true);
    assert.equal(evaluate('(()=>{const image=document.querySelector("[data-current] .cartridge").getBoundingClientRect(); return image.left>=0 && image.right<=innerWidth && Math.abs(image.height/image.width-1402/1122)<0.01;})()'),true,`cropped or distorted cartridge at ${width}`);
    assert.equal(evaluate('(()=>{const r=document.querySelector("[data-current]").getBoundingClientRect();return Math.abs((r.left+r.right)/2-innerWidth/2)<1;})()'),true,`cartridge not centered at ${width}`);
    assert.equal(evaluate('[...document.querySelectorAll(".carousel-controls button")].filter(b=>!b.hidden).every(b=>b.offsetWidth>=44 && b.offsetHeight>=44)'),true);
    assert.equal(evaluate('[...document.querySelectorAll(".cartridge-link")].filter(a=>a.tabIndex===0).length'),1);
    if(width===393 || width===1440)call('screenshot',`build/carousel-${width}.png`);
  }
  const routes=['tiny-neighbors/','no-escape/','sketchpad/'];
  for(let i=1;i<=7;i++){call('click','.next');settle();assert.equal(selected(),routes[i%3]);}
  for(let i=1;i<=7;i++){call('click','.previous');settle();assert.equal(selected(),routes[(7-i)%3]);}
  call('focus',current);call('press','ArrowLeft');settle();assert.equal(selected(),'sketchpad/');
  assert.equal(evaluate('document.activeElement.hasAttribute("data-current")'),true);
  call('press','ArrowRight');settle();assert.equal(selected(),'tiny-neighbors/');
  call('hover',current);
  call('wait','300');
  assert.equal(evaluate('getComputedStyle(document.querySelector("[data-current] .cartridge")).transform'), 'matrix(1.035, 0, 0, 1.035, 0, 0)');
  call('click','.rotation');call('hover',current);
  call('set','media','reduced-motion');
  assert.equal(evaluate('getComputedStyle(document.querySelector("[data-current] .cartridge")).transform'), 'none');
  assert.equal(evaluate('getComputedStyle(document.querySelector(".cartridge")).transitionDuration'), '0s');
  assert.equal(evaluate('getComputedStyle(document.querySelector(".rotation")).display'),'none');
  call('focus','.brand');call('mouse','move','10','10');
  x=trackX();call('wait','500');assert.equal(trackX(),x,'reduced motion stops automatic movement');
  call('set','viewport','393','852');
  // A real pointer drag advances instead of accidentally launching the game.
  call('mouse','move','260','310');call('mouse','down');
  call('mouse','move','200','310');call('mouse','move','120','310');call('mouse','up');settle();
  assert.equal(selected(),'no-escape/');assert.equal(evaluate('location.pathname'),new URL(url).pathname);
  call('mouse','move','120','310');call('mouse','down');call('mouse','move','260','310');call('mouse','up');settle();
  assert.equal(selected(),'tiny-neighbors/');
  call('wait','450');
  call('focus',current);
  call('press','Enter');
  call('wait','--load','networkidle');
  assert.match(call('get','url'), /\/tiny-neighbors\//);
  assert.equal(evaluate('document.querySelector("#garden").dataset.digest'), '0 4bd10eb037a4d2c2 2b59ad8bac17b3ad');
  for(const action of [2,2,2,2,2,3,3,3,3,5]) call('click',`[data-action="${action}"]`);
  assert.match(evaluate('document.querySelector("#objective").textContent'), /new friend/);
  // The standalone ROM assets remain available without cluttering the game UI.
  const downloads = evaluate(`(async()=>Promise.all(['../garden-view.rom','../garden-world.rom'].map(async path=>{
    const response=await fetch(path); return [response.status,(await response.arrayBuffer()).byteLength];
  })))()`);
  assert.deepEqual(downloads,[[200,285],[200,969]]);
  call('click','.back');
  call('wait','--load','networkidle');
  assert.match(evaluate('document.title'),/Playinghaus/);
  call('click',current);
  call('wait','--load','networkidle');
  assert.match(call('get','url'), /\/tiny-neighbors\//);
  assert.equal(evaluate('document.querySelector("#garden").dataset.digest'), '0 4bd10eb037a4d2c2 2b59ad8bac17b3ad');
  call('click','.back');
  call('wait','--load','networkidle');
  choose('no-escape/');call('focus',current);
  call('press','Enter');
  call('wait','--load','networkidle');
  assert.match(call('get','url'), /\/no-escape\//);
  assert.equal(evaluate('document.querySelector("#error").hidden'),true);
  assert.equal(evaluate('document.querySelector("canvas").width'),128);
  call('click','.back');
  assert.match(evaluate('document.title'),/Playinghaus/);
  choose('sketchpad/');call('focus',current); call('press','Enter');
  call('wait','--fn',`document.querySelector('#status')?.textContent.includes('Move only')`);
  assert.match(call('get','url'),/\/sketchpad\//);
  call('click','[data-action="5"]'); call('click','[data-action="2"]');
  assert.equal(evaluate(`document.querySelector('[data-action="5"]').getAttribute('aria-pressed')`),'true');
  assert.deepEqual(evaluate(`Array.from(document.querySelector('canvas').getContext('2d').getImageData(69,49,1,1).data)`),[32,53,75,255]);
  call('click','.back'); call('wait','--load','networkidle');
  assert.match(evaluate('document.title'),/Playinghaus/);
  choose('sketchpad/');call('click',current);
  call('wait','--fn',`document.querySelector('#status')?.textContent.includes('Move only')`);
  assert.deepEqual(evaluate(`Array.from(document.querySelector('canvas').getContext('2d').getImageData(69,49,1,1).data)`),[244,241,233,255]);
  call('click','.back'); call('wait','--load','networkidle');
  call('screenshot','build/playinghaus-three-cartridges.png','--full');
  const errors=JSON.parse(call('errors','--json'));
  assert.equal(errors.success,true);
  assert.deepEqual(errors.data.errors,[]);
  console.log('Playinghaus passed: five widths, bidirectional seamless loops, slow desktop drift, hover/focus/persistent pause, mobile drag without accidental launch, keyboard navigation, hover zoom, reduced motion, all three cartridges launch, drawing and fresh return, ROM downloads; no browser errors.');
} finally { call('close'); }
