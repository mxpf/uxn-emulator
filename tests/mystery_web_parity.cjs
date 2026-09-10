const assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const create=require('../build/mystery-check/mystery.js');
let script='0r.'+'912345967899669r'+'.'.repeat(14)+'0'+ '4'.repeat(128)+'4r'+'.'.repeat(128)+'r'+'.'.repeat(128);
for(const followup of ['', 'q'])for(let who=0;who<4;who++)for(let how=0;how<4;how++)for(let what=0;what<4;what++){
 const actions='q12345'+followup+'6'.repeat(who)+'7'.repeat(how)+'8'.repeat(what)+'9';
 script+='0'+actions+'r'+'.'.repeat(actions.length);
}
script+='0abcdmxr.....';
for (const actions of ['qf4qfq1235666789889q', 'q4fq12356667889', 'f4f'])
 script+='0'+actions+'r'+'.'.repeat(actions.length)+'r.0';
(async()=>{
 const native=spawnSync('./build/mystery_probe',[],{input:script,encoding:'utf8'});assert.equal(native.status,0,native.stderr);
 const lines=native.stdout.trim().split('\n'),m=await create();assert.equal(lines.length,script.length);
 for(let i=0;i<script.length;i++){
  const result=m._mystery_test_op(script.charCodeAt(i));
  assert.equal(`${result} ${m.UTF8ToString(m._mystery_digest())}`,lines[i],`checkpoint ${i}: ${script[i]}`);
 }
 console.log(`Mystery native/Wasm parity: ${script.length} canonical full-state, pixel, queue and trace checkpoints match, including replay and recording bounds.`);
})().catch(e=>{console.error(e);process.exitCode=1;});
