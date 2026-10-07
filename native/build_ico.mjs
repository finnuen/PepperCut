import fs from 'node:fs';

function renderIconPixelsRGBA(size) {
  const rgba = new Uint8Array(size * size * 4);
  const scale = size / 32;

  const distToSegment = (px, py, x1, y1, x2, y2) => {
    const dx = x2 - x1;
    const dy = y2 - y1;
    const l2 = dx * dx + dy * dy;
    if (l2 === 0) return Math.hypot(px - x1, py - y1);
    let t = ((px - x1) * dx + (py - y1) * dy) / l2;
    t = Math.max(0, Math.min(1, t));
    return Math.hypot(px - (x1 + t * dx), py - (y1 + t * dy));
  };

  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const nx = (x + 0.5) / scale;
      const ny = (y + 0.5) / scale;

      const cx = Math.max(5.5, Math.min(26.5, nx));
      const cy = Math.max(5.5, Math.min(26.5, ny));
      const cornerDist = Math.hypot(nx - cx, ny - cy);
      if (cornerDist > 5.8) continue;

      const shade = 1 - ((nx + ny) / 64) * 0.12;
      let r = Math.round(220 * shade);
      let g = Math.round(38 * shade);
      let b = Math.round(38 * shade);
      const edgeAlpha =
        cornerDist > 4.8
          ? Math.max(0, Math.min(255, Math.round((5.8 - cornerDist) * 255)))
          : 255;

      // Centered White Scissors on Red Background (No cable)
      // Finger ring 1 (top-left loop): center (8.5, 11.0), radius 3.6
      const dRing1 = Math.abs(Math.hypot(nx - 8.5, ny - 11.0) - 3.6);
      // Finger ring 2 (bottom-left loop): center (8.5, 21.0), radius 3.6
      const dRing2 = Math.abs(Math.hypot(nx - 8.5, ny - 21.0) - 3.6);

      // Blade 1 (from top handle through center pivot (15.5, 16.0) to bottom-right tip (26.0, 22.5))
      const dBlade1 = distToSegment(nx, ny, 11.5, 13.2, 26.0, 22.5);
      // Blade 2 (from bottom handle through center pivot (15.5, 16.0) to top-right tip (26.0, 9.5))
      const dBlade2 = distToSegment(nx, ny, 11.5, 18.8, 26.0, 9.5);

      const minWhiteDist = Math.min(dRing1, dRing2, dBlade1, dBlade2);

      if (minWhiteDist < 1.45) {
        const wFactor =
          minWhiteDist < 0.85 ? 1 : Math.max(0, 1 - (minWhiteDist - 0.85) / 0.6);
        const dPivot = Math.hypot(nx - 16.2, ny - 16.0);
        const finalW = dPivot < 0.95 ? 0 : wFactor;
        r = Math.round(r * (1 - finalW) + 255 * finalW);
        g = Math.round(g * (1 - finalW) + 255 * finalW);
        b = Math.round(b * (1 - finalW) + 255 * finalW);
      }

      const idx = (y * size + x) * 4;
      rgba[idx] = r;
      rgba[idx + 1] = g;
      rgba[idx + 2] = b;
      rgba[idx + 3] = edgeAlpha;
    }
  }
  return rgba;
}

function buildDib(size) {
  const rgba = renderIconPixelsRGBA(size);
  const xorSize = size * size * 4;
  const andRowBytes = Math.ceil(size / 32) * 4;
  const andSize = andRowBytes * size;
  const buf = new Uint8Array(40 + xorSize + andSize);
  const dv = new DataView(buf.buffer);

  dv.setUint32(0, 40, true);
  dv.setInt32(4, size, true);
  dv.setInt32(8, size * 2, true);
  dv.setUint16(12, 1, true);
  dv.setUint16(14, 32, true);
  dv.setUint32(16, 0, true);
  dv.setUint32(20, xorSize + andSize, true);

  let offset = 40;
  for (let y = size - 1; y >= 0; y--) {
    for (let x = 0; x < size; x++) {
      const src = (y * size + x) * 4;
      buf[offset++] = rgba[src + 2];
      buf[offset++] = rgba[src + 1];
      buf[offset++] = rgba[src + 0];
      buf[offset++] = rgba[src + 3];
    }
  }
  for (let y = size - 1; y >= 0; y--) {
    for (let x = 0; x < size; x++) {
      const alpha = rgba[(y * size + x) * 4 + 3];
      if (alpha < 128) {
        const byteIndex = offset + (size - 1 - y) * andRowBytes + (x >> 3);
        buf[byteIndex] |= 1 << (7 - (x & 7));
      }
    }
  }
  return buf;
}

const sizes = [16, 32, 48, 64];
const dibs = sizes.map((s) => buildDib(s));
const headerSize = 6 + sizes.length * 16;
const totalSize = headerSize + dibs.reduce((acc, d) => acc + d.length, 0);
const ico = new Uint8Array(totalSize);
const dv = new DataView(ico.buffer);

dv.setUint16(0, 0, true);
dv.setUint16(2, 1, true);
dv.setUint16(4, sizes.length, true);

let dataOffset = headerSize;
for (let i = 0; i < sizes.length; i++) {
  const s = sizes[i];
  const dib = dibs[i];
  const entryOff = 6 + i * 16;
  ico[entryOff + 0] = s;
  ico[entryOff + 1] = s;
  ico[entryOff + 2] = 0;
  ico[entryOff + 3] = 0;
  dv.setUint16(entryOff + 4, 1, true);
  dv.setUint16(entryOff + 6, 32, true);
  dv.setUint32(entryOff + 8, dib.length, true);
  dv.setUint32(entryOff + 12, dataOffset, true);
  ico.set(dib, dataOffset);
  dataOffset += dib.length;
}

fs.mkdirSync('public', { recursive: true });
fs.writeFileSync('native/PepperCut.ico', ico);
fs.writeFileSync('public/PepperCut.ico', ico);
console.log('Generated multi-resolution PepperCut.ico (white scissors on red):', ico.length, 'bytes');
