const assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path'),crypto=require('node:crypto');
const {execFileSync,spawnSync}=require('node:child_process');
const session='mystery-rules-browser-'+process.pid;
const call=(...args)=>execFileSync('agent-browser',['--session',session,...args],{encoding:'utf8',timeout:45000});
const evaluate=js=>{const r=JSON.parse(call('eval',js,'--json'));assert.equal(r.success,true,JSON.stringify(r.error));return r.data.result;};
const text=()=>evaluate('document.querySelector("#narration").textContent');
const hash=()=>evaluate(`(async()=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',document.querySelector('canvas').getContext('2d').getImageData(0,0,256,192).data))).map(n=>n.toString(16).padStart(2,'0')).join(''))()`);
const temp=fs.mkdtempSync(path.join(os.tmpdir(),'mystery-rules-browser-'));fs.mkdirSync(path.join(temp,'build'));
let script='';
const native=()=>{
 const r=spawnSync(path.resolve('build/mystery_single_probe'),['--pixels',script],{cwd:temp,maxBuffer:1024*1024});
 assert.equal(r.status,0,String(r.stderr));assert.equal(hash(),crypto.createHash('sha256').update(r.stdout).digest('hex'));
};
const key=k=>{call('focus','canvas');call('press',k);script+=k;};
const clickQuestion=()=>{
 const point=evaluate(`(()=>{const r=document.querySelector('canvas').getBoundingClientRect();return [r.x+200*r.width/256,r.y+12*r.height/192]})()`);
 call('mouse','move',...point.map(n=>String(Math.round(n))));call('mouse','down');call('mouse','up');script+='q';
};
try {
 for(const [name,url,rom] of [
  ['returned',process.argv[2]||'http://127.0.0.1:8771/','build/mystery-single.rom'],
  ['retained',process.argv[3]||'http://127.0.0.1:8772/','build/mystery-single-retained.rom']
 ]) {
  fs.copyFileSync(rom,path.join(temp,'build/mystery-single.rom'));
  call('open',url);call('wait','--fn','!document.querySelector("#replay").disabled');call('snapshot','-i');
  for(const [w,h] of [[390,844],[1440,900]]) {
   call('set','viewport',String(w),String(h));call('click','#restart');script='';
   assert.match(text(),/Q LOCKED/);
   const locked=hash();key('q');assert.equal(hash(),locked);
   call('click','[data-action="4"]');script+='4';
   if(name==='retained') {
    assert.match(text(),/Q LOCKED/);const stillLocked=hash();key('q');assert.equal(hash(),stillLocked);
    call('click','[data-action="5"]');script+='5';
   }
   assert.match(text(),/Q ASK CORA/);native();
   clickQuestion();assert.match(text(),/Q ANSWERED/);
   assert.ok(text().includes(name==='returned' ? 'THE MISSING PAGE IS THERE' : 'I KEPT THE LEDGER PAGE'));
   native();const answered=hash();key('q');assert.equal(hash(),answered);
   for(const k of (name==='returned' ? '5123666789' : '12366678889')) key(k);
   assert.match(text(),/DOES NOT FIT/);native();
   for(const k of (name==='returned' ? '889' : '89')) key(k);
   assert.match(text(),/CASE CLOSED/);
   assert.ok(text().includes(name==='returned' ? '8 KEPT: NOTHING' : '8 KEPT: LEDGER PAGE'));native();
   const final=hash();call('click','#replay');
   call('wait','--fn',`document.querySelector('#status').textContent.startsWith('Replay verified')`);
   assert.equal(hash(),final);
   assert.ok(evaluate('document.documentElement.scrollHeight<=innerHeight && document.documentElement.scrollWidth<=innerWidth'));
   call('screenshot',`build/mystery-rules-${name}-${w}.png`);
   call('click','#replay');call('click','#restart');call('wait','650');script='';native();assert.match(text(),/Q LOCKED/);
  }
  assert.deepEqual(JSON.parse(call('errors','--json')).data.errors,[]);
  console.log(name+': browser unlock, changed outcome, duplicate handling, exact native pixels, replay/reset, desktop and mobile passed.');
 }
} finally { call('close'); }
