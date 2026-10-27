"use strict";
(() => {
  const canvas = document.getElementById("display");
  const context = canvas.getContext("2d", {alpha: false});
  context.imageSmoothingEnabled = false;
  const status = document.getElementById("status");
  const scale = document.getElementById("scale");
  const viewport = document.getElementById("viewport");
  // Pixel-format conversion only. All sprites, font rasterization, positioning,
  // clipping and animation frames came from C++; there are no JS drawing rules.
  const frames = window.PFME_FRAMES.map(encoded => {
    const bytes = atob(encoded);
    if (bytes.length !== 240 * 240 * 2) throw new Error("Invalid frame size");
    const pixels = context.createImageData(240, 240);
    for (let i = 0; i < 240 * 240; i++) {
      const value = bytes.charCodeAt(i * 2) | (bytes.charCodeAt(i * 2 + 1) << 8);
      const r = (value >> 11) & 31, g = (value >> 5) & 63, b = value & 31;
      pixels.data.set([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), 255], i * 4);
    }
    return pixels;
  });
  function fitScale() {
    const top = viewport.getBoundingClientRect().top;
    return Math.max(1, Math.floor(Math.min(viewport.clientWidth, window.innerHeight - top - 20) / 240));
  }
  function resize() {
    const factor = scale.value === "fit" ? fitScale() : Number(scale.value);
    canvas.style.width = `${240 * factor}px`;
    canvas.style.height = `${240 * factor}px`;
    document.getElementById("dimensions").textContent = `${240 * factor}×${240 * factor} · logical 240×240`;
  }
  scale.value = String(Math.min(3, fitScale()));
  scale.addEventListener("change", resize);
  window.addEventListener("resize", resize);
  resize();
  const start = performance.now();
  let shown = -1;
  function animate(now) {
    const tick = Math.floor((now - start) / 800);
    const frame = tick === 0 || tick % 2 === 1 ? 0 : 1;
    if (frame !== shown) { context.putImageData(frames[frame], 0, 0); shown = frame; }
    requestAnimationFrame(animate);
  }
  requestAnimationFrame(animate);
  status.textContent = "Offline title-screen capture ready. No server connection.";
  function control(intent) {
    const meaning = {buttonA: "New Game: character creation is not ported.", buttonB: "Load Game: persistence is not ported.", clockwise: "Encoder clockwise: no title-screen selection.", counterclockwise: "Encoder counterclockwise: no title-screen selection.", press: "Encoder press: no title-screen action.", back: "Back/cancel: already at the title screen."};
    status.textContent = meaning[intent];
  }
  const reserved = new Set(["w", "a", "s", "d"]);
  function validKeys() {
    const a = document.getElementById("keyA").value.toLowerCase();
    const b = document.getElementById("keyB").value.toLowerCase();
    const valid = /^[a-z0-9]$/.test(a) && /^[a-z0-9]$/.test(b) && a !== b && !reserved.has(a) && !reserved.has(b);
    if (!valid) status.textContent = "Choose distinct letter/number keys for A and B, excluding WASD.";
    return valid ? [a, b] : null;
  }
  document.getElementById("keyA").addEventListener("input", validKeys);
  document.getElementById("keyB").addEventListener("input", validKeys);
  document.addEventListener("keydown", event => {
    if (/^(INPUT|SELECT|TEXTAREA|BUTTON)$/.test(event.target.tagName) || event.ctrlKey || event.metaKey || event.altKey) return;
    const keys = validKeys();
    const key = event.key.toLowerCase();
    const intent = ["arrowup", "arrowright", "w", "d"].includes(key) ? "clockwise" :
      ["arrowdown", "arrowleft", "s", "a"].includes(key) ? "counterclockwise" :
      key === "enter" ? "press" : key === "escape" ? "back" :
      keys && key === keys[0] ? "buttonA" : keys && key === keys[1] ? "buttonB" : null;
    if (intent) { event.preventDefault(); control(intent); }
  });
  document.getElementById("buttons").addEventListener("click", event => {
    if (event.target.dataset.control) control(event.target.dataset.control);
  });
})();
