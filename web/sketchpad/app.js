'use strict';
(async () => {
  const paper = document.getElementById('paper');
  const buttons = [...document.querySelectorAll('[data-action]')];
  const status = document.getElementById('status'), error = document.getElementById('error');
  let machine, ready = false;
  const stop = reason => {
    ready = false; buttons.forEach(button => { button.disabled = true; });
    status.textContent = 'Sketch stopped.'; error.textContent = reason; error.hidden = false;
  };
  try {
    machine = await createSketch({locateFile: (path, prefix) => `${prefix}${path}?v=toggles-1`});
    const context = paper.getContext('2d');
    if (!context) throw new Error('Canvas is unavailable.');
    const image = context.createImageData(128, 96);
    const check = ok => { if (!ok) throw new Error(machine.UTF8ToString(machine._sketch_error()) || 'The sketch could not accept this action.'); };
    const render = () => {
      const offset = machine._sketch_pixels() >>> 2;
      for (let i = 0; i < 128 * 96; i++) {
        const pixel = machine.HEAPU32[offset + i];
        image.data[i * 4] = pixel >>> 16 & 255; image.data[i * 4 + 1] = pixel >>> 8 & 255;
        image.data[i * 4 + 2] = pixel & 255; image.data[i * 4 + 3] = 255;
      }
      context.putImageData(image, 0, 0);
      const mode = machine._sketch_mode();
      document.querySelector('[data-action="5"]').setAttribute('aria-pressed', String(mode === 1));
      document.querySelector('[data-action="6"]').setAttribute('aria-pressed', String(mode === 2));
      const message = ['Move only — both tools are off.', 'Draw on — moving leaves a trail.', 'Erase on — moving removes marks.'][mode];
      if (status.textContent !== message) status.textContent = message;
    };
    const apply = action => {
      if (!ready) return;
      try { check(machine._sketch_action(action)); render(); }
      catch (e) { stop(e.message || String(e)); }
    };
    check(machine._sketch_reset()); render(); ready = true;
    buttons.forEach(button => {
      button.disabled = false;
      button.addEventListener('click', () => apply(Number(button.dataset.action)));
    });
    document.addEventListener('keydown', event => {
      if (event.altKey || event.ctrlKey || event.metaKey || event.target.closest('input,textarea,select,[contenteditable]')) return;
      // Keep native Space/Enter activation when a button has keyboard focus.
      if (event.target.closest('button') && (event.key === ' ' || event.key === 'Enter')) return;
      const action = {ArrowUp:1, ArrowRight:2, ArrowDown:3, ArrowLeft:4, ' ':5, x:6, X:6}[event.key];
      if (!action) return;
      event.preventDefault(); if (!event.repeat) apply(action);
    });
  } catch (e) { stop(e.message || String(e)); }
})();
