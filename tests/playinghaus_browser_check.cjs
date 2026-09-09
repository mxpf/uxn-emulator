/* Homepage-to-game navigation, responsive layout and static-only entry page. */
const assert = require('node:assert/strict');
const {execFileSync} = require('node:child_process');
const url = process.argv[2] || 'http://127.0.0.1:8765/';
const call = (...args) => execFileSync('agent-browser', ['--session','playinghaus-check',...args], {encoding:'utf8'});
const evaluate = source => {
  const response = JSON.parse(call('eval', source, '--json'));
  assert.equal(response.success, true, JSON.stringify(response.error));
  return response.data.result;
};
try {
  call('open', url);
  call('wait','--load','networkidle');
  assert.match(evaluate('document.title'), /Playinghaus/);
  assert.equal(evaluate('document.scripts.length'), 0);
  assert.equal(evaluate('Array.from(document.images).every(i=>i.complete && i.naturalWidth>0)'), true);
  assert.equal(evaluate('document.querySelector("nav, footer, .console, .about, .box-cover, .game-copy, .start, main p, button")'), null);
  assert.equal(evaluate('getComputedStyle(document.documentElement).backgroundColor'), 'rgb(255, 255, 255)');
  assert.deepEqual(evaluate('Array.from(document.querySelectorAll("main a"), a=>a.getAttribute("href"))'), ['tiny-neighbors/','no-escape/']);
  call('scrollintoview','a[href="no-escape/"]');
  call('wait','--load','networkidle');
  assert.deepEqual(evaluate('Array.from(document.querySelectorAll("img"), i=>[i.getAttribute("src"),i.naturalWidth,i.naturalHeight])'), [['playinghaus-chrome.png',1942,809],['tiny-neighbors-cartridge.png',1122,1402],['no-escape-cartridge.png',1122,1402]]);
  assert.equal(evaluate('(()=>{const r=[...document.querySelectorAll(".cartridge")].map(i=>i.getBoundingClientRect());return r[1].top>r[0].bottom;})()'),true);
  for(const width of [320,393,540,768,1440]) {
    call('set','viewport',String(width),'900');
    assert.equal(evaluate('document.documentElement.scrollWidth <= innerWidth'), true, `overflow at ${width}`);
    assert.equal(evaluate('document.querySelector(".cartridge-link").getBoundingClientRect().height >= 44'),true);
    assert.equal(evaluate('(()=>{const image=document.querySelector(".cartridge").getBoundingClientRect(); return image.left>=0 && image.right<=innerWidth && Math.abs(image.height/image.width-1402/1122)<0.01;})()'),true,`cropped or distorted cartridge at ${width}`);
    assert.equal(evaluate('(()=>{const r=document.querySelector(".cartridge").getBoundingClientRect();return Math.abs((r.left+r.right)/2-innerWidth/2)<1;})()'),true,`cartridge not centered at ${width}`);
  }
  call('hover','a[href="tiny-neighbors/"]');
  call('wait','300');
  assert.equal(evaluate('getComputedStyle(document.querySelector(".cartridge")).transform'), 'matrix(1.035, 0, 0, 1.035, 0, 0)');
  call('set','media','reduced-motion');
  assert.equal(evaluate('getComputedStyle(document.querySelector(".cartridge")).transform'), 'none');
  assert.equal(evaluate('getComputedStyle(document.querySelector(".cartridge")).transitionDuration'), '0s');
  call('set','viewport','393','852');
  call('focus','a[href="tiny-neighbors/"]');
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
  call('click','a[href="tiny-neighbors/"]');
  call('wait','--load','networkidle');
  assert.match(call('get','url'), /\/tiny-neighbors\//);
  assert.equal(evaluate('document.querySelector("#garden").dataset.digest'), '0 4bd10eb037a4d2c2 2b59ad8bac17b3ad');
  call('click','.back');
  call('wait','--load','networkidle');
  call('focus','a[href="no-escape/"]');
  call('press','Enter');
  call('wait','--load','networkidle');
  assert.match(call('get','url'), /\/no-escape\//);
  assert.equal(evaluate('document.querySelector("#error").hidden'),true);
  assert.equal(evaluate('document.querySelector("canvas").width'),128);
  call('click','.back');
  assert.match(evaluate('document.title'),/Playinghaus/);
  const errors=JSON.parse(call('errors','--json'));
  assert.equal(errors.success,true);
  assert.deepEqual(errors.data.errors,[]);
  console.log('Playinghaus passed: five widths, stacked cartridges, no copy or button, hover zoom, reduced motion, keyboard/click launch of both games, garden encounter, ROM downloads, return navigation, no browser errors.');
} finally { call('close'); }
