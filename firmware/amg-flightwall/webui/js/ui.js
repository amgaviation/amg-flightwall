// ui.js — tiny DOM helpers, toasts, modals. No frameworks.

export function el(tag, attrs = {}, ...children) {
  const node = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (k === 'class') node.className = v;
    else if (k === 'html') node.innerHTML = v;
    else if (k.startsWith('on') && typeof v === 'function') node.addEventListener(k.slice(2), v);
    else if (v === true) node.setAttribute(k, '');
    else if (v !== false && v !== null && v !== undefined) node.setAttribute(k, v);
  }
  for (const child of children.flat()) {
    if (child === null || child === undefined || child === false) continue;
    node.append(child.nodeType ? child : document.createTextNode(String(child)));
  }
  return node;
}

export function clear(node) {
  while (node.firstChild) node.removeChild(node.firstChild);
  return node;
}

export function toast(message, kind = '') {
  const root = document.getElementById('toast-root');
  const t = el('div', { class: `toast ${kind}` }, message);
  root.append(t);
  setTimeout(() => {
    t.style.opacity = '0';
    t.style.transition = 'opacity .3s';
    setTimeout(() => t.remove(), 320);
  }, 3400);
}

// showModal(build) — build(close) returns a node placed inside .modal chrome.
// Returns a promise resolving with the value passed to close().
export function showModal(build, { dismissable = true } = {}) {
  const root = document.getElementById('modal-root');
  return new Promise((resolve) => {
    const close = (value) => { clear(root); resolve(value); };
    const box = el('div', { class: 'modal' }, build(close));
    clear(root).append(box);
    if (dismissable) {
      root.onclick = (e) => { if (e.target === root) close(null); };
    } else {
      root.onclick = null;
    }
    const first = box.querySelector('input, select, textarea, button');
    if (first) first.focus();
  });
}

export function confirmModal(title, message, confirmLabel = 'Confirm', danger = false) {
  return showModal((close) => el('div', {},
    el('h3', {}, title),
    el('p', { class: 'modal-sub' }, message),
    el('div', { class: 'btn-row' },
      el('button', { class: danger ? 'danger' : '', onclick: () => close(true) }, confirmLabel),
      el('button', { class: 'ghost', onclick: () => close(null) }, 'Cancel'),
    ),
  ));
}

export function field(labelText, input) {
  return el('label', { class: 'field' }, el('span', {}, labelText), input);
}

export function checkRow(labelText, input) {
  return el('label', { class: 'check-row' }, input, labelText);
}

export function fmtUptime(s) {
  s = Math.max(0, Math.floor(s));
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  if (d > 0) return `${d}d ${h}h ${m}m`;
  if (h > 0) return `${h}h ${m}m ${s % 60}s`;
  return `${m}m ${s % 60}s`;
}

export function fmtBytes(n) {
  if (n >= 1048576) return `${(n / 1048576).toFixed(1)} MB`;
  if (n >= 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${n} B`;
}
