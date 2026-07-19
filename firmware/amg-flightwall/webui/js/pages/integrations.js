// Integrations — location, METAR, flight provider/watchlist, AMG bridge.
// Secrets (bridge token) go through POST /api/secrets, never /api/config.
import { api } from '../api.js';
import { el, field, checkRow, toast } from '../ui.js';

export function renderIntegrations(root, state) {
  const cfg = state.config;

  const lat = el('input', { type: 'number', step: 'any', value: cfg.location.lat, onchange: () => { cfg.location.lat = Number(lat.value); } });
  const lon = el('input', { type: 'number', step: 'any', value: cfg.location.lon, onchange: () => { cfg.location.lon = Number(lon.value); } });
  const radius = el('input', { type: 'number', min: '5', max: '250', value: cfg.location.radius_nm, onchange: () => { cfg.location.radius_nm = Number(radius.value); } });

  const provider = el('select', { onchange: () => { cfg.flights.provider = provider.value; } },
    ['adsblol', 'opensky'].map((p) => el('option', { value: p, selected: p === cfg.flights.provider }, p)));
  const flightPoll = el('input', { type: 'number', min: '5', max: '600', value: cfg.flights.poll_s, onchange: () => { cfg.flights.poll_s = Number(flightPoll.value); } });
  const watchlist = el('input', {
    type: 'text', value: (cfg.flights.watchlist || []).join(', '), placeholder: 'N123AM, N456XY',
    onchange: () => {
      cfg.flights.watchlist = watchlist.value.split(',').map((s) => s.trim().toUpperCase()).filter(Boolean);
    },
  });

  const station = el('input', {
    type: 'text', value: cfg.metar.station, maxlength: '8',
    onchange: () => { cfg.metar.station = station.value.trim().toUpperCase(); },
  });
  const metarPoll = el('input', { type: 'number', min: '60', max: '3600', value: cfg.metar.poll_s, onchange: () => { cfg.metar.poll_s = Number(metarPoll.value); } });

  const amgUrl = el('input', {
    type: 'text', value: cfg.amg.base_url, placeholder: 'https://amgaviationgroup.com',
    onchange: () => { cfg.amg.base_url = amgUrl.value.trim(); },
  });
  const amgPoll = el('input', { type: 'number', min: '15', max: '3600', value: cfg.amg.poll_s, onchange: () => { cfg.amg.poll_s = Number(amgPoll.value); } });
  const notifySub = el('input', { type: 'checkbox', checked: !!cfg.amg.notify_on_submission, onchange: () => { cfg.amg.notify_on_submission = notifySub.checked; } });
  const notifyReq = el('input', { type: 'checkbox', checked: !!cfg.amg.notify_on_request, onchange: () => { cfg.amg.notify_on_request = notifyReq.checked; } });

  const tokenInput = el('input', { type: 'password', placeholder: cfg.amg.token_set ? '••••••••  (token set — enter to replace)' : 'paste bridge token', autocomplete: 'off' });

  const tz = el('input', { type: 'text', value: cfg.time.tz, onchange: () => { cfg.time.tz = tz.value.trim(); } });
  const ntp = el('input', { type: 'text', value: cfg.time.ntp, onchange: () => { cfg.time.ntp = ntp.value.trim(); } });

  root.append(
    el('h1', { class: 'page-title' }, 'Integrations'),
    el('p', { class: 'page-sub' }, 'Data sources feeding the panel: flights, weather, and the AMG operations bridge.'),
    el('div', { class: 'grid cols-2' },
      el('div', { class: 'card' },
        el('h2', {}, 'Location'),
        el('div', { class: 'row' }, field('Latitude', lat), field('Longitude', lon)),
        field('Radar radius (nm)', radius),
      ),
      el('div', { class: 'card' },
        el('h2', {}, 'Flights'),
        el('div', { class: 'row' }, field('Provider', provider), field('Poll every (s)', flightPoll)),
        field('Watchlist (tail numbers, comma-separated)', watchlist),
      ),
      el('div', { class: 'card' },
        el('h2', {}, 'METAR'),
        el('div', { class: 'row' }, field('Station', station), field('Poll every (s)', metarPoll)),
      ),
      el('div', { class: 'card' },
        el('h2', {}, 'Time'),
        field('POSIX TZ string', tz),
        field('NTP server', ntp),
      ),
      el('div', { class: 'card' },
        el('h2', {}, 'AMG Bridge'),
        field('Base URL', amgUrl),
        field('Poll every (s)', amgPoll),
        checkRow('Notify on new submission', notifySub),
        checkRow('Notify on new request', notifyReq),
        field(cfg.amg.token_set ? 'Bridge token (set)' : 'Bridge token (not set)', tokenInput),
        el('div', { class: 'btn-row' },
          el('button', {
            class: 'ghost',
            onclick: async () => {
              const token = tokenInput.value.trim();
              if (!token) { toast('Enter a token first', 'err'); return; }
              try {
                await api.postSecrets({ amg_token: token });
                tokenInput.value = '';
                cfg.amg.token_set = true;
                toast('Bridge token stored', 'ok');
              } catch (e) { toast(`Token store failed: ${e.message}`, 'err'); }
            },
          }, cfg.amg.token_set ? 'Replace token' : 'Store token'),
        ),
      ),
    ),
    el('div', { class: 'btn-row', style: 'margin-top:16px' },
      el('button', {
        onclick: async () => {
          try {
            await api.putConfig(cfg);
            state.config = cfg;
            toast('Integration settings saved', 'ok');
          } catch (e) { toast(`Save failed: ${e.message}`, 'err'); }
        },
      }, 'Save integrations'),
      el('button', { class: 'ghost', onclick: () => state.reload() }, 'Discard changes'),
    ),
  );
  return () => {};
}
