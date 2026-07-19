// Playlist — enable / reorder / duration per scene, plus per-scene settings
// forms driven by the GET /api/scenes catalog ("fields" reference config paths).
import { api } from '../api.js';
import { el, field, checkRow, toast } from '../ui.js';

function getPath(obj, path) {
  return path.split('.').reduce((o, k) => (o == null ? undefined : o[k]), obj);
}

function setPath(obj, path, value) {
  const keys = path.split('.');
  let o = obj;
  for (const k of keys.slice(0, -1)) {
    if (typeof o[k] !== 'object' || o[k] === null) o[k] = {};
    o = o[k];
  }
  o[keys.at(-1)] = value;
}

// One input control per catalog field type; writes into cfg on change.
function fieldControl(f, cfg) {
  const current = getPath(cfg, f.key);
  switch (f.type) {
    case 'number': {
      const input = el('input', {
        type: 'number', value: current ?? 0,
        min: f.min ?? null, max: f.max ?? null, step: f.step ?? null,
        onchange: () => setPath(cfg, f.key, Number(input.value)),
      });
      return field(f.label, input);
    }
    case 'bool': {
      const input = el('input', {
        type: 'checkbox', checked: !!current,
        onchange: () => setPath(cfg, f.key, input.checked),
      });
      return checkRow(f.label, input);
    }
    case 'color': {
      const input = el('input', {
        type: 'color', value: current || '#176CFF',
        onchange: () => setPath(cfg, f.key, input.value),
      });
      return field(f.label, input);
    }
    case 'select': {
      const input = el('select', {
        onchange: () => setPath(cfg, f.key, input.value),
      }, (f.options || []).map((o) => el('option', { value: o, selected: o === current }, o)));
      return field(f.label, input);
    }
    case 'csv': {
      const input = el('input', {
        type: 'text', value: Array.isArray(current) ? current.join(', ') : '',
        placeholder: 'N123AM, N456XY',
        onchange: () => setPath(cfg, f.key,
          input.value.split(',').map((s) => s.trim().toUpperCase()).filter(Boolean)),
      });
      return field(f.label, input);
    }
    case 'datetime': {
      // stored as unix epoch seconds
      const dt = current ? new Date(current * 1000) : null;
      const iso = dt && current > 0
        ? new Date(dt.getTime() - dt.getTimezoneOffset() * 60000).toISOString().slice(0, 16)
        : '';
      const input = el('input', {
        type: 'datetime-local', value: iso,
        onchange: () => setPath(cfg, f.key,
          input.value ? Math.floor(new Date(input.value).getTime() / 1000) : 0),
      });
      return field(f.label, input);
    }
    default: {
      const input = el('input', {
        type: 'text', value: current ?? '', maxlength: f.max_len ?? null,
        onchange: () => setPath(cfg, f.key, input.value),
      });
      return field(f.label, input);
    }
  }
}

export function renderPlaylist(root, state) {
  const cfg = state.config;
  const catalog = state.scenes || [];
  const byId = Object.fromEntries(catalog.map((s) => [s.id, s]));
  const list = el('div', {});

  function move(i, delta) {
    const j = i + delta;
    if (j < 0 || j >= cfg.playlist.length) return;
    const tmp = cfg.playlist[i];
    cfg.playlist[i] = cfg.playlist[j];
    cfg.playlist[j] = tmp;
    paint();
  }

  function paint() {
    list.replaceChildren(...cfg.playlist.map((slot, i) => {
      const meta = byId[slot.id] || { name: slot.id, fields: [] };
      const enabled = el('input', {
        type: 'checkbox', checked: !!slot.enabled,
        onchange: () => { slot.enabled = enabled.checked; paint(); },
      });
      const dur = el('input', {
        class: 'dur', type: 'number', min: '3', max: '600', value: slot.duration_s,
        onchange: () => { slot.duration_s = Math.max(3, Number(dur.value) || 10); },
      });
      const row = el('div', { class: `slot ${slot.enabled ? '' : 'disabled'}` },
        enabled,
        el('div', { class: 'slot-name' }, meta.name,
          el('div', { class: 'slot-id' }, slot.id)),
        dur, el('span', { class: 'dur-unit' }, 's'),
        el('div', { class: 'slot-btns' },
          el('button', { class: 'ghost small', title: 'Move up', disabled: i === 0, onclick: () => move(i, -1) }, '▲'),
          el('button', { class: 'ghost small', title: 'Move down', disabled: i === cfg.playlist.length - 1, onclick: () => move(i, 1) }, '▼'),
          el('button', {
            class: 'ghost small', title: 'Show now',
            onclick: async () => {
              try { await api.activateScene(slot.id, slot.duration_s); toast(`Showing ${meta.name}`, 'ok'); }
              catch (e) { toast(`Activate failed: ${e.message}`, 'err'); }
            },
          }, '▶'),
        ),
        (meta.fields && meta.fields.length)
          ? el('div', { class: 'scene-form' }, meta.fields.map((f) => fieldControl(f, cfg)))
          : null,
      );
      return el('div', { class: 'slot-outer' }, row);
    }));
  }

  paint();

  root.append(
    el('h1', { class: 'page-title' }, 'Playlist'),
    el('p', { class: 'page-sub' }, 'Rotation order, dwell time, and per-scene settings. ▶ shows a scene immediately.'),
    list,
    el('div', { class: 'btn-row' },
      el('button', {
        onclick: async () => {
          try {
            const res = await api.putConfig(cfg);
            toast('Playlist saved', 'ok');
            if (res && res.reboot_required) toast('Reboot required for some changes', 'err');
            state.config = cfg;
          } catch (e) { toast(`Save failed: ${e.message}`, 'err'); }
        },
      }, 'Save playlist'),
      el('button', { class: 'ghost', onclick: () => state.reload() }, 'Discard changes'),
    ),
  );
  return () => {};
}
