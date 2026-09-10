'use strict';
(async () => {
  const paper = document.getElementById('paper');
  const buttons = [...document.querySelectorAll('[data-action]')];
  const status = document.getElementById('status'), error = document.getElementById('error');
  const save = document.getElementById('save'), open = document.getElementById('open');
  const picker = document.getElementById('drawing-file'), fileStatus = document.getElementById('file-status');
  const controls = [...buttons, save, open];
  let machine, ready = false;
  const stop = reason => {
    ready = false; controls.forEach(button => { button.disabled = true; });
    status.textContent = 'Sketch stopped.'; error.textContent = reason; error.hidden = false;
  };
  try {
    machine = await createSketch({locateFile: (path, prefix) => `${prefix}${path}?v=documents-1`});
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
    save.disabled = open.disabled = false;
    save.addEventListener('click', () => {
      if (!ready) return;
      try {
        check(machine._sketch_export());
        const offset = machine._sketch_document(), length = machine._sketch_document_size();
        const blob = new Blob([machine.HEAPU8.slice(offset, offset + length)], {type:'application/octet-stream'});
        const url = URL.createObjectURL(blob), link = document.createElement('a');
        link.href = url; link.download = 'drawing.sketch'; document.body.append(link); link.click(); link.remove();
        setTimeout(() => URL.revokeObjectURL(url), 1000);
        fileStatus.textContent = 'Drawing download ready. Keep it to reopen later.';
      } catch (e) { fileStatus.textContent = `Could not save: ${e.message}`; }
    });
    open.addEventListener('click', () => { if (ready) picker.click(); });
    picker.addEventListener('change', async () => {
      const file = picker.files[0]; picker.value = '';
      if (!file || !ready) return;
      ready = false; controls.forEach(button => { button.disabled = true; });
      try {
        const length = machine._sketch_document_size();
        if (file.size !== length) throw new Error('Not a supported Sketchpad file.');
        const bytes = new Uint8Array(await file.arrayBuffer());
        machine.HEAPU8.set(bytes, machine._sketch_document());
        if (!machine._sketch_import(bytes.length)) {
          const fatal = machine.UTF8ToString(machine._sketch_error());
          if (fatal) { stop(fatal); return; }
          throw new Error('Not a supported Sketchpad file.');
        }
      } catch (e) {
        fileStatus.textContent = `${e.message} Your drawing is unchanged.`;
        ready = true; controls.forEach(button => { button.disabled = false; }); return;
      }
      try { render(); fileStatus.textContent = 'Drawing opened.'; }
      catch (e) { stop(e.message || String(e)); return; }
      ready = true; controls.forEach(button => { button.disabled = false; });
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
