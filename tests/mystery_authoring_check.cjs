const fs=require('node:fs'),os=require('node:os'),path=require('node:path'),assert=require('node:assert/strict');
const {spawnSync}=require('node:child_process');
const root=process.cwd(),temp=fs.mkdtempSync(path.join(os.tmpdir(),'mystery-authoring-'));
fs.mkdirSync(path.join(temp,'examples'));fs.mkdirSync(path.join(temp,'build'));
for(const name of ['presentation','deduction']) fs.copyFileSync(`examples/mystery-${name}.tal`,path.join(temp,`examples/mystery-${name}.tal`));
const data=JSON.parse(fs.readFileSync('examples/mystery-room.json','utf8'));
const compile=variant=>{
 fs.writeFileSync(path.join(temp,'examples/mystery-room.json'),JSON.stringify(variant));
 return spawnSync(process.execPath,[path.join(root,'tools/build_mystery.cjs')],{cwd:temp,encoding:'utf8'});
};
const revised=structuredClone(data);revised.messages[0]='RAIN TAPS AGAINST THE WINDOW.\nTHE STUDY IS LOCKED.';
assert.equal(compile(revised).status,0);
const assembled=spawnSync(path.join(root,'bin/uxncli'),[path.join(root,'build/drifblim.rom'),'build/mystery-presentation.tal','build/revised.rom'],{cwd:temp,encoding:'utf8'});
assert.equal(assembled.status,0,assembled.stderr);
const rom=fs.readFileSync(path.join(temp,'build/revised.rom')),original=fs.readFileSync('build/mystery-presentation.rom');
assert.deepEqual(rom.subarray(0,0xf00),original.subarray(0,0xf00));
assert.ok(original.subarray(0x700,0xf00).every(b=>b===0),'Presentation code must not overlap input/output buffers.');
assert.ok(rom.includes(Buffer.from(revised.messages[0])));
const mutations=[d=>d.messages.pop(),d=>d.solution[0]=0,d=>d.solution[0]=4,d=>d.evidenceMasks[0]=2,d=>d.choices[0].pop(),d=>d.labels[0]='TOO LONG LABEL',d=>d.messages[0]='A'.repeat(41),d=>d.messages[0]='BAD ♥',d=>d.rectangles[0].push(1),d=>d.rectangles[0][2]=255,d=>d.hotspots[0][4]=6];
for(const mutate of mutations){const invalid=structuredClone(data);mutate(invalid);assert.notEqual(compile(invalid).status,0);}
console.log('Mystery authoring passed: changed story text compiles with identical presentation code; invalid text, extents, record widths and actions rejected.');

const revisedRule=structuredClone(data);revisedRule.solution=[1,2,3];
assert.equal(compile(revisedRule).status,0);
const ded=spawnSync(path.join(root,'bin/uxncli'),[path.join(root,'build/drifblim.rom'),'build/mystery-deduction.tal','build/revised-deduction.rom'],{cwd:temp,encoding:'utf8'});
assert.equal(ded.status,0,ded.stderr);
const newRule=fs.readFileSync(path.join(temp,'build/revised-deduction.rom')),oldRule=fs.readFileSync('build/mystery-deduction.rom');
assert.deepEqual(newRule.subarray(0,-4),oldRule.subarray(0,-4));
assert.deepEqual([...newRule.subarray(-4,-1)],[1,2,3]);
console.log('Authored solution also changes without host or deduction-code edits.');
