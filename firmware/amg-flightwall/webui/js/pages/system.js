// System — device detail, live log tail (SSE), OTA upload with progress,
// reboot, admin password set/change.
import { api, onEvent, uploadOta, sha256Hex, storeAuth } from '../api.js';
import { el, field, toast, confirmModal, fmtUptime, fmtBytes } from '../ui.js';

const HEALTH_ORDER = ['display', 'configuration', 'network', 'storage', 'application'];

export function renderSystem(root, state) {
  const kv = el('table', { class: 'kv' });
  const healthTable = el('table', { class: 'kv' });
  const logView = el('div', { id: 'log-view' });
  let autoscroll = true;
  logView.addEventListener('scroll', () => {
    autoscroll = logView.scrollTop + logView.clientHeight >= logView.scrollHeight - 8;
  });

  function logLine(line) {
    const cls = /\b(err|error|fail)/i.test(line) ? 'log-err'
      : /\b(warn|degraded|retry)/i.test(line) ? 'log-warn' : '';
    const node = el('div', { class: cls }, line);
    logView.append(node);
    while (logView.childElementCount > 400) logView.firstElementChild.remove();
    if (autoscroll) logView.scrollTop = logView.scrollHeight;
  }

  function paintStatus(s) {
    if (!s) return;
    kv.replaceChildren(
      row('Firmware', s.version || '—'),
      row('Uptime', fmtUptime(s.uptime_s || 0)),
      row('Free heap', fmtBytes(s.heap || 0)),
      row('Wi-Fi', s.wifi ? `${s.wifi.state} · ${s.wifi.ssid || '—'} · ${s.wifi.ip || '—'}` : '—'),
      row('RSSI', s.wifi && s.wifi.rssi !== undefined ? `${s.wifi.rssi} dBm` : '—'),
      row('Active scene', s.scene || '—'),
      row('Time synced', s.time_synced ? 'yes' : 'NO'),
    );
    const h = s.health || {};
    const subs = h.subsystems || {};
    healthTable.replaceChildren(
      row('Overall', healthCell(h.overall || 'unknown')),
      ...HEALTH_ORDER.map((name) => row(name, healthCell(subs[name] || 'unknown'))),
    );
  }

  function row(k, v) {
    return el('tr', {}, el('td', {}, k), el('td', {}, v));
  }

  function healthCell(level) {
    return el('span', {},
      el('span', { class: `health-dot health-${level}` }), level);
  }

  // --- OTA ---
  const fileInput = el('input', { type: 'file', accept: '.bin,application/octet-stream' });
  const bar = el('div', {});
  const progress = el('div', { class: 'progress' }, bar);
  const otaStatus = el('div', { class: 'stat-note' }, 'Select the firmware.bin built for hd_wf2_live.');
  const otaBtn = el('button', {
    onclick: async () => {
      const file = fileInput.files && fileInput.files[0];
      if (!file) { toast('Choose a firmware .bin first', 'err'); return; }
      const go = await confirmModal('Flash firmware?',
        `Upload ${file.name} (${fmtBytes(file.size)}) to the inactive OTA slot? The device reboots into it on success.`,
        'Upload & flash', true);
      if (!go) return;
      otaBtn.disabled = true;
      otaStatus.textContent = 'Uploading…';
      try {
        const res = await uploadOta(file, (frac) => {
          bar.style.width = `${Math.round(frac * 100)}%`;
          otaStatus.textContent = `Uploading… ${Math.round(frac * 100)}%`;
        });
        otaStatus.textContent = `Flashed OK — sha256 ${String(res.sha256 || '').slice(0, 16)}… Device rebooting.`;
        toast('OTA complete — device rebooting', 'ok');
      } catch (e) {
        otaStatus.textContent = `OTA failed: ${e.message}`;
        toast(`OTA failed: ${e.message}`, 'err');
      } finally {
        otaBtn.disabled = false;
      }
    },
  }, 'Upload & flash');

  // --- admin password ---
  const newPass = el('input', { type: 'password', placeholder: 'new admin password', autocomplete: 'new-password' });
  const newPass2 = el('input', { type: 'password', placeholder: 'repeat', autocomplete: 'new-password' });

  root.append(
    el('h1', { class: 'page-title' }, 'System'),
    el('p', { class: 'page-sub' }, 'Device state, live logs, firmware updates, and access control.'),
    el('div', { class: 'grid cols-2' },
      el('div', { class: 'card' }, el('h2', {}, 'Device'), kv),
      el('div', { class: 'card' }, el('h2', {}, 'Health'), healthTable),
      el('div', { class: 'card' },
        el('h2', {}, 'OTA Update'),
        field('Firmware image', fileInput),
        progress, otaStatus,
        el('div', { class: 'btn-row' }, otaBtn),
      ),
      el('div', { class: 'card' },
        el('h2', {}, 'Admin Password'),
        el('p', { class: 'page-sub' },
          state.status && state.status.password_set
            ? 'Password is set. Changing it signs out other browsers.'
            : 'No password set — the device is unprotected until you create one.'),
        field('New password', newPass),
        field('Repeat', newPass2),
        el('div', { class: 'btn-row' },
          el('button', {
            onclick: async () => {
              const p = newPass.value;
              if (p.length < 8) { toast('Use at least 8 characters', 'err'); return; }
              if (p !== newPass2.value) { toast('Passwords do not match', 'err'); return; }
              try {
                await api.postSecrets({ admin_password: p });
                storeAuth(await sha256Hex(p));
                newPass.value = ''; newPass2.value = '';
                if (state.status) state.status.password_set = true;
                toast('Admin password updated', 'ok');
              } catch (e) { toast(`Password change failed: ${e.message}`, 'err'); }
            },
          }, state.status && state.status.password_set ? 'Change password' : 'Set password'),
        ),
      ),
    ),
    el('div', { class: 'card', style: 'margin-top:14px' },
      el('h2', {}, 'Logs — live tail'),
      logView,
      el('div', { class: 'btn-row' },
        el('button', {
          class: 'ghost',
          onclick: async () => {
            try {
              const text = await api.getLogs();
              logView.replaceChildren();
              String(text).split('\n').filter(Boolean).forEach(logLine);
            } catch (e) { toast(`Log fetch failed: ${e.message}`, 'err'); }
          },
        }, 'Reload buffer'),
        el('button', { class: 'ghost', onclick: () => logView.replaceChildren() }, 'Clear view'),
        el('button', {
          class: 'danger',
          onclick: async () => {
            const go = await confirmModal('Reboot device?', 'The panel blanks for a few seconds while the device restarts.', 'Reboot', true);
            if (!go) return;
            try { await api.reboot(); toast('Reboot requested', 'ok'); }
            catch (e) { toast(`Reboot failed: ${e.message}`, 'err'); }
          },
        }, 'Reboot device'),
      ),
    ),
  );

  // initial fills
  paintStatus(state.status);
  api.getStatus().then((s) => { state.status = s; paintStatus(s); }).catch(() => {});
  api.getLogs().then((text) => {
    String(text).split('\n').filter(Boolean).forEach(logLine);
  }).catch(() => {});

  const offLog = onEvent('log', logLine);
  const offStatus = onEvent('status', (s) => { state.status = s; paintStatus(s); });
  return () => { offLog(); offStatus(); };
}
