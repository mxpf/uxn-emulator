'use strict';
(async () => {
  const $ = id => document.getElementById(id);
  const directions = [...document.querySelectorAll('[data-action]')];
  let machine, running = false, failed = false, previous = 0, flags = 0;
  const held = new Map();
  const repeatDelay = 250, repeatInterval = 100;
  let nextRepeat = 0;
  const stop = message => {
    running = false; failed = true; held.clear();
    $('error').textContent = message; $('error').hidden = false;
    for (const button of document.querySelectorAll('button')) button.disabled = button.id !== 'reset' || !machine;
    $('status').textContent = 'Stopped';
  };
  try {
    machine = await createNoEscape({locateFile: (path, prefix) => `${prefix}${path}?v=controls-2`});
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
      if (flags & 1) held.clear();
      if ((flags & 4) || ((flags & 1) && !(flags & 2))) running = false;
      const rejected = machine._no_escape_rejected();
      $('status').textContent = ((flags & 4) ? 'Replay verified — exact match.' : (flags & 2) ? (running ? 'Replaying…' : 'Replay paused.') : (flags & 1) ? 'Recording full. Replay or start a new game.' : running ? 'Recording your moves.' : 'Paused.') + (rejected ? ` ${rejected} input${rejected === 1 ? '' : 's'} rejected: queue full.` : '');
      $('pause').textContent = running ? 'Pause' : 'Resume';
      $('pause').disabled = !!((flags & 4) || ((flags & 1) && !(flags & 2)));
      $('replay').disabled = !!((flags & 2) && !(flags & 4));
      $('reset').disabled = false;
      for (const button of directions) button.disabled = !running || !!(flags & 1);
    }
    function reset() {
      held.clear();
      check(machine._no_escape_reset()); failed = false; $('error').hidden = true;
      for (let i = 0; i < 3; i++) check(machine._no_escape_step());
      running = true; previous = performance.now(); render();
    }
    function input(action) {
      if (!running || failed || (flags & 1)) return;
      // A burst can fill the external queue between animation frames. Give
      // this demo's three-node pipeline one bounded round to admit the next
      // discrete press. No unbounded JS queue or rejected-and-retried input.
      for (let i = 0; !machine._no_escape_input_ready() && i < 3; i++) check(machine._no_escape_step());
      if (machine._no_escape_flags() & 1) { render(); return; }
      if (!machine._no_escape_input_ready()) throw new Error('Movement could not be admitted after a scheduler round.');
      check(machine._no_escape_input(action));
      render();
    }
    function replay() { if (failed) return; held.clear(); check(machine._no_escape_replay()); running = true; previous = performance.now(); render(); }
    function pause() { if (failed) return; held.clear(); running = false; render(); }
    function toggle() { if (failed || $('pause').disabled) return; held.clear(); running = !running; previous = performance.now(); render(); }
    const safe = fn => (...args) => { try { fn(...args); } catch (error) { stop(error.message || String(error)); } };
    $('reset').addEventListener('click', safe(reset)); $('replay').addEventListener('click', safe(replay)); $('pause').addEventListener('click', safe(toggle));
    for (const button of directions) button.addEventListener('click', safe(() => input(Number(button.dataset.action))));
    document.addEventListener('keydown', safe(event => {
      if (event.altKey || event.ctrlKey || event.metaKey || event.target.closest('input,textarea,select,[contenteditable]')) return;
      const action = {ArrowUp:1, ArrowRight:2, ArrowDown:3, ArrowLeft:4}[event.key];
      if (action) {
        event.preventDefault();
        if (event.repeat || held.has(event.key) || !running || failed || (flags & 1)) return;
        held.set(event.key, action); nextRepeat = performance.now() + repeatDelay;
        input(action); return;
      }
      if (event.repeat) return;
      if (event.key.toLowerCase() === 'r') { event.preventDefault(); replay(); }
      if (event.key.toLowerCase() === 'n') { event.preventDefault(); reset(); }
      if (event.key.toLowerCase() === 'p') { event.preventDefault(); toggle(); }
    }));
    document.addEventListener('keyup', event => {
      if (held.delete(event.key)) nextRepeat = performance.now() + repeatInterval;
    });
    window.addEventListener('blur', pause);
    document.addEventListener('visibilitychange', () => { if (document.hidden) pause(); });
    function frame(now) {
      try {
        if (running && !failed && now - previous >= 16) {
          previous = now;
          for (let i = 0; i < 3; i++) check(machine._no_escape_step());
          // Sample held direction, not OS repeat events. A delayed frame emits
          // at most one repeat; it never catches up a backlog of missed ticks.
          if (held.size && now >= nextRepeat) {
            input([...held.values()].at(-1)); nextRepeat = now + repeatInterval;
          }
          render();
        }
      } catch (error) { stop(error.message || String(error)); }
      requestAnimationFrame(frame);
    }
    reset(); requestAnimationFrame(frame);
  } catch (error) { stop(error.message || String(error)); }
})();
