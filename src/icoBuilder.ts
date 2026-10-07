/**
 * Programmatic generator for PepperCut's icon:
 * White scissors on a crimson red (#DC2626) background (no cable).
 */

export function renderIconPixelsRGBA(size: number): Uint8Array {
  const rgba = new Uint8Array(size * size * 4);
  const scale = size / 32;

  const distToSegment = (px: number, py: number, x1: number, y1: number, x2: number, y2: number) => {
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

      const cx = Math.max(6, Math.min(26, nx));
      const cy = Math.max(6, Math.min(26, ny));
      const cornerDist = Math.hypot(nx - cx, ny - cy);
      if (cornerDist > 6.2) {
        continue;
      }

      const shade = 1 - ((nx + ny) / 64) * 0.12;
      let r = Math.round(220 * shade);
      let g = Math.round(38 * shade);
      let b = Math.round(38 * shade);
      const edgeAlpha =
        cornerDist > 5.2
          ? Math.max(0, Math.min(255, Math.round((6.2 - cornerDist) * 255)))
          : 255;

      // White Scissors (no cable)
      const dRing1 = Math.abs(Math.hypot(nx - 8.5, ny - 11.0) - 3.6);
      const dRing2 = Math.abs(Math.hypot(nx - 8.5, ny - 21.0) - 3.6);
      const dBlade1 = distToSegment(nx, ny, 11.5, 13.2, 26.0, 22.5);
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
