R"DELPHESPAGE(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Delphes event summary</title>
<!--jsroot_importmap-->
<style>
  :root {
    --bg: #f4f5f7; --panel: #ffffff; --text: #1f2430; --muted: #6b7280;
    --border: #e2e5ea; --accent: #0a6ed1; --accent-soft: #e8f1fb; --coll: #0a6ed1;
  }
  * { box-sizing: border-box; }
  html, body { height: 100%; margin: 0; }
  body {
    display: flex; flex-direction: column; background: var(--bg); color: var(--text);
    font: 14px/1.4 system-ui, -apple-system, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
  }
  header {
    display: flex; align-items: center; gap: 20px; flex-wrap: wrap;
    padding: 10px 16px; background: var(--panel); border-bottom: 1px solid var(--border);
  }
  h1 { margin: 0; font-size: 16px; font-weight: 600; white-space: nowrap; }
  h1 span { color: var(--muted); font-weight: 400; }
  .group { display: flex; align-items: center; gap: 6px; }
  .sample { margin-left: auto; }
  button {
    font: inherit; color: var(--text); background: var(--panel); cursor: pointer;
    border: 1px solid var(--border); border-radius: 6px; padding: 4px 10px; min-height: 30px;
  }
  button:hover:not(:disabled) { border-color: var(--accent); color: var(--accent); }
  button:disabled { opacity: .5; cursor: default; }
  button.primary { background: var(--accent); border-color: var(--accent); color: #fff; }
  button.primary:hover:not(:disabled) { color: #fff; filter: brightness(1.1); }
  input[type=number] {
    font: inherit; width: 84px; padding: 4px 6px; min-height: 30px; text-align: right;
    border: 1px solid var(--border); border-radius: 6px; font-variant-numeric: tabular-nums;
  }
  .muted { color: var(--muted); font-variant-numeric: tabular-nums; }
  .bar { width: 140px; height: 6px; border-radius: 3px; background: #e5e7eb; overflow: hidden; }
  .bar > div { height: 100%; width: 0; background: var(--accent); transition: width .2s; }
  #chips { display: flex; flex-wrap: wrap; gap: 8px; padding: 12px 16px 6px; }
  .chip {
    display: flex; align-items: center; gap: 8px; padding: 5px 12px; border-radius: 999px;
    background: var(--panel); border: 1px solid var(--border); cursor: pointer; user-select: none;
  }
  .chip:hover { border-color: var(--accent); }
  .chip.active { border-color: var(--accent); background: var(--accent-soft); }
  .dot { width: 10px; height: 10px; border-radius: 50%; flex: none; }
  .chip .count { color: var(--muted); font-variant-numeric: tabular-nums; }
  #legend { display: flex; flex-wrap: wrap; gap: 18px; padding: 4px 18px 8px; color: var(--muted); font-size: 12px; }
  #legend span { display: inline-flex; align-items: center; gap: 6px; }
  #legend span[hidden] { display: none; }
  .sw { display: inline-block; width: 22px; height: 10px; }
  .sw.fill { background: color-mix(in srgb, var(--coll) 35%, transparent); border-top: 2px solid var(--coll); }
  .sw.line { border-top: 2px solid var(--coll); height: 0; }
  .sw.dash { border-top: 2px dashed var(--coll); height: 0; }
  .sw.dot { width: 8px; height: 8px; border-radius: 50%; background: #111; }
  .sw.mark { width: 2px; height: 12px; background: #111; }
  main {
    position: relative; flex: 1; min-height: 0; margin: 0 16px 16px;
    background: var(--panel); border: 1px solid var(--border); border-radius: 10px;
  }
  #plots {
    position: absolute; inset: 0; padding: 10px; display: grid; gap: 10px;
    grid-template-columns: repeat(2, minmax(0, 1fr)); grid-template-rows: repeat(2, minmax(0, 1fr));
  }
  .plot { display: flex; flex-direction: column; min-height: 0; border: 1px solid var(--border); border-radius: 8px; }
  .plot h2 { margin: 0; padding: 6px 10px 0; font-size: 13px; font-weight: 600; color: var(--muted); }
  .plot .draw { position: relative; flex: 1; min-height: 0; }
  @media (max-width: 900px) { #plots { grid-template-columns: minmax(0, 1fr); grid-auto-rows: 320px; overflow-y: auto; } }
  #message { position: absolute; inset: 0; display: flex; align-items: center; justify-content: center; color: var(--muted); }
</style>
</head>
<body>
  <header>
    <h1>Delphes <span>· event summary</span></h1>
    <div class="group">
      <button id="prev" title="Previous event (left arrow)">&#8249;</button>
      <span class="muted">Event</span>
      <input id="event" type="number" min="0" value="0" title="Go to event (Enter)">
      <span id="entries" class="muted">/ &ndash;</span>
      <button id="next" title="Next event (right arrow)">&#8250;</button>
    </div>
    <div class="group sample">
      <span id="sampleText" class="muted">Sample distributions not filled</span>
      <div class="bar" id="bar" hidden><div id="barFill"></div></div>
      <button id="fill" class="primary" title="Read all events to build the sample distributions">Fill from sample</button>
    </div>
  </header>
  <div id="chips"></div>
  <div id="legend">
    <span data-role="all"><i class="sw fill"></i>sample, all objects (per event)</span>
    <span data-role="leading"><i class="sw line"></i>sample, leading object</span>
    <span data-role="subleading"><i class="sw dash"></i>sample, subleading object</span>
    <span data-role="event"><i class="sw dot"></i>this event</span>
    <span data-role="marker"><i class="sw mark"></i>this event (multiplicity)</span>
  </div>
  <main>
    <div id="message">Connecting&hellip;</div>
    <div id="plots"></div>
  </main>

<script type="module">
import { connectWebWindow } from 'jsroot/webwindow';
import { draw, cleanup, parse, resize, gStyle } from 'jsroot';

// the colour table of JSROOT, to add the collection colours
const colors = await import(new URL('./base/colors.mjs', import.meta.resolve('jsroot')).href);
gStyle.fOptStat = 0;
gStyle.fOptTitle = 0;
gStyle.fPadGridY = true;
gStyle.fPadLeftMargin = 0.14;
gStyle.fPadRightMargin = 0.04;
gStyle.fPadTopMargin = 0.05;
gStyle.fPadBottomMargin = 0.16;
gStyle.fGridColor = colors.addColor('rgb(220, 223, 228)');
gStyle.fFrameLineColor = colors.addColor('rgb(150, 155, 165)');

function rgba(hex, alpha) {
  const n = parseInt(hex.slice(1), 16);
  return `rgba(${n >> 16 & 255}, ${n >> 8 & 255}, ${n & 255}, ${alpha})`;
}
function darker(hex) {
  const n = parseInt(hex.slice(1), 16), f = c => Math.round(c * 0.7);
  return `rgb(${f(n >> 16 & 255)}, ${f(n >> 8 & 255)}, ${f(n & 255)})`;
}

// look of each kind of object, from the collection colour
function applyStyle(obj, role, hex) {
  const main = colors.addColor(rgba(hex, 1)), dark = colors.addColor(darker(hex));
  const black = 1;
  obj.fLineWidth = 2;
  switch (role) {
    case 'all':
      obj.fLineColor = main; obj.fFillColor = colors.addColor(rgba(hex, 0.28)); obj.fFillStyle = 1001; break;
    case 'leading':
      obj.fLineColor = dark; obj.fFillStyle = 0; break;
    case 'subleading':
      obj.fLineColor = dark; obj.fLineStyle = 2; obj.fFillStyle = 0; break;
    case 'event':
      obj.fLineColor = black; obj.fMarkerColor = black; obj.fMarkerStyle = 20; obj.fMarkerSize = 1.1; break;
    case 'marker':
      obj.fLineColor = black; obj.fLineWidth = 3; break;
  }
  for (const axis of [obj.fXaxis, obj.fYaxis]) {
    if (!axis) continue;
    axis.fLabelColor = axis.fTitleColor = colors.addColor('rgb(60, 64, 72)');
    axis.fAxisColor = gStyle.fFrameLineColor;
  }
}

let drawing = Promise.resolve();

const $ = id => document.getElementById(id);
let handle = null, collections = [], selected = null, entries = 0, current = 0, sampleDone = false;

function send(msg) { if (handle) handle.send(msg); }

function setMessage(text) {
  $('message').textContent = text || '';
  $('message').hidden = !text;
}

function select(name) {
  if (name === selected) return;
  selected = name;
  const c = collections.find(c => c.name === name);
  if (c) document.documentElement.style.setProperty('--coll', c.color);
  for (const chip of document.querySelectorAll('.chip'))
    chip.classList.toggle('active', chip.dataset.name === name);
  send('SELECT:' + name);
}

function init(data) {
  collections = data.collections;
  const chips = $('chips');
  chips.innerHTML = '';
  for (const c of collections) {
    const chip = document.createElement('div');
    chip.className = 'chip';
    chip.dataset.name = c.name;
    chip.title = c.type;
    chip.innerHTML = `<i class="dot" style="background:${c.color}"></i><span>${c.name}</span><span class="count"></span>`;
    chip.onclick = () => select(c.name);
    chips.appendChild(chip);
  }
  setMessage(collections.length ? '' : 'No collections to show');
  const keep = collections.find(c => c.name === selected);
  selected = null;
  // by default, the first branch displayed in the views
  const first = collections.find(c => c.shown) || collections[0];
  if (collections.length) select(keep ? keep.name : first.name);
}

function showEvent(data) {
  entries = data.entries;
  current = data.event;
  $('event').value = current;
  $('event').max = entries - 1;
  $('entries').textContent = `/ ${entries}`;
  $('prev').disabled = current <= 0;
  $('next').disabled = current >= entries - 1;
  for (const c of data.collections) {
    const chip = document.querySelector(`.chip[data-name="${CSS.escape(c.name)}"] .count`);
    if (chip) chip.textContent = c.label;
  }
  showProgress(data.sample);
}

function showProgress(s) {
  sampleDone = s.done > 0;
  const running = s.running;
  $('bar').hidden = !running;
  $('barFill').style.width = s.total ? `${100 * s.done / s.total}%` : '0';
  $('fill').disabled = running;
  $('fill').textContent = sampleDone && !running ? 'Refill' : 'Fill from sample';
  $('sampleText').textContent = running ? `Reading events ${s.done} / ${s.total}`
    : sampleDone ? `Sample: ${s.done} events` : 'Sample distributions not filled';
}

function showPlot(data) {
  const nl = data.indexOf('\n');
  if (data.slice(0, nl) !== selected) return;
  const plots = JSON.parse(data.slice(nl + 1)).plots;
  const hex = collections.find(c => c.name === selected)?.color || '#0a6ed1';
  // draw one update after the other
  drawing = drawing.then(() => drawPlots(plots, hex)).catch(e => console.error(e));
}

async function drawPlots(plots, hex) {
  const grid = $('plots');
  if (grid.children.length !== plots.length) {
    for (const cell of grid.querySelectorAll('.draw')) cleanup(cell);
    grid.innerHTML = '';
    for (const p of plots) {
      const cell = document.createElement('div');
      cell.className = 'plot';
      cell.innerHTML = '<h2></h2><div class="draw"></div>';
      grid.appendChild(cell);
    }
  }
  const roles = new Set(plots.flatMap(p => p.objects.map(o => o.role)));
  for (const item of document.querySelectorAll('#legend [data-role]'))
    item.hidden = !roles.has(item.dataset.role);
  for (let i = 0; i < plots.length; ++i) {
    const p = plots[i], cell = grid.children[i], dom = cell.querySelector('.draw');
    cell.querySelector('h2').textContent = p.title;
    cleanup(dom);
    for (let k = 0; k < p.objects.length; ++k) {
      const o = p.objects[k], obj = parse(o.json);
      applyStyle(obj, o.role, hex);
      let opt = o.role === 'event' ? 'P' : (o.role === 'marker' ? '' : 'hist');
      if (k === 0)
        opt += ';tickx;ticky;gridy' + (p.logx ? ';logx' : '') + (p.logy ? ';logy' : '');
      else
        opt = 'same;' + opt;
      await draw(dom, obj, opt);
    }
  }
}

$('prev').onclick = () => send('GOTO:' + (current - 1));
$('next').onclick = () => send('GOTO:' + (current + 1));
$('event').onchange = () => send('GOTO:' + $('event').value);
$('fill').onclick = () => send('FILL');
document.addEventListener('keydown', e => {
  if (e.target.tagName === 'INPUT') return;
  if (e.key === 'ArrowLeft' && current > 0) send('GOTO:' + (current - 1));
  if (e.key === 'ArrowRight' && current < entries - 1) send('GOTO:' + (current + 1));
});
let resizeTimer = null;
window.addEventListener('resize', () => {
  clearTimeout(resizeTimer);
  resizeTimer = setTimeout(() => { for (const d of document.querySelectorAll('.draw')) resize(d); }, 150);
});

connectWebWindow({
  receiver: {
    onWebsocketOpened(h) { handle = h; setMessage(''); send('READY'); },
    onWebsocketMsg(h, msg) {
      if (typeof msg !== 'string') return;
      const p = msg.indexOf(':');
      const kind = msg.slice(0, p), data = msg.slice(p + 1);
      if (kind === 'INIT') init(JSON.parse(data));
      else if (kind === 'EVENT') showEvent(JSON.parse(data));
      else if (kind === 'PROGRESS') showProgress(JSON.parse(data));
      else if (kind === 'PLOT') showPlot(data);
    },
    onWebsocketClosed() { handle = null; setMessage('Disconnected from ROOT'); }
  }
});
</script>
</body>
</html>
)DELPHESPAGE"
