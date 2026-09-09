/* Actual browser adapter + real Wasm, with controlled event/frame timing. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const create = require('../build/web/no-escape/no-escape.js');
(async () => {
  const machine = await create();
  let now = 0, frame, admitted = [];
  const input = machine._no_escape_input;
  machine._no_escape_input = action => { admitted.push(action); return input(action); };
  class Element {
    constructor(id,action) { this.id=id; this.dataset={action}; this.listeners={}; this.hidden=true; }
    addEventListener(name,fn) { (this.listeners[name] ||= []).push(fn); }
    fire(name,event={}) { for(const fn of this.listeners[name] || []) fn(event); }
    closest() { return null; }
    click() { if(!this.disabled) this.fire('click'); }
  }
  const nodes=Object.fromEntries(['board','error','status','pause','replay','reset'].map(id=>[id,new Element(id)]));
  const directions=[1,2,3,4].map(action=>new Element('',String(action)));
  const buttons=[nodes.pause,nodes.replay,nodes.reset,...directions];
  nodes.board.getContext=()=>({createImageData:()=>({data:new Uint8ClampedArray(128*96*4)}),putImageData:()=>{}});
  const document=new Element(),window=new Element();
  document.getElementById=id=>nodes[id];
  document.querySelectorAll=selector=>selector==='button'?buttons:directions;
  await vm.runInNewContext(fs.readFileSync('web/no-escape/app.js','utf8'),{
    document,window,createNoEscape:async()=>machine,performance:{now:()=>now},requestAnimationFrame:fn=>{frame=fn;}
  });
  const key=(key,type='keydown',repeat=false)=>document.fire(type,{key,repeat,target:nodes.board,preventDefault(){}});
  const tick=(delta=16)=>{now+=delta;frame(now);assert.equal(nodes.error.hidden,true,nodes.error.textContent);};
  const reset=()=>{nodes.reset.click();admitted=[];};
  const replay=()=>{
    nodes.replay.click();
    tick();
    if(!(machine._no_escape_flags()&4)) {
      nodes.pause.click();
      const pausedReplay=machine.UTF8ToString(machine._no_escape_digest());
      tick(1000);assert.equal(machine.UTF8ToString(machine._no_escape_digest()),pausedReplay);
      nodes.pause.click();
    }
    for(let i=0;!(machine._no_escape_flags()&4)&&i<3000;i++)tick();
    assert.ok(machine._no_escape_flags()&4,'replay did not finish');
    assert.equal(machine._no_escape_rejected(),0);
  };
  // Sixty discrete taps delivered without a single animation frame.
  reset();
  const burst=Array.from({length:60},(_,i)=>[2,1,4,3][i%4]);
  for(const action of burst) directions[action-1].click();
  assert.deepEqual(admitted,burst);
  assert.equal(machine._no_escape_rejected(),0);
  // All recorded attempts were accepted, including the final queued boundary.
  assert.equal(Number(machine.UTF8ToString(machine._no_escape_digest()).split(' ')[1]),60);
  replay();
  reset();key('ArrowRight');
  for(let i=0;i<500;i++) key('ArrowRight','keydown',true);
  assert.deepEqual(admitted,[2],'OS repeats must not become queued moves');
  tick(249);assert.equal(admitted.length,1);
  tick(17);assert.deepEqual(admitted,[2,2]);
  tick(2000);assert.deepEqual(admitted,[2,2,2],'late frame must emit only one repeat');
  key('ArrowRight','keyup');tick(2000);assert.equal(admitted.length,3);
  replay();
  // Latest held direction wins; release resumes the previous direction.
  reset();key('ArrowRight');key('ArrowUp');tick(250);
  key('ArrowUp','keyup');tick(100);
  assert.deepEqual(admitted,[2,1,1,2]);
  window.fire('blur');tick(1000);assert.equal(admitted.length,4);
  nodes.pause.click();tick(1000);assert.equal(admitted.length,4,'blur cleared held state');
  key('ArrowLeft');nodes.pause.click();tick(1000);
  nodes.pause.click();tick(1000);assert.equal(admitted.length,5,'pause cleared held state');
  reset();tick(1000);assert.equal(admitted.length,0,'reset cleared held state');
  // Pause freezes the exact machine state and disables directional controls.
  nodes.pause.click();
  const paused=machine.UTF8ToString(machine._no_escape_digest());
  key('ArrowRight');directions[1].click();tick(1000);
  assert.deepEqual(admitted,[]);assert.equal(directions[1].disabled,true);
  assert.equal(machine.UTF8ToString(machine._no_escape_digest()),paused);
  nodes.pause.click();tick(1000);assert.deepEqual(admitted,[],'paused keys must not resume as held');
  assert.equal(directions[1].disabled,false);
  key('ArrowRight','keyup');key('ArrowRight');tick();assert.deepEqual(admitted,[2]);
  replay();assert.deepEqual(admitted,[2],'replay never submits live held input');
  // Sustained hold reaches the existing recording bound safely and replays.
  reset();key('ArrowRight');
  for(let i=0;!(machine._no_escape_flags()&1)&&i<3000;i++)tick();
  assert.ok(machine._no_escape_flags()&1,'recording limit not reached');
  assert.equal(machine._no_escape_rejected(),0);
  assert.equal(machine._no_escape_input_ready(),0);
  const count=admitted.length;tick(2000);assert.equal(admitted.length,count);
  replay();
  console.log('Browser input adapter passed with real Wasm: 60 same-frame taps, 500 OS repeats, paced hold, 2s frame stalls, direction changes, true pause in live play and replay, blur/reset, recording bound and exact replay; zero rejected moves.');
})().catch(error=>{console.error(error);process.exitCode=1;});
