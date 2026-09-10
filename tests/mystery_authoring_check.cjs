const fs=require('node:fs'),os=require('node:os'),path=require('node:path'),assert=require('node:assert/strict');
const {spawnSync}=require('node:child_process');
const root=process.cwd(),temp=fs.mkdtempSync(path.join(os.tmpdir(),'mystery-authoring-'));
fs.mkdirSync(path.join(temp,'examples'));fs.mkdirSync(path.join(temp,'build'));
for(const name of ['presentation','deduction','single']) fs.copyFileSync(`examples/mystery-${name}.tal`,path.join(temp,`examples/mystery-${name}.tal`));
const data=JSON.parse(fs.readFileSync('examples/mystery-room.json','utf8'));
const compile=(variant,target)=>{
 fs.writeFileSync(path.join(temp,'examples/mystery-room.json'),JSON.stringify(variant));
 return spawnSync(process.execPath,[path.join(root,'tools/build_mystery.cjs'),...(target?[target]:[])],{cwd:temp,encoding:'utf8'});
};
const revised=structuredClone(data);revised.messages[0]='RAIN TAPS AGAINST THE WINDOW.\nTHE STUDY IS LOCKED.';
assert.equal(compile(revised).status,0);
const assembled=spawnSync(path.join(root,'bin/uxncli'),[path.join(root,'build/drifblim.rom'),'build/mystery-presentation.tal','build/revised.rom'],{cwd:temp,encoding:'utf8'});
assert.equal(assembled.status,0,assembled.stderr);
const rom=fs.readFileSync(path.join(temp,'build/revised.rom')),original=fs.readFileSync('build/mystery-presentation.rom');
assert.deepEqual(rom.subarray(0,0xf00),original.subarray(0,0xf00));
assert.ok(original.subarray(0x700,0xf00).every(b=>b===0),'Presentation code must not overlap input/output buffers.');
assert.ok(rom.includes(Buffer.from(revised.messages[0])));
const mutations=[d=>d.messages.pop(),d=>d.followupSolution[2]=4,d=>d.questionLabels.pop(),d=>d.questionLabels[0]='A'.repeat(13),d=>d.solution[0]=0,d=>d.solution[0]=4,d=>d.evidenceMasks[0]=2,d=>d.choices[0].pop(),d=>d.labels[0]='TOO LONG LABEL',d=>d.messages[0]='A'.repeat(41),d=>d.messages[0]='BAD ♥',d=>d.rectangles[0].push(1),d=>d.rectangles[0][2]=255,d=>d.hotspots[0][4]=6];
for(const mutate of mutations){const invalid=structuredClone(data);mutate(invalid);assert.notEqual(compile(invalid).status,0);}
console.log('Mystery authoring passed: changed story text compiles with identical presentation code; invalid text, extents, record widths and actions rejected.');

const revisedRule=structuredClone(data);revisedRule.solution=[1,2,3];revisedRule.followupSolution=[2,3,1];
assert.equal(compile(revisedRule).status,0);
const ded=spawnSync(path.join(root,'bin/uxncli'),[path.join(root,'build/drifblim.rom'),'build/mystery-deduction.tal','build/revised-deduction.rom'],{cwd:temp,encoding:'utf8'});
assert.equal(ded.status,0,ded.stderr);
const newRule=fs.readFileSync(path.join(temp,'build/revised-deduction.rom')),oldRule=fs.readFileSync('build/mystery-deduction.rom');
assert.deepEqual(newRule.subarray(0,-7),oldRule.subarray(0,-7));
assert.deepEqual([...newRule.subarray(-7,-1)],[1,2,3,2,3,1]);
console.log('Authored solution also changes without host or deduction-code edits.');

const revisedSingle=structuredClone(data);
revisedSingle.messages[0]='A CHANGED ACCOUNT.';
revisedSingle.solution=[1,2,3];revisedSingle.followupSolution=[2,3,1];
assert.equal(compile(revisedSingle,'build/mystery-single.tal').status,0);
const singleAssembly=spawnSync(path.join(root,'bin/uxncli'),[path.join(root,'build/drifblim.rom'),'build/mystery-single.tal','build/revised-single.rom'],{cwd:temp,encoding:'utf8'});
assert.equal(singleAssembly.status,0,singleAssembly.stderr);
const single=fs.readFileSync(path.join(temp,'build/revised-single.rom')),oldSingle=fs.readFileSync('build/mystery-single.rom');
assert.deepEqual(single.subarray(0,0xe00),oldSingle.subarray(0,0xe00),'Single-ROM executable/input space stays unchanged when prose and solutions change.');
assert.ok(oldSingle.subarray(0x700,0xe00).every(b=>b===0),'Single-ROM code must not overlap input space.');
assert.deepEqual([...single.subarray(0xe00,0xe0b)],[...data.evidenceMasks,1,2,3,2,3,1]);
assert.ok(single.includes(Buffer.from(revisedSingle.messages[0])));
console.log('Single-ROM prose and both solutions change as data, with identical executable code.');
