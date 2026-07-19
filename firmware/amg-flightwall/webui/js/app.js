// app.js — bootstrap, hash router, auth prompts, shared state.
import { api, onEvent, startEvents, setLoginPrompt, sha256Hex, storeAuth, storedAuth } from './api.js';
import { el, clear, showModal, toast } from './ui.js';
import { renderDashboard } from './pages/dashboard.js';
import { renderPlaylist } from './pages/playlist.js';
import { renderDisplay } from './pages/display.js';
import { renderIntegrations } from './pages/integrations.js';
import { renderSystem } from './pages/system.js';
import { renderSetup } from './pages/setup.js';

const ROUTES = [
  { hash: '#/dashboard', label: 'Dashboard', render: renderDashboard },
  { hash: '#/playlist', label: 'Playlist', render: renderPlaylist, needsConfig: true },
  { hash: '#/display', label: 'Display', render: renderDisplay, needsConfig: true },
  { hash: '#/integrations', label: 'Integrations', render: renderIntegrations, needsConfig: true },
  { hash: '#/system', label: 'System', render: renderSystem },
  { hash: '#/setup', label: 'Setup', render: renderSetup },
];

const state = {
  status: null,
  config: null,
  scenes: null,
  reload: () => route(true),
};

let teardown = null;

function nav() {
  const box = document.getElementById('nav');
  clear(box).append(...ROUTES.map((r) => el('a', {
    href: r.hash,
    class: location.hash === r.hash ? 'active' : '',
  }, el('span', { class: 'nav-tick' }), r.label)));
}

async function route(force = false) {
  const page = document.getElementById('page');
  let target = ROUTES.find((r) => r.hash === location.hash);
  if (!target) {
    location.hash = (state.status && state.status.setup_mode) ? '#/setup' : '#/dashboard';
    return;
  }
  nav();
  if (teardown) { try { teardown(); } catch { /* noop */ } teardown = null; }
  clear(page);
  try {
    if (target.needsConfig && (force || !state.config || !state.scenes)) {
      const [config, scenes] = await Promise.all([api.getConfig(), api.getScenes()]);
      state.config = config;
      state.scenes = Array.isArray(scenes) ? scenes : scenes.scenes || [];
    }
    teardown = target.render(page, state) || null;
  } catch (e) {
    page.append(el('div', { class: 'warn-box' }, `Failed to load page: ${e.message}`));
  }
}

// --- auth prompts -----------------------------------------------------------
function promptLogin() {
  return showModal((close) => {
    const pass = el('input', { type: 'password', placeholder: 'admin password', autocomplete: 'current-password' });
    const err = el('div', { class: 'modal-err' }, '');
    const submit = async () => {
      if (!pass.value) { err.textContent = 'Enter the password.'; return; }
      close(await sha256Hex(pass.value));
    };
    pass.addEventListener('keydown', (e) => { if (e.key === 'Enter') submit(); });
    return el('div', {},
      el('h3', {}, 'Sign in'),
      el('p', { class: 'modal-sub' }, 'This action requires the FlightWall admin password.'),
      pass, err,
      el('div', { class: 'btn-row', style: 'margin-top:14px' },
        el('button', { onclick: submit }, 'Sign in'),
        el('button', { class: 'ghost', onclick: () => close(null) }, 'Cancel')),
    );
  }, { dismissable: false });
}
setLoginPrompt(promptLogin);

async function firstRunPassword() {
  await showModal((close) => {
    const p1 = el('input', { type: 'password', placeholder: 'choose admin password (min 8 chars)', autocomplete: 'new-password' });
    const p2 = el('input', { type: 'password', placeholder: 'repeat', autocomplete: 'new-password', style: 'margin-top:8px' });
    const err = el('div', { class: 'modal-err' }, '');
    const submit = async () => {
      if (p1.value.length < 8) { err.textContent = 'Use at least 8 characters.'; return; }
      if (p1.value !== p2.value) { err.textContent = 'Passwords do not match.'; return; }
      try {
        await api.postSecrets({ admin_password: p1.value });
        storeAuth(await sha256Hex(p1.value));
        if (state.status) state.status.password_set = true;
        toast('Admin password created', 'ok');
        close(true);
      } catch (e) { err.textContent = `Failed: ${e.message}`; }
    };
    p2.addEventListener('keydown', (e) => { if (e.key === 'Enter') submit(); });
    return el('div', {},
      el('h3', {}, 'Secure this device'),
      el('p', { class: 'modal-sub' },
        'No admin password is set. Anyone on your network could reconfigure the panel. Create one now — settings changes will require it.'),
      p1, p2, err,
      el('div', { class: 'btn-row', style: 'margin-top:14px' },
        el('button', { onclick: submit }, 'Create password'),
        el('button', { class: 'ghost', onclick: () => close(false) }, 'Later')),
    );
  }, { dismissable: false });
}

// --- boot -------------------------------------------------------------------
function applyStatus(s) {
  state.status = s;
  document.getElementById('setup-banner').classList.toggle('hidden', !s.setup_mode);
}

async function boot() {
  nav();
  window.addEventListener('hashchange', () => route());
  onEvent('status', applyStatus);
  onEvent('sse-state', (up) => {
    const pill = document.getElementById('conn-pill');
    pill.textContent = up ? 'LINK LIVE' : 'OFFLINE';
    pill.className = `conn-pill ${up ? 'conn-on' : 'conn-off'}`;
  });

  try {
    const s = await api.getStatus();
    applyStatus(s);
    if (s.setup_mode && !location.hash) location.hash = '#/setup';
    if (!s.setup_mode && !s.password_set) await firstRunPassword();
  } catch (e) {
    toast(`Cannot reach device: ${e.message}`, 'err');
  }

  startEvents();
  route();
}

boot();
