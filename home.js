'use strict';
(() => {
  const carousel = document.querySelector('.carousel');
  const viewport = carousel.querySelector('.carousel-window');
  const track = carousel.querySelector('.cartridge-track');
  const originals = [...track.children];
  const count = originals.length;
  if (count < 2) return;
  const reducedMotion = matchMedia('(prefers-reduced-motion: reduce)');
  const desktop = matchMedia('(min-width: 761px) and (hover: hover) and (pointer: fine)');
  const rotation = carousel.querySelector('.rotation');
  // Three copies give both edges real neighbors; resets happen between
  // visually identical positions. Only the centered original is tabbable.
  const clone = link => {
    const copy = link.cloneNode(true);
    copy.dataset.clone = '';
    copy.setAttribute('aria-hidden', 'true');
    copy.tabIndex = -1;
    return copy;
  };
  track.prepend(...originals.map(clone));
  track.append(...originals.map(clone));
  for (const img of track.querySelectorAll('img')) img.draggable = false;
  // Links are natively draggable too; keep a swipe inside our pointer flow.
  viewport.addEventListener('dragstart', event => event.preventDefault());
  let index = count, busy = false, timer, pointer, suppressClick = false, clickTimer;
  let drift = 0, step = 0, center = 0, hovered = false, focused = false, paused = false, lastTime;
  const measure = () => {
    const width = originals[0].getBoundingClientRect().width;
    const gap = parseFloat(getComputedStyle(track).columnGap);
    step = width + gap;
    center = viewport.clientWidth / 2 - width / 2;
  };
  const render = (animate = false, offset = 0) => {
    track.style.transition = animate && !reducedMotion.matches ? 'transform 320ms cubic-bezier(.22,.7,.2,1)' : 'none';
    track.style.transform = `translateX(${center - index * step - drift + offset}px)`;
  };
  const update = (announce = true) => {
    const active = (index + (drift > step / 2 ? 1 : 0)) % count;
    const moveFocus = originals.includes(document.activeElement);
    originals.forEach((link,i) => {
      link.tabIndex = i === active ? 0 : -1;
      link.toggleAttribute('data-current', i === active);
      if (i === active) link.removeAttribute('aria-hidden');
      else link.setAttribute('aria-hidden', 'true');
    });
    carousel.querySelector('.position').textContent = `${active + 1} / ${count}`;
    if (announce) carousel.querySelector('.carousel-status').textContent = `${originals[active].getAttribute('aria-label')}, ${active + 1} of ${count}`;
    if (moveFocus) originals[active].focus({preventScroll:true});
  };
  const finish = () => {
    clearTimeout(timer);
    index = count + ((index % count) + count) % count;
    drift = 0;
    busy = false;
    carousel.removeAttribute('data-moving');
    render();
    update();
  };
  const move = direction => {
    if (busy) return;
    busy = true;
    carousel.setAttribute('data-moving', '');
    index += direction;
    drift = 0;
    render(true);
    if (reducedMotion.matches) finish();
    else timer = setTimeout(finish, 380);
  };
  track.addEventListener('transitionend', event => {
    if (event.target === track && event.propertyName === 'transform' && busy) finish();
  });
  carousel.querySelector('.previous').addEventListener('click', () => move(-1));
  carousel.querySelector('.next').addEventListener('click', () => move(1));
  carousel.addEventListener('keydown', event => {
    if (event.altKey || event.ctrlKey || event.metaKey || !['ArrowLeft','ArrowRight'].includes(event.key)) return;
    event.preventDefault();
    move(event.key === 'ArrowRight' ? 1 : -1);
  });
  carousel.addEventListener('mouseenter', () => { hovered = true; });
  carousel.addEventListener('mouseleave', () => { hovered = false; });
  carousel.addEventListener('focusin', () => { focused = true; });
  carousel.addEventListener('focusout', event => { focused = carousel.contains(event.relatedTarget); });
  const configureRotation = () => {
    rotation.hidden = !desktop.matches || reducedMotion.matches;
    lastTime = undefined;
    finish();
  };
  rotation.addEventListener('click', () => {
    paused = !paused;
    rotation.textContent = paused ? '▷' : 'Ⅱ';
    rotation.setAttribute('aria-label', paused ? 'Resume automatic movement' : 'Pause automatic movement');
  });
  viewport.addEventListener('pointerdown', event => {
    if (!event.isPrimary || event.button !== 0 || busy) return;
    pointer = {id:event.pointerId, x:event.clientX, y:event.clientY, dx:0, dragging:false};
  });
  viewport.addEventListener('pointermove', event => {
    if (!pointer || event.pointerId !== pointer.id) return;
    const dx = event.clientX - pointer.x, dy = event.clientY - pointer.y;
    if (!pointer.dragging) {
      if (Math.abs(dy) > 12 && Math.abs(dy) > Math.abs(dx)) { pointer = null; return; }
      if (Math.abs(dx) < 12) return;
      pointer.dragging = true;
      viewport.setPointerCapture(event.pointerId);
    }
    pointer.dx = Math.max(-viewport.clientWidth, Math.min(viewport.clientWidth, dx));
    render(false, pointer.dx);
  });
  const release = event => {
    if (!pointer || event.pointerId !== pointer.id) return;
    const {dragging, dx} = pointer;
    pointer = null;
    if (viewport.hasPointerCapture(event.pointerId)) viewport.releasePointerCapture(event.pointerId);
    if (!dragging) return;
    suppressClick = true;
    clearTimeout(clickTimer);
    clickTimer = setTimeout(() => { suppressClick = false; }, 400);
    if (event.type === 'pointerup' && Math.abs(dx) >= 40) move(dx < 0 ? 1 : -1);
    else render(true);
  };
  viewport.addEventListener('pointerup', release);
  viewport.addEventListener('pointercancel', release);
  viewport.addEventListener('lostpointercapture', event => {
    // Touch initially captures the image implicitly. Its bubbling release
    // when capture transfers to the viewport is not the end of the swipe.
    if (event.target === viewport) release(event);
  });
  viewport.addEventListener('click', event => {
    if (suppressClick || busy) { event.preventDefault(); event.stopPropagation(); suppressClick = false; }
  }, true);
  carousel.classList.add('ready');
  carousel.querySelector('.carousel-controls').hidden = false;
  measure();
  render();
  update(false);
  new ResizeObserver(() => { pointer = null; measure(); finish(); }).observe(viewport);
  window.addEventListener('pageshow', finish);
  reducedMotion.addEventListener('change', configureRotation);
  desktop.addEventListener('change', configureRotation);
  configureRotation();
  const tick = now => {
    const elapsed = lastTime === undefined ? 0 : Math.min(now - lastTime, 48);
    lastTime = now;
    const moving = desktop.matches && !reducedMotion.matches && !document.hidden &&
      !paused && !hovered && !focused && !busy && !pointer;
    if (moving) {
      // Slow, continuous motion; no layout reads or live announcements per frame.
      drift += elapsed * .018;
      if (drift >= step) {
        drift -= step;
        index = count + (index + 1) % count;
      }
      render();
      const active = originals[(index + (drift > step / 2 ? 1 : 0)) % count];
      if (!active.hasAttribute('data-current')) update(false);
    }
    requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);
})();
