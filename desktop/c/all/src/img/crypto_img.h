// crypto_img.h - 图片加解密算法（与 HTML 版互通）
#ifndef CRYPTO_IMG_H
#define CRYPTO_IMG_H

#include <stdint.h>

// 就地变换像素数据（RGBA，w*h*4 字节）。enc=1 加密，0 解密。
void img_encrypt(uint8_t *px, const wchar_t *key, int rounds, int w, int h);
void img_decrypt(uint8_t *px, const wchar_t *key, int rounds, int w, int h);

#endif
