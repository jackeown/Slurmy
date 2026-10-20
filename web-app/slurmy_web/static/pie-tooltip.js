'use strict';

window.SlurmyPie = (() => {
  const charts = new WeakMap();

  function hitTest(slices, x, y) {
    if (x * x + y * y > 1 || (x === 0 && y === 0)) return null;
    // CSS conic gradients start at twelve o'clock and turn clockwise.
    const fraction = (Math.atan2(x, -y) / (2 * Math.PI) + 1) % 1;
    let end = 0;
    for (const slice of slices) {
      end += slice.fraction;
      if (fraction < end) return slice;
    }
    return null;
  }

  function setSegments(chart, slices) {
    chart.removeAttribute('title');
    let state = charts.get(chart);
    if (!state) {
      const tooltip = document.createElement('div');
      tooltip.className = 'pie-tooltip';
      tooltip.setAttribute('role', 'tooltip');
      tooltip.hidden = true;
      document.body.append(tooltip);
      state = {tooltip, slices: [], pointer: null};
      charts.set(chart, state);
      const hide = () => { tooltip.hidden = true; state.pointer = null; };
      state.update = () => {
        if (!state.pointer) return;
        const {x, y} = state.pointer, bounds = chart.getBoundingClientRect();
        const slice = bounds.width && bounds.height
          ? hitTest(state.slices, (x - bounds.left) / bounds.width * 2 - 1,
                    (y - bounds.top) / bounds.height * 2 - 1) : null;
        if (!slice) { tooltip.hidden = true; return; }
        tooltip.textContent = slice.label;
        tooltip.hidden = false;
        tooltip.style.left = `${Math.max(8, Math.min(x + 14, window.innerWidth - tooltip.offsetWidth - 8))}px`;
        tooltip.style.top = `${Math.max(8, Math.min(y + 14, window.innerHeight - tooltip.offsetHeight - 8))}px`;
      };
      chart.addEventListener('pointermove', event => {
        if (event.pointerType === 'touch') { hide(); return; }
        state.pointer = {x: event.clientX, y: event.clientY};
        state.update();
      });
      chart.addEventListener('pointerleave', hide);
      chart.addEventListener('pointercancel', hide);
      window.addEventListener('blur', hide);
      window.addEventListener('scroll', hide, true);
      window.addEventListener('resize', hide);
      document.addEventListener('keydown', event => { if (event.key === 'Escape') hide(); });
    }
    state.slices = slices;
    state.update();
  }

  return {setSegments, hitTest};
})();
