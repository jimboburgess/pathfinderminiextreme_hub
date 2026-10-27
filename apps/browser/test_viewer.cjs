// Logic-only tests; this is not a browser or a substitute for visual QA.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const handlers = {};
const elements = {};
let render, pixels, count = 0;
function element(id) {
  return elements[id] ??= {
    style: {}, value: '', textContent: '', clientWidth: 1100,
    addEventListener(type, callback) { handlers[id + ':' + type] = callback; },
    getBoundingClientRect() { return {top: 150}; }
  };
}
const canvas = element('display');
canvas.width = canvas.height = 240;
const context = {
  createImageData(w, h) { return {data: new Uint8ClampedArray(w*h*4)}; },
  putImageData(data, x, y) { assert.equal(x, 0); assert.equal(y, 0); pixels = data; count++; }
};
canvas.getContext = () => context;
element('keyA').value = 'j'; element('keyB').value = 'k';
const windowMock = {innerHeight: 950, addEventListener(type, fn) {handlers['window:' + type] = fn;}};
const sandbox = {
  window: windowMock, document: {getElementById: element, addEventListener(type, fn) {handlers['document:' + type] = fn;}},
  performance: {now: () => 100}, atob: s => Buffer.from(s, 'base64').toString('binary'),
  requestAnimationFrame: callback => {render = callback;}, Uint8ClampedArray
};
vm.createContext(sandbox);
const viewer = process.argv[2];
vm.runInContext(fs.readFileSync(path.join(viewer, 'frames.js'), 'utf8'), sandbox);
vm.runInContext(fs.readFileSync(path.join(viewer, 'viewer.js'), 'utf8'), sandbox);
assert.equal(element('scale').value, '3');
assert.equal(canvas.style.width, '720px');
assert.equal(context.imageSmoothingEnabled, false);
render(100);
const first = pixels;
// Every decoded RGB565 pixel must preserve the original color components.
const source = fs.readFileSync(path.join(viewer, 'title.rgb565'));
for (let i = 0; i < 57600; i++) {
  const v = source.readUInt16LE(i*2);
  assert.equal(pixels.data[i*4] >> 3, v >> 11);
  assert.equal(pixels.data[i*4+1] >> 2, (v >> 5) & 63);
  assert.equal(pixels.data[i*4+2] >> 3, v & 31);
  assert.equal(pixels.data[i*4+3], 255);
}
for (let scale = 1; scale <= 5; scale++) {
  element('scale').value = String(scale);
  handlers['scale:change']();
  assert.equal(canvas.style.width, `${scale*240}px`);
  assert.equal(canvas.style.height, `${scale*240}px`);
  assert.equal(canvas.width, 240); assert.equal(canvas.height, 240);
  assert.equal(pixels, first); assert.equal(count, 1);
}
element('scale').value = 'fit'; handlers['scale:change']();
assert.equal(canvas.style.width, '720px');
element('viewport').clientWidth = 239;
handlers['window:resize']();
assert.equal(canvas.style.width, '240px'); // Scroll rather than fractional shrink.
render(900); assert.equal(pixels, first);
render(1700); assert.notEqual(pixels, first); // Zoom did not restart animation.
const animated = pixels;
function key(key, target = 'CANVAS') {
  let prevented = false;
  handlers['document:keydown']({key, target: {tagName: target}, preventDefault() {prevented = true;}});
  return prevented;
}
for (const k of ['ArrowUp', 'ArrowRight', 'w', 'd']) {
  assert.equal(key(k), true); assert.match(element('status').textContent, /clockwise/);
}
for (const k of ['ArrowDown', 'ArrowLeft', 's', 'a']) {
  assert.equal(key(k), true); assert.match(element('status').textContent, /counterclockwise/);
}
key('j'); assert.match(element('status').textContent, /New Game/);
key('k'); assert.match(element('status').textContent, /Load Game/);
key('Enter'); assert.match(element('status').textContent, /no title-screen action/);
key('Escape'); assert.match(element('status').textContent, /already at the title/);
element('keyA').value = 'q'; key('q'); assert.match(element('status').textContent, /New Game/);
assert.equal(key('q', 'INPUT'), false);
element('keyB').value = 'q'; assert.equal(key('q'), false);
assert.match(element('status').textContent, /distinct/);
assert.equal(pixels, animated);
console.log('Viewer logic: RGB565 conversion, scales 1–5, integer fit, animation continuity, contextual input intents and key validation passed. Browser visual QA still required.');
