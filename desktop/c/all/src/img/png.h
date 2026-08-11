// png.h - 极简 PNG 解码/编码（自带 zlib inflate/deflate）
#ifndef PNG_H
#define PNG_H

#include <stddef.h>
#include <stdint.h>

// 解码：读 PNG 文件字节，输出 RGBA 像素（每像素 4 字节，行优先）。
// 返回 0 成功；非 0 失败。
// *width,*height 输出尺寸；*pixels 分配内存（调用者 free）。
// *palette: 若源为调色板/灰度，统一转 RGBA。
int png_decode(const uint8_t *data, size_t len,
               int *width, int *height, uint8_t **pixels);

// 编码：将 RGBA 像素写为 PNG 字节（使用 stored/zlib 兼容压缩）。
// 返回编码后字节长度，0 失败。输出缓冲区需足够大（建议 len*2+4096）。
size_t png_encode(const uint8_t *pixels, int width, int height,
                  uint8_t *out, size_t out_cap);

#endif
