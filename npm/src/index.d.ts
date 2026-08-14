export function hashKey(key: string): number;

export function encryptText(text: string, key: string, rounds?: number): string;
export function decryptText(cipher: string, key: string, rounds?: number): string;

export function encryptImageRGBA(
  px: Uint8ClampedArray | Uint8Array,
  key: string,
  rounds: number,
  w: number,
  h: number
): void;
export function decryptImageRGBA(
  px: Uint8ClampedArray | Uint8Array,
  key: string,
  rounds: number,
  w: number,
  h: number
): void;

export interface DecodedPNG {
  rgba: Uint8ClampedArray;
  w: number;
  h: number;
}

export function decodePNGFile(path: string): Promise<DecodedPNG>;
export function encryptPNGFile(
  inPath: string,
  outPath: string,
  key: string,
  rounds?: number
): Promise<void>;
export function decryptPNGFile(
  inPath: string,
  outPath: string,
  key: string,
  rounds?: number
): Promise<void>;

export const _internal: {
  MOD: number;
  isControl(code: number): boolean;
  makeShift(key: string, pos: number): number;
};
