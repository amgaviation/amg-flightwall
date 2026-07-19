// Dashboard — live panel preview (SSE frames), status cards, quick actions.
import { api, onEvent } from '../api.js';
import { el, field, checkRow, toast, fmtUptime, fmtBytes } from '../ui.js';
import { makeFrameRenderer } from '../frame.js';

const LEVELS = ['info', 'success', 'warn', 'alert'];

export function renderDashboard(root, state) {
  const canvas = el('canvas', { id: 'preview' });
  const frameStamp = el('span', {}, 'awaiting frames…');
  const sseStamp = el('span', {}, 'SSE: connecting');

  const cards = el('div', { class: 'grid cards' });

  // quick action: message
  const msgText = el('input', { type: 'text', placeholder: 'WELCOME N123AM', maxlength: '120' });
  const msgColor = el('input', { type: 'color', value: '#176CFF' });
  const msgDur = el('input', { type: 'number', value: '15', min: '1', max: '600' });
  const msgScroll = el('input', { type: 'checkbox', checked: true });

  // quick action: notify
  const ntfTitle = el('input', { type: 'text', placeholder: 'CREW ALERT', maxlength: '48' });
  const ntfBody = el('input', { type: 'text', placeholder: 'Wheels up 15 min', maxlength: '120' });
  const ntfLevel = el('select', {}, LEVELS.map((l) => el('option', { value: l }, l)));

  root.append(
    el('h1', { class: 'page-title' }, 'Dashboard'),
    el('p', { class: 'page-sub' }, 'Live panel mirror and device vitals.'),
    el('div', { class: 'grid cols-2' },
      el('div', {},
        el('div', { class: 'preview-wrap' }, canvas),
        el('div', { class: 'preview-meta' }, frameStamp, sseStamp),
      ),
      el('div', {},
        cards,
        el('div', { class: 'card', style: 'margin-top:14px' },
          el('h2', {}, 'Send Message'),
          field('Text', msgText),
          el('div', { class: 'row' },
            el('div', { class: 'narrow' }, field('Color', msgColor)),
            field('Duration (s)', msgDur),
            el('div', { class: 'narrow' }, field('Scroll', checkRow('scroll', msgScroll))),
          ),
          el('div', { class: 'btn-row' },
            el('button', {
              onclick: async () => {
                if (!msgText.value.trim()) { toast('Message text required', 'err'); return; }
                try {
                  await api.postMessage({
                    text: msgText.value.trim(),
                    color: msgColor.value,
                    duration_s: Number(msgDur.value) || 15,
                    scroll: msgScroll.checked,
                  });
                  toast('Message sent to panel', 'ok');
                } catch (e) { toast(`Message failed: ${e.message}`, 'err'); }
              },
            }, 'Show on panel'),
          ),
        ),
        el('div', { class: 'card', style: 'margin-top:14px' },
          el('h2', {}, 'Test Notification'),
          field('Title', ntfTitle),
          field('Body', ntfBody),
          el('div', { class: 'row' },
            field('Level', ntfLevel),
            el('div', { class: 'narrow', style: 'align-self:end' },
              el('button', {
                onclick: async () => {
                  if (!ntfTitle.value.trim()) { toast('Title required', 'err'); return; }
                  try {
                    await api.postNotify({
                      title: ntfTitle.value.trim(),
                      body: ntfBody.value.trim(),
                      level: ntfLevel.value,
                    });
                    toast('Notification fired', 'ok');
                  } catch (e) { toast(`Notify failed: ${e.message}`, 'err'); }
                },
              }, 'Fire'),
            ),
          ),
        ),
      ),
    ),
  );

  function statCard(label, valueNode, note) {
    return el('div', { class: 'card' },
      el('h2', {}, label),
      valueNode,
      note ? el('div', { class: 'stat-note' }, note) : null,
    );
  }

  function paintStatus(s) {
    if (!s) return;
    cards.replaceChildren(
      statCard('Wi-Fi',
        el('div', { class: `stat-value ${s.wifi && s.wifi.state === 'online' ? 'stat-ok' : 'stat-warn'}` },
          s.wifi ? s.wifi.state.toUpperCase() : '—'),
        s.wifi && s.wifi.ssid ? `${s.wifi.ssid} ${s.wifi.rssi ?? ''} dBm` : 'not connected'),
      statCard('Uptime', el('div', { class: 'stat-value' }, fmtUptime(s.uptime_s || 0)), `fw ${s.version || '?'}`),
      statCard('Heap', el('div', { class: 'stat-value stat-accent' }, fmtBytes(s.heap || 0)), 'free'),
      statCard('Scene', el('div', { class: 'stat-value' }, (s.scene || '—').toUpperCase()),
        s.time_synced ? 'clock synced' : 'clock NOT synced'),
    );
  }

  const renderFrame = makeFrameRenderer(canvas);
  let frames = 0;
  const offFrame = onEvent('frame', (b64) => {
    if (renderFrame(b64)) {
      frames += 1;
      frameStamp.textContent = `128×64 RGB565 · frame ${frames}`;
    }
  });
  const offStatus = onEvent('status', paintStatus);
  const offSse = onEvent('sse-state', (up) => {
    sseStamp.textContent = up ? 'SSE: live' : 'SSE: reconnecting…';
  });

  paintStatus(state.status);
  // top up immediately in case SSE status cadence (5 s) hasn't fired yet
  api.getStatus().then(paintStatus).catch(() => {});

  return () => { offFrame(); offStatus(); offSse(); };
}
