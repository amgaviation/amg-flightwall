// Setup — Wi-Fi scan/join. Served from the same bundle in captive-portal AP
// mode; also reachable from STA mode to re-provision.
import { api } from '../api.js';
import { el, field, toast } from '../ui.js';

export function renderSetup(root, state) {
  const netList = el('div', {}, el('p', { class: 'page-sub' }, 'Scanning…'));
  const ssidInput = el('input', { type: 'text', placeholder: 'network name', autocomplete: 'off' });
  const passInput = el('input', { type: 'password', placeholder: 'password (blank for open network)', autocomplete: 'off' });
  const joinBtn = el('button', {
    onclick: async () => {
      const ssid = ssidInput.value.trim();
      if (!ssid) { toast('Pick or type a network name', 'err'); return; }
      joinBtn.disabled = true;
      try {
        await api.wifiJoin(ssid, passInput.value);
        toast(`Joining ${ssid} — device is rebooting into station mode`, 'ok');
        netList.replaceChildren(el('div', { class: 'warn-box' },
          `Credentials stored. The device is rebooting and will join "${ssid}". `,
          'Reconnect your computer to the normal network, then open ',
          el('a', { href: 'http://flightwall.local' }, 'http://flightwall.local'), '.'));
      } catch (e) {
        toast(`Join failed: ${e.message}`, 'err');
      } finally {
        joinBtn.disabled = false;
      }
    },
  }, 'Join network');

  async function scan() {
    netList.replaceChildren(el('p', { class: 'page-sub' }, 'Scanning…'));
    try {
      const res = await api.wifiScan();
      const nets = (res.networks || []).slice().sort((a, b) => b.rssi - a.rssi);
      if (!nets.length) {
        netList.replaceChildren(el('p', { class: 'page-sub' }, 'No networks found — rescan.'));
        return;
      }
      netList.replaceChildren(...nets.map((n) => {
        const item = el('div', {
          class: 'wifi-net',
          onclick: () => {
            ssidInput.value = n.ssid;
            document.querySelectorAll('.wifi-net').forEach((x) => x.classList.remove('selected'));
            item.classList.add('selected');
            passInput.focus();
          },
        },
        el('span', { class: 'ssid' }, n.ssid),
        el('span', { class: 'meta' }, n.secure ? 'WPA' : 'open', ` · ${n.rssi} dBm`));
        return item;
      }));
    } catch (e) {
      netList.replaceChildren(el('p', { class: 'page-sub' }, `Scan failed: ${e.message}`));
    }
  }

  const inSetup = state.status && state.status.setup_mode;
  root.append(
    el('h1', { class: 'page-title' }, 'Wi-Fi Setup'),
    el('p', { class: 'page-sub' },
      inSetup
        ? 'The device is in access-point setup mode. Pick your hangar/home network below.'
        : 'Device is online — use this page to move it to a different network.'),
    el('div', { class: 'grid cols-2' },
      el('div', { class: 'card' },
        el('h2', {}, 'Networks'),
        netList,
        el('div', { class: 'btn-row' },
          el('button', { class: 'ghost', onclick: scan }, 'Rescan')),
      ),
      el('div', { class: 'card' },
        el('h2', {}, 'Join'),
        field('SSID', ssidInput),
        field('Password', passInput),
        el('div', { class: 'btn-row' }, joinBtn),
      ),
    ),
  );

  scan();
  return () => {};
}
