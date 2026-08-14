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
  -h, --help          Show this help

Examples:
  echo "hello" | crypt-lite text encrypt -k secret
  crypt-lite text decrypt -k secret -i enc.txt -o dec.txt
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

function writeText(file, text) {
  if (file) fs.writeFileSync(file, text, 'utf8');
  else process.stdout.write(text + '\n');
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
      const text = await readText(args.in);
      const result =
        args.op === 'encrypt'
          ? crypto.encryptText(text, args.key, args.rounds)
          : crypto.decryptText(text, args.key, args.rounds);
      writeText(args.out, result);
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
