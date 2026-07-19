// Display — brightness, night mode schedule, and the advanced geometry panel
// (geometry changes flag reboot_required).
import { api } from '../api.js';
import { el, field, checkRow, toast } from '../ui.js';

const DRIVERS = ['FM6126A', 'FM6124', 'SHIFTREG', 'ICN2038S', 'MBI5124', 'DP3246_SM5368'];

export function renderDisplay(root, state) {
  const cfg = state.config;
  const d = cfg.display;
  const g = d.geometry;

  const brightness = el('input', { type: 'range', min: '0', max: '255', value: d.brightness });
  const brightnessVal = el('span', { class: 'stat-note' }, String(d.brightness));
  brightness.addEventListener('input', () => {
    d.brightness = Number(brightness.value);
    brightnessVal.textContent = brightness.value;
  });

  const nightB = el('input', { type: 'range', min: '0', max: '255', value: d.night_brightness });
  const nightBVal = el('span', { class: 'stat-note' }, String(d.night_brightness));
  nightB.addEventListener('input', () => {
    d.night_brightness = Number(nightB.value);
    nightBVal.textContent = nightB.value;
  });

  const nightStart = el('input', { type: 'time', value: d.night_start, onchange: () => { d.night_start = nightStart.value; } });
  const nightEnd = el('input', { type: 'time', value: d.night_end, onchange: () => { d.night_end = nightEnd.value; } });
  const offIdle = el('input', { type: 'checkbox', checked: !!d.off_when_idle, onchange: () => { d.off_when_idle = offIdle.checked; } });

  const num = (key, min, max) => el('input', {
    type: 'number', value: g[key], min, max,
    onchange: (e) => { g[key] = Number(e.target.value); },
  });
  const panelW = num('panel_w', 8, 256);
  const panelH = num('panel_h', 8, 128);
  const chain = num('chain', 1, 8);
  const latch = num('latch_blanking', 0, 4);
  const minRefresh = num('min_refresh', 30, 240);
  const driver = el('select', { onchange: () => { g.driver = driver.value; } },
    DRIVERS.map((x) => el('option', { value: x, selected: x === g.driver }, x)));
  const clkphase = el('input', { type: 'checkbox', checked: !!g.clkphase, onchange: () => { g.clkphase = clkphase.checked; } });

  root.append(
    el('h1', { class: 'page-title' }, 'Display'),
    el('p', { class: 'page-sub' }, 'Panel brightness, night dimming, and low-level HUB75 geometry.'),
    el('div', { class: 'grid cols-2' },
      el('div', { class: 'card' },
        el('h2', {}, 'Brightness'),
        field('Day brightness', el('div', {}, brightness, brightnessVal)),
        field('Night brightness', el('div', {}, nightB, nightBVal)),
        el('div', { class: 'row' },
          field('Night starts', nightStart),
          field('Night ends', nightEnd),
        ),
        checkRow('Panel off when idle (no playlist content)', offIdle),
      ),
      el('div', { class: 'card' },
        el('h2', {}, 'Panel Geometry — advanced'),
        el('div', { class: 'warn-box' },
          'Changing geometry re-initializes the HUB75 driver and requires a reboot. ',
          'Wrong values can blank the panel until corrected — the web UI stays reachable.'),
        el('details', { class: 'advanced', open: true },
          el('summary', {}, 'HUB75 driver parameters'),
          el('div', { class: 'row' },
            field('Panel W', panelW), field('Panel H', panelH), field('Chain', chain)),
          el('div', { class: 'row' },
            field('Driver', driver),
            field('Latch blanking', latch),
            field('Min refresh Hz', minRefresh)),
          checkRow('Clock phase (clkphase)', clkphase),
        ),
      ),
    ),
    el('div', { class: 'btn-row', style: 'margin-top:16px' },
      el('button', {
        onclick: async () => {
          try {
            const res = await api.putConfig(cfg);
            state.config = cfg;
            if (res && res.reboot_required) {
              toast('Saved — reboot required to apply geometry', 'err');
            } else {
              toast('Display settings applied live', 'ok');
            }
          } catch (e) { toast(`Save failed: ${e.message}`, 'err'); }
        },
      }, 'Save display settings'),
      el('button', { class: 'ghost', onclick: () => state.reload() }, 'Discard changes'),
    ),
  );
  return () => {};
}
