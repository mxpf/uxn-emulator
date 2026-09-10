/* Shared geometry contract: fit real controls, not a clipped/scrolling panel. */
const assert = require('node:assert/strict');
const sizes = [[320,568],[390,844],[760,900],[1440,1000],
  [393,659],[600,700],[1024,600],[1440,700],[667,375],[844,390],[932,430]];
function checkTypography(evaluate) {
  evaluate('document.fonts.ready.then(()=>true)');
  const font = evaluate(`(() => ({
    loaded: [...document.fonts].some(f=>f.family==='Orbitron' && f.status==='loaded'),
    copy: [...document.querySelectorAll('h1, p, button, kbd, footer, .back, .position')]
      .every(el=>getComputedStyle(el).fontFamily.startsWith('Orbitron')),
    selfHosted: performance.getEntriesByType('resource').some(r=>new URL(r.name).origin===location.origin && new URL(r.name).pathname.endsWith('/fonts/orbitron-latin.woff2')),
    externalFonts: performance.getEntriesByType('resource').some(r=>/fonts\\.(googleapis|gstatic)\\.com/.test(r.name))
  }))()`);
  assert.deepEqual(font,{loaded:true,copy:true,selfHosted:true,externalFonts:false});
}
function checkViewport(evaluate, width, height) {
  const layout = evaluate(`(() => {
    const rect = el => el.getBoundingClientRect().toJSON();
    const visible = el => el.getBoundingClientRect().width && el.getBoundingClientRect().height;
    const targets = [...document.querySelectorAll('button, .back')].filter(visible);
    return {
      canvas: rect(document.querySelector('canvas')),
      panel: rect(document.querySelector('section')),
      back: rect(document.querySelector('.back')),
      width: document.documentElement.scrollWidth, height: document.documentElement.scrollHeight,
      targets: targets.map(el => ({name: el.textContent, ...rect(el),
        reachable: el.contains(document.elementFromPoint(
          el.getBoundingClientRect().x + el.getBoundingClientRect().width / 2,
          el.getBoundingClientRect().y + el.getBoundingClientRect().height / 2))})),
      copy: [...document.querySelectorAll('h1, [role="status"], .keys, .keyboard, footer')].filter(visible).map(rect)
    };
  })()`);
  const label = `${width}×${height}`;
  assert.ok(layout.width <= width && layout.height <= height, `${label}: page scrolls ${JSON.stringify(layout)}`);
  assert.ok(Math.abs(layout.canvas.height - layout.canvas.width * .75) < 1, `${label}: distorted canvas`);
  assert.ok(layout.canvas.width >= 128 && layout.canvas.height >= 96, `${label}: unusable display`);
  assert.ok(Math.abs(layout.back.bottom - height) < 1, `${label}: back is not at bottom`);
  const inside = r => r.left >= -.5 && r.top >= -.5 && r.right <= width + .5 && r.bottom <= height + .5;
  assert.ok(inside(layout.canvas), `${label}: display outside viewport`);
  for (const r of layout.targets) {
    assert.ok(r.width >= 44 && r.height >= 44, `${label}: small target ${r.name}`);
    assert.ok(inside(r) && r.reachable, `${label}: inaccessible target ${r.name}`);
  }
  const overlaps = (a,b) => Math.min(a.right,b.right) - Math.max(a.left,b.left) > 1 &&
    Math.min(a.bottom,b.bottom) - Math.max(a.top,b.top) > 1;
  const content = [layout.canvas,...layout.targets,...layout.copy];
  for (let i=0;i<content.length;i++) {
    assert.ok(inside(content[i]), `${label}: content outside viewport`);
    for (let j=i+1;j<content.length;j++) assert.ok(!overlaps(content[i],content[j]), `${label}: overlapping content ${i}/${j}`);
  }
  return layout;
}
module.exports = {sizes,checkViewport,checkTypography};
