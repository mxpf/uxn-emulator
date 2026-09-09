/* Shared console layout and real browser input for both published games. */
const assert = require('node:assert/strict');
const {execFileSync} = require('node:child_process');
const root = process.argv[2] || 'http://127.0.0.1:8766/';
const call = (...args) => execFileSync('agent-browser', ['--session','console-check',...args], {encoding:'utf8'});
const evaluate = source => {
  const response = JSON.parse(call('eval',source,'--json'));
  assert.equal(response.success,true,JSON.stringify(response.error));
  return response.data.result;
};
const position = () => evaluate(`(() => {
  const p=document.querySelector('canvas').getContext('2d').getImageData(0,0,128,96).data;
  let x=128,y=96,count=0;
  for(let i=0;i<p.length;i+=4) if(p[i]===220 && p[i+1]===92 && p[i+2]===54) {
    x=Math.min(x,(i/4)%128);y=Math.min(y,Math.floor(i/4/128));count++;
  }
  return [x/8,y/8,count];
})()`);
try {
  for(const route of ['tiny-neighbors/','no-escape/']) {
    call('open',new URL(route,root).href);
    call('wait','--load','networkidle');
    call('snapshot','-i');
    for(const [width,height] of [[320,568],[390,844],[760,900],[1440,1000]]) {
      call('set','viewport',String(width),String(height));
      call('eval','scrollTo(0,0)');
      const layout=evaluate(`(() => {
        const c=document.querySelector('canvas').getBoundingClientRect(),b=document.querySelector('.back').getBoundingClientRect();
        return {x:c.x,y:c.y,width:c.width,height:c.height,overflow:document.documentElement.scrollWidth>innerWidth,
          touch:getComputedStyle(document.querySelector('.touch')).display,keyboard:getComputedStyle(document.querySelector('.keyboard')).display,
          backBottom:b.bottom,pageHeight:document.documentElement.scrollHeight,
          targets:[...document.querySelectorAll('button')].filter(b=>b.getBoundingClientRect().width).every(b=>b.getBoundingClientRect().height>=44)};
      })()`);
      assert.deepEqual([layout.x,layout.y,layout.width,layout.height],[0,0,width,width*.75]);
      assert.equal(layout.overflow,false);
      assert.equal(layout.targets,true);
      assert.ok(Math.abs(layout.backBottom-layout.pageHeight)<1);
      assert.equal(layout.touch,width<=760?'flex':'none');
      assert.equal(layout.keyboard,width<=760?'none':'block');
      if(width===390) assert.equal(layout.backBottom,height);
      if(width===390 || width===1440) call('screenshot',`build/${route.slice(0,-1)}-${width}.png`,'--full');
    }
    call('set','viewport','390','844');
    const center=evaluate(`(() => {const b=document.querySelector('[data-action="2"]').getBoundingClientRect();return [b.x+b.width/2,b.y+b.height/2];})()`);
    call('mouse','move',...center.map(n=>String(Math.round(n))));call('mouse','down');call('wait','120');
    assert.equal(evaluate('getComputedStyle(document.querySelector(".right")).transform'),'matrix(1, 0, 0, 1, 0, 5)');
    call('screenshot',`build/${route.slice(0,-1)}-pressed.png`);
    call('mouse','up');call('wait','120');
    assert.equal(evaluate('getComputedStyle(document.querySelector(".right")).transform'),'none');
    if(route==='no-escape/') {
      // Pause must freeze movement, including arrow keys, until resumed.
      evaluate('document.querySelector("#reset").click();document.querySelector("#pause").click()');
      assert.deepEqual(position(),[8,6,64]);
      assert.equal(evaluate('document.querySelector(".right").disabled'),true);
      call('focus','canvas');call('press','ArrowRight');call('wait','150');assert.deepEqual(position(),[8,6,64]);
      call('click','#pause');
      call('click','[data-action="2"]');call('wait','100');assert.deepEqual(position(),[9,6,64]);
      call('click','[data-action="1"]');call('wait','100');assert.deepEqual(position(),[9,5,64]);
      call('focus','canvas');call('press','ArrowLeft');call('wait','100');assert.deepEqual(position(),[8,5,64]);
      call('press','ArrowDown');call('wait','100');assert.deepEqual(position(),[8,6,64]);
      call('click','#replay');call('wait','--fn','document.querySelector("#status").textContent.includes("Replay verified")');
      assert.deepEqual(position(),[8,6,64]);
      assert.equal(evaluate('document.querySelector(".right").disabled'),true);
      call('click','#reset');
      evaluate('window.dispatchEvent(new Event("blur"))');
      assert.equal(evaluate('document.querySelector("#pause").textContent'),'Resume');
      const paused=position();call('wait','100');assert.deepEqual(position(),paused);
      call('set','viewport','1440','1000');call('focus','canvas');call('press','ArrowRight');
      assert.deepEqual(position(),[8,6,64]);
      call('press','p');call('press','ArrowRight');call('wait','100');
      assert.deepEqual(position(),[9,6,64]);
      call('press','r');call('wait','--fn','document.querySelector("#status").textContent.includes("Replay verified")');
      call('press','n');assert.equal(evaluate('document.querySelector("#replay").disabled'),false);
      // Real page handlers under a same-frame burst and blocked main thread.
      evaluate(`(() => {
        document.querySelector('#reset').click();
        for(let i=0;i<60;i++) document.querySelector('[data-action="'+[2,1,4,3][i%4]+'"]').click();
        const board=document.querySelector('canvas');
        board.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowRight',bubbles:true}));
        for(let i=0;i<500;i++) board.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowRight',repeat:true,bubbles:true}));
        const end=performance.now()+300;while(performance.now()<end) {}
        board.dispatchEvent(new KeyboardEvent('keyup',{key:'ArrowRight',bubbles:true}));
      })()`);
      call('wait','300');assert.deepEqual(position(),[9,6,64]);
      assert.doesNotMatch(evaluate('document.querySelector("#status").textContent'),/rejected/);
      call('click','#replay');call('wait','--fn','document.querySelector("#status").textContent.includes("Replay verified")');
      assert.deepEqual(position(),[9,6,64]);
    }
    assert.equal(evaluate('document.querySelector("#error").hidden'),true);
    call('click','.back');assert.equal(evaluate('location.pathname'),new URL(root).pathname);
  }
  const errors=JSON.parse(call('errors','--json'));
  assert.equal(errors.success,true);assert.deepEqual(errors.data.errors,[]);
  console.log('Both console interfaces passed: four widths, flush full-width 4:3 screen, mobile/desktop controls, bottom back navigation, 44px targets, real press/release depression; No Escape! touch/keyboard movement, exact canvas pixels, replay, reset, blur/pause, same-frame bursts and delayed frames without rejected moves; no page errors.');
} finally {call('close');}
