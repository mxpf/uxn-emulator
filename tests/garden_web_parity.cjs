const assert = require('node:assert/strict');
const {execFileSync} = require('node:child_process');
const createGarden = require('../build/web/garden.js');
const {nativeCases,verifyCases} = require('./garden_scenarios.cjs');
(async () => {
  const expected = execFileSync('./build/garden_native_digest', {encoding:'utf8'}).trim().split('\n');
  const m = await createGarden();
  const digest = () => m.UTF8ToString(m._garden_web_digest());
  assert.equal(m._garden_web_reset(), 1);
  assert.equal(digest(), expected[0]);
  let seed = 42;
  const greeting = [2,2,2,2,2,3,3,3,3,5];
  for (let i=0; i<1010; i++) {
    seed = (Math.imul(seed,1664525)+1013904223) >>> 0;
    assert.equal(m._garden_web_step(i<10 ? greeting[i] : (seed>>>16)%6), 1);
    assert.equal(digest(), expected[i+1], `native/Wasm divergence at tick ${i+1}`);
  }
  assert.equal(m._garden_web_step(256), 0);
  assert.equal(m._garden_web_step(0), 0);
  assert.equal(m._garden_web_reset(), 1);
  assert.equal(digest(), expected[0]);
  console.log('Native/WebAssembly parity: 1,011 matching message + pixel fingerprints; fault/reset passed.');
  const summaries=await verifyCases(createGarden,nativeCases());
  console.log(`Extended parity: ${summaries.reduce((n,s)=>n+s.checkpoints,0)} exact world snapshots + message/pixel fingerprints across ${summaries.length} scenarios; five invalid-action/reset cases passed.`);
})().catch(error => { console.error(error); process.exitCode=1; });
