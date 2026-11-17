// Cartridge-local packing only; no runtime content loader or narrative engine.
const fs=require('node:fs'),assert=require('node:assert/strict');
const data=JSON.parse(fs.readFileSync(process.argv[2]||'examples/account.json'));
assert.deepEqual(Object.keys(data).sort(),['initialBelief','text']);
assert.ok(data.initialBelief===0||data.initialBelief===1);
const names=['record-kept','record-returned','belief-kept','belief-returned','shared'];
assert.deepEqual(Object.keys(data.text).sort(),[...names].sort());
for(const s of Object.values(data.text)) {
 assert.equal(typeof s,'string');assert.match(s,/^[A-Z .:]+$/);assert.ok(s.length>0&&s.length<=100);
}
for(const role of ['record','witness','single']) {
 let source=fs.readFileSync('examples/account-'+role+'.tal','utf8');
 if(role!=='witness') source+='\n@say DUP2 LDA DUP ?char POP POP2 BRK\n@char #18 DEO INC2 !say\n';
 // Fixed resource offsets let tests distinguish executable edits from data edits.
 source+='\n|0f00\n@initial-belief '+(role==='record'?'00':'0'+data.initialBelief)+'\n';
 if(role!=='witness') {
  source+='|1000\n';
  for(const name of names) source+='@'+name+' '+[...Buffer.from(data.text[name]+'\n')].map(n=>n.toString(16).padStart(2,'0')).join(' ')+' 00\n';
 }
 fs.writeFileSync('build/account-'+role+'.tal',source);
}
