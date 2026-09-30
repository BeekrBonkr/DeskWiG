#pragma once

// The structured ("Design") side of the widget editor: an element tree,
// a property inspector and click/drag on the preview. It works on the same
// layout object the code editor produces and writes every change back to
// the code, so undo lives in the code editor's history and both views
// always agree. Served at /designer.js and loaded by the editor page.

static const char DESIGNER_JS[] = R"js(
(function () {
'use strict';

const ADD_TYPES = ['text', 'box', 'line', 'rect', 'bar', 'chart', 'circle', 'ellipse', 'arc', 'triangle', 'polygon', 'image'];
const ROLES = ['text', 'dim', 'ok', 'warn', 'bad', 'accent', 'bg'];
const LED_MODES = ['off', 'solid', 'breathe', 'blink', 'pulse', 'rainbow'];

let model = null;      // the layout object (same one the last render used)
let sel = 'root';      // selected path: 'root' or "1.0.2"
let hover = null;
let drag = null;
let inspTimer = 0;

const treeEl = $('tree'), inspEl = $('insp'), ov = $('ov'), octx = ov.getContext('2d');

// ---------- model helpers ----------
function getEl(path) {
  if (!model) return null;
  if (path === 'root') return model;
  let arr = model.elements, el = null;
  for (const seg of path.split('.')) {
    if (!Array.isArray(arr)) return null;
    el = arr[parseInt(seg, 10)];
    if (!el) return null;
    arr = el.children;
  }
  return el;
}

function parentOf(path) {
  if (path === 'root') return null;
  const segs = path.split('.');
  const index = parseInt(segs.pop(), 10);
  const parentPath = segs.length ? segs.join('.') : 'root';
  const parent = getEl(parentPath);
  if (!parent) return null;
  const arr = parentPath === 'root' ? model.elements : parent.children;
  return { arr, index, parentPath, parent };
}

function findNode(root, path) {
  if (!root) return null;
  if (path === 'root') return root;
  let n = root;
  for (const seg of path.split('.')) { n = n.children[parseInt(seg, 10)]; if (!n) return null; }
  return n;
}

// Writes the model to the code editor; that re-renders and rebuilds the tree.
// A no-op write is skipped so it does not land in the undo history.
function commit() {
  clearTimeout(inspTimer); inspTimer = 0;
  const text = fmt(model);
  if (text !== srcGet()) srcSet(text);
}
// Commits a debounced field edit right away (before an undo, so the undo
// applies to it rather than being overtaken by the pending write).
window.designerFlush = function () { if (inspTimer) commit(); };

// Re-renders from the model without touching the code (used while dragging).
function quickRender() {
  try { renderObj(validate(model), false); $('err').textContent = ''; }
  catch (e) { $('err').textContent = e.message; }
}

function clean(el) {
  // Drop empty style objects so the JSON stays tidy.
  if (el.style && typeof el.style === 'object' && !Object.keys(el.style).length) delete el.style;
}

// ---------- hooks from the page ----------
// The page re-renders every couple of seconds to show fresh data values.
// Only a change to the code is a reason to rebuild the tree and inspector;
// rebuilding on every render closed open dropdowns and lost the caret
// while typing in a field.
let builtSrc = null;
window.designerOnRender = function (j, fromText) {
  const src = srcGet();
  if (src !== builtSrc) {
    if (fromText) model = j;
    if (!getEl(sel)) sel = 'root';
    builtSrc = src;
    rebuildPanels();
  }
  drawOverlay();
};

// Rebuilds the tree and inspector, keeping focus and caret position in
// whichever field was being edited (the field's own edit caused the rebuild).
function rebuildPanels() {
  const a = document.activeElement;
  let keep = null;
  if (a && inspEl.contains(a) && a.dataset.field) {
    keep = { f: a.dataset.field, s: a.selectionStart, e: a.selectionEnd };
  }
  buildTree();
  buildInspector();
  if (keep) {
    const c = inspEl.querySelector('[data-field="' + keep.f + '"]');
    if (c) { c.focus(); try { if (keep.s !== null && keep.s !== undefined) c.setSelectionRange(keep.s, keep.e); } catch (e) {} }
  }
}
window.designerOnError = function () { drawOverlay(); };

// ---------- overlay ----------
function drawOverlay() {
  octx.clearRect(0, 0, ov.width, ov.height);
  const draw = (path, color, dash) => {
    const n = findNode(lastRoot, path);
    if (!n) return;
    const w = Math.max(n.lw, 1), h = Math.max(n.lh, 1);
    octx.strokeStyle = color; octx.lineWidth = 2; octx.setLineDash(dash);
    octx.strokeRect(n.lx * S + 1, n.ly * S + 1, w * S - 2, h * S - 2);
  };
  if (hover && hover !== sel) draw(hover, 'rgba(255,255,255,0.5)', [4, 4]);
  if (sel) draw(sel, sel === 'root' ? 'rgba(0,158,255,0.6)' : '#00c8ff', []);
}

// ---------- canvas selection and drag ----------
function canvasPoint(e) {
  const r = cv.getBoundingClientRect();
  return { x: (e.clientX - r.left) / r.width * 170, y: (e.clientY - r.top) / r.height * 320 };
}

function hitTest(n, x, y) {
  for (let i = n.children.length - 1; i >= 0; i--) {
    const r = hitTest(n.children[i], x, y);
    if (r) return r;
  }
  if (n.path === 'root') return n;
  const w = Math.max(n.lw, 4), h = Math.max(n.lh, 4);
  return (x >= n.lx && x < n.lx + w && y >= n.ly && y < n.ly + h) ? n : null;
}

cv.addEventListener('pointerdown', e => {
  if (!lastRoot || !model) return;
  const p = canvasPoint(e);
  const n = hitTest(lastRoot, p.x, p.y);
  if (!n) return;
  select(n.path);
  const el = getEl(n.path);
  if (n.path !== 'root' && n.hasXY && el) {
    drag = { path: n.path, sx: e.clientX, sy: e.clientY, ox: el.x | 0, oy: el.y | 0, moved: false, scale: cv.getBoundingClientRect().width / 170 };
    cv.setPointerCapture(e.pointerId);
  }
  e.preventDefault();
});

cv.addEventListener('pointermove', e => {
  if (!lastRoot) return;
  if (drag) {
    const el = getEl(drag.path);
    if (!el) { drag = null; return; }
    const nx = drag.ox + Math.round((e.clientX - drag.sx) / drag.scale);
    const ny = drag.oy + Math.round((e.clientY - drag.sy) / drag.scale);
    if (nx !== el.x || ny !== el.y) { el.x = nx; el.y = ny; drag.moved = true; quickRender(); drawOverlay(); }
    return;
  }
  const p = canvasPoint(e);
  const n = hitTest(lastRoot, p.x, p.y);
  const h = n && n.path !== 'root' ? n.path : null;
  if (h !== hover) { hover = h; drawOverlay(); }
});

cv.addEventListener('pointerup', e => {
  if (drag) { if (drag.moved) commit(); drag = null; }
});
cv.addEventListener('pointerleave', () => { if (!drag && hover) { hover = null; drawOverlay(); } });

function select(path) {
  sel = path;
  buildTree();
  buildInspector();
  drawOverlay();
}

// ---------- tree ----------
function labelFor(el) {
  const t = el.type;
  if (t === 'text') return 'text  ' + JSON.stringify(String(el.text ?? '')).slice(0, 26);
  if (t === 'box') return 'box  (' + ((el.children || []).length) + ')' + (el.style && el.style.direction === 'row' ? ' row' : '');
  if (t === 'image') return 'image  ' + String(el.src || '').slice(0, 20);
  if (t === 'bar' || t === 'arc') return t + '  ' + String(el.value ?? '');
  return t;
}

function buildTree() {
  if (!model) return;
  treeEl.innerHTML = '';
  const addRow = (path, depth, text, el) => {
    const row = document.createElement('div');
    row.className = 'trow' + (path === sel ? ' sel' : '') + (path === 'root' ? ' root' : '');
    row.style.paddingLeft = (8 + depth * 14) + 'px';
    row.dataset.path = path;
    row.textContent = text;
    row.title = path === 'root' ? 'widget' : 'element ' + path;
    row.onclick = () => select(path);
    if (path !== 'root') {
      row.draggable = true;
      row.addEventListener('dragstart', ev => { ev.dataTransfer.setData('text/plain', path); ev.dataTransfer.effectAllowed = 'move'; row.classList.add('dragging'); });
      row.addEventListener('dragend', () => { row.classList.remove('dragging'); clearDropMarks(); });
    }
    row.addEventListener('dragover', ev => {
      ev.preventDefault();
      const zone = dropZone(row, ev, el);
      clearDropMarks();
      row.classList.add('drop-' + zone);
    });
    row.addEventListener('dragleave', () => row.classList.remove('drop-before', 'drop-after', 'drop-into'));
    row.addEventListener('drop', ev => {
      ev.preventDefault();
      const from = ev.dataTransfer.getData('text/plain');
      const zone = dropZone(row, ev, el);
      clearDropMarks();
      moveElement(from, path, zone);
    });
    treeEl.appendChild(row);
  };
  addRow('root', 0, (model.name || 'Widget') + '  (' + (model.elements || []).length + ' elements)', model);
  const walk = (arr, prefix, depth) => {
    (arr || []).forEach((el, i) => {
      const path = prefix ? prefix + '.' + i : String(i);
      addRow(path, depth, labelFor(el), el);
      if (el.type === 'box') walk(el.children, path, depth + 1);
    });
  };
  walk(model.elements, '', 1);
}

function clearDropMarks() { treeEl.querySelectorAll('.trow').forEach(r => r.classList.remove('drop-before', 'drop-after', 'drop-into')); }

// Where a drop on this row lands: into a box (middle), or before/after it.
function dropZone(row, ev, el) {
  if (row.dataset.path === 'root') return 'into';
  const r = row.getBoundingClientRect();
  const y = (ev.clientY - r.top) / r.height;
  const isBox = el && el.type === 'box';
  if (isBox && y > 0.3 && y < 0.7) return 'into';
  return y < 0.5 ? 'before' : 'after';
}

function moveElement(from, to, zone) {
  if (!from || from === to || from === 'root') return;
  if (to !== 'root' && (to === from || to.startsWith(from + '.'))) return;   // not into itself
  const src = parentOf(from);
  if (!src) return;
  const el = src.arr[src.index];
  src.arr.splice(src.index, 1);
  // Recompute the destination after removal, by walking from the model again.
  let destArr, destIndex;
  if (zone === 'into') {
    const target = to === 'root' ? model : getEl(adjustPath(to, from));
    if (!target) { src.arr.splice(src.index, 0, el); return; }
    if (to === 'root') { destArr = model.elements; }
    else { if (!Array.isArray(target.children)) target.children = []; destArr = target.children; }
    destIndex = destArr.length;
  } else {
    const p = parentOf(adjustPath(to, from));
    if (!p) { src.arr.splice(src.index, 0, el); return; }
    destArr = p.arr;
    destIndex = p.index + (zone === 'after' ? 1 : 0);
  }
  destArr.splice(destIndex, 0, el);
  sel = pathOfEl(el) || 'root';
  commit();
}

// After removing `from`, a sibling that came after it shifts down by one.
function adjustPath(path, from) {
  const fs = from.split('.'), ps = path.split('.');
  if (ps.length < fs.length) return path;
  for (let i = 0; i < fs.length - 1; i++) if (fs[i] !== ps[i]) return path;
  const k = fs.length - 1;
  if (parseInt(ps[k], 10) > parseInt(fs[k], 10)) ps[k] = String(parseInt(ps[k], 10) - 1);
  return ps.join('.');
}

function pathOfEl(target) {
  let found = null;
  const walk = (arr, prefix) => {
    (arr || []).forEach((el, i) => {
      const path = prefix ? prefix + '.' + i : String(i);
      if (el === target) found = path;
      if (!found && el.type === 'box') walk(el.children, path);
    });
  };
  walk(model.elements, '');
  return found;
}

// ---------- toolbar actions ----------
const DEFAULTS = {
  text: { type: 'text', text: 'Text' },
  box: { type: 'box', style: { direction: 'row', gap: 6 }, children: [] },
  line: { type: 'line', h: 1, color: 'dim' },
  rect: { type: 'rect', w: 60, h: 24, fill: true, color: 'accent', style: { radius: 6 } },
  bar: { type: 'bar', h: 10, value: '{wifi.pct}', color: 'ok' },
  circle: { type: 'circle', w: 16, color: 'ok' },
  ellipse: { type: 'ellipse', w: 40, h: 24, color: 'warn' },
  arc: { type: 'arc', w: 60, value: '{wifi.pct}', color: 'accent' },
  triangle: { type: 'triangle', points: [[0, 20], [12, 0], [24, 20]], color: 'accent' },
  polygon: { type: 'polygon', points: [[12, 0], [24, 8], [20, 24], [4, 24], [0, 8]], color: 'ok' },
  image: { type: 'image', src: '', w: 48 },
  chart: { type: 'chart', series: '', h: 48, style: { kind: 'area' } }
};

// Keys sampled on the setup page, for a new chart's default series.
let seriesNames = [];
fetch('/api/series').then(r => r.json()).then(j => { seriesNames = (j.series || []).map(x => x.key); }).catch(() => {});

function addElement(type) {
  if (!model) return;
  const el = JSON.parse(JSON.stringify(DEFAULTS[type]));
  if (type === 'image') el.src = imageNames[0] || 'https://';
  if (type === 'chart') el.series = seriesNames[0] || 'wifi.rssi';
  let arr = model.elements;
  const cur = getEl(sel);
  if (sel !== 'root' && cur && cur.type === 'box') { if (!cur.children) cur.children = []; arr = cur.children; }
  else if (sel !== 'root') { const p = parentOf(sel); if (p) { p.arr.splice(p.index + 1, 0, el); sel = pathOfEl(el); commit(); return; } }
  arr.push(el);
  sel = pathOfEl(el) || 'root';
  commit();
}

function deleteSelected() {
  const p = parentOf(sel);
  if (!p) return;
  p.arr.splice(p.index, 1);
  // Keep editing nearby: the element that took this slot, else the previous one, else the parent.
  const next = p.arr[p.index] || p.arr[p.index - 1];
  sel = next ? pathOfEl(next) : p.parentPath;
  commit();
}

function duplicateSelected() {
  const p = parentOf(sel);
  if (!p) return;
  const copy = JSON.parse(JSON.stringify(p.arr[p.index]));
  p.arr.splice(p.index + 1, 0, copy);
  sel = pathOfEl(copy);
  commit();
}

function moveSelected(delta) {
  const p = parentOf(sel);
  if (!p) return;
  const j = p.index + delta;
  if (j < 0 || j >= p.arr.length) return;
  const [el] = p.arr.splice(p.index, 1);
  p.arr.splice(j, 0, el);
  sel = pathOfEl(el);
  commit();
}

$('addType').addEventListener('change', () => { const t = $('addType').value; if (t) addElement(t); $('addType').value = ''; });
$('elDup').onclick = duplicateSelected;
$('elDel').onclick = deleteSelected;
$('elUp').onclick = () => moveSelected(-1);
$('elDown').onclick = () => moveSelected(1);
$('undo').onclick = () => { if (editor) editor.undo(); };
$('redo').onclick = () => { if (editor) editor.redo(); };

// ---------- inspector ----------
// Field specs: key on the element (or in its style when style is true),
// a control kind, and the element types it applies to.
const FIELDS = [
  { key: 'name', label: 'Name', kind: 'text', types: ['root'] },
  { key: 'text', label: 'Text', kind: 'text', types: ['text'], hint: '{keys} and math work here' },
  { key: 'value', label: 'Value', kind: 'text', types: ['bar', 'arc'], hint: '0-100 after expansion' },
  { key: 'src', label: 'Image', kind: 'image', types: ['image'] },
  { key: 'series', label: 'Series', kind: 'text', types: ['chart'], hint: 'a key sampled under History on the setup page' },
  { key: 'window', label: 'Window s', kind: 'num', types: ['chart'], hint: 'seconds shown; empty = everything kept' },
  { key: 'kind', label: 'Kind', kind: 'sel', opts: ['', 'line', 'area', 'bars', 'dots'], types: ['chart'], style: true },
  { key: 'min', label: 'Min', kind: 'num', types: ['chart'], style: true, hint: 'empty = fit the data' },
  { key: 'max', label: 'Max', kind: 'num', types: ['chart'], style: true },
  { key: 'font', label: 'Font', kind: 'font', types: ['text'] },
  { key: 'size', label: 'Size', kind: 'num', types: ['text'], hint: '1-40 bitmap, 6-160 with a font' },
  { key: 'align', label: 'Align', kind: 'sel', opts: ['', 'left', 'center', 'right'], types: ['text'] },
  { key: 'color', label: 'Color', kind: 'color', types: ['text', 'line', 'rect', 'bar', 'chart', 'circle', 'ellipse', 'arc', 'triangle', 'polygon'] },
  { key: 'fill', label: 'Fill', kind: 'bool', types: ['rect', 'circle', 'ellipse', 'triangle', 'polygon'] },
  { key: 'class', label: 'Class', kind: 'class', types: ['*'] },
  { key: 'position', label: 'Position', kind: 'position', types: ['*'] },
  { key: 'x', label: 'X', kind: 'num', types: ['*'], abs: true },
  { key: 'y', label: 'Y', kind: 'num', types: ['*'], abs: true },
  { key: 'x2', label: 'X2', kind: 'num', types: ['line'], abs: true },
  { key: 'y2', label: 'Y2', kind: 'num', types: ['line'], abs: true },
  { key: 'w', label: 'Width', kind: 'num', types: ['*'] },
  { key: 'h', label: 'Height', kind: 'num', types: ['*'] },
  { key: 'start', label: 'Start °', kind: 'num', types: ['arc'] },
  { key: 'end', label: 'End °', kind: 'num', types: ['arc'] },
  { key: 'points', label: 'Points', kind: 'points', types: ['triangle', 'polygon'], hint: 'x,y pairs: 0,40 24,0 48,40' },
  { key: 'refresh', label: 'Refresh s', kind: 'num', types: ['image'] },
  { key: 'direction', label: 'Direction', kind: 'sel', opts: ['', 'column', 'row'], types: ['root', 'box'], style: true },
  { key: 'gap', label: 'Gap', kind: 'num', types: ['root', 'box'], style: true },
  { key: 'padding', label: 'Padding', kind: 'num', types: ['root', 'box'], style: true },
  { key: 'align', label: 'Align items', kind: 'sel', opts: ['', 'stretch', 'start', 'center', 'end'], types: ['root', 'box'], style: true },
  { key: 'justify', label: 'Justify', kind: 'sel', opts: ['', 'start', 'center', 'end', 'between'], types: ['root', 'box'], style: true },
  { key: 'background', label: 'Background', kind: 'color', types: ['root', 'box', 'rect', 'text', 'bar', 'chart', 'arc', 'circle', 'ellipse', 'triangle', 'polygon', 'image'], style: true },
  { key: 'gradient', label: 'Gradient to', kind: 'color', types: ['root', 'box', 'rect', 'bar', 'chart'], style: true, hint: 'fades the fill into this colour' },
  { key: 'gradientDir', label: 'Gradient', kind: 'sel', opts: ['', 'down', 'right'], types: ['root', 'box', 'rect', 'bar', 'chart'], style: true },
  { key: 'border', label: 'Border', kind: 'color', types: ['box', 'rect', 'circle', 'ellipse', 'triangle', 'polygon'], style: true },
  { key: 'borderWidth', label: 'Border width', kind: 'num', types: ['box', 'rect', 'circle', 'ellipse', 'triangle', 'polygon'], style: true },
  { key: 'radius', label: 'Radius', kind: 'num', types: ['box', 'rect', 'bar', 'chart'], style: true },
  { key: 'thickness', label: 'Thickness', kind: 'num', types: ['arc', 'line', 'chart'], style: true },
  { key: 'fit', label: 'Fit', kind: 'sel', opts: ['', 'contain', 'cover', 'stretch'], types: ['image'], style: true },
  { key: 'image', label: 'Background image', kind: 'image', types: ['root', 'box'], style: true }
];

function applies(f, el, type) {
  if (!(f.types.includes(type) || (f.types.includes('*') && type !== 'root'))) return false;
  if (f.abs) return !!(el.x !== undefined && el.y !== undefined) || (el.style && el.style.position === 'absolute');
  return true;
}

function getVal(el, f) {
  const o = f.style ? (el.style || {}) : el;
  return o[f.key];
}

function setVal(el, f, v) {
  if (f.style) {
    if (!el.style) el.style = {};
    if (v === '' || v === undefined || v === null || (typeof v === 'number' && isNaN(v))) delete el.style[f.key];
    else el.style[f.key] = v;
    clean(el);
  } else {
    if (v === '' || v === undefined || v === null || (typeof v === 'number' && isNaN(v))) delete el[f.key];
    else el[f.key] = v;
  }
}

function row(label, control, hint) {
  const r = document.createElement('div');
  r.className = 'irow';
  const l = document.createElement('label');
  l.textContent = label;
  r.appendChild(l);
  r.appendChild(control);
  if (hint) { const h = document.createElement('span'); h.className = 'ihint'; h.textContent = hint; r.appendChild(h); }
  return r;
}

function debouncedCommit() { clearTimeout(inspTimer); inspTimer = setTimeout(() => { commit(); }, 300); }

function makeControl(f, el) {
  const v = getVal(el, f);
  let c;
  switch (f.kind) {
    case 'text': case 'num': {
      // Numbers take a plain value or a {template}, so the field is text with a numeric keyboard.
      c = document.createElement('input');
      c.type = 'text';
      if (f.kind === 'num') c.inputMode = 'decimal';
      c.value = v === undefined ? '' : v;
      c.autocapitalize = 'off'; c.autocorrect = 'off';
      c.addEventListener('input', () => {
        if (f.kind === 'num') {
          const t = c.value.trim();
          setVal(el, f, t === '' ? '' : /^-?\d+(\.\d+)?$/.test(t) ? parseFloat(t) : t);
          t.includes('{') ? debouncedCommit() : commit();
        } else { setVal(el, f, c.value); debouncedCommit(); }
      });
      break;
    }
    case 'sel': case 'font': case 'class': {
      c = document.createElement('select');
      const opts = f.kind === 'sel' ? f.opts : f.kind === 'font' ? ['', ...fontNames] : ['', ...Object.keys((model && model.styles) || {})];
      for (const o of opts) { const op = document.createElement('option'); op.value = o; op.textContent = o === '' ? (f.kind === 'font' ? 'bitmap (default)' : f.kind === 'class' ? 'none' : 'default') : o; c.appendChild(op); }
      c.value = v === undefined ? '' : v;
      c.addEventListener('change', () => { setVal(el, f, c.value); commit(); });
      break;
    }
    case 'bool': {
      c = document.createElement('input');
      c.type = 'checkbox';
      c.checked = !!v;
      c.addEventListener('change', () => { setVal(el, f, c.checked ? true : ''); commit(); });
      break;
    }
    case 'color': {
      c = document.createElement('div');
      c.className = 'colorctl';
      const t = document.createElement('input');
      t.type = 'text'; t.value = v === undefined ? '' : v; t.placeholder = 'role, #rrggbb or {key}';
      t.setAttribute('list', 'roleList');
      const sw = document.createElement('input');
      sw.type = 'color';
      sw.value = /^#[0-9a-f]{6}$/i.test(t.value) ? t.value : '#ffffff';
      t.addEventListener('input', () => { setVal(el, f, t.value.trim()); if (/^#[0-9a-f]{6}$/i.test(t.value)) sw.value = t.value; debouncedCommit(); });
      sw.addEventListener('input', () => { t.value = sw.value; setVal(el, f, sw.value); debouncedCommit(); });
      c.appendChild(t); c.appendChild(sw);
      break;
    }
    case 'image': {
      c = document.createElement('input');
      c.type = 'text'; c.value = v === undefined ? '' : v; c.placeholder = 'uploaded name or https://…';
      c.setAttribute('list', 'imageList');
      c.addEventListener('input', () => { setVal(el, f, c.value.trim()); debouncedCommit(); });
      break;
    }
    case 'points': {
      c = document.createElement('input');
      c.type = 'text';
      c.value = Array.isArray(v) ? v.map(p => p.join(',')).join(' ') : '';
      c.addEventListener('input', () => {
        const pts = c.value.trim().split(/\s+/).filter(Boolean).map(p => p.split(',').map(n => parseInt(n, 10)));
        if (pts.every(p => p.length === 2 && p.every(n => !isNaN(n)))) { el.points = pts; debouncedCommit(); }
      });
      break;
    }
    case 'position': {
      c = document.createElement('select');
      for (const o of ['flow', 'absolute']) { const op = document.createElement('option'); op.value = o; op.textContent = o; c.appendChild(op); }
      const abs = (el.x !== undefined && el.y !== undefined) || (el.style && el.style.position === 'absolute');
      c.value = abs ? 'absolute' : 'flow';
      c.addEventListener('change', () => {
        if (c.value === 'absolute') { if (el.x === undefined) el.x = 0; if (el.y === undefined) el.y = 0; }
        else { delete el.x; delete el.y; delete el.x2; delete el.y2; if (el.style) { delete el.style.position; clean(el); } }
        commit();
      });
      break;
    }
  }
  return c;
}

function buildInspector() {
  if (!model) return;
  inspEl.innerHTML = '';
  const el = getEl(sel);
  if (!el) return;
  const type = sel === 'root' ? 'root' : el.type;
  const head = document.createElement('div');
  head.className = 'ihead';
  head.textContent = sel === 'root' ? 'Widget' : (type + '  ·  element ' + sel);
  inspEl.appendChild(head);

  if (sel !== 'root') {
    const ts = document.createElement('select');
    for (const t of ADD_TYPES) { const op = document.createElement('option'); op.value = t; op.textContent = t; ts.appendChild(op); }
    ts.value = el.type;
    ts.addEventListener('change', () => { el.type = ts.value; if (el.type === 'box' && !el.children) el.children = []; commit(); });
    inspEl.appendChild(row('Type', ts));
  }
  for (const f of FIELDS) {
    if (!applies(f, el, type)) continue;
    const c = makeControl(f, el);
    const focusable = c.tagName === 'DIV' ? c.querySelector('input') : c;
    if (focusable) focusable.dataset.field = (f.style ? 'style.' : '') + f.key;
    inspEl.appendChild(row(f.label, c, f.hint));
  }
  if (sel === 'root') buildLed();
}

// ---------- LED section (root) ----------
function ledRuleRow(rule, i) {
  const box = document.createElement('div');
  box.className = 'lrule';
  const mk = (label, key, kind, opts) => {
    let c;
    if (kind === 'sel') {
      c = document.createElement('select');
      for (const o of opts) { const op = document.createElement('option'); op.value = o; op.textContent = o === '' ? 'keep' : o; c.appendChild(op); }
      c.value = rule[key] === undefined ? '' : rule[key];
      c.addEventListener('change', () => { if (c.value === '') delete rule[key]; else rule[key] = c.value; commit(); });
    } else {
      c = document.createElement('input');
      c.type = kind === 'num' ? 'number' : 'text';
      c.value = rule[key] === undefined ? '' : rule[key];
      c.addEventListener('input', () => { const v = kind === 'num' ? parseFloat(c.value) : c.value; if (c.value === '' || (kind === 'num' && isNaN(v))) delete rule[key]; else rule[key] = v; debouncedCommit(); });
    }
    c.dataset.field = 'led.' + i + '.' + key;
    box.appendChild(row(label, c));
  };
  const title = document.createElement('div');
  title.className = 'ihead small';
  title.textContent = i < 0 ? 'LED default' : 'Rule ' + (i + 1) + '  (first match wins)';
  box.appendChild(title);
  if (i >= 0) {
    mk('When', 'when', 'text');
    mk('Key', 'key', 'text');
    mk('Is', 'is', 'text');
  }
  mk('Color', 'color', 'text');
  mk('Mode', 'mode', 'sel', ['', ...LED_MODES]);
  mk('Speed ms', 'speed', 'num');
  mk('Brightness %', 'brightness', 'num');
  if (i >= 0) {
    const del = document.createElement('button');
    del.textContent = 'Remove rule';
    del.className = 'danger';
    del.onclick = () => { model.led.rules.splice(i, 1); if (!model.led.rules.length) delete model.led.rules; commit(); };
    box.appendChild(del);
  }
  return box;
}

function buildLed() {
  const head = document.createElement('div');
  head.className = 'ihead';
  head.textContent = 'Status LED';
  inspEl.appendChild(head);
  if (!model.led) {
    const b = document.createElement('button');
    b.textContent = 'Enable LED for this widget';
    b.onclick = () => { model.led = { color: 'ok', mode: 'solid' }; commit(); };
    inspEl.appendChild(b);
    const h = document.createElement('p'); h.className = 'hint'; h.textContent = 'Without an LED section the LED stays off while this widget is shown.';
    inspEl.appendChild(h);
    return;
  }
  inspEl.appendChild(ledRuleRow(model.led, -1));
  (model.led.rules || []).forEach((r, i) => inspEl.appendChild(ledRuleRow(r, i)));
  const add = document.createElement('button');
  add.textContent = 'Add rule';
  add.onclick = () => { if (!model.led.rules) model.led.rules = []; model.led.rules.push({ when: 'ping.0.ms > 100', color: 'warn', mode: 'blink', speed: 500 }); commit(); };
  inspEl.appendChild(add);
  const off = document.createElement('button');
  off.textContent = 'Remove LED section';
  off.className = 'danger';
  off.onclick = () => { delete model.led; commit(); };
  inspEl.appendChild(off);
}

// Datalists for colour roles and image names.
const dl = document.createElement('datalist'); dl.id = 'roleList';
for (const r of ROLES) { const o = document.createElement('option'); o.value = r; dl.appendChild(o); }
document.body.appendChild(dl);
const il = document.createElement('datalist'); il.id = 'imageList';
document.body.appendChild(il);
const refreshImageList = () => { il.innerHTML = ''; for (const n of imageNames) { const o = document.createElement('option'); o.value = n; il.appendChild(o); } };
setTimeout(refreshImageList, 1500);
setTimeout(refreshImageList, 5000);

// First build once the page has rendered something.
if (lastRoot) { try { model = parse(); builtSrc = srcGet(); } catch (e) {} buildTree(); buildInspector(); drawOverlay(); }
})();
)js";
