// Fixed cartridge-local schema, not a host narrative language.
const fs = require('node:fs'), assert = require('node:assert/strict');
const data = JSON.parse(fs.readFileSync('examples/mystery-room.json', 'utf8'));
const target = process.argv[2];
const knowledge = ['build/mystery-knowledge-multi.tal', 'build/mystery-knowledge-single.tal'].includes(target);
const single = ['build/mystery-single.tal', 'build/mystery-single-retained.tal', 'build/mystery-knowledge-single.tal'].includes(target);
assert.ok(!process.argv[3] || single, 'Question data is only supported by the single-ROM reference.');
const question = single && !knowledge ? JSON.parse(fs.readFileSync(process.argv[3] || 'examples/mystery-question-returned.json', 'utf8')) : null;
const addition = knowledge ? JSON.parse(fs.readFileSync('examples/mystery-knowledge.json', 'utf8')) : null;
if (addition) {
 for (const [index, text] of Object.entries(addition.messages)) data.messages[Number(index)] = text;
 data.questionLabels = addition.questionLabels;
 data.followupSolution = addition.deductionSolution;
 assert.equal(addition.whyChoices.length, 4);
 addition.whyChoices.forEach(s => assert.ok(!s.includes('\n') && s.length <= 27));
}
if (question) {
 assert.deepEqual(Object.keys(question).sort(), ['answerText','requiredFacts','solution']);
 assert.ok(Number.isInteger(question.requiredFacts) && question.requiredFacts > 0 && question.requiredFacts < 64);
 assert.ok(['baseline','followup'].includes(question.solution));
 data.messages[11] = question.answerText;
}
const hex = n => { assert.ok(Number.isInteger(n) && n >= 0 && n <= 255); return n.toString(16).padStart(2, '0'); };
assert.equal(data.messages.length, knowledge ? 19 : 13);
assert.equal(data.hotspots.length, 5);
assert.equal(data.labels.length, 5);
assert.equal(data.choices.length, 3);
data.choices.forEach(c => assert.equal(c.length, 4));
assert.equal(data.solution.length, 3);
data.solution.forEach(n => assert.ok(Number.isInteger(n) && n >= 1 && n <= 3));
assert.equal(data.followupSolution.length, 3);
data.followupSolution.forEach(n => assert.ok(Number.isInteger(n) && n >= 1 && n <= 3));
assert.equal(data.questionLabels.length, knowledge ? 4 : 3);
data.questionLabels.forEach(s => assert.ok(typeof s === 'string' && !s.includes('\n') && s.length <= 12));
assert.equal(data.evidenceMasks.length, 5);
let seen = 0;
for (const n of data.evidenceMasks) {
 assert.ok(Number.isInteger(n) && n > 0 && n < 64 && !(seen & n)); seen |= n;
}
assert.equal(seen, 63);
const strings = [data.title, ...data.labels, ...data.choices.flat(), '9 CONCLUDE', ...data.messages, ...data.questionLabels, ...(addition ? addition.whyChoices : [])];
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
let presentation = fs.readFileSync('examples/mystery-presentation.tal', 'utf8');
// These are exact, bounded assembly substitutions, not an authored language.
function replaceOnce(source, old, value) {
 assert.equal(source.split(old).length, 2, old);
 return source.replace(old, value);
}
if (knowledge) {
 presentation = replaceOnce(presentation, '#04 LDZ #0e ADD ;text JSR2',
  '#04 LDZ #06 LDZ #02 LTH ?kept-label #2a ADD !answer-label\n @kept-label #0e ADD\n @answer-label ;text JSR2');
 presentation = replaceOnce(presentation, '#06 LDZ #20 ADD', '#06 LDZ #26 ADD');
 presentation = replaceOnce(presentation, '#06 #24 DEO #06 LDZ #01 NEQ ?question-label',
  '#06 #24 DEO #06 LDZ #01 LTH ?question-label #06 LDZ #02 GTH ?question-label');
}
const rules = '\n@evidence ' + data.evidenceMasks.map(hex).join(' ') +
 '\n@solution ' + data.solution.map(hex).join(' ') +
 '\n@followup-solution ' + data.followupSolution.map(hex).join(' ') + '\n';
const interaction = question ? '\n@interaction-rule ' + hex(question.requiredFacts) + ' ' +
 hex(question.solution === 'followup' ? 1 : 0) + '\n' : '';
const viewStart = presentation.indexOf('@paint\n');
assert.ok(viewStart >= 0 && presentation.indexOf('@paint\n', viewStart + 1) < 0);
let knowledgeSource;
if (knowledge) {
 knowledgeSource = fs.readFileSync('examples/mystery-knowledge.tal', 'utf8');
 knowledgeSource = replaceOnce(knowledgeSource, '( KNOWLEDGE-SETUP )', single ? '' : '#0900 #d6 DEO2 #02 #d8 DEO #01 #dc DEO');
 knowledgeSource = replaceOnce(knowledgeSource, '( KNOWLEDGE-RECEIVE )', single ? '' : '#dd DEI #01 EQU ?witness-reply #0b LDZ ?done');
 knowledgeSource = replaceOnce(knowledgeSource, '( KNOWLEDGE-ACCOUNT )', fs.readFileSync('examples/mystery-knowledge-' + (single ? 'local' : 'remote') + '.tal', 'utf8'));
}
let out = (knowledge ? knowledgeSource + '\n' + presentation.slice(viewStart) + '\n|0f00\n' + rules : single ? fs.readFileSync('examples/mystery-single.tal', 'utf8') +
 '\n' + presentation.slice(viewStart) + '\n|0f00\n' + rules + interaction : presentation) + '\n|1000\n';
out += '@rects\n' + data.rectangles.map(r => r.map(hex).join(' ')).join('\n') + '\n@rects-end\n';
out += '@hotspots\n' + hotspots.map(r => r.map(hex).join(' ')).join('\n') + '\n@hotspots-end\n';
let address = 0x1000 + data.rectangles.length*5 + hotspots.length*5 + strings.length*3;
out += '@texts\n' + strings.map(s => { const row = hex(address>>8)+' '+hex(address&255)+' '+hex(s.length); address += s.length; return row; }).join('\n') + '\n';
assert.ok(address < 65536);
out += strings.map(s => Array.from(Buffer.from(s)).map(hex).join(' ')).join('\n') + '\n';
assert.ok(!target || knowledge || ['build/mystery-presentation.tal', 'build/mystery-deduction.tal', 'build/mystery-single.tal', 'build/mystery-single-retained.tal'].includes(target));
if (single || knowledge) fs.writeFileSync(target, out);
else if (!target || target === 'build/mystery-presentation.tal') fs.writeFileSync('build/mystery-presentation.tal', out);
if (!target || target === 'build/mystery-deduction.tal') fs.writeFileSync('build/mystery-deduction.tal', fs.readFileSync('examples/mystery-deduction.tal', 'utf8') + rules);
