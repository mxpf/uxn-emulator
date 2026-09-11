// Local packaging only. No browser-side story state, timer or progression.
const fs=require('node:fs'),assert=require('node:assert/strict');
const target=process.argv[2];
assert.ok(['build/mystery-progression-multi-web','build/mystery-progression-single-web'].includes(target));
let html=fs.readFileSync('web/mystery/index.html','utf8');
html=html.replace('Q asks Cora when unlocked.','Q begins the page-return incident. Asking Bram beforehand asks him to wait.');
html=html.replace('8 · Kept what?','8 · Third answer');
html=html.replace('Q asks when unlocked','Q begins return · ask Bram beforehand to request a wait');
html=html.replace('<div class="session">','<div class="session"><button data-action="10" disabled>Q · Return</button>');
fs.writeFileSync(target+'/index.html',html);
for(const file of ['style.css','app.js'])fs.copyFileSync('web/mystery/'+file,target+'/'+file);
fs.appendFileSync(target+'/style.css','\n.session button{min-width:60px;flex:0 1 84px}\n');
fs.copyFileSync('web/fonts.css',target+'/fonts.css');fs.mkdirSync(target+'/fonts',{recursive:true});
for(const file of ['orbitron-latin.woff2','OFL-Orbitron.txt'])fs.copyFileSync('web/fonts/'+file,target+'/fonts/'+file);
