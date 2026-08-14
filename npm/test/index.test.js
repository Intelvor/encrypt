'use strict';

const assert = require('assert');
const crypto = require('../src/index.js');

let passed = 0;
function ok(name, cond) {
  if (!cond) throw new Error('FAILED: ' + name);
  passed++;
  console.log('  ok - ' + name);
}

// Round-trip text
function rtText(text, key, rounds) {
  const enc = crypto.encryptText(text, key, rounds);
  const dec = crypto.decryptText(enc, key, rounds);
  ok(`text round-trip (r=${rounds}) "${text.slice(0, 12)}..."`, dec === text);
  return enc;
}

console.log('Text:');
rtText('Hello, 世界! 🌍', 'secret', 1);
rtText('Hello, 世界! 🌍', 'secret', 3);
rtText('', 'k', 1);
rtText('abc', '', 1); // empty key
const e1 = rtText('same input', 'keyA', 2);
const e2 = rtText('same input', 'keyB', 2);
ok('different keys -> different cipher', e1 !== e2);
const e3 = rtText('same input', 'keyA', 5);
ok('different rounds -> different cipher', e1 !== e3);

// Determinism: same input/key/rounds -> same output
ok(
  'deterministic',
  crypto.encryptText('repeat', 'k', 2) === crypto.encryptText('repeat', 'k', 2)
);

// hashKey matches FNV-1a reference
ok('hashKey("")', crypto.hashKey('') === 0x811c9dc5);
ok('hashKey("a")', crypto.hashKey('a') === (((0x811c9dc5 ^ 97) * 0x01000193) | 0) >>> 0);

// Image round-trip (RGBA buffer)
console.log('Image:');
function makeRGBA(w, h, fill) {
  const buf = new Uint8ClampedArray(w * h * 4);
  for (let i = 0; i < w * h; i++) {
    buf[i * 4] = fill[i % 3] || 10;
    buf[i * 4 + 1] = (i * 7) & 0xff;
    buf[i * 4 + 2] = (i * 13) & 0xff;
    buf[i * 4 + 3] = 255;
  }
  return buf;
}
function rtImage(w, h, key, rounds) {
  const orig = makeRGBA(w, h, [10, 20, 30]);
  const work = orig.slice();
  crypto.encryptImageRGBA(work, key, rounds, w, h);
  // encrypted should differ from original (very unlikely identical)
  let changed = false;
  for (let i = 0; i < work.length; i++) if (work[i] !== orig[i]) { changed = true; break; }
  ok(`image changed after encrypt ${w}x${h} r=${rounds}`, changed);
  crypto.decryptImageRGBA(work, key, rounds, w, h);
  ok(
    `image round-trip ${w}x${h} r=${rounds}`,
    Buffer.compare(Buffer.from(work), Buffer.from(orig)) === 0
  );
}
rtImage(4, 4, 'k', 1);
rtImage(7, 5, 'secret', 3);
rtImage(1, 1, 'x', 1);

// Alpha preserved
(function () {
  const w = 3, h = 3;
  const orig = makeRGBA(w, h, [1, 2, 3]);
  const work = orig.slice();
  crypto.encryptImageRGBA(work, 'k', 2, w, h);
  crypto.decryptImageRGBA(work, 'k', 2, w, h);
  let alphaOk = true;
  for (let i = 0; i < w * h; i++) if (work[i * 4 + 3] !== 255) alphaOk = false;
  ok('alpha channel preserved (0xFF000000 mask)', alphaOk);
})();

console.log(`\nAll ${passed} checks passed.`);
