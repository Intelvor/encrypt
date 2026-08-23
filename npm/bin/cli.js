#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('../src/index.js');

const USAGE = `
crypt-lite — lightweight text + image encrypt/decrypt

Usage:
  crypt-lite text  <encrypt|decrypt> -k KEY [-r ROUNDS] [-i IN] [-o OUT]
  crypt-lite image <encrypt|decrypt> -k KEY [-r ROUNDS] -i IN -o OUT

Options:
  -k, --key KEY       Encryption key (required)
  -r, --rounds N      Number of rounds (default: 1)
  -i, --in FILE       Input file (text or PNG). Omit for text to read stdin.
  -o, --out FILE      Output file. Omit to write stdout (text) / required for image.
  --raw               Text: emit/read raw UTF-8 ciphertext (default is Base64 for encrypt).
                      Ciphertext can contain lone surrogates / newlines that break
                      UTF-8 transport; Base64 avoids that.
  -h, --help          Show this help

Examples:
  echo "hello" | crypt-lite text encrypt -k secret        # Base64 out
  crypt-lite text decrypt -k secret -i enc.b64 -o dec.txt # Base64 in auto-detected
  crypt-lite image encrypt -k secret -i in.png -o out.png
  crypt-lite image decrypt -k secret -r 3 -i out.png -o dec.png
`;

function parseArgs(argv) {
  const args = { rounds: 1 };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '-k' || a === '--key') args.key = argv[++i];
    else if (a === '-r' || a === '--rounds') args.rounds = parseInt(argv[++i], 10) || 1;
    else if (a === '-i' || a === '--in') args.in = argv[++i];
    else if (a === '-o' || a === '--out') args.out = argv[++i];
    else if (a === '--raw') args.raw = true;
    else if (a === '-h' || a === '--help') args.help = true;
    else if (!args.mode && (a === 'text' || a === 'image')) args.mode = a;
    else if (!args.op && (a === 'encrypt' || a === 'decrypt')) args.op = a;
    else args._ = (args._ || []).concat(a);
  }
  return args;
}

function readStdin() {
  return new Promise((resolve) => {
    let data = '';
    process.stdin.setEncoding('utf8');
    process.stdin.on('data', (c) => (data += c));
    process.stdin.on('end', () => resolve(data));
    // If no piped input, resolve empty.
    if (process.stdin.isTTY) resolve('');
  });
}

async function readText(file) {
  if (file) return fs.readFileSync(file, 'utf8');
  return await readStdin();
}

function writeText(file, text, base64) {
  const out = base64 ? Buffer.from(text, 'utf8').toString('base64') : text;
  if (file) fs.writeFileSync(file, out, 'utf8');
  else process.stdout.write(out + '\n');
}

// 检测输入是否为 Base64 密文：长度合理、可解码为 UTF-8 且含换行/孤立代理项时更可能是。
// 简单起见：能被 base64 解码且解码结果不含 NUL 的视为 Base64。
function maybeBase64(s) {
  const t = s.trim();
  if (!t || t.length < 4) return null;
  // 去掉可能换行的 base64 后尝试解码
  const compact = t.replace(/\s+/g, '');
  if (!/^[A-Za-z0-9+/]*={0,2}$/.test(compact) || compact.length % 4 !== 0) return null;
  try {
    const buf = Buffer.from(compact, 'base64');
    if (buf.includes(0)) return null; // 含 NUL，不太可能是文本密文
    return buf.toString('utf8');
  } catch (e) {
    return null;
  }
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help || !args.mode || !args.op) {
    process.stdout.write(USAGE);
    process.exit(args.help ? 0 : 1);
  }
  if (!args.key) {
    process.stderr.write('Error: key is required (-k KEY)\n');
    process.exit(1);
  }
  if (args.mode === 'image' && (!args.in || !args.out)) {
    process.stderr.write('Error: image mode requires -i IN and -o OUT\n');
    process.exit(1);
  }

  try {
    if (args.mode === 'text') {
      let text = await readText(args.in);
      if (args.op === 'decrypt' && !args.raw) {
        const decoded = maybeBase64(text);
        if (decoded !== null) text = decoded;
      }
      const result =
        args.op === 'encrypt'
          ? crypto.encryptText(text, args.key, args.rounds)
          : crypto.decryptText(text, args.key, args.rounds);
      // 加密默认 Base64 输出（密文可能含孤立代理项/换行，破坏 UTF-8 传输）
      const base64Out = args.op === 'encrypt' && !args.raw;
      writeText(args.out, result, base64Out);
    } else {
      if (args.op === 'encrypt')
        await crypto.encryptPNGFile(args.in, args.out, args.key, args.rounds);
      else await crypto.decryptPNGFile(args.in, args.out, args.key, args.rounds);
      process.stderr.write(
        `${args.op === 'encrypt' ? 'Encrypted' : 'Decrypted'} ${args.in} -> ${args.out} (rounds=${args.rounds})\n`
      );
    }
  } catch (e) {
    process.stderr.write('Error: ' + e.message + '\n');
    process.exit(1);
  }
}

main();
