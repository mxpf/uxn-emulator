// Disposable authored revision probe, not a new authoring language or live edit.
const fs=require('node:fs'),os=require('node:os'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const repo=process.cwd(),root=fs.mkdtempSync(path.join(os.tmpdir(),'last-visitor-progression-'));
const names=['mystery-room.json','mystery-progression.json','mystery-presentation.tal','mystery-progression.tal','mystery-progression-local.tal','mystery-progression-remote.tal'];
const run=(exe,args)=>{const r=spawnSync(exe,args,{cwd:root,encoding:'utf8'});assert.equal(r.status,0,r.stderr);return r.stdout.trim();};
const once=(s,a,b)=>{assert.equal(s.split(a).length,2,a);return s.replace(a,b);};
try {
 for(const dir of ['examples','tools','build'])fs.mkdirSync(path.join(root,dir));
 for(const name of names)fs.copyFileSync('examples/'+name,path.join(root,'examples',name));
 fs.copyFileSync('tools/build_mystery.cjs',path.join(root,'tools/build_mystery.cjs'));
 fs.copyFileSync('build/mystery-progression-desk.rom',path.join(root,'build/mystery-progression-desk.rom'));
 const original=JSON.parse(fs.readFileSync('examples/mystery-progression.json','utf8'));
 const save=d=>fs.writeFileSync(path.join(root,'examples/mystery-progression.json'),JSON.stringify(d));
 const build=(mode,ok=true)=>{const r=spawnSync(process.execPath,['tools/build_mystery.cjs',`build/mystery-progression-${mode}.tal`],{cwd:root,encoding:'utf8'});assert.equal(r.status===0,ok,r.stderr);return ok?fs.readFileSync(path.join(root,`build/mystery-progression-${mode}.tal`),'utf8'):null;};
 const assemble=mode=>run(path.join(repo,'bin/uxncli'),[path.join(repo,'build/drifblim.rom'),`build/mystery-progression-${mode}.tal`,`build/mystery-progression-${mode}.rom`]);
 for(const mode of ['multi','single']) {
  save(original);const before=build(mode);
  const changed=structuredClone(original);changed.messages[13]=changed.messages[13].replace('CORA REACHED ME.','CORA CAME BACK.');save(changed);
  const after=build(mode);assert.equal(before.split('|0f00')[0],after.split('|0f00')[0]);assert.notEqual(before,after);
  for(const bad of [d=>d.messages[13]='X'.repeat(41),d=>d.whyChoices[1]='X'.repeat(28),d=>d.questionLabels[1]='Q'.repeat(13)]) {
   const d=structuredClone(original);bad(d);save(d);build(mode,false);
  }
  save(original);build(mode);assemble(mode);
 }
 const probe=()=>run(path.join(repo,'build/test_mystery_progression'),['--revision-receipts']);
 assert.equal(probe(),'1\n1');
 // Bounded revision: Cora reaches Bram one local progression step sooner.
 // Change each implementation's own Cora threshold, not desk timing or routes.
 const localPath=path.join(root,'examples/mystery-progression-local.tal');
 fs.writeFileSync(localPath,once(fs.readFileSync(localPath,'utf8'),'#0c LDZ #03 NEQ ?cora-done','#0c LDZ #02 NEQ ?cora-done'));
 const remotePath=path.join(root,'examples/mystery-progression-remote.tal');
 let remote=fs.readFileSync(remotePath,'utf8');
 remote=once(remote,'#0c LDZ #02 GTH ?done','#0c LDZ #01 GTH ?done');
 remote=once(remote,'#0c LDZ #03 NEQ ?continue-cora','#0c LDZ #02 NEQ ?continue-cora');
 remote=once(remote,'#0c LDZ #03 NEQ ?done','#0c LDZ #02 NEQ ?done');
 fs.writeFileSync(remotePath,remote);
 for(const mode of ['multi','single']) {build(mode);assemble(mode);}
 assert.equal(probe(),'1\n2');
 console.log('Prose-only executable stability in both variants; 6 invalid text/label builds rejected. Earlier-return revision: multi still INBOX, single LEDGER. Existing incoming-route rotation delays delivery behind Bram continuation. Revision mismatch preserved as evidence; live fixture untouched.');
} finally {fs.rmSync(root,{recursive:true,force:true});}
