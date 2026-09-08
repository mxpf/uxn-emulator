'use strict';
(async () => {
  const $ = id => document.getElementById(id);
  const controls = [...document.querySelectorAll('button')];
  const canvas = $('garden');
  let machine, running = false, failed = false, previous = 0, pending = 0;
  const held = new Map();
  const actions = {ArrowUp:1, w:1, ArrowRight:2, d:2, ArrowDown:3, s:3, ArrowLeft:4, a:4, ' ':5};
  const stop = message => {
    running = false; failed = true; held.clear(); pending = 0;
    $('error').hidden = false;
    $('error').textContent = `Execution stopped: ${message}`;
    controls.forEach(button => { button.disabled = button.id !== 'reset' || !machine; });
    $('play').textContent = 'Play';
  };
  try {
    machine = await createGarden();
    const context = canvas.getContext('2d');
    if (!context) throw new Error('Canvas 2D is unavailable.');
    const image = context.createImageData(128, 96);
    function render() {
      const pointer = machine._garden_web_pixels() >>> 2;
      for (let i = 0; i < 128 * 96; i++) {
        const pixel = machine.HEAPU32[pointer + i];
        image.data[i*4] = pixel >>> 16 & 255;
        image.data[i*4+1] = pixel >>> 8 & 255;
        image.data[i*4+2] = pixel & 255;
        image.data[i*4+3] = 255;
      }
      context.putImageData(image, 0, 0);
      const state = machine.HEAPU8.subarray(machine._garden_web_state(), machine._garden_web_state()+200);
      $('objective').textContent = state[5] ? 'A new friend. Stay a while.' : state[4] ? 'It sees you. Say hello with Space.' : 'Find the little golden creature.';
      const digest = machine.UTF8ToString(machine._garden_web_digest());
      $('digest').textContent = digest;
      $('clock').textContent = `Tick ${digest.split(' ')[0]} · ${failed ? 'Stopped' : running ? 'Live' : 'Paused'}`;
      const lines = [];
      for (let i = Math.max(0, machine._garden_web_history_count()-14); i < machine._garden_web_history_count(); i++)
        lines.push(machine.UTF8ToString(machine._garden_web_history(i)));
      $('messages').textContent = lines.join('\n');
      $('play').textContent = running ? 'Pause' : 'Play';
      $('step').disabled = running || failed;
    }
    function step(action) {
      if (failed) return;
      if (!machine._garden_web_step(action)) stop(machine.UTF8ToString(machine._garden_web_error()));
      render();
    }
    function reset() {
      running = false; failed = false; pending = 0; held.clear();
      $('error').hidden = true;
      controls.forEach(button => { button.disabled = false; });
      if (!machine._garden_web_reset()) stop(machine.UTF8ToString(machine._garden_web_error()));
      render();
    }
    function pause() { running = false; pending = 0; held.clear(); render(); }
    function toggle() {
      if (failed) return;
      if (running) pause();
      else { running = true; previous = performance.now(); render(); }
    }
    function input(action) {
      if (running) pending = action;
      else step(action);
    }
    $('play').addEventListener('click', toggle);
    $('step').addEventListener('click', () => step(0));
    $('reset').addEventListener('click', reset);
    document.querySelectorAll('[data-action]').forEach(button =>
      button.addEventListener('click', () => input(Number(button.dataset.action))));
    document.addEventListener('keydown', event => {
      if (event.altKey || event.ctrlKey || event.metaKey) return;
      // Space must still activate a focused HTML button/link normally.
      if (event.key === ' ' && event.target.closest('button,a,summary')) return;
      const key = event.key.length === 1 ? event.key.toLowerCase() : event.key;
      if (!(key in actions) && key !== 'p' && key !== 'n') return;
      event.preventDefault();
      if (event.repeat) return;
      if (key === 'p') toggle();
      else if (key === 'n') { if (!running) step(0); }
      else { held.set(key, actions[key]); input(actions[key]); }
    });
    document.addEventListener('keyup', event => held.delete(event.key.length === 1 ? event.key.toLowerCase() : event.key));
    window.addEventListener('blur', pause);
    document.addEventListener('visibilitychange', () => { if (document.hidden) pause(); });
    function frame(now) {
      if (running && now - previous >= 250) {
        const movement = [...held.values()].find(action => action !== 5) || 0;
        step(pending || movement); pending = 0; previous = now;
      }
      requestAnimationFrame(frame);
    }
    reset();
    requestAnimationFrame(frame);
  } catch (error) { stop(error.message || String(error)); }
})();
