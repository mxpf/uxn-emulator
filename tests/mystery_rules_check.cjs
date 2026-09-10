// Both variants run the same native executable; only the ROM's data differs.
const fs=require('node:fs'),os=require('node:os'),path=require('node:path');
const assert=require('node:assert/strict'),crypto=require('node:crypto');
const {spawnSync}=require('node:child_process');
const root=process.cwd(),temp=fs.mkdtempSync(path.join(os.tmpdir(),'mystery-rules-'));
fs.mkdirSync(path.join(temp,'build'));fs.mkdirSync(path.join(temp,'examples'));
for(const f of ['mystery-single.tal','mystery-presentation.tal','mystery-room.json'])
 fs.copyFileSync(path.join(root,'examples',f),path.join(temp,'examples',f));
const a=fs.readFileSync('build/mystery-single.rom'),b=fs.readFileSync('build/mystery-single-retained.rom');
assert.deepEqual(a.subarray(0,0xe00),b.subarray(0,0xe00));
assert.deepEqual(a.subarray(0xe00,0xe0b),b.subarray(0xe00,0xe0b));
assert.deepEqual([...a.subarray(0xe0b,0xe0d)],[24,1]);
assert.deepEqual([...b.subarray(0xe0b,0xe0d)],[56,0]);
const digest=crypto.createHash('sha256').update(a.subarray(0,0xe00)).digest('hex');
console.log('Identical executable region SHA-256:',digest);
console.log('Only rule data and authored text resources differ; no native executable rebuild between variants.');
const compile=rule=>{
 fs.writeFileSync(path.join(temp,'examples/question.json'),JSON.stringify(rule));
 return spawnSync(process.execPath,[path.join(root,'tools/build_mystery.cjs'),'build/mystery-single.tal','examples/question.json'],{cwd:temp,encoding:'utf8'});
};
const rule=JSON.parse(fs.readFileSync('examples/mystery-question-returned.json'));
assert.equal(compile(rule).status,0);
const invalid=[
 r=>r.requiredFacts=0,r=>r.requiredFacts=64,r=>r.requiredFacts=-1,r=>r.requiredFacts=1.5,
 r=>r.requiredFacts='24',r=>delete r.requiredFacts,r=>r.solution='anything',
 r=>delete r.solution,r=>r.answerText='A'.repeat(41),r=>r.answerText='BAD ♥',
 r=>r.answerText=null,r=>r.jumpAddress=1234
];
for(const mutate of invalid) {const r=structuredClone(rule);mutate(r);assert.notEqual(compile(r).status,0);}
console.log(invalid.length+' malformed authored rule records rejected.');
const run=(rom,mode)=>{
 fs.writeFileSync(path.join(temp,'build/mystery-single.rom'),rom);
 const result=spawnSync(path.resolve(process.env.MYSTERY_RULE_TEST || './build/test_mystery_rules'),[mode],{cwd:temp,encoding:'utf8'});
 assert.equal(result.status,0,result.stderr+'\n'+result.stdout);
 process.stdout.write(result.stdout);
};
run(a,'returned');run(b,'retained');
for(const [offset,value] of [[0xe0b,0],[0xe0b,64],[0xe0c,2],[0xe0c,255]]) {
 const bad=Buffer.from(a);bad[offset]=value;run(bad,'reject');
}
console.log('Data-defined prerequisite and consequence checks passed for both variants.');
