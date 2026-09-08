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
  assert.equal(evaluate('document.querySelector(".cartridge")'), null);
  assert.deepEqual(evaluate('Array.from(document.querySelectorAll(".box-cover img"), i=>[i.getAttribute("src"),i.naturalWidth,i.naturalHeight])'), [['tiny-neighbors-cover.png',1024,1536]]);
  for(const width of [320,393,540,768,1440]) {
    call('set','viewport',String(width),'900');
    assert.equal(evaluate('document.documentElement.scrollWidth <= innerWidth'), true, `overflow at ${width}`);
    assert.equal(evaluate('document.querySelector(".start").getBoundingClientRect().height >= 44'),true);
    assert.equal(evaluate('document.querySelector(".game-copy").scrollWidth <= document.querySelector(".game-copy").clientWidth'),true,`clipped title at ${width}`);
    assert.equal(evaluate('(()=>{const cover=document.querySelector(".box-cover img").getBoundingClientRect(); const screen=document.querySelector(".screen").getBoundingClientRect(); return cover.left>=screen.left && cover.right<=screen.right && Math.abs(cover.height/cover.width-1.5)<0.01;})()'),true,`cropped or distorted cover at ${width}`);
  }
  call('set','viewport','393','852');
  call('focus','.start');
  call('press','Enter');
  call('wait','--load','networkidle');
  assert.match(call('get','url'), /\/tiny-neighbors\//);
  assert.equal(evaluate('document.querySelector("#digest").textContent'), '0 4bd10eb037a4d2c2 2b59ad8bac17b3ad');
  for(const action of [2,2,2,2,2,3,3,3,3,5]) call('click',`[data-action="${action}"]`);
  assert.match(evaluate('document.querySelector("#objective").textContent'), /new friend/);
  // Download links must resolve from the nested game route.
  const downloads = evaluate(`(async()=>Promise.all(Array.from(document.querySelectorAll('a[download]'),async a=>{
    const response=await fetch(a.href); return [response.status,(await response.arrayBuffer()).byteLength];
  })))()`);
  assert.deepEqual(downloads,[[200,285],[200,969]]);
  call('click','.home-link');
  call('wait','--load','networkidle');
  assert.match(evaluate('document.title'),/Playinghaus/);
  call('click','a[href="#about"]');
  assert.match(call('get','url'),/#about/);
  call('click','nav a[href="#library"]');
  assert.match(call('get','url'),/#library/);
  const errors=JSON.parse(call('errors','--json'));
  assert.equal(errors.success,true);
  assert.deepEqual(errors.data.errors,[]);
  console.log('Playinghaus passed: five widths, static homepage, painted cover art, keyboard launch, game encounter, ROM downloads, return navigation, section links, no browser errors.');
} finally { call('close'); }
