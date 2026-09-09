'use strict';
(async () => {
  const $ = id => document.getElementById(id);
  const directions = [...document.querySelectorAll('[data-action]')];
  let machine, running = false, failed = false, previous = 0, flags = 0;
  const stop = message => {
    running = false; failed = true;
    $('error').textContent = message; $('error').hidden = false;
    for (const button of document.querySelectorAll('button')) button.disabled = button.id !== 'reset' || !machine;
    $('status').textContent = 'Stopped';
  };
  try {
    machine = await createNoEscape();
    const context = $('board').getContext('2d');
    if (!context) throw new Error('Canvas is unavailable.');
    const image = context.createImageData(128, 96);
    const check = result => { if (!result) throw new Error(machine.UTF8ToString(machine._no_escape_error()) || 'The game stopped. Start a new game to try again.'); };
    function render() {
      const pointer = machine._no_escape_pixels() >>> 2;
      for (let i = 0; i < 128 * 96; i++) {
        const pixel = machine.HEAPU32[pointer + i];
        image.data[i*4] = pixel >>> 16 & 255; image.data[i*4+1] = pixel >>> 8 & 255;
        image.data[i*4+2] = pixel & 255; image.data[i*4+3] = 255;
      }
      context.putImageData(image, 0, 0);
      flags = machine._no_escape_flags();
      if ((flags & 4) || ((flags & 1) && !(flags & 2))) running = false;
      const rejected = machine._no_escape_rejected();
      $('status').textContent = ((flags & 4) ? 'Replay verified — exact match.' : (flags & 2) ? (running ? 'Replaying…' : 'Replay paused.') : (flags & 1) ? 'Recording full. Replay or start a new game.' : running ? 'Recording your moves.' : 'Paused.') + (rejected ? ` ${rejected} input${rejected === 1 ? '' : 's'} rejected: queue full.` : '');
      $('pause').textContent = running ? 'Pause' : 'Resume';
      $('pause').disabled = !!((flags & 4) || ((flags & 1) && !(flags & 2)));
      $('replay').disabled = !!((flags & 2) && !(flags & 4));
      $('reset').disabled = false;
      for (const button of directions) button.disabled = !!(flags & 1);
    }
    function reset() {
      check(machine._no_escape_reset()); failed = false; $('error').hidden = true;
      for (let i = 0; i < 3; i++) check(machine._no_escape_step());
      running = true; previous = performance.now(); render();
    }
    function input(action) {
      if (failed || (flags & 1)) return;
      check(machine._no_escape_input(action));
      if (!running) for (let i = 0; i < 3; i++) check(machine._no_escape_step());
      render();
    }
    function replay() { if (failed) return; check(machine._no_escape_replay()); running = true; previous = performance.now(); render(); }
    function pause() { if (failed) return; running = false; render(); }
    function toggle() { if (failed || $('pause').disabled) return; running = !running; previous = performance.now(); render(); }
    const safe = fn => (...args) => { try { fn(...args); } catch (error) { stop(error.message || String(error)); } };
    $('reset').addEventListener('click', safe(reset)); $('replay').addEventListener('click', safe(replay)); $('pause').addEventListener('click', safe(toggle));
    for (const button of directions) button.addEventListener('click', safe(() => input(Number(button.dataset.action))));
    document.addEventListener('keydown', safe(event => {
      if (event.altKey || event.ctrlKey || event.metaKey || event.target.closest('input,textarea,select,[contenteditable]')) return;
      const action = {ArrowUp:1, ArrowRight:2, ArrowDown:3, ArrowLeft:4}[event.key];
      if (action) { event.preventDefault(); input(action); return; }
      if (event.repeat) return;
      if (event.key.toLowerCase() === 'r') { event.preventDefault(); replay(); }
      if (event.key.toLowerCase() === 'n') { event.preventDefault(); reset(); }
      if (event.key.toLowerCase() === 'p') { event.preventDefault(); toggle(); }
    }));
    window.addEventListener('blur', pause);
    document.addEventListener('visibilitychange', () => { if (document.hidden) pause(); });
    function frame(now) {
      try {
        if (running && !failed && now - previous >= 16) {
          previous = now;
          for (let i = 0; i < 3; i++) check(machine._no_escape_step());
          render();
        }
      } catch (error) { stop(error.message || String(error)); }
      requestAnimationFrame(frame);
    }
    reset(); requestAnimationFrame(frame);
  } catch (error) { stop(error.message || String(error)); }
})();
