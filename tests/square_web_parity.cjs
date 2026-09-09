const assert = require('node:assert/strict');
const {spawnSync} = require('node:child_process');
const create = require('../build/web/no-escape/no-escape.js');
(async () => {
  const operations = '0...' + '2...'.repeat(20) + '3...'.repeat(16) + '4...1...'.repeat(20) + '.'.repeat(780) + '4...RV0' + '22222' + '.'.repeat(30) + 'RV0x1RV';
  const native = spawnSync('./build/square_native_digest', [], {input:operations, encoding:'utf8', maxBuffer:4*1024*1024});
  assert.equal(native.status, 0, native.stderr);
  const expected = native.stdout.trim().split('\n');
  const machine = await create();
  for (let i = 0; i < operations.length; i++) {
    const op = operations[i]; let ok;
    if(op === '0') ok = machine._no_escape_reset();
    else if(/[1-4]/.test(op)) ok = machine._no_escape_input(Number(op));
    else if(op === '.') ok = machine._no_escape_step();
    else if(op === 'R') ok = machine._no_escape_replay();
    else if(op === 'V') {
      let limit = 0; ok = 1;
      while(!(machine._no_escape_flags() & 4) && ok && limit++ <= 8193) ok = machine._no_escape_step();
      ok = ok && (machine._no_escape_flags() & 4);
    } else if(op === 'x') ok = machine._no_escape_input(256);
    const actual = `${Number(!!ok)} ${machine.UTF8ToString(machine._no_escape_digest())}`;
    assert.equal(actual, expected[i], `operation ${i} (${op})`);
  }
  assert.equal(expected.length, operations.length);
  console.log(`No Escape!: ${operations.length} native/WebAssembly checkpoints match (trace, full guest RAM, devices/stacks/queues, pixels); movement, limits, idle gap, rejection, reset and replay passed.`);
})().catch(error => { console.error(error); process.exitCode = 1; });
