// Real files through native and browser shells; no test-only app hooks.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawnSync,execFileSync} = require('node:child_process');
const {sizes,checkViewport} = require('./viewport_fit_check.cjs');
const dir = fs.mkdtempSync(path.join(os.tmpdir(),'sketch-documents-'));
const nativeFile = path.join(dir,'native.sketch'), browserFile = path.join(dir,'browser.sketch');
const native = args => spawnSync('./bin/sketchpad',args,{encoding:'utf8',env:{...process.env,SDL_VIDEODRIVER:'dummy'}});
const session = `sketch-documents-${process.pid}`;
const call = (...args) => execFileSync('agent-browser',['--session',session,...args],{encoding:'utf8',timeout:45000});
const evaluate = code => {const r=JSON.parse(call('eval',code,'--json')); assert.equal(r.success,true,JSON.stringify(r.error)); return r.data.result;};
const pixels = () => evaluate(`Array.from(document.querySelector('canvas').getContext('2d').getImageData(0,0,128,96).data)`);
const uploaded = () => call('wait','--fn',`!document.querySelector('#open').disabled`);
try {
  let r = native(['--script','52235614','--save',nativeFile]); assert.equal(r.status,0,r.stderr);
  const original = fs.readFileSync(nativeFile); assert.equal(original.length,784);
  assert.equal(original.subarray(0,8).toString(),'SKETCH01');
  r=native(['--open',nativeFile,'--script','','--save',path.join(dir,'native-roundtrip.sketch')]);
  assert.equal(r.status,0,r.stderr); assert.deepEqual(fs.readFileSync(path.join(dir,'native-roundtrip.sketch')),original);
  r=native(['--script','5','--save',nativeFile]); assert.notEqual(r.status,0); assert.deepEqual(fs.readFileSync(nativeFile),original);
  call('open',process.argv[2] || 'http://127.0.0.1:8766/sketchpad/');
  call('wait','--fn',`!document.querySelector('#open').disabled`); call('snapshot','-i');
  // Both native and browser produce the exact same document for the same actions.
  evaluate(`'52235614'.split('').forEach(a=>document.querySelector('[data-action="'+a+'"]').click())`);
  const expectedPixels=pixels();
  call('download','#save',browserFile); assert.deepEqual(fs.readFileSync(browserFile),original);
  call('reload'); uploaded(); assert.notDeepEqual(pixels(),expectedPixels);
  call('upload','#drawing-file',nativeFile); uploaded(); assert.deepEqual(pixels(),expectedPixels);
  assert.equal(evaluate(`document.querySelector('#file-status').textContent`),'Drawing opened.');
  assert.equal(evaluate(`document.querySelector('[data-action="6"]').getAttribute('aria-pressed')`),'true');
  call('download','#save',path.join(dir,'browser-roundtrip.sketch'));
  assert.deepEqual(fs.readFileSync(path.join(dir,'browser-roundtrip.sketch')),original);
  r=native(['--open',browserFile,'--script','','--save',path.join(dir,'browser-to-native.sketch')]);
  assert.equal(r.status,0,r.stderr); assert.deepEqual(fs.readFileSync(path.join(dir,'browser-to-native.sketch')),original);
  const cases = [original.subarray(0,783),Buffer.concat([original,Buffer.from([0])]),Buffer.alloc(784),Buffer.from(original),Buffer.from(original)];
  cases[3][783]=2; cases[4][12]=3;
  for(let i=0;i<cases.length;i++) {
    const bad=path.join(dir,`bad-${i}.sketch`); fs.writeFileSync(bad,cases[i]);
    r=native(['--open',bad,'--script','']); assert.notEqual(r.status,0);
    call('upload','#drawing-file',bad); uploaded();
    assert.match(evaluate(`document.querySelector('#file-status').textContent`),/unchanged/);
    assert.deepEqual(pixels(),expectedPixels);
    assert.equal(evaluate(`document.querySelector('[data-action="6"]').getAttribute('aria-pressed')`),'true');
  }
  for(const [width,height] of sizes) {
    call('set','viewport',String(width),String(height)); checkViewport(evaluate,width,height);
  }
  // Slow/failed reads cannot accept edits or leave the interface disabled.
  evaluate(`window.originalRead=File.prototype.arrayBuffer;
    File.prototype.arrayBuffer=function(){return new Promise((resolve,reject)=>{window.rejectRead=reject})}`);
  call('upload','#drawing-file',nativeFile);
  assert.equal(evaluate(`document.querySelector('#open').disabled && document.querySelector('#save').disabled`),true);
  evaluate(`document.querySelector('canvas').dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowRight',bubbles:true}));
    window.rejectRead(new Error('Could not read file.')); File.prototype.arrayBuffer=window.originalRead`);
  uploaded(); assert.deepEqual(pixels(),expectedPixels);
  // Reselecting the same file works, and editing continues after rejection.
  call('upload','#drawing-file',nativeFile); uploaded();
  call('upload','#drawing-file',nativeFile); uploaded(); assert.deepEqual(pixels(),expectedPixels);
  call('click','[data-action="2"]'); assert.notDeepEqual(pixels(),expectedPixels);
  assert.equal(evaluate(`document.querySelector('#error').hidden`),true);
  const errors=JSON.parse(call('errors','--json')); assert.deepEqual(errors.data.errors,[]);
  console.log('Sketch documents passed: actual downloads/uploads, native↔browser byte-exact files and pixels, tools restored, reload, repeated open, malformed files unchanged, editing after rejection, no native overwrite.');
} finally { call('close'); console.log(`Test files: ${dir}`); }
