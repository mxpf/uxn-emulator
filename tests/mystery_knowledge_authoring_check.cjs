// Bounded authoring regression in a disposable copy; never edits live ROM inputs.
const fs=require('node:fs'),os=require('node:os'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=fs.mkdtempSync(path.join(os.tmpdir(),'last-visitor-authoring-'));
const names=['mystery-room.json','mystery-knowledge.json','mystery-presentation.tal','mystery-knowledge.tal','mystery-knowledge-local.tal','mystery-knowledge-remote.tal'];
try {
 for(const dir of ['examples','tools','build']) fs.mkdirSync(path.join(root,dir));
 for(const name of names) fs.copyFileSync('examples/'+name,path.join(root,'examples',name));
 fs.copyFileSync('tools/build_mystery.cjs',path.join(root,'tools/build_mystery.cjs'));
 const original=JSON.parse(fs.readFileSync('examples/mystery-knowledge.json','utf8'));
 const build=(mode,ok=true)=>{const r=spawnSync(process.execPath,['tools/build_mystery.cjs',`build/mystery-knowledge-${mode}.tal`],{cwd:root,encoding:'utf8'});assert.equal(r.status===0,ok,r.stderr);return ok?fs.readFileSync(path.join(root,`build/mystery-knowledge-${mode}.tal`),'utf8'):null;};
 const save=d=>fs.writeFileSync(path.join(root,'examples/mystery-knowledge.json'),JSON.stringify(d));
 for(const mode of ['multi','single']) {
  save(original);const before=build(mode);
  const changed=structuredClone(original);changed.messages[13]=changed.messages[13].replace('I HAVE HEARD NOTHING SINCE.','NOBODY HAS TOLD ME OTHERWISE.');save(changed);
  const after=build(mode);assert.equal(before.split('|0f00')[0],after.split('|0f00')[0]);assert.notEqual(before,after);
  for(const bad of [d=>d.messages[13]='X'.repeat(41),d=>d.deductionSolution=[3,1,4],d=>d.whyChoices[1]='X'.repeat(28),d=>d.questionLabels[2]='Q'.repeat(13)]) {
   const d=structuredClone(original);bad(d);save(d);build(mode,false);
  }
 }
 console.log('Both knowledge variants: prose-only revision leaves executable TAL unchanged; overlong text/choice/question and invalid solution rejected (8 negative builds).');
} finally {fs.rmSync(root,{recursive:true,force:true});}
