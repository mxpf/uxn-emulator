/* Informational measurements, never timing-based pass/fail thresholds. */
const {nativeCases,verifyCases} = require('./garden_scenarios.cjs');
const {execFileSync} = require('node:child_process');
const createGarden = require('../build/web/garden.js');

function stats(values) {
  const sorted=[...values].sort((a,b)=>a-b);
  const at=p=>sorted[Math.ceil(p*sorted.length)-1];
  const rounded=n=>Number(n.toFixed(3));
  return {samples:sorted.length,min:rounded(sorted[0]),median:rounded(at(.5)),
    p95:rounded(at(.95)),max:rounded(sorted.at(-1)),
    mean:rounded(sorted.reduce((a,b)=>a+b,0)/sorted.length)};
}

function timingSummary(summaries) {
  return {unit:'microseconds per synchronous step',
    all:stats(summaries.flatMap(s=>s.us)),
    scenarios:summaries.map(s=>({name:s.name,...stats(s.us)}))};
}

async function main() {
  /* Discard a complete pass. Native processes start afresh per scenario;
   * Wasm instances also start afresh, but engine/module caches may be warm. */
  await verifyCases(createGarden,nativeCases());
  const cases=nativeCases();
  const wasm=await verifyCases(createGarden,cases);
  const rows=cases.flatMap(c=>c.rows.slice(1));
  const range=values=>[Math.min(...values),Math.max(...values)];
  console.log(JSON.stringify({
    node:process.version,platform:process.platform,arch:process.arch,
    memory:JSON.parse(execFileSync('./build/garden_probe',['--sizes'],{encoding:'utf8'})),
    scenarios:cases.length,steps:rows.length,
    instructions:{view:range(rows.map(r=>r.instructions[0])),
      world:range(rows.map(r=>r.instructions[1])),
      total:range(rows.map(r=>r.instructions[0]+r.instructions[1]))},
    messagesPerStep:range(rows.map(r=>r.messages)),bytesPerStep:range(rows.map(r=>r.bytes)),
    schedulerTurnsPerStep:range(rows.map(r=>r.turns)),
    traceEventsPerStep:range(rows.map(r=>r.traceEvents)),
    queuePeak:[0,1].map(i=>Math.max(...rows.map(r=>r.queuePeak[i]))),
    sendFull:rows.reduce((n,r)=>n+r.sendFull,0),
    boot:cases[0].rows[0],
    native:timingSummary(cases.map(c=>({name:c.name,us:c.rows.slice(1).map(r=>r.us)}))),
    nodeWasm:timingSummary(wasm)
  },null,2));
}
if(require.main===module) main().catch(error=>{console.error(error);process.exitCode=1;});
module.exports={stats,timingSummary};
