# crypt-lite

Lightweight **text + image** encrypt/decrypt for Node.js, the browser and the CLI.

Key + rounds obfuscation. The algorithm here is **byte-for-byte identical** to the
[encrypt](https://github.com/Intelvor/encrypt) desktop (C), web and Android
builds, so outputs are fully interoperable across platforms.

> ⚠️ This is **lightweight obfuscation**, not modern cryptography (no KDF / salt /
> authentication). Use it for casual privacy, content masking, etc. **Do not use it
> to protect sensitive data.**

## Install

```bash
npm i crypt-lite          # library
npm i -g crypt-lite       # also installs the CLI (crypt-lite)
```

## Library usage

```js
const { encryptText, decryptText, encryptImageRGBA, decryptImageRGBA } = require('crypt-lite');

// --- Text ---
const enc = encryptText('Hello, 世界! 🌍', 'secret', 3);
const dec = decryptText(enc, 'secret', 3); // === 'Hello, 世界! 🌍'

// --- Image (RGBA buffer, platform-agnostic) ---
// px is a Uint8ClampedArray / Uint8Array of length w*h*4 (RGBA).
encryptImageRGBA(px, 'secret', 4, w, h);   // mutates in place
decryptImageRGBA(px, 'secret', 4, w, h);   // restores

// --- Image files (Node only, PNG) ---
const { encryptPNGFile, decryptPNGFile, decodePNGFile } = require('crypt-lite');
await encryptPNGFile('in.png', 'out.png', 'secret', 4);
await decryptPNGFile('out.png', 'dec.png', 'secret', 4);
```

### API

| Function | Description |
|----------|-------------|
| `encryptText(text, key, rounds=1)` | Encrypt a string. |
| `decryptText(cipher, key, rounds=1)` | Decrypt a string. |
| `hashKey(key)` | FNV-1a hash of the key (returns `uint32`). |
| `encryptImageRGBA(px, key, rounds, w, h)` | Encrypt an RGBA buffer in place. |
| `decryptImageRGBA(px, key, rounds, w, h)` | Decrypt an RGBA buffer in place. |
| `decodePNGFile(path)` | Node: decode a PNG → `{ rgba, w, h }`. |
| `encryptPNGFile(in, out, key, rounds=1)` | Node: encrypt a PNG file. |
| `decryptPNGFile(in, out, key, rounds=1)` | Node: decrypt a PNG file. |

In the browser, supply an RGBA `Uint8ClampedArray` straight from
`ctx.getImageData(...).data` and write it back with `ctx.putImageData(...)`.

## CLI

```
crypt-lite text  <encrypt|decrypt> -k KEY [-r ROUNDS] [-i IN] [-o OUT]
crypt-lite image <encrypt|decrypt> -k KEY [-r ROUNDS] -i IN -o OUT
```

```bash
echo "hello" | crypt-lite text encrypt -k secret
crypt-lite text decrypt -k secret -i enc.txt -o dec.txt
crypt-lite image encrypt -k secret -r 4 -i in.png -o out.png
crypt-lite image decrypt -k secret -r 4 -i out.png -o dec.png
```

`text` reads from stdin / writes to stdout when `-i`/`-o` are omitted.
`image` requires both `-i` and `-o` (PNG only).

## How it works

- **Text**: key → pseudo-random shift sequence → per-codepoint shift, multi-round.
  Codepoint indices are used (surrogate pairs count as 1), so multi-round is
  reversible and works for any Unicode.
- **Image**: each round = pixel permutation (Fisher–Yates) + byte XOR (RGB only,
  alpha preserved), seeded by `keyHash ⊕ round·0x9E3779B9`.

## Distribution: npm vs GitHub Packages

The same code is published to **two** registries under two names:

| Registry | Package name | Install |
|----------|--------------|---------|
| npm (public) | `crypt-lite` | `npm i crypt-lite` |
| GitHub Packages | `@Intelvor/crypt-lite` | `npm i @Intelvor/crypt-lite` |

Both resolve to the identical algorithm — they are just two release channels for
the same code. GitHub Packages requires an auth token for install (your
`GITHUB_TOKEN` / a PAT with `read:packages`), so for everyday use the public npm
package `crypt-lite` is simpler.

## License

MIT
