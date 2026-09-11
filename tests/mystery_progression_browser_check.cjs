const assert=require('node:assert/strict'),{execFileSync,spawnSync}=require('node:child_process'),crypto=require('node:crypto');
const call=(...args)=>execFileSync('agent-browser',['--session',`progression-${process.pid}`,...args],{encoding:'utf8',timeout:45000});
const evaluate=js=>{const r=JSON.parse(call('eval',js,'--json'));assert.equal(r.success,true,JSON.stringify(r.error));return r.data.result;};
const text=()=>evaluate(`document.querySelector('#narration').textContent`);
const hash=()=>evaluate(`(async()=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',document.querySelector('canvas').getContext('2d').getImageData(0,0,256,192).data))).map(n=>n.toString(16).padStart(2,'0')).join(''))()`);
let script='',probe,comparisons=0;
const match=()=>{const r=spawnSync(probe,['--pixels',script],{maxBuffer:1024*1024});assert.equal(r.status,0);assert.equal(hash(),crypto.createHash('sha256').update(r.stdout).digest('hex'));comparisons++;};
const key=k=>{call('focus','canvas');call('press',k);script+=k;match();};
const button=k=>{call('click',`[data-action="${k==='q'?10:k}"]`);script+=k;match();};
try {
 for(const [variant,port] of [['multi',8776],['single',8777]]) {
  probe=`./build/mystery_progression_${variant}_probe`;
  call('open',`http://127.0.0.1:${port}/`);call('wait','--fn',`!document.querySelector('#replay').disabled`);call('snapshot','-i');
  for(const [w,h] of [[320,568],[390,844],[1440,900],[667,375]]) {
   call('set','viewport',String(w),String(h));
   const fit=evaluate(`(()=>({w:document.documentElement.scrollWidth,h:document.documentElement.scrollHeight,buttons:[...document.querySelectorAll('button,.back')].every(e=>{const r=e.getBoundingClientRect();return r.width>=44&&r.height>=44&&r.left>=0&&r.right<=innerWidth+.1&&r.top>=0&&r.bottom<=innerHeight+.1})}))()`);
   assert.ok(fit.w<=w&&fit.h<=h&&fit.buttons,JSON.stringify({w,h,fit}));
  }
  for(const [early,w,h] of [[true,390,844],[false,1440,900]]) {
   call('set','viewport',String(w),String(h));call('click','#restart');script='';match();
   button('q');assert.match(text(),/Q LOCKED/);
   for(const k of early?'123458':'4')button(k);
   const xy=evaluate(`(()=>{const r=document.querySelector('canvas').getBoundingClientRect();return [r.x+200*r.width/256,r.y+12*r.height/192]})()`);
   call('mouse','move',...xy.map(n=>String(Math.round(n))));call('mouse','down');call('mouse','up');script+='f';match();
   assert.match(text(),/8 WHY: \?/);assert.match(text(),/HIS CLOSING ROUND/);
   for(const k of early?'23':'1235')button(k);
   button('3');assert.match(text(),early?/MY RECEIPT SAYS LEDGER/:/MY RECEIPT SAYS SEALED INBOX/);
   call('screenshot',`build/progression-${variant}-${early?'early':'late'}.png`);
   for(const k of early?'666789':'6667889')key(k);
   assert.match(text(),/DOES NOT FIT/);
   for(const k of early?'89':'889')key(k);
   assert.match(text(),/THE TIMING FITS. EXIT OPEN/);
   const ending=hash();call('click','#replay');
   call('wait','--fn',`document.querySelector('#status').textContent.startsWith('Replay verified')`);assert.equal(hash(),ending);
   call('click','#restart');script='';match();
  }
  assert.deepEqual(JSON.parse(call('errors','--json')).data.errors,[]);
 }
 console.log(`${comparisons} native/browser pixel comparisons: both variants, early/late outcomes, wrong and revised deductions, real Q hotspot/button/keyboard input, replay/restart, four viewport sizes; no page errors.`);
} finally {call('close');}
