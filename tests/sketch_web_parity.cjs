const assert = require('node:assert/strict');
const {spawnSync} = require('node:child_process');
const create = require('../build/sketch-check/sketch.js');
const operations = '0E' + 'BE'.repeat(36) + 'CE'.repeat(28) + 'DE'.repeat(36) + 'AE'.repeat(28)
  + 'F..FEE' + '.'.repeat(80) + 'x22225....F0BECEDEAE' + 'A'.repeat(20) + 'D'.repeat(20) + 'ExF'
  + '0EBBBECCFADDFDEEFFE';
(async () => {
  const native = spawnSync('./build/sketch_probe', [], {input:operations, encoding:'utf8'});
  assert.equal(native.status, 0, native.stderr);
  const expected = native.stdout.trim().split('\n'), module = await create();
  assert.equal(expected.length, operations.length);
  for (let i = 0; i < operations.length; i++) {
    const result = module._sketch_test_op(operations.charCodeAt(i));
    assert.equal(`${result} ${module.UTF8ToString(module._sketch_test_digest())}`, expected[i], `checkpoint ${i} (${operations[i]})`);
  }
  console.log(`Sketchpad: ${operations.length} native/Wasm checkpoints match: full guest-state, trace and pixel fingerprints; drawing, erasing, edges, idle gaps, full queue, invalid input and reset.`);
})().catch(e => {console.error(e); process.exitCode = 1;});
