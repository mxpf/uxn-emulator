const {execFileSync}=require('node:child_process'),assert=require('node:assert/strict');
const session='account-check-'+process.pid;
const call=(...a)=>execFileSync('agent-browser',['--session',session,...a],{encoding:'utf8',timeout:45000});
const value=js=>{const r=JSON.parse(call('eval',js,'--json'));assert.equal(r.success,true);return r.data.result;};
const text=()=>value('document.querySelector("#output").textContent');
try {
 call('open',process.argv[2]||'http://127.0.0.1:8773/');
 call('wait','--fn','!document.querySelector("#reset").disabled');call('snapshot','-i');
 for(const mode of ['multi','single']) {
  call('select','#variant',mode==='multi'?'0':'1');
  for(const [width,height]of [[390,844],[1440,900]]) {
   call('set','viewport',String(width),String(height));call('click','#reset');
   for(const n of '13242')call('click','[data-action="'+n+'"]');
   const native=execFileSync('./bin/account-study',[mode,'13242','--trace'],{encoding:'utf8'});
   const [expected,trace]=native.split('Replay: exact.\n');
   assert.equal(text(),expected);assert.equal(value('document.querySelector("#trace").textContent'),trace);
   assert.match(text(),/NOW BACK IN THE DRAWER\.\nBRAM: CORA STILL HAS THE PAGE/);
   assert.ok(text().endsWith('BRAM: THE PAGE IS BACK IN THE DRAWER.\n'));
   call('click','#replay');assert.match(value('document.querySelector("#status").textContent'),/Replay verified/);
   assert.equal(text(),expected);
   call('screenshot','build/account-'+mode+'-'+width+'.png');
   assert.ok(value('document.documentElement.scrollWidth<=innerWidth'));
   call('click','#reset');for(const n of '432')call('press',n);
   assert.ok(text().endsWith('BRAM: CORA STILL HAS THE PAGE.\n'));
  }
 }
 assert.deepEqual(JSON.parse(call('errors','--json')).data.errors,[]);
 console.log('Both account variants: desktop/mobile buttons, keyboard timing-order contrast, exact native transcript AND routed trace, reset and replay passed.');
}finally{call('close');}
