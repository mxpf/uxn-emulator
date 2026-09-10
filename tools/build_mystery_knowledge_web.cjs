// Packaging only: the existing mystery view/input code receives no story logic.
const fs = require('node:fs'), assert = require('node:assert/strict');
const target=process.argv[2];
assert.ok(['build/mystery-knowledge-multi-web','build/mystery-knowledge-single-web'].includes(target));
let html=fs.readFileSync('web/mystery/index.html','utf8');
html=html.replace('Q asks Cora when unlocked.', 'Q follows up: ask Cora, then show Bram.');
html=html.replace('8 · Kept what?', '8 · Third answer');
html=html.replace('Q asks when unlocked', 'Q follows up · after the return, 8 explains why');
html=html.replace('<div class="session">','<div class="session"><button data-action="10" disabled>Q · Follow up</button>');
fs.writeFileSync(target+'/index.html',html);
for(const file of ['style.css','app.js']) fs.copyFileSync('web/mystery/'+file,target+'/'+file);
// Keep the added follow-up button within the existing session row on phones.
fs.appendFileSync(target+'/style.css', '\n.session button{min-width:60px;flex:0 1 84px}\n');
fs.copyFileSync('web/fonts.css',target+'/fonts.css');
fs.mkdirSync(target+'/fonts',{recursive:true});
for(const file of ['orbitron-latin.woff2','OFL-Orbitron.txt']) fs.copyFileSync('web/fonts/'+file,target+'/fonts/'+file);
