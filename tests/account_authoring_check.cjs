// Actual content edits in an isolated build tree; never replace live ROM files.
const fs=require('node:fs'),os=require('node:os'),path=require('node:path'),assert=require('node:assert/strict');
const {spawnSync}=require('node:child_process');
const root=process.cwd(),temp=fs.mkdtempSync(path.join(os.tmpdir(),'account-authoring-'));
fs.mkdirSync(path.join(temp,'examples'));fs.mkdirSync(path.join(temp,'build'));
for(const role of ['record','single','witness'])fs.copyFileSync('examples/account-'+role+'.tal',path.join(temp,'examples/account-'+role+'.tal'));
const config=JSON.parse(fs.readFileSync('examples/account.json'));
function pack(data,valid=true){
 fs.writeFileSync(path.join(temp,'examples/account.json'),JSON.stringify(data));
 const r=spawnSync(process.execPath,[path.join(root,'tools/build_account.cjs')],{cwd:temp,encoding:'utf8'});
 if(!valid){assert.notEqual(r.status,0);return;}
 assert.equal(r.status,0,r.stderr);
 const roms={};
 for(const role of ['record','single','witness']){
  const p='build/account-'+role+'.rom';
  const r=spawnSync(path.join(root,'bin/uxncli'),[path.join(root,'build/drifblim.rom'),'build/account-'+role+'.tal',p],{cwd:temp,encoding:'utf8'});
  assert.equal(r.status,0,r.stderr);roms[role]=fs.readFileSync(path.join(temp,p));
 }
 return roms;
}
const padded=b=>{const p=Buffer.alloc(65280);b.copy(p);return p;};
const base=pack(config);
const prose=structuredClone(config);prose.text['belief-kept']='BRAM: I THINK CORA KEPT IT.';
const changed=pack(prose);
// Direct text-label operands relocate when an earlier string changes length.
// No hand-written executable source changes, but assembled code bytes DO.
for(const r of ['record','single'])assert.notDeepEqual(padded(base[r]).subarray(0,0xe00),padded(changed[r]).subarray(0,0xe00));
assert.deepEqual(base.witness,changed.witness);
assert.notDeepEqual(base.record,changed.record);assert.notDeepEqual(base.single,changed.single);
for(const mode of ['multi','single']){
 const result=spawnSync(path.join(root,'bin/account-study'),[mode,'2'],{cwd:temp,encoding:'utf8'});
 assert.equal(result.status,0,result.stderr);assert.match(result.stdout,/BRAM: I THINK CORA KEPT IT/);assert.match(result.stdout,/Replay: exact/);
}
const prior=structuredClone(config);prior.initialBelief=1;
const initialized=pack(prior);
assert.deepEqual(base.record,initialized.record);
for(const r of ['witness','single']){
 assert.deepEqual(padded(base[r]).subarray(0,0xe00),padded(initialized[r]).subarray(0,0xe00));
 assert.notDeepEqual(base[r],initialized[r]);
}
for(const mode of ['multi','single']){
 const result=spawnSync(path.join(root,'bin/account-study'),[mode,'2'],{cwd:temp,encoding:'utf8'});
 assert.equal(result.status,0,result.stderr);
 assert.match(result.stdout,/DESK RECORD: CORA KEPT/);assert.match(result.stdout,/BRAM: THE PAGE IS BACK/);
}
for(const mutate of [d=>d.initialBelief=2,d=>d.text.shared='',d=>d.text.shared='X'.repeat(101),d=>d.text.shared='BAD ♥',d=>d.extra=1,d=>delete d.text.shared]){
 const bad=structuredClone(config);mutate(bad);pack(bad,false);
}
console.log('Authoring: prose revision relocates label operands in BOTH presentation executables, without source edits; initial-belief revision leaves executable bytes unchanged; both controls replay revised content; six malformed configurations rejected.');
for(const role of ['record','witness','single'])console.log(role+': '+base[role].length+' ROM file bytes.');
