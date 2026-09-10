'use strict';
(async () => {
  const canvas = document.querySelector('canvas'), status = document.querySelector('#status'),
        narration = document.querySelector('#narration');
  const actions = [...document.querySelectorAll('[data-action]')],
        replay = document.querySelector('#replay'), restart = document.querySelector('#restart');
  let m, timer = null;
  const controls = () => {
    const disabled = !!m._mystery_failed(), playing = !!m._mystery_playing();
    actions.forEach(b => b.disabled = disabled || playing || !!m._mystery_sealed());
    replay.disabled = disabled || playing;
    restart.disabled = false;
    canvas.setAttribute('aria-disabled', String(disabled || playing));
  };
  const fail = e => {
    if (timer) clearInterval(timer);
    timer = null;
    status.textContent = e.message || String(e);
    [...actions, replay].forEach(b => b.disabled = true);
  };
  try {
    m = await createMystery();
    const ctx = canvas.getContext('2d');
    if (!ctx) throw Error('Canvas unavailable.');
    const image = ctx.createImageData(256, 192);
    const check = ok => {
      if (!ok)
        throw Error(m.UTF8ToString(m._mystery_error()) || 'This action could not be completed.');
    };
    const render = () => {
      const ptr = m._mystery_pixels() >>> 2;
      for (let i = 0; i < 256 * 192; i++) {
        const p = m.HEAPU32[ptr + i];
        image.data[i * 4] = p >>> 16 & 255;
        image.data[i * 4 + 1] = p >>> 8 & 255;
        image.data[i * 4 + 2] = p & 255;
        image.data[i * 4 + 3] = 255;
      }
      ctx.putImageData(image, 0, 0);
      narration.textContent = m.UTF8ToString(m._mystery_text());
      controls();
    };
    const reset = () => {
      if (timer) clearInterval(timer);
      timer = null;
      check(m._mystery_reset());
      render();
      status.textContent = 'Session recorded locally. Nothing is uploaded.';
    };
    const act = fn => {
      if (m._mystery_playing() || m._mystery_failed() || m._mystery_sealed()) return;
      try {
        const ok = fn();
        render();
        if (ok)
          status.textContent = 'Session recorded locally. Nothing is uploaded.';
        else if (m._mystery_sealed())
          status.textContent = m.UTF8ToString(m._mystery_error());
        else
          check(ok);
      } catch (e) {
        fail(e);
      }
    };
    actions.forEach(
        b => b.addEventListener(
            'click', () => act(() => m._mystery_action(Number(b.dataset.action)))));
    canvas.addEventListener('click', e => {
      const r = canvas.getBoundingClientRect(),
            x = Math.floor((e.clientX - r.left) * 256 / r.width),
            y = Math.floor((e.clientY - r.top) * 192 / r.height);
      if (x >= 0 && x < 256 && y >= 0 && y < 192) act(() => m._mystery_click(x, y));
    });
    const play = () => {
      if (m._mystery_playing()) return;
      try {
        check(m._mystery_play());
        render();
        status.textContent = 'Replaying recorded actions…';
        timer = setInterval(() => {
          try {
            check(m._mystery_tick());
            render();
            if (!m._mystery_playing()) {
              clearInterval(timer);
              timer = null;
              status.textContent = 'Replay verified: identical state, messages and pixels.';
            }
          } catch (e) {
            fail(e);
          }
        }, 450);
      } catch (e) {
        fail(e);
      }
    };
    replay.addEventListener('click', play);
    restart.addEventListener('click', () => {
      try {
        reset();
      } catch (e) {
        fail(e);
      }
    });
    document.addEventListener('keydown', e => {
      if (e.repeat || e.metaKey || e.ctrlKey || e.altKey ||
          e.target.closest('input,textarea,select,[contenteditable]'))
        return;
      if (/^[1-9]$/.test(e.key)) {
        e.preventDefault();
        act(() => m._mystery_action(Number(e.key)));
      } else if (e.key.toLowerCase() === 'r') {
        e.preventDefault();
        play();
      } else if (e.key.toLowerCase() === 'n') {
        e.preventDefault();
        try {
          reset();
        } catch (error) {
          fail(error);
        }
      }
    });
    reset();
  } catch (e) {
    fail(e);
  }
})();
