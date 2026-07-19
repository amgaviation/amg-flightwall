// api.js — fetch wrapper with X-Auth handling, SHA-256, and the SSE event bus.
// Auth model (per platform contract): mutating routes require header
//   X-Auth: <hex sha256(admin password)>
// The hash is computed client-side and kept in localStorage. On 401 we clear
// it, prompt for the password (login modal supplied by app.js), and retry.

const AUTH_KEY = 'amgfw.auth';

let loginPrompt = null; // set by app.js: async () => hex-hash or null (cancelled)

export function setLoginPrompt(fn) { loginPrompt = fn; }

export function storedAuth() { return localStorage.getItem(AUTH_KEY) || ''; }
export function storeAuth(hash) { localStorage.setItem(AUTH_KEY, hash); }
export function clearAuth() { localStorage.removeItem(AUTH_KEY); }

// --- SHA-256 -> hex. WebCrypto when available (localhost / https); pure-JS
// fallback for plain-http LAN origins where crypto.subtle is undefined.
export async function sha256Hex(text) {
  const bytes = new TextEncoder().encode(text);
  if (globalThis.crypto && crypto.subtle) {
    const digest = await crypto.subtle.digest('SHA-256', bytes);
    return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, '0')).join('');
  }
  return sha256Fallback(bytes);
}

function sha256Fallback(bytes) {
  const K = [
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2];
  const H = [0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19];
  const rr = (x, n) => (x >>> n) | (x << (32 - n));
  const len = bytes.length;
  const bitLen = len * 8;
  const padded = new Uint8Array((((len + 8) >> 6) + 1) << 6);
  padded.set(bytes);
  padded[len] = 0x80;
  const dv = new DataView(padded.buffer);
  dv.setUint32(padded.length - 4, bitLen >>> 0);
  dv.setUint32(padded.length - 8, Math.floor(bitLen / 0x100000000));
  const w = new Uint32Array(64);
  for (let off = 0; off < padded.length; off += 64) {
    for (let i = 0; i < 16; i++) w[i] = dv.getUint32(off + i * 4);
    for (let i = 16; i < 64; i++) {
      const s0 = rr(w[i - 15], 7) ^ rr(w[i - 15], 18) ^ (w[i - 15] >>> 3);
      const s1 = rr(w[i - 2], 17) ^ rr(w[i - 2], 19) ^ (w[i - 2] >>> 10);
      w[i] = (w[i - 16] + s0 + w[i - 7] + s1) >>> 0;
    }
    let [a, b, c, d, e, f, g, h] = H;
    for (let i = 0; i < 64; i++) {
      const S1 = rr(e, 6) ^ rr(e, 11) ^ rr(e, 25);
      const ch = (e & f) ^ (~e & g);
      const t1 = (h + S1 + ch + K[i] + w[i]) >>> 0;
      const S0 = rr(a, 2) ^ rr(a, 13) ^ rr(a, 22);
      const maj = (a & b) ^ (a & c) ^ (b & c);
      const t2 = (S0 + maj) >>> 0;
      h = g; g = f; f = e; e = (d + t1) >>> 0; d = c; c = b; b = a; a = (t1 + t2) >>> 0;
    }
    H[0] = (H[0] + a) >>> 0; H[1] = (H[1] + b) >>> 0; H[2] = (H[2] + c) >>> 0; H[3] = (H[3] + d) >>> 0;
    H[4] = (H[4] + e) >>> 0; H[5] = (H[5] + f) >>> 0; H[6] = (H[6] + g) >>> 0; H[7] = (H[7] + h) >>> 0;
  }
  return H.map((x) => x.toString(16).padStart(8, '0')).join('');
}

// --- request core -----------------------------------------------------------
async function rawRequest(method, path, body, isForm) {
  const headers = {};
  const auth = storedAuth();
  if (auth) headers['X-Auth'] = auth;
  let payload;
  if (isForm) {
    payload = body; // FormData — browser sets content-type + boundary
  } else if (body !== undefined) {
    headers['Content-Type'] = 'application/json';
    payload = JSON.stringify(body);
  }
  return fetch(path, { method, headers, body: payload });
}

async function request(method, path, body, isForm) {
  let res = await rawRequest(method, path, body, isForm);
  while (res.status === 401) {
    clearAuth();
    if (!loginPrompt) throw new ApiError(401, 'authentication required');
    const hash = await loginPrompt();
    if (!hash) throw new ApiError(401, 'authentication cancelled');
    storeAuth(hash);
    res = await rawRequest(method, path, body, isForm);
    if (res.status === 401) clearAuth();
  }
  if (!res.ok) {
    let detail = res.statusText;
    try {
      const j = await res.json();
      if (j && j.error) detail = j.error;
    } catch { /* not json */ }
    throw new ApiError(res.status, detail);
  }
  const ct = res.headers.get('content-type') || '';
  if (ct.includes('application/json')) return res.json();
  return res.text();
}

export class ApiError extends Error {
  constructor(status, message) {
    super(message);
    this.status = status;
  }
}

export const api = {
  getStatus: () => request('GET', '/api/status'),
  getConfig: () => request('GET', '/api/config'),
  putConfig: (cfg) => request('PUT', '/api/config', cfg),
  postSecrets: (secrets) => request('POST', '/api/secrets', secrets),
  getScenes: () => request('GET', '/api/scenes'),
  activateScene: (id, durationS) =>
    request('POST', '/api/scene/activate', durationS ? { id, duration_s: durationS } : { id }),
  postMessage: (msg) => request('POST', '/api/message', msg),
  postNotify: (n) => request('POST', '/api/notify', n),
  getLogs: () => request('GET', '/api/logs'),
  reboot: () => request('POST', '/api/reboot', {}),
  wifiScan: () => request('GET', '/api/wifi/scan'),
  wifiJoin: (ssid, pass) => request('POST', '/api/wifi', { ssid, pass }),
};

// OTA upload uses XHR for upload progress events (fetch has no upload progress).
export function uploadOta(file, onProgress) {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/ota');
    const auth = storedAuth();
    if (auth) xhr.setRequestHeader('X-Auth', auth);
    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable && onProgress) onProgress(e.loaded / e.total);
    };
    xhr.onload = () => {
      if (xhr.status >= 200 && xhr.status < 300) {
        try { resolve(JSON.parse(xhr.responseText)); }
        catch { resolve({ ok: true }); }
      } else if (xhr.status === 401) {
        reject(new ApiError(401, 'authentication required — sign in and retry'));
      } else {
        reject(new ApiError(xhr.status, xhr.responseText || 'upload failed'));
      }
    };
    xhr.onerror = () => reject(new ApiError(0, 'network error during upload'));
    const form = new FormData();
    form.append('firmware', file, file.name || 'firmware.bin');
    xhr.send(form);
  });
}

// --- SSE bus ----------------------------------------------------------------
// One shared EventSource for /api/events. Pages subscribe to typed events
// ('frame' | 'status' | 'log') plus 'sse-state' for connectivity.
const listeners = new Map(); // type -> Set<fn>
let source = null;

export function onEvent(type, fn) {
  if (!listeners.has(type)) listeners.set(type, new Set());
  listeners.get(type).add(fn);
  return () => listeners.get(type).delete(fn);
}

function emit(type, data) {
  const set = listeners.get(type);
  if (set) for (const fn of set) { try { fn(data); } catch (e) { console.error(e); } }
}

export function startEvents() {
  if (source) return;
  source = new EventSource('/api/events');
  source.onopen = () => emit('sse-state', true);
  source.onerror = () => emit('sse-state', false); // EventSource auto-reconnects
  for (const type of ['frame', 'status', 'log']) {
    source.addEventListener(type, (ev) => {
      let data = ev.data;
      if (type === 'status') {
        try { data = JSON.parse(ev.data); } catch { return; }
      }
      emit(type, data);
    });
  }
}
