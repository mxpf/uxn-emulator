/* Exercise actual coarse-pointer CSS, including landscape phones and tablets. */
const assert = require('node:assert/strict');
const {execFileSync} = require('node:child_process');
const {checkViewport} = require('./viewport_fit_check.cjs');
const root = process.argv[2] || 'http://127.0.0.1:8766/';
const call = (...args) => execFileSync('agent-browser', ['--session',`mobile-fit-${process.pid}`,...args], {encoding:'utf8',timeout:45000});
const evaluate = code => {
  const result = JSON.parse(call('eval',code,'--json'));
  assert.equal(result.success,true,JSON.stringify(result.error));
  return result.data.result;
};
(async () => {
  let socket;
  try {
    call('open',root);
    // The CLI's device preset changes dimensions but does not enable touch.
    // Enable Chrome's real touch emulation on this isolated test tab.
    const endpoint = JSON.parse(call('get','cdp-url','--json')).data.cdpUrl;
    socket = new WebSocket(endpoint);
    await new Promise((resolve,reject) => {
      const timer=setTimeout(()=>reject(new Error('Touch emulation connection timed out')),10000);
      socket.onopen=()=>{clearTimeout(timer);resolve();};
      socket.onerror=error=>{clearTimeout(timer);reject(error);};
    });
    let nextId=0;
    const pending = new Map();
    socket.onmessage = event => {
      const message=JSON.parse(event.data), request=pending.get(message.id);
      if (!request) return;
      pending.delete(message.id);
      clearTimeout(request.timer);
      if (message.error) request.reject(new Error(JSON.stringify(message.error)));
      else request.resolve(message.result);
    };
    const send = (method,params={},sessionId) => new Promise((resolve,reject) => {
      const id=++nextId;
      const timer=setTimeout(()=>{pending.delete(id);reject(new Error(`Touch emulation timed out: ${method}`));},15000);
      pending.set(id,{resolve,reject,timer});
      socket.send(JSON.stringify({id,method,params,sessionId}));
    });
    const target=(await send('Target.getTargets')).targetInfos.find(t=>t.type==='page' && t.url===root);
    assert.ok(target,'Test tab exists');
    const {sessionId}=await send('Target.attachToTarget',{targetId:target.targetId,flatten:true});
    await send('Emulation.setTouchEmulationEnabled',{enabled:true,maxTouchPoints:5},sessionId);
    for (const route of (process.argv.includes('--carousel-only') ? [] : ['tiny-neighbors/','no-escape/','sketchpad/'])) {
      call('open',new URL(route,root).href);
      call('wait','--fn','!document.querySelector("[data-action]").disabled');
      call('snapshot','-i');
      assert.equal(evaluate('matchMedia("(pointer: coarse)").matches'),true);
      for (const [width,height] of [[320,568],[390,664],[390,844],[667,375],[844,390],[932,430],[1024,600],[1024,768]]) {
        call('set','viewport',String(width),String(height));
        checkViewport(evaluate,width,height);
        assert.equal(evaluate('getComputedStyle(document.querySelector(".dpad")).display'),'grid');
        if (width===844) call('screenshot',`build/${route.slice(0,-1)}-touch-landscape.png`);
      }
      // Buttons and mode changes stay in view after rotating back to portrait.
      call('set','viewport','320','568');
      call('click','[data-action="2"]');
      if (route==='sketchpad/') call('click','[data-action="5"]');
      else call('click',route==='no-escape/'?'#pause':'#play');
      checkViewport(evaluate,320,568);
    }
    call('open',root);call('wait','--fn','document.querySelector(".carousel")?.classList.contains("ready")');
    call('set','viewport','393','852');call('snapshot','-i');
    assert.equal(evaluate('matchMedia("(pointer: coarse)").matches'),true);
    assert.equal(evaluate('document.querySelector(".rotation").hidden'),true);
    evaluate(`new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)))`);
    evaluate(`window.gestureLog=[];for(const type of ['pointerdown','pointermove','pointerup','pointercancel','lostpointercapture'])document.querySelector('.carousel-window').addEventListener(type,e=>gestureLog.push([type,e.clientX,e.clientY,e.pointerType]));`);
    const touch = (type,points) => send('Input.dispatchTouchEvent',{type,touchPoints:points},sessionId);
    await touch('touchStart',[{x:260,y:310}]);
    await touch('touchMove',[{x:200,y:310}]);
    await touch('touchMove',[{x:120,y:310}]);
    await touch('touchEnd',[]);
    call('wait','--fn','!document.querySelector(".carousel").hasAttribute("data-moving")');
    assert.equal(evaluate('document.querySelector("[data-current]").getAttribute("href")'),'no-escape/',JSON.stringify(evaluate('gestureLog')));
    assert.equal(evaluate('location.pathname'),new URL(root).pathname);
    call('wait','450');call('click','[data-current]');
    call('wait','--fn','!document.querySelector("[data-action]").disabled');
    assert.equal(evaluate('location.pathname'),new URL('no-escape/',root).pathname);
    console.log(`${process.argv.includes('--carousel-only') ? '' : 'All three consoles passed coarse-pointer phone/tablet checks: eight sizes, rotation, reachable 44px controls, bottom back links and no scrolling. '}Homepage carousel passed real touch swipe without accidental launch, then click-to-play.`);
  } finally { if(socket) socket.close(); call('close'); }
})().catch(error => {console.error(error);process.exitCode=1;});
