const assert = require('node:assert/strict');
const {execFileSync} = require('node:child_process');
const call = (...args) => execFileSync('agent-browser',['--session','runner-check',...args],{encoding:'utf8'});
try {
  call('open',process.argv[2] || 'http://127.0.0.1:8768/');
  call('wait','--fn','document.querySelector("#result").textContent.includes("runner checks passed")');
  call('snapshot','-i');
  assert.equal(call('get','text','#result').trim(),execFileSync('./build/test_runner',{encoding:'utf8'}).trim());
  const errors=JSON.parse(call('errors','--json'));
  assert.equal(errors.success,true);assert.deepEqual(errors.data.errors,[]);
  call('screenshot','build/runner-browser.png');
  console.log('Runner browser passed: complete native test output matches, including branching/reordered applications, exact-state replay, startup validation and device attachments; no page errors.');
} finally {call('close');}
