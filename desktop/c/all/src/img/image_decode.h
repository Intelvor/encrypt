// image_decode.h - 统一图片解码（PNG 原生 + JPG/BMP/GIF 等用 GDI+），输出 RGBA
#ifndef IMAGE_DECODE_H
#define IMAGE_DECODE_H

#include <stdint.h>

// 解码图片文件为 RGBA 像素（w*h*4）。返回 0 成功；非 0 失败。
// 支持：PNG（原生解码）、JPEG/BMP/GIF/TIFF（GDI+，Windows XP+）
int image_decode_file(const wchar_t *path, int *width, int *height, uint8_t **pixels);

#endif
