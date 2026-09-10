const assert=require('node:assert/strict'),{execFileSync,spawnSync}=require('node:child_process'),crypto=require('node:crypto');
const call=(...args)=>execFileSync('agent-browser',['--session',`mystery-check-${process.pid}`,...args],{encoding:'utf8',timeout:45000});
const evaluate=js=>{const r=JSON.parse(call('eval',js,'--json'));assert.equal(r.success,true,JSON.stringify(r.error));return r.data.result;};
const text=()=>evaluate(`document.querySelector('#narration').textContent`);
const hash=()=>evaluate(`(async()=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',document.querySelector('canvas').getContext('2d').getImageData(0,0,256,192).data))).map(n=>n.toString(16).padStart(2,'0')).join(''))()`);
const matchNative=script=>{const r=spawnSync(process.argv[3] || './build/mystery_probe',['--pixels',script],{maxBuffer:1024*1024});assert.equal(r.status,0);assert.equal(hash(),crypto.createHash('sha256').update(r.stdout).digest('hex'));};
const click=(x,y)=>{const point=evaluate(`(()=>{const r=document.querySelector('canvas').getBoundingClientRect();return [r.x+${x}*r.width/256,r.y+${y}*r.height/192]})()`);call('mouse','move',...point.map(n=>String(Math.round(n))));call('mouse','down');call('mouse','up');};
try{
 call('open',process.argv[2]||'http://127.0.0.1:8770/');call('wait','--fn',`!document.querySelector('#replay').disabled`);call('snapshot','-i');
 assert.match(text(),/ONE FALSE ACCOUNT/);matchNative('');
 for(const [w,h] of [[320,568],[390,844],[600,700],[1440,900],[1024,600],[667,375],[844,390]]){
  call('set','viewport',String(w),String(h));
  const layout=evaluate(`(()=>{const c=document.querySelector('canvas').getBoundingClientRect();return {width:document.documentElement.scrollWidth,height:document.documentElement.scrollHeight,ratio:c.width/c.height,canvas:c.width,targets:[...document.querySelectorAll('button,.back')].map(el=>{const r=el.getBoundingClientRect();return r.width>=44&&r.height>=44&&r.left>=0&&r.right<=innerWidth+.1&&r.top>=0&&r.bottom<=innerHeight+.1&&el.contains(document.elementFromPoint(r.x+r.width/2,r.y+r.height/2))})}})()`);
  assert.ok(layout.width<=w&&layout.height<=h,`${w}x${h} scrolls: ${JSON.stringify(layout)}`);assert.ok(Math.abs(layout.ratio-4/3)<.01);assert.ok(layout.canvas>=192);assert.ok(layout.targets.every(Boolean),JSON.stringify(layout));
  if(w===390||w===1440)call('screenshot',`build/mystery-${w}.png`);
 }
 call('set','viewport','390','844');

 // Regression: names and their padded lower edges must work, not only objects.
 for(const [w,h] of [[390,844],[1440,900]]) {
  call('set','viewport',String(w),String(h));
  for(const [x,y,a,phrase] of [[30,62,1,'ROUND SEAL'],[153,103,2,'NEXT GUEST'],[100,108,3,'NEVER ENTERED'],[24,108,4,'TRIANGLE-SEALED ENVELOPE'],[195,112,5,'LOCK IS INTACT']]) {
   call('click','#restart');click(x,y);assert.ok(text().includes(phrase),`Label ${a} at ${w}x${h}`);matchNative(String(a));
   call('click','#restart');call('click',`[data-action="${a}"]`);assert.ok(text().includes(phrase));matchNative(String(a));
  }
 }
 call('click','#restart');call('set','viewport','390','844');

 call('click','[data-action="9"]');assert.match(text(),/EVIDENCE IS MISSING/);matchNative('9');
 // Five actual canvas hotspots expose the six facts.
 let script='9';
 for(const [x,y,a,phrase] of [[30,50,1,'ROUND SEAL'],[150,70,2,'NEXT GUEST'],[100,60,3,'NEVER ENTERED'],[30,90,4,'TRIANGLE-SEALED ENVELOPE'],[200,50,5,'LOCK IS INTACT']]){
  click(x,y);script+=a;assert.ok(text().includes(phrase));matchNative(script);
 }
 call('focus','canvas');call('press','9');script+='9';assert.match(text(),/INCOMPLETE/);matchNative(script);
 // Native focused-button activation and keyboard cycle the same ROM inputs.
 call('focus','[data-action="6"]');call('press','Enter');script+='6';
 call('focus','canvas');call('press','7');script+='7';call('press','8');script+='8';
 call('press','9');script+='9';assert.match(text(),/DOES NOT FIT/);matchNative(script);
 assert.match(text(),/6 LAST: ADA/);assert.match(text(),/7 ENTRY: SUN KEY/);assert.match(text(),/8 KEPT: LEDGER PAGE/);
 call('press','6');call('press','6');script+='66';
 call('click','[data-action="9"]');script+='9';assert.match(text(),/CASE CLOSED/);matchNative(script);
 const final=hash();call('click','#replay');assert.equal(evaluate(`document.querySelector('[data-action="1"]').disabled`),true);
 call('wait','--fn',`document.querySelector('#status').textContent.startsWith('Replay verified')`);assert.equal(hash(),final);
 call('screenshot','build/mystery-ending.png');
 // A reset during replay cancels the old timer and starts a genuinely fresh room.
 call('click','#replay');call('click','#restart');call('wait','650');assert.match(text(),/ONE FALSE ACCOUNT/);matchNative('');
 // Q is cartridge-gated: early requests and duplicates do not repaint.
 for (const [w,h] of [[390,844],[1440,900]]) {
  call('set','viewport',String(w),String(h));call('click','#restart');
  assert.match(text(),/Q LOCKED/);
  const locked=hash();call('focus','canvas');call('press','q');click(200,12);
  assert.equal(hash(),locked);matchNative('qf');
  call('press','4');assert.match(text(),/Q ASK CORA/);matchNative('qf4');
  click(200,12);assert.match(text(),/Q ANSWERED/);
  assert.match(text(),/THE MISSING PAGE IS THERE/);matchNative('qf4f');
  const answered=hash();call('focus','canvas');call('press','q');click(200,12);
  assert.equal(hash(),answered);matchNative('qf4fqf');
  let branch='qf4fqf';
  for(const action of '1235666789'){call('press',action);branch+=action;}
  assert.match(text(),/DOES NOT FIT/);matchNative(branch);
  call('press','8');call('press','8');call('press','9');branch+='889';
  assert.match(text(),/CASE CLOSED/);assert.match(text(),/SHE KEPT NOTHING/);matchNative(branch);
  const revised=hash();call('click','#replay');
  call('wait','--fn',`document.querySelector('#status').textContent.startsWith('Replay verified')`);
  assert.equal(hash(),revised);
  call('screenshot',`build/mystery-followup-${w}.png`);
  call('click','#replay');call('click','#restart');call('wait','650');
  assert.match(text(),/Q LOCKED/);matchNative('');
 }
 const before=hash();click(0,190);assert.equal(hash(),before);
 assert.deepEqual(JSON.parse(call('errors','--json')).data.errors,[]);
 console.log('Mystery browser passed: real object clicks, keyboard and buttons, native pixel parity, discovery-gated follow-up, changed accepted conclusion and duplicate suppression, replay/reset, seven responsive sizes and no page errors.');
}finally{call('close');}
