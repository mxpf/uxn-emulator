// Fixed cartridge-local schema, not a host narrative language.
const fs = require('node:fs'), assert = require('node:assert/strict');
const data = JSON.parse(fs.readFileSync('examples/mystery-room.json', 'utf8'));
const target = process.argv[2];
const single = ['build/mystery-single.tal', 'build/mystery-single-retained.tal'].includes(target);
assert.ok(!process.argv[3] || single, 'Question data is only supported by the single-ROM reference.');
const question = single ? JSON.parse(fs.readFileSync(process.argv[3] || 'examples/mystery-question-returned.json', 'utf8')) : null;
if (question) {
 assert.deepEqual(Object.keys(question).sort(), ['answerText','requiredFacts','solution']);
 assert.ok(Number.isInteger(question.requiredFacts) && question.requiredFacts > 0 && question.requiredFacts < 64);
 assert.ok(['baseline','followup'].includes(question.solution));
 data.messages[11] = question.answerText;
}
const hex = n => { assert.ok(Number.isInteger(n) && n >= 0 && n <= 255); return n.toString(16).padStart(2, '0'); };
assert.equal(data.messages.length, 13);
assert.equal(data.hotspots.length, 5);
assert.equal(data.labels.length, 5);
assert.equal(data.choices.length, 3);
data.choices.forEach(c => assert.equal(c.length, 4));
assert.equal(data.solution.length, 3);
data.solution.forEach(n => assert.ok(Number.isInteger(n) && n >= 1 && n <= 3));
assert.equal(data.followupSolution.length, 3);
data.followupSolution.forEach(n => assert.ok(Number.isInteger(n) && n >= 1 && n <= 3));
assert.equal(data.questionLabels.length, 3);
data.questionLabels.forEach(s => assert.ok(typeof s === 'string' && !s.includes('\n') && s.length <= 12));
assert.equal(data.evidenceMasks.length, 5);
let seen = 0;
for (const n of data.evidenceMasks) {
 assert.ok(Number.isInteger(n) && n > 0 && n < 64 && !(seen & n)); seen |= n;
}
assert.equal(seen, 63);
const strings = [data.title, ...data.labels, ...data.choices.flat(), '9 CONCLUDE', ...data.messages, ...data.questionLabels];
for (const s of strings) {
 assert.equal(typeof s, 'string'); assert.match(s, /^[A-Z0-9 .:\n?-]+$/);
 assert.ok(s.length <= 160 && s.split('\n').length <= 4);
 for (const line of s.split('\n')) assert.ok(line.length <= 40, line);
}
data.labels.forEach(s => assert.ok(!s.includes('\n') && s.length <= 6));
data.choices.flat().forEach(s => assert.ok(!s.includes('\n') && s.length <= 27));
assert.ok(data.rectangles.length <= 64);
for (const r of data.rectangles) {
 assert.equal(r.length, 5); r.forEach(hex);
 const [x,y,w,h,c] = r; assert.ok(w > 0 && h > 0 && x+w <= 256 && y+h <= 192 && c < 7);
}
const actions = new Set();
for (const r of data.hotspots) {
 assert.equal(r.length, 5); r.forEach(hex);
 const [x,y,right,bottom,a] = r;
 assert.ok(x < right && y < bottom && bottom <= 114 && a >= 1 && a <= 5);
 actions.add(a);
}
assert.equal(actions.size, 5);
// Four hypothesis controls and an optional question, separate from story objects.
const hotspots = [...data.hotspots, [8,114,176,124,6], [8,124,176,134,7], [8,134,176,144,8], [182,114,248,144,9], [174,4,248,22,10]];
const presentation = fs.readFileSync('examples/mystery-presentation.tal', 'utf8');
const rules = '\n@evidence ' + data.evidenceMasks.map(hex).join(' ') +
 '\n@solution ' + data.solution.map(hex).join(' ') +
 '\n@followup-solution ' + data.followupSolution.map(hex).join(' ') + '\n';
const interaction = single ? '\n@interaction-rule ' + hex(question.requiredFacts) + ' ' +
 hex(question.solution === 'followup' ? 1 : 0) + '\n' : '';
const viewStart = presentation.indexOf('@paint\n');
assert.ok(viewStart >= 0 && presentation.indexOf('@paint\n', viewStart + 1) < 0);
let out = (single ? fs.readFileSync('examples/mystery-single.tal', 'utf8') +
 '\n' + presentation.slice(viewStart) + '\n|0f00\n' + rules + interaction : presentation) + '\n|1000\n';
out += '@rects\n' + data.rectangles.map(r => r.map(hex).join(' ')).join('\n') + '\n@rects-end\n';
out += '@hotspots\n' + hotspots.map(r => r.map(hex).join(' ')).join('\n') + '\n@hotspots-end\n';
let address = 0x1000 + data.rectangles.length*5 + hotspots.length*5 + strings.length*3;
out += '@texts\n' + strings.map(s => { const row = hex(address>>8)+' '+hex(address&255)+' '+hex(s.length); address += s.length; return row; }).join('\n') + '\n';
assert.ok(address < 65536);
out += strings.map(s => Array.from(Buffer.from(s)).map(hex).join(' ')).join('\n') + '\n';
assert.ok(!target || ['build/mystery-presentation.tal', 'build/mystery-deduction.tal', 'build/mystery-single.tal', 'build/mystery-single-retained.tal'].includes(target));
if (single) fs.writeFileSync(target, out);
else if (!target || target === 'build/mystery-presentation.tal') fs.writeFileSync('build/mystery-presentation.tal', out);
if (!target || target === 'build/mystery-deduction.tal') fs.writeFileSync('build/mystery-deduction.tal', fs.readFileSync('examples/mystery-deduction.tal', 'utf8') + rules);
