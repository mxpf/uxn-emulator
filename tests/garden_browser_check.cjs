/* Run against a served build/web directory. Uses the agent-browser CLI. */
const assert = require('node:assert/strict');
const {execFileSync} = require('node:child_process');
const url = process.argv[2] || 'http://127.0.0.1:8765/tiny-neighbors/';
const call = (...args) => execFileSync('agent-browser', ['--session','garden-check',...args], {encoding:'utf8'});
const evaluate = source => {
  const response = JSON.parse(call('eval',source,'--json'));
  assert.equal(response.success,true, JSON.stringify(response.error));
  return response.data.result;
};
try {
  call('open',url);
  call('wait','--load','networkidle');
  const native = execFileSync('./build/garden_native_digest',{encoding:'utf8'}).trim().split('\n');
  assert.equal(evaluate('document.querySelector("#digest").textContent'),native[0]);
  // All checkpoints in a real browser engine, not only Node's Wasm runtime.
  const actual = evaluate(`(async () => {
    const m=await createGarden(); m._garden_web_reset();
    const digest=()=>m.UTF8ToString(m._garden_web_digest());
    const rows=[digest()], greeting=[2,2,2,2,2,3,3,3,3,5]; let seed=42;
    for(let i=0;i<1010;i++) {
      seed=(Math.imul(seed,1664525)+1013904223)>>>0;
      if(!m._garden_web_step(i<10?greeting[i]:(seed>>>16)%6)) throw Error('step failed');
      rows.push(digest());
    }
    return rows;
  })()`);
  assert.deepEqual(actual,native);
  // Actual keyboard events through the page host; paused inputs step once.
  call('focus','#garden');
  for(const key of ['ArrowRight','ArrowRight','ArrowRight','ArrowRight','ArrowRight','ArrowDown','ArrowDown','ArrowDown','ArrowDown','Space']) call('press',key);
  assert.equal(evaluate('document.querySelector("#digest").textContent'),native[10]);
  assert.match(evaluate('document.querySelector("#objective").textContent'),/new friend/);
  // Check the displayed RGBA canvas, not just the C-side pixel buffer.
  const canvasHash = evaluate(`(() => {
    const bytes=document.querySelector('#garden').getContext('2d').getImageData(0,0,128,96).data;
    let h=14695981039346656037n;
    for(let i=0;i<bytes.length;i+=4) for(const j of [2,1,0,3]) h=BigInt.asUintN(64,(h^BigInt(bytes[i+j]))*1099511628211n);
    return h.toString(16).padStart(16,'0');
  })()`);
  assert.equal(canvasHash,native[10].split(' ')[2]);
  call('click','#reset');
  assert.equal(evaluate('document.querySelector("#digest").textContent'),native[0]);
  call('click','[data-action="2"]');
  assert.equal(evaluate('document.querySelector("#digest").textContent'),native[1]);
  call('click','#play');
  assert.equal(evaluate('document.querySelector("#step").disabled'),true);
  call('wait','600');
  assert.ok(Number(evaluate('document.querySelector("#digest").textContent.split(" ")[0]'))>1);
  evaluate('window.dispatchEvent(new Event("blur"))');
  const paused = evaluate('document.querySelector("#digest").textContent');
  call('wait','350');
  assert.equal(evaluate('document.querySelector("#digest").textContent'),paused);
  assert.equal(evaluate('document.querySelector("#play").textContent'),'Play');
  call('set','viewport','390','844');
  assert.equal(evaluate('document.documentElement.scrollWidth <= innerWidth'),true);
  assert.equal(evaluate('document.querySelector("#error").hidden'),true);
  const errors = JSON.parse(call('errors','--json'));
  assert.equal(errors.success,true);
  assert.deepEqual(errors.data.errors,[]);
  console.log('Browser passed: 1,011 parity checkpoints, canvas pixels, keyboard greeting, buttons, play, blur/pause, reset, mobile width, no page errors.');
} finally { call('close'); }
