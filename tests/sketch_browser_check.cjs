const assert = require('node:assert/strict');
const {execFileSync} = require('node:child_process');
const call = (...args) => execFileSync('agent-browser', ['--session','sketch-verify',...args], {encoding:'utf8'});
const evaluate = code => {
  const answer = JSON.parse(call('eval',code,'--json'));
  assert.equal(answer.success, true, JSON.stringify(answer.error)); return answer.data.result;
};
const cell = (x,y) => evaluate(`Array.from(document.querySelector('canvas').getContext('2d').getImageData(${x*4+1},${y*4+1},1,1).data)`);
const mode = expected => {
  assert.deepEqual(evaluate(`[...document.querySelectorAll('.marks button')].map(b=>b.getAttribute('aria-pressed'))`),
    [String(expected===1),String(expected===2)]);
  assert.match(evaluate(`document.querySelector('#status').textContent`),[/Move only/,/Draw on/,/Erase on/][expected]);
};
try {
  const url = process.argv[2] || 'http://127.0.0.1:8766/sketchpad/';
  call('open',url);
  call('wait','--fn','document.querySelector("#status").textContent.includes("Move only")');
  call('snapshot','-i');
  for (const [width,height] of [[320,568],[390,844],[760,900],[1440,1000]]) {
    call('set','viewport',String(width),String(height));
    const layout = evaluate(`(() => {
      const c=document.querySelector('canvas').getBoundingClientRect();
      return {width:c.width,height:c.height,overflow:document.documentElement.scrollWidth>innerWidth,
        targets:[...document.querySelectorAll('button')].every(b=>b.getBoundingClientRect().width>=44 && b.getBoundingClientRect().height>=44)};
    })()`);
    assert.equal(layout.overflow,false); assert.equal(layout.targets,true);
    assert.equal(layout.height,layout.width*.75);
    if(width<=600) assert.equal(layout.width,width);
    if(width===390 || width===1440) call('screenshot',`build/sketch-${width}.png`,'--full');
  }
  call('set','viewport','390','844');
  mode(0);
  call('click','[data-action="5"]'); mode(1); assert.deepEqual(cell(16,12),[32,53,75,255]);
  call('click','[data-action="2"]'); mode(1);
  assert.deepEqual(cell(16,12),[32,53,75,255]); assert.deepEqual(cell(17,12),[32,53,75,255]);
  call('focus','canvas'); call('press','Space'); mode(0); assert.deepEqual(cell(17,12),[32,53,75,255]);
  call('press','ArrowRight'); assert.deepEqual(cell(18,12),[244,241,233,255]);
  call('press','x'); mode(2);
  call('press','ArrowLeft'); assert.deepEqual(cell(17,12),[244,241,233,255]);
  call('press','ArrowLeft'); assert.deepEqual(cell(16,12),[244,241,233,255]);
  call('press','x'); mode(0);
  // Space on a focused movement button performs that button's native action,
  // not an accidental draw in addition to its movement.
  call('focus','[data-action="2"]'); call('press','Space');
  assert.deepEqual(cell(16,12),[244,241,233,255]);
  call('focus','[data-action="5"]'); call('press','Enter');
  mode(1); assert.deepEqual(cell(17,12),[32,53,75,255]);
  call('focus','canvas'); call('press','x'); mode(2);
  call('press','ArrowLeft'); call('press','x'); mode(0);
  // Test the real page handlers against an independent paper model after
  // every operation, including boundaries and ignored OS key repeats.
  const verified = evaluate(`(() => {
    const canvas=document.querySelector('canvas'), ctx=canvas.getContext('2d'), paper=new Uint8Array(768);
    let x=16,y=12,mode=0,checks=0;
    const verify=()=>{
      if(document.querySelector('[data-action="5"]').getAttribute('aria-pressed')!==String(mode===1) ||
         document.querySelector('[data-action="6"]').getAttribute('aria-pressed')!==String(mode===2)) throw Error('Mode indicator mismatch');
      const actual=ctx.getImageData(0,0,128,96).data;
      for(let row=0;row<96;row++) for(let col=0;col<128;col++) {
        let color=paper[Math.floor(row/4)*32+Math.floor(col/4)]?[32,53,75]:[244,241,233];
        if(Math.floor(col/4)===x && Math.floor(row/4)===y && (col%4===0 || col%4===3 || row%4===0 || row%4===3)) color=[213,139,54];
        const offset=(row*128+col)*4;
        if(color.some((n,i)=>actual[offset+i]!==n) || actual[offset+3]!==255) throw Error('Pixel mismatch '+col+','+row+' at action '+checks);
      }
      checks++;
    };
    const apply=a=>{
      document.querySelector('[data-action="'+a+'"]').click();
      if(a===1)y=Math.max(0,y-1); if(a===2)x=Math.min(31,x+1);
      if(a===3)y=Math.min(23,y+1); if(a===4)x=Math.max(0,x-1);
      if(a===5)mode=mode===1?0:1; if(a===6)mode=mode===2?0:2;
      if(mode)paper[y*32+x]=mode===1?1:0;
      verify();
    };
    verify();
    for(let a=1;a<=4;a++) {for(let i=0;i<40;i++)apply(a);apply(5);}
    for(let i=0;i<30;i++) {apply(2);apply(5);apply(3);apply(6);}
    for(let i=0;i<500;i++)for(const key of ['ArrowRight',' ','x'])canvas.dispatchEvent(new KeyboardEvent('keydown',{key,repeat:true,bubbles:true}));
    verify(); window.dispatchEvent(new Event('blur')); verify();
    return checks;
  })()`);
  assert.equal(verified,287);
  // Check the tactile pressed state using real pointer input.
  call('scrollintoview','[data-action="5"]');
  const point = evaluate(`(() => {const r=document.querySelector('[data-action="5"]').getBoundingClientRect();return [Math.round(r.x+r.width/2),Math.round(r.y+r.height/2)];})()`);
  call('mouse','move',...point.map(String)); call('mouse','down');
  assert.equal(evaluate(`getComputedStyle(document.querySelector('[data-action="5"]')).transform`),'matrix(1, 0, 0, 1, 0, 5)');
  call('mouse','up');
  assert.equal(evaluate(`getComputedStyle(document.querySelector('[data-action="5"]')).transform`),'none');
  call('screenshot','build/sketch-drawing.png','--full');
  assert.equal(evaluate('document.querySelector("#error").hidden'),true);
  assert.ok(evaluate(`document.querySelector('.back').getBoundingClientRect().height`) >= 44);
  call('click','.back'); call('wait','--load','networkidle');
  assert.equal(evaluate('location.pathname'),new URL('../',url).pathname);
  assert.equal(evaluate(`document.querySelector('a[href="sketchpad/"]').getAttribute('aria-label')`),'Open Sketchpad');
  const errors=JSON.parse(call('errors','--json')); assert.equal(errors.success,true); assert.deepEqual(errors.data.errors,[]);
  console.log('Sketch browser passed: 287 full-pixel and toggle-state reference checks, draw/erase trails, toggle-off, mutually exclusive modes, keyboard/touch buttons, edge clamping, ignored repeats, native keyboard button activation, tactile depression and four responsive widths; no page errors.');
} finally { call('close'); }
