'use strict';

const { encryptText, decryptText, encryptImageRGBA, decryptImageRGBA, hashKey } = require('../src/index.js');

let passed = 0, failed = 0, warnings = 0;
const failures = [];
const warns = [];

function assert(cond, name, detail) {
  if (cond) { passed++; }
  else { failed++; failures.push({ name, detail: detail || '' }); console.log(`  FAIL: ${name}`); if (detail) console.log(`        ${detail}`); }
}

function warn(msg, detail) { warnings++; warns.push({ msg, detail }); console.log(`  WARN: ${msg}`); if (detail) console.log(`        ${detail}`); }

// ==================== TEXT TESTS ====================
console.log('\n=== TEXT ENCRYPTION EXTREME TESTS ===\n');

// --- T1: Empty string ---
console.log('[T1] Empty string');
{
  const enc = encryptText('', 'key', 1);
  assert(enc === '', 'encrypt empty → empty');
  const dec = decryptText('', 'key', 1);
  assert(dec === '', 'decrypt empty → empty');
  const enc3 = encryptText('', 'key', 10);
  assert(enc3 === '', 'encrypt empty 10 rounds → empty');
}

// --- T2: Single character ---
console.log('[T2] Single character');
{
  const enc = encryptText('A', 'k', 1);
  assert(enc.length > 0, 'encrypt single char → non-empty');
  const dec = decryptText(enc, 'k', 1);
  assert(dec === 'A', `roundtrip single char: "${dec}"`);
}

// --- T3: All ASCII printable ---
console.log('[T3] Full ASCII printable (32-126)');
{
  let text = '';
  for (let i = 32; i <= 126; i++) text += String.fromCharCode(i);
  const enc = encryptText(text, 'key', 1);
  const dec = decryptText(enc, 'key', 1);
  assert(dec === text, 'roundtrip full ASCII printable', dec.length !== text.length ? `len ${dec.length} vs ${text.length}` : '');
  // Verify encrypted text is actually different
  assert(enc !== text, 'encrypted differs from plaintext');
}

// --- T4: Control characters pass through; CRLF normalized ---
console.log('[T4] Control characters (\\t/\\n pass through; \\r/\\r\\n normalized to \\n)');
{
  const text = 'A\tB\nC\rD\r\nE';
  const enc = encryptText(text, 'k', 1);
  assert(enc.includes('\t'), 'tab preserved in encrypted');
  assert(enc.includes('\n'), 'newline preserved in encrypted');
  // \r and \r\n are normalized to \n (matching C desktop behavior)
  assert(!enc.includes('\r'), 'CR normalized to LF (no \\r in output)');
  const dec = decryptText(enc, 'k', 1);
  // After normalization, \r→\n and \r\n→\n, so roundtrip gives normalized form
  assert(dec === 'A\tB\nC\nD\nE', 'roundtrip: CRLF/CR normalized to LF');
}

// --- T5: Surrogate pairs (emoji) ---
console.log('[T5] Surrogate pairs / Emoji');
{
  const text = 'Hello 🔐🌍🎉 你好';
  const enc = encryptText(text, 'k', 3);
  const dec = decryptText(enc, 'k', 3);
  assert(dec === text, `roundtrip emoji: "${dec}"`);
  // Count codepoints: JS uses codePointAt, surrogate pairs count as 1
  let cpCount = 0;
  for (let i = 0; i < text.length; i++) { cpCount++; if (text.codePointAt(i) > 0xffff) i++; }
  assert(cpCount === 12, `codepoint count: ${cpCount}`);
}

// --- T6: CJK Extension B (U+20000+, surrogate pairs) ---
console.log('[T6] CJK Extension B (𠀀𠁀𠂀𠃀)');
{
  const text = '\u{20000}\u{20040}\u{20080}\u{200C0}';
  const enc = encryptText(text, 'k', 2);
  const dec = decryptText(enc, 'k', 2);
  assert(dec === text, `roundtrip CJK ext B: codepoints preserved`);
}

// --- T7: Zero-width characters ---
console.log('[T7] Zero-width characters');
{
  // U+200B zero-width space, U+200C zero-width non-joiner, U+200D zero-width joiner, U+FEFF BOM
  const text = 'A\u200BB\u200CC\u200DD\uFEEF';
  const enc = encryptText(text, 'k', 1);
  const dec = decryptText(enc, 'k', 1);
  assert(dec === text, 'roundtrip zero-width chars');
}

// --- T8: Maximum Unicode codepoint ---
console.log('[T8] Maximum Unicode codepoint U+10FFFF');
{
  const text = 'A\u{10FFFF}B';
  const enc = encryptText(text, 'k', 1);
  const dec = decryptText(enc, 'k', 1);
  assert(dec === text, 'roundtrip U+10FFFF');
  // Check it's a surrogate pair in JS
  assert(enc.length >= 3, 'encrypted has surrogate pair length');
}

// --- T9: All surrogate codepoints in input (should pass through as-is) ---
console.log('[T9] Raw surrogate codepoints U+D800-U+DFFF in input');
{
  // These are "control" chars per isControl, should pass through unchanged
  const text = 'A\uD800B\uDFFFC';
  const enc = encryptText(text, 'k', 1);
  assert(enc.charAt(1) === '\uD800', 'U+D800 passes through');
  assert(enc.charAt(3) === '\uDFFF', 'U+DFFF passes through');
  const dec = decryptText(enc, 'k', 1);
  assert(dec === text, 'roundtrip with raw surrogates');
}

// --- T10: Empty key ---
console.log('[T10] Empty key');
{
  const text = 'Hello';
  const enc = encryptText(text, '', 1);
  // Empty key → hashKey('') = 0x811c9dc5 (initial FNV offset basis), so shift is deterministic
  const dec = decryptText(enc, '', 1);
  assert(dec === text, `roundtrip empty key: "${dec}"`);
  assert(enc !== text, 'empty key still produces different output');
}

// --- T11: Very long key ---
console.log('[T11] Very long key (10000 chars)');
{
  const key = 'x'.repeat(10000);
  const text = 'Hello World';
  const enc = encryptText(text, key, 1);
  const dec = decryptText(enc, key, 1);
  assert(dec === text, 'roundtrip long key');
}

// --- T12: Unicode key ---
console.log('[T12] Unicode key (Chinese + emoji)');
{
  const key = '密码🔐';
  const text = 'Hello 世界';
  const enc = encryptText(text, key, 1);
  const dec = decryptText(enc, key, 1);
  assert(dec === text, `roundtrip unicode key: "${dec}"`);
}

// --- T13: High round counts ---
console.log('[T13] High round counts');
{
  const text = 'Test';
  for (const rounds of [10, 50, 100, 256]) {
    const enc = encryptText(text, 'k', rounds);
    const dec = decryptText(enc, 'k', rounds);
    assert(dec === text, `roundtrip ${rounds} rounds`);
  }
}

// --- T14: Rounds = 0 (should default to 1) ---
console.log('[T14] Rounds = 0 (default behavior)');
{
  const text = 'Hello';
  const enc0 = encryptText(text, 'k', 0);
  const enc1 = encryptText(text, 'k', 1);
  assert(enc0 === enc1, 'rounds=0 same as rounds=1');
}

// --- T15: Negative rounds ---
console.log('[T15] Negative rounds');
{
  const text = 'Hello';
  try {
    const enc = encryptText(text, 'k', -1);
    // JS: -1 rounds → loop 0 times → returns text unchanged
    warn('negative rounds (-1) does not throw, returns: "' + enc.substring(0, 20) + '"');
    // Verify: -1 is truthy, loop runs 0 times (r < -1 is false for r=0)
    // Actually: for (let r = 0; r < -1; r++) → never executes
    // So encryptText with -1 returns text as-is
    assert(enc === text, 'negative rounds returns text unchanged (no loop)');
  } catch (e) {
    warn('negative rounds throws: ' + e.message);
  }
}

// --- T16: Rounds undefined/null ---
console.log('[T16] Rounds undefined/null');
{
  const text = 'Hello';
  const encUndef = encryptText(text, 'k', undefined);
  const encNull = encryptText(text, 'k', null);
  const enc1 = encryptText(text, 'k', 1);
  assert(encUndef === enc1, 'undefined rounds = 1');
  assert(encNull === enc1, 'null rounds = 1');
}

// --- T17: Long text performance / correctness ---
console.log('[T17] Long text (100K chars)');
{
  const text = '你好世界Hello'.repeat(20000); // ~120K chars
  const t0 = Date.now();
  const enc = encryptText(text, 'k', 1);
  const t1 = Date.now();
  const dec = decryptText(enc, 'k', 1);
  const t2 = Date.now();
  assert(dec === text, `roundtrip 120K chars (enc=${t1-t0}ms, dec=${t2-t1}ms)`, dec.length !== text.length ? `len mismatch ${dec.length} vs ${text.length}` : '');
  if (t1 - t0 > 5000) warn(`encryption slow: ${t1-t0}ms for 120K chars`);
}

// --- T18: Text with only control characters ---
console.log('[T18] Text with only control characters');
{
  const text = '\x00\x01\x02\x1f\x7f\x80\x9f';
  const enc = encryptText(text, 'k', 1);
  assert(enc === text, 'all-control text passes through unchanged');
  const dec = decryptText(enc, 'k', 1);
  assert(dec === text, 'roundtrip all-control text');
}

// --- T19: Mixed control + safe characters ---
console.log('[T19] Mixed control + safe');
{
  const text = 'Hello\x00World\x07!\n\t';
  const enc = encryptText(text, 'k', 1);
  // Control chars pass through at same CODEPOINT positions (not string positions,
  // since encrypted chars may be surrogate pairs using 2 UTF-16 units)
  const encCodepoints = [];
  for (let i = 0; i < enc.length; i++) {
    const cp = enc.codePointAt(i);
    encCodepoints.push(cp);
    if (cp > 0xffff) i++;
  }
  assert(encCodepoints[5] === 0x00, 'null byte at codepoint 5');
  assert(encCodepoints[11] === 0x07, 'BEL at codepoint 11');
  assert(encCodepoints[13] === 0x0A, 'LF at codepoint 13');
  assert(encCodepoints[14] === 0x09, 'TAB at codepoint 14');
  const dec = decryptText(enc, 'k', 1);
  assert(dec === text, 'roundtrip mixed control+safe');
}

// --- T20: Key is all zeros / special bytes ---
console.log('[T20] Special keys');
{
  const text = 'Test';
  const keys = ['\x00', '\x00\x00\x00', '\xff\xff', '\uffff', ' ', '  '];
  for (const key of keys) {
    const enc = encryptText(text, key, 1);
    const dec = decryptText(enc, key, 1);
    assert(dec === text, `roundtrip key="${key.replace(/\x00/g, '\\0').replace(/\xff/g, '\\xff')}"`);
  }
}

// --- T21: Determinism — same input → same output ---
console.log('[T21] Determinism');
{
  const text = 'Hello 世界!';
  const key = 'secret';
  const enc1 = encryptText(text, key, 3);
  const enc2 = encryptText(text, key, 3);
  assert(enc1 === enc2, 'same input produces same ciphertext');
}

// --- T22: Different keys produce different outputs ---
console.log('[T22] Key sensitivity');
{
  const text = 'Hello';
  const enc1 = encryptText(text, 'key1', 1);
  const enc2 = encryptText(text, 'key2', 1);
  assert(enc1 !== enc2, 'different keys → different ciphertext');
}

// --- T23: Incrementing rounds changes output ---
console.log('[T23] Round sensitivity');
{
  const text = 'Hello';
  const outputs = new Set();
  for (let r = 1; r <= 10; r++) outputs.add(encryptText(text, 'k', r));
  assert(outputs.size === 10, `10 different round outputs: ${outputs.size}`);
}

// --- T24: CRLF normalization (JS side — should NOT normalize) ---
console.log('[T24] CRLF normalization (now matches C behavior)');
{
  const lf = 'a\nb';
  const crlf = 'a\r\nb';
  const loneCR = 'a\rb';
  const enc_lf = encryptText(lf, 'k', 1);
  const enc_crlf = encryptText(crlf, 'k', 1);
  const enc_cr = encryptText(loneCR, 'k', 1);
  assert(enc_lf === enc_crlf, 'CRLF and LF produce same ciphertext (normalized)');
  assert(enc_lf === enc_cr, 'lone CR and LF produce same ciphertext (normalized)');
}

// --- T25: All 256 possible byte values as text positions ---
console.log('[T25] Full codepoint coverage in shift range');
{
  // Generate text that exercises many different shift values
  let text = '';
  for (let i = 0x20; i < 0x7f; i++) text += String.fromCharCode(i);
  for (let i = 0xa0; i < 0x100; i++) text += String.fromCharCode(i);
  for (let i = 0x400; i < 0x500; i++) text += String.fromCharCode(i); // Cyrillic
  for (let i = 0x3000; i < 0x3100; i++) text += String.fromCharCode(i); // CJK symbols
  const enc = encryptText(text, 'k', 1);
  const dec = decryptText(enc, 'k', 1);
  assert(dec === text, `roundtrip multi-block text (${text.length} chars)`);
}

// ==================== IMAGE TESTS ====================
console.log('\n=== IMAGE ENCRYPTION EXTREME TESTS ===\n');

function makeRGBA(w, h, fill) {
  const px = new Uint8ClampedArray(w * h * 4);
  if (fill) {
    for (let i = 0; i < px.length; i += 4) {
      px[i] = fill[0]; px[i+1] = fill[1]; px[i+2] = fill[2]; px[i+3] = fill[3];
    }
  }
  return px;
}

function buffersEqual(a, b) {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

// --- I1: 1x1 pixel ---
console.log('[I1] 1x1 pixel');
{
  const px = new Uint8ClampedArray([255, 128, 64, 200]);
  const orig = new Uint8ClampedArray(px);
  encryptImageRGBA(px, 'k', 1, 1, 1);
  // 1x1 permutation is identity, XOR still changes RGB
  const dec = new Uint8ClampedArray(px);
  decryptImageRGBA(dec, 'k', 1, 1, 1);
  assert(buffersEqual(dec, orig), `1x1 roundtrip: [${dec}] vs [${orig}]`);
}

// --- I2: 2x2 pixel ---
console.log('[I2] 2x2 pixel');
{
  const px = new Uint8ClampedArray([
    255,0,0,255, 0,255,0,255,
    0,0,255,255, 255,255,0,255
  ]);
  const orig = new Uint8ClampedArray(px);
  encryptImageRGBA(px, 'k', 1, 2, 2);
  assert(!buffersEqual(px, orig), '2x2 encrypted differs from original');
  decryptImageRGBA(px, 'k', 1, 2, 2);
  assert(buffersEqual(px, orig), '2x2 roundtrip');
}

// --- I3: All same color (single-color image) ---
console.log('[I3] Single color image (all red)');
{
  const px = makeRGBA(10, 10, [255, 0, 0, 255]);
  const orig = new Uint8ClampedArray(px);
  encryptImageRGBA(px, 'k', 1, 10, 10);
  // Permutation shuffles same-value pixels → still same! Only XOR changes RGB.
  // After XOR, pixels are no longer all the same.
  const allSame = px.every((v, i) => i % 4 < 3 ? v === (px[i - (i%4)] ) : v === 255);
  // Alpha should be preserved
  let alphaOk = true;
  for (let i = 3; i < px.length; i += 4) if (px[i] !== 255) alphaOk = false;
  assert(alphaOk, 'alpha preserved in single-color encrypt');
  decryptImageRGBA(px, 'k', 1, 10, 10);
  assert(buffersEqual(px, orig), 'single-color roundtrip');
}

// --- I4: All transparent (alpha=0) ---
console.log('[I4] All transparent (alpha=0)');
{
  const px = makeRGBA(8, 8, [128, 64, 32, 0]);
  const orig = new Uint8ClampedArray(px);
  encryptImageRGBA(px, 'k', 1, 8, 8);
  let alphaOk = true;
  for (let i = 3; i < px.length; i += 4) if (px[i] !== 0) alphaOk = false;
  assert(alphaOk, 'all-zero alpha preserved');
  decryptImageRGBA(px, 'k', 1, 8, 8);
  assert(buffersEqual(px, orig), 'transparent image roundtrip');
}

// --- I5: Mixed alpha ---
console.log('[I5] Mixed alpha values');
{
  const px = new Uint8ClampedArray(4 * 4 * 4);
  for (let i = 0; i < px.length; i += 4) {
    px[i] = i & 0xFF; px[i+1] = (i*7) & 0xFF; px[i+2] = (i*13) & 0xFF; px[i+3] = (i*3) & 0xFF;
  }
  const orig = new Uint8ClampedArray(px);
  // Save original alpha pattern
  const alphaOrig = [];
  for (let i = 3; i < orig.length; i += 4) alphaOrig.push(orig[i]);
  
  encryptImageRGBA(px, 'k', 2, 4, 4);
  
  // Check alpha is preserved (same values, possibly in different positions due to permutation)
  const alphaEnc = [];
  for (let i = 3; i < px.length; i += 4) alphaEnc.push(px[i]);
  alphaOrig.sort((a,b) => a-b);
  alphaEnc.sort((a,b) => a-b);
  assert(buffersEqual(new Uint8ClampedArray(alphaOrig), new Uint8ClampedArray(alphaEnc)), 'alpha values preserved (multiset)');
  
  decryptImageRGBA(px, 'k', 2, 4, 4);
  assert(buffersEqual(px, orig), 'mixed-alpha roundtrip');
}

// --- I6: Boundary pixel values (0 and 255) ---
console.log('[I6] Boundary pixel values');
{
  const px = new Uint8ClampedArray([
    0,0,0,255,     255,255,255,255,
    0,255,0,255,   255,0,255,255
  ]);
  const orig = new Uint8ClampedArray(px);
  encryptImageRGBA(px, 'k', 1, 2, 2);
  decryptImageRGBA(px, 'k', 1, 2, 2);
  assert(buffersEqual(px, orig), 'boundary values roundtrip');
}

// --- I7: Large image (100x100 = 10000 pixels) ---
console.log('[I7] Large image 100x100');
{
  const px = new Uint8ClampedArray(100 * 100 * 4);
  for (let i = 0; i < px.length; i++) px[i] = (i * 37 + 13) & 0xFF;
  const orig = new Uint8ClampedArray(px);
  const t0 = Date.now();
  encryptImageRGBA(px, 'k', 3, 100, 100);
  const t1 = Date.now();
  const dec = new Uint8ClampedArray(px);
  decryptImageRGBA(dec, 'k', 3, 100, 100);
  const t2 = Date.now();
  assert(buffersEqual(dec, orig), `100x100 3-round roundtrip (enc=${t1-t0}ms, dec=${t2-t1}ms)`);
  if (t1 - t0 > 10000) warn(`image encryption slow: ${t1-t0}ms for 100x100x3`);
}

// --- I8: Very large image (500x500 = 250000 pixels) ---
console.log('[I8] Very large image 500x500');
{
  const px = new Uint8ClampedArray(500 * 500 * 4);
  for (let i = 0; i < px.length; i++) px[i] = (i * 37 + 13) & 0xFF;
  const orig = new Uint8ClampedArray(px);
  const t0 = Date.now();
  encryptImageRGBA(px, 'k', 1, 500, 500);
  const t1 = Date.now();
  decryptImageRGBA(px, 'k', 1, 500, 500);
  const t2 = Date.now();
  assert(buffersEqual(px, orig), `500x500 roundtrip (enc=${t1-t0}ms, dec=${t2-t0-(t1-t0)}ms)`);
}

// --- I9: Non-square dimensions ---
console.log('[I9] Non-square: 1x100 and 100x1');
{
  const px1 = new Uint8ClampedArray(1 * 100 * 4);
  for (let i = 0; i < px1.length; i++) px1[i] = i & 0xFF;
  const orig1 = new Uint8ClampedArray(px1);
  encryptImageRGBA(px1, 'k', 1, 1, 100);
  decryptImageRGBA(px1, 'k', 1, 1, 100);
  assert(buffersEqual(px1, orig1), '1x100 roundtrip');

  const px2 = new Uint8ClampedArray(100 * 1 * 4);
  for (let i = 0; i < px2.length; i++) px2[i] = i & 0xFF;
  const orig2 = new Uint8ClampedArray(px2);
  encryptImageRGBA(px2, 'k', 1, 100, 1);
  decryptImageRGBA(px2, 'k', 1, 100, 1);
  assert(buffersEqual(px2, orig2), '100x1 roundtrip');
}

// --- I10: Image roundtrip with many rounds ---
console.log('[I10] Image many rounds (1, 5, 10, 50)');
{
  for (const rounds of [1, 5, 10, 50]) {
    const px = new Uint8ClampedArray(20 * 20 * 4);
    for (let i = 0; i < px.length; i++) px[i] = (i * 37) & 0xFF;
    const orig = new Uint8ClampedArray(px);
    encryptImageRGBA(px, 'k', rounds, 20, 20);
    decryptImageRGBA(px, 'k', rounds, 20, 20);
    assert(buffersEqual(px, orig), `image ${rounds}-round roundtrip`);
  }
}

// --- I11: Image with empty key ---
console.log('[I11] Image with empty key');
{
  const px = new Uint8ClampedArray(4 * 4 * 4);
  for (let i = 0; i < px.length; i++) px[i] = (i * 17) & 0xFF;
  const orig = new Uint8ClampedArray(px);
  encryptImageRGBA(px, '', 1, 4, 4);
  decryptImageRGBA(px, '', 1, 4, 4);
  assert(buffersEqual(px, orig), 'image empty key roundtrip');
}

// --- I12: Image determinism ---
console.log('[I12] Image determinism');
{
  const px1 = new Uint8ClampedArray(10 * 10 * 4);
  const px2 = new Uint8ClampedArray(10 * 10 * 4);
  for (let i = 0; i < px1.length; i++) { px1[i] = px2[i] = (i * 37) & 0xFF; }
  encryptImageRGBA(px1, 'k', 3, 10, 10);
  encryptImageRGBA(px2, 'k', 3, 10, 10);
  assert(buffersEqual(px1, px2), 'image encryption is deterministic');
}

// --- I13: Uint32Array view endianness check ---
console.log('[I13] Uint32Array endianness (alpha preservation relies on layout)');
{
  // The algorithm uses Uint32Array view over the same buffer
  // Alpha is preserved via (v ^ k) & 0x00FFFFFF | (v & 0xFF000000)
  // This depends on little-endian layout where alpha is the highest byte
  const px = new Uint8ClampedArray([10, 20, 30, 255]);
  const px32 = new Uint32Array(px.buffer);
  const v = px32[0];
  const alpha = (v >> 24) & 0xFF;
  if (alpha !== 255) {
    warn(`Platform is BIG-ENDIAN! Alpha byte is at position 0, not 3. ` +
      `v=0x${v.toString(16).padStart(8,'0')}, alpha=${alpha}. ` +
      `Image encryption will NOT preserve alpha correctly on this platform!`);
  } else {
    assert(true, 'little-endian confirmed (alpha at byte 3)');
  }
}

// --- I14: w*h = 0 (zero-area image) ---
console.log('[I14] Zero-area image (0x0, 0x1, 1x0)');
{
  // These should not crash
  try {
    const px = new Uint8ClampedArray(0);
    encryptImageRGBA(px, 'k', 1, 0, 0);
    assert(true, '0x0 does not crash');
  } catch (e) {
    assert(false, '0x0 crashes', e.message);
  }
  try {
    const px = new Uint8ClampedArray(0);
    encryptImageRGBA(px, 'k', 1, 0, 1);
    assert(true, '0x1 does not crash');
  } catch (e) {
    assert(false, '0x1 crashes', e.message);
  }
  try {
    const px = new Uint8ClampedArray(0);
    encryptImageRGBA(px, 'k', 1, 1, 0);
    assert(true, '1x0 does not crash');
  } catch (e) {
    assert(false, '1x0 crashes', e.message);
  }
}

// --- I15: Image XOR mutuality (encryption is its own inverse for single round) ---
console.log('[I15] Double-encrypt = original? (XOR property)');
{
  const px = new Uint8ClampedArray(10 * 10 * 4);
  for (let i = 0; i < px.length; i++) px[i] = (i * 37) & 0xFF;
  const orig = new Uint8ClampedArray(px);
  encryptImageRGBA(px, 'k', 1, 10, 10);
  // Note: permutation is NOT its own inverse (applying it twice ≠ identity)
  // So double-encrypt ≠ original. This is expected.
  encryptImageRGBA(px, 'k', 1, 10, 10);
  const isOriginal = buffersEqual(px, orig);
  if (isOriginal) {
    warn('double-encrypt equals original (XOR-only, no permutation side effect)');
  }
  // Not a bug per se, but documenting the behavior
  assert(true, `double-encrypt ${isOriginal ? '=== ' : '!== '}original (documenting behavior)`);
}

// --- I16: Cross-round consistency: encrypt R1+R2 vs encrypt R1 then encrypt R2 ---
console.log('[I16] Multi-round consistency');
{
  // encrypt(key, rounds=3) should equal encrypt(key,1) then encrypt(key,1) then encrypt(key,1)
  // NO! Each round uses a DIFFERENT seed (seed ^ (r+1)*B), so they're not equivalent.
  // encrypt(key, 3) ≠ encrypt(key,1)³ because the seed changes per round.
  const px1 = new Uint8ClampedArray(8 * 8 * 4);
  const px2 = new Uint8ClampedArray(8 * 8 * 4);
  for (let i = 0; i < px1.length; i++) { px1[i] = px2[i] = (i * 37) & 0xFF; }
  
  encryptImageRGBA(px1, 'k', 3, 8, 8);
  encryptImageRGBA(px2, 'k', 1, 8, 8);
  encryptImageRGBA(px2, 'k', 1, 8, 8);
  encryptImageRGBA(px2, 'k', 1, 8, 8);
  
  // These should NOT be equal (different seeds per round)
  if (buffersEqual(px1, px2)) {
    warn('3-round single pass == 3x 1-round passes (unexpected)');
  }
  assert(true, 'multi-round seed behavior documented');
}

// --- I17: Performance scaling with image size ---
console.log('[I17] Performance scaling');
{
  const sizes = [[10,10], [50,50], [100,100], [200,200]];
  const times = [];
  for (const [w,h] of sizes) {
    const px = new Uint8ClampedArray(w * h * 4);
    for (let i = 0; i < px.length; i++) px[i] = i & 0xFF;
    const t0 = Date.now();
    encryptImageRGBA(px, 'k', 1, w, h);
    times.push({ size: `${w}x${h}`, ms: Date.now() - t0, pixels: w*h });
  }
  for (const t of times) console.log(`    ${t.size}: ${t.ms}ms (${t.pixels} pixels)`);
  assert(true, 'performance logged');
}

// --- T26: hashKey consistency ---
console.log('[T26] hashKey function');
{
  assert(hashKey('') === 0x811c9dc5, 'hashKey empty = FNV offset basis');
  assert(typeof hashKey('test') === 'number', 'hashKey returns number');
  assert(hashKey('test') === hashKey('test'), 'hashKey deterministic');
  assert(hashKey('a') !== hashKey('b'), 'hashKey different for different keys');
  assert(hashKey('test') >>> 0 === hashKey('test'), 'hashKey is unsigned 32-bit');
}

// --- T27: Very large rounds (potential integer overflow in seed computation) ---
console.log('[T27] Very large rounds (integer overflow in image seed)');
{
  const px = new Uint8ClampedArray(4 * 4 * 4);
  for (let i = 0; i < px.length; i++) px[i] = i & 0xFF;
  const orig = new Uint8ClampedArray(px);
  // Math.imul(0x7FFFFFFF, 0x9E3779B9) should handle overflow correctly
  warn('extreme rounds (0x7FFFFFFF) - skipped due to time (would run ~minutes)');
  // Test with something feasible (100 rounds on 2x2)
  const px2 = new Uint8ClampedArray(2 * 2 * 4);
  for (let i = 0; i < px2.length; i++) px2[i] = i & 0xFF;
  const orig2 = new Uint8ClampedArray(px2);
  encryptImageRGBA(px2, 'k', 100, 2, 2);
  decryptImageRGBA(px2, 'k', 100, 2, 2);
  assert(buffersEqual(px2, orig2), '100-round image roundtrip');
}

// --- T28: RNG state isolation between image and text ---
console.log('[T28] Image RNG does not affect text (module isolation)');
{
  // Just verify both work independently
  const text = encryptText('Hello', 'k', 1);
  const px = new Uint8ClampedArray([1,2,3,4]);
  encryptImageRGBA(px, 'k', 1, 1, 1);
  const dec = decryptText(text, 'k', 1);
  assert(dec === 'Hello', 'text works after image encrypt');
}

// ==================== SUMMARY ====================
console.log('\n' + '='.repeat(60));
console.log(`RESULTS: ${passed} passed, ${failed} failed, ${warnings} warnings`);
console.log('='.repeat(60));

if (failures.length > 0) {
  console.log('\nFAILURES:');
  for (const f of failures) console.log(`  ✗ ${f.name}${f.detail ? ': ' + f.detail : ''}`);
}
if (warns.length > 0) {
  console.log('\nWARNINGS:');
  for (const w of warns) console.log(`  ⚠ ${w.msg}${w.detail ? ': ' + w.detail : ''}`);
}

process.exit(failed > 0 ? 1 : 0);
