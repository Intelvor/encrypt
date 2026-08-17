'use strict';

/**
 * crypt-lite — lightweight text + image encrypt/decrypt.
 *
 * The text and image algorithms here are byte-for-byte identical to the
 * encrypt desktop (C), web and Android builds, so outputs are fully
 * interoperable across platforms.
 *
 * NOTE: this is obfuscation, NOT modern cryptography (no KDF / salt /
 * authentication). Do NOT use it to protect sensitive data.
 */

// ===================== Text =====================

const MOD = 0x110000;

function isControl(code) {
  return (
    (code <= 0x1f) ||
    (code >= 0x7f && code <= 0x9f) ||
    (code >= 0xd800 && code <= 0xdfff)
  );
}

// Build the safe codepoint maps once.
const SAFE_TOTAL = (function () {
  let n = 0;
  for (let i = 0; i < MOD; i++) if (!isControl(i)) n++;
  return n;
})();

const fwdMap = new Uint32Array(SAFE_TOTAL);
const revMap = new Int32Array(MOD);
(function buildMaps() {
  let safeIdx = 0;
  for (let code = 0; code < MOD; code++) {
    if (!isControl(code)) {
      fwdMap[safeIdx] = code;
      revMap[code] = safeIdx;
      safeIdx++;
    } else {
      revMap[code] = -1;
    }
  }
})();

function hashKey(key) {
  let h = 0x811c9dc5;
  for (let i = 0; i < key.length; i++) {
    h ^= key.charCodeAt(i);
    h = (h * 0x01000193) | 0;
  }
  return h >>> 0;
}

function makeShift(key, pos) {
  const seed = hashKey(key);
  let x = (seed + pos * 0x9e3779b9) | 0;
  x = (x ^ (x >>> 16)) * 0x45d9f3b | 0;
  x = (x ^ (x >>> 16)) * 0x45d9f3b | 0;
  x = x ^ (x >>> 16);
  return (x >>> 0) % 256;
}

function safeShift(pos, delta) {
  let p = (pos + delta) % SAFE_TOTAL;
  if (p < 0) p += SAFE_TOTAL;
  while (isControl(fwdMap[p])) p = (p + 1) % SAFE_TOTAL;
  return p;
}

// Normalize CRLF → LF, matching the C desktop version's normalize_lf().
// Order matters: \r\n first (so it doesn't become \n\n), then lone \r.
function _normalizeLF(s) {
  return s.replace(/\r\n/g, '\n').replace(/\r/g, '\n');
}

function encryptText(text, key, rounds) {
  text = _normalizeLF(text);
  rounds = rounds || 1;
  for (let r = 0; r < rounds; r++) text = _shiftText(text, key, true);
  return text;
}

function decryptText(cipher, key, rounds) {
  cipher = _normalizeLF(cipher);
  rounds = rounds || 1;
  for (let r = 0; r < rounds; r++) cipher = _shiftText(cipher, key, false);
  return cipher;
}

function _shiftText(text, key, enc) {
  let result = '';
  let cp = 0;
  for (let i = 0; i < text.length; i++) {
    const code = text.codePointAt(i);
    const safePos = revMap[code];
    if (safePos === -1) {
      result += String.fromCodePoint(code);
    } else {
      const shift = makeShift(key, cp);
      const newPos = safeShift(safePos, enc ? shift : -shift);
      result += String.fromCodePoint(fwdMap[newPos]);
    }
    if (code > 0xffff) i++;
    cp++;
  }
  return result;
}

// ===================== Image (RGBA buffer) =====================

const B = 0x9e3779b9;

function rngNext(st) {
  let s = st.s;
  s ^= s << 13;
  s >>>= 0;
  s ^= s >>> 17;
  s ^= s << 5;
  s >>>= 0;
  st.s = s;
  return s >>> 0;
}

function roundTransform(px32, perm, npx, xv, out32, st, enc) {
  for (let i = 0; i < npx; i++) perm[i] = i;
  for (let i = npx - 1; i > 0; i--) {
    const j = rngNext(st) % (i + 1);
    const t = perm[i];
    perm[i] = perm[j];
    perm[j] = t;
  }
  const nbyte = npx * 4;
  for (let i = 0; i < nbyte; i++) xv[i] = rngNext(st) & 0xff;

  if (enc) {
    for (let i = 0; i < npx; i++) {
      const v = px32[i];
      const k =
        (xv[i * 4] |
          (xv[i * 4 + 1] << 8) |
          (xv[i * 4 + 2] << 16) |
          (xv[i * 4 + 3] << 24)) >>>
        0;
      out32[perm[i]] = (((v ^ k) & 0x00ffffff) | (v & 0xff000000)) >>> 0;
    }
  } else {
    for (let i = 0; i < npx; i++) out32[i] = px32[perm[i]];
    for (let i = 0; i < npx; i++) {
      const v = out32[i];
      const k =
        (xv[i * 4] |
          (xv[i * 4 + 1] << 8) |
          (xv[i * 4 + 2] << 16) |
          (xv[i * 4 + 3] << 24)) >>>
        0;
      out32[i] = (((v ^ k) & 0x00ffffff) | (v & 0xff000000)) >>> 0;
    }
  }
  px32.set(out32);
}

/**
 * Encrypt an RGBA pixel buffer in place.
 * @param {Uint8ClampedArray|Uint8Array} px RGBA buffer (length = w*h*4)
 * @param {string} key
 * @param {number} rounds
 * @param {number} w
 * @param {number} h
 */
function encryptImageRGBA(px, key, rounds, w, h) {
  rounds = rounds || 1;
  _transformImage(px, key, rounds, w, h, true);
}

function decryptImageRGBA(px, key, rounds, w, h) {
  rounds = rounds || 1;
  _transformImage(px, key, rounds, w, h, false);
}

function _transformImage(px, key, rounds, w, h, enc) {
  const npx = w * h;
  const nbyte = npx * 4;
  const px32 = new Uint32Array(px.buffer, px.byteOffset, npx);
  const perm = new Uint32Array(npx);
  const xv = new Uint8Array(nbyte);
  const out32 = new Uint32Array(npx);
  const seedBase = hashKey(key);
  const st = { s: 0 };
  if (enc) {
    for (let r = 0; r < rounds; r++) {
      st.s = (seedBase ^ Math.imul(r + 1, B)) >>> 0;
      roundTransform(px32, perm, npx, xv, out32, st, true);
    }
  } else {
    for (let r = rounds - 1; r >= 0; r--) {
      st.s = (seedBase ^ Math.imul(r + 1, B)) >>> 0;
      roundTransform(px32, perm, npx, xv, out32, st, false);
    }
  }
}

// ===================== Node PNG helpers (lazy) =====================

function _pngjs() {
  // Required only in Node when reading/writing PNG files.
  try {
    return require('pngjs');
  } catch (e) {
    throw new Error(
      'pngjs is required for PNG file I/O in Node. Install with: npm i pngjs'
    );
  }
}

/**
 * Decode a PNG file to an RGBA buffer. Node only.
 * @returns {Promise<{rgba:Uint8ClampedArray,w:number,h:number}>}
 */
async function decodePNGFile(path) {
  const { PNG } = _pngjs();
  const fs = require('fs');
  const data = await new Promise((resolve, reject) => {
    fs.readFile(path, (err, buf) => (err ? reject(err) : resolve(buf)));
  });
  const png = PNG.sync.read(data);
  return {
    rgba: new Uint8ClampedArray(png.data.buffer, png.data.byteOffset, png.data.length),
    w: png.width,
    h: png.height,
  };
}

/**
 * Encrypt a PNG file. Node only.
 */
async function encryptPNGFile(inPath, outPath, key, rounds) {
  const { PNG } = _pngjs();
  const fs = require('fs');
  const { rgba, w, h } = await decodePNGFile(inPath);
  encryptImageRGBA(rgba, key, rounds, w, h);
  const png = new PNG({ width: w, height: h });
  png.data.set(Buffer.from(rgba));
  await new Promise((resolve, reject) => {
    fs.writeFile(outPath, PNG.sync.write(png), (err) => (err ? reject(err) : resolve()));
  });
}

/**
 * Decrypt a PNG file. Node only.
 */
async function decryptPNGFile(inPath, outPath, key, rounds) {
  const { PNG } = _pngjs();
  const fs = require('fs');
  const { rgba, w, h } = await decodePNGFile(inPath);
  decryptImageRGBA(rgba, key, rounds, w, h);
  const png = new PNG({ width: w, height: h });
  png.data.set(Buffer.from(rgba));
  await new Promise((resolve, reject) => {
    fs.writeFile(outPath, PNG.sync.write(png), (err) => (err ? reject(err) : resolve()));
  });
}

module.exports = {
  // text
  encryptText,
  decryptText,
  hashKey,
  // image (RGBA buffer, platform-agnostic)
  encryptImageRGBA,
  decryptImageRGBA,
  // image (PNG files, Node only)
  decodePNGFile,
  encryptPNGFile,
  decryptPNGFile,
};
