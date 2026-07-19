// frame.js — decode base64 RGB565 frames (128x64, big-endian per pixel) onto a canvas.

export const FRAME_W = 128;
export const FRAME_H = 64;

export function makeFrameRenderer(canvas) {
  canvas.width = FRAME_W;
  canvas.height = FRAME_H;
  const ctx = canvas.getContext('2d');
  const image = ctx.createImageData(FRAME_W, FRAME_H);
  const rgba = image.data;

  return function render(base64) {
    let raw;
    try { raw = atob(base64); } catch { return false; }
    if (raw.length < FRAME_W * FRAME_H * 2) return false;
    let di = 0;
    for (let i = 0; i < FRAME_W * FRAME_H * 2; i += 2) {
      const v = (raw.charCodeAt(i) << 8) | raw.charCodeAt(i + 1);
      const r = (v >> 11) & 0x1f;
      const g = (v >> 5) & 0x3f;
      const b = v & 0x1f;
      rgba[di++] = (r << 3) | (r >> 2);
      rgba[di++] = (g << 2) | (g >> 4);
      rgba[di++] = (b << 3) | (b >> 2);
      rgba[di++] = 255;
    }
    ctx.putImageData(image, 0, 0);
    return true;
  };
}
