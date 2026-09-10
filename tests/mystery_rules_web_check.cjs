const fs=require('node:fs'),os=require('node:os'),path=require('node:path'),assert=require('node:assert/strict');
const {spawnSync}=require('node:child_process');
let script='0r.';
for(let subset=0;subset<32;subset++) {
 let actions='q';
 for(let a=1;a<=5;a++) if(subset & (1<<(a-1))) actions+=a;
 actions+='q666789889q';
 script+='0'+actions+'r'+'.'.repeat(actions.length);
}
script+='0q4q5qq12366678889qr..0';
script+='0'+'4'.repeat(128)+'4r'+'.'.repeat(128);
(async()=>{
 for(const [name,rom,module] of [
  ['returned','build/mystery-single.rom','../build/mystery-single-check/mystery.js'],
  ['retained','build/mystery-single-retained.rom','../build/mystery-retained-check/mystery.js']
 ]) {
  const temp=fs.mkdtempSync(path.join(os.tmpdir(),'mystery-rule-parity-'));
  fs.mkdirSync(path.join(temp,'build'));
  fs.copyFileSync(rom,path.join(temp,'build/mystery-single.rom'));
  const native=spawnSync(path.resolve('build/mystery_single_probe'),[],{input:script,cwd:temp,encoding:'utf8'});
  assert.equal(native.status,0,native.stderr);
  const lines=native.stdout.trim().split('\n'),m=await require(module)();
  assert.equal(lines.length,script.length);
  for(let i=0;i<script.length;i++) {
   const result=m._mystery_test_op(script.charCodeAt(i));
   assert.equal(`${result} ${m.UTF8ToString(m._mystery_digest())}`,lines[i],`${name} checkpoint ${i}: ${script[i]}`);
  }
  console.log(`${name}: ${script.length} native/Wasm full-state fingerprints match, including gating, outcomes, replay/reset and recording limits.`);
 }
})().catch(e=>{console.error(e);process.exitCode=1;});
