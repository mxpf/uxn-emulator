/* Shared, deterministic test inputs; never shipped with the game. */
const assert = require('node:assert/strict');
const {execFileSync} = require('node:child_process');
const repeat = (action, n) => Array(n).fill(action);
const greeting = [...repeat(2,5), ...repeat(3,4), 5];
function random(seed, count, choices) {
  return Array.from({length:count}, () => {
    seed = (Math.imul(seed,1664525) + 1013904223) >>> 0;
    return (seed >>> 16) % choices;
  });
}
const scenarios = [
  {name:'wandering-and-clock-wrap', actions:repeat(0,1024), points:{
    3:[2,2,9,6,0,0,3], 6:[2,2,9,7,0,0,6], 9:[2,2,8,7,0,0,9],
    12:[2,2,8,6,0,0,12], 256:[2,2,9,6,0,0,0]}, noFriend:true},
  {name:'proximity-without-greeting', actions:[...greeting.slice(0,9), ...repeat(0,512)],
    points:{9:[7,6,8,7,1,0,9], 521:[7,6,8,7,1,0,9]}, noFriend:true},
  {name:'perimeter-and-distant-greeting', actions:[4,4,1,1,5],
    points:{2:[1,2], 4:[1,1], 5:[1,1]}, noFriend:true},
  {name:'water-collision', actions:[...repeat(2,8),3,3,3],
    points:{9:[10,3], 10:[10,3], 11:[10,3]}, noFriend:true},
  {name:'creature-collision-and-repeated-greeting', actions:[...greeting.slice(0,9),3,2,5,5],
    points:{10:[7,7,8,7,1,0,10], 11:[7,7,8,7,1,0,11], 12:[7,7,8,7,2,1,12], 13:[7,7,8,7,2,1,13]}},
  {name:'seed-42-no-greeting', actions:random(42,1024,5), noFriend:true},
  {name:'seed-2026-no-greeting', actions:random(2026,1024,5), noFriend:true},
  {name:'seed-73-mixed-actions', actions:random(73,1024,6)},
  {name:'friendship-then-seed-42', actions:[...greeting,...random(42,1024,6)],
    points:{10:[7,6,8,7,2,1,10]}, friendFrom:10}
];

function nativeCases() {
  return scenarios.map(s => {
    const rows = execFileSync('./build/garden_probe', {encoding:'utf8',
      input:s.actions.join('\n')+'\n', maxBuffer:8*1024*1024}).trim().split('\n').map(JSON.parse);
    assert.equal(rows.length,s.actions.length+1,s.name);
    rows.forEach((r,tick) => {
      const state = Array.from(Buffer.from(r.state,'hex'));
      assert.equal(state.length,200);
      assert.equal(state[6],tick%256,`${s.name}: clock`);
      assert.equal(state[7],0,`${s.name}: reserved byte`);
      assert.equal(r.state.slice(16),rows[0].state.slice(16),`${s.name}: terrain changed`);
      if(s.noFriend) assert.equal(state[5],0,`${s.name}: unexpected friendship`);
      if(s.friendFrom && tick>=s.friendFrom) assert.deepEqual(state.slice(2,6),[8,7,2,1],s.name);
      if(s.points?.[tick]) assert.deepEqual(state.slice(0,s.points[tick].length),s.points[tick],`${s.name}: tick ${tick}`);
      assert.equal(r.sendFull,0,s.name);
      if(tick) {
        assert.equal(r.messages,2,s.name);
        assert.equal(r.bytes,201,s.name);
        assert.deepEqual(r.queuePeak,[1,1],s.name);
        assert.equal(r.turns,2,s.name);
        assert.equal(r.traceEvents,6,s.name);
      }
    });
    return {name:s.name,actions:s.actions,rows};
  });
}

/* Self-contained so the exact same checks can run in Node and Chromium.
 * Timing includes only the synchronous C/Wasm step, not digest/readback. */
async function verifyCases(createGarden, cases) {
  const m = await createGarden();
  const digest = () => m.UTF8ToString(m._garden_web_digest());
  const state = () => {
    const p=m._garden_web_state();
    return Array.from(m.HEAPU8.subarray(p,p+200), b=>b.toString(16).padStart(2,'0')).join('');
  };
  const summaries=[];
  for(const c of cases) {
    if(m._garden_web_reset()!==1) throw Error(`${c.name}: reset failed`);
    const us=[];
    for(let i=0;i<c.rows.length;i++) {
      if(i) {
        const start=performance.now();
        const ok=m._garden_web_step(c.actions[i-1]);
        us.push((performance.now()-start)*1000);
        if(ok!==1) throw Error(`${c.name}: step ${i} failed`);
      }
      if(digest()!==c.rows[i].digest || state()!==c.rows[i].state)
        throw Error(`${c.name}: divergence at tick ${i}`);
    }
    summaries.push({name:c.name,checkpoints:c.rows.length,us});
  }
  /* Validate before byte narrowing; all failures must stay terminal until reset. */
  for(const invalid of [-1,6,255,256,2147483647]) {
    if(m._garden_web_reset()!==1) throw Error('fault setup reset failed');
    for(let i=0;i<3;i++) if(m._garden_web_step(0)!==1) throw Error('fault setup step failed');
    const before=digest(), snapshot=state();
    if(m._garden_web_step(invalid)!==0 || m._garden_web_step(0)!==0)
      throw Error(`nonterminal invalid action ${invalid}`);
    if(digest()!==before || state()!==snapshot) throw Error('fault changed guest state');
    if(!m.UTF8ToString(m._garden_web_error())) throw Error('missing fault diagnostic');
    if(m._garden_web_reset()!==1 || digest()!==cases[0].rows[0].digest || state()!==cases[0].rows[0].state)
      throw Error('reset did not restore boot');
  }
  return summaries;
}
module.exports={scenarios,nativeCases,verifyCases};
