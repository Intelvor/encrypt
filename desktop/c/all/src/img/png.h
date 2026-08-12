// png.h - 极简 PNG 编码（解码由 GDI+ 处理）
#ifndef PNG_H
#define PNG_H

#include <stddef.h>
#include <stdint.h>

// 编码：将 RGBA 像素写为 PNG 字节（使用 stored/zlib 兼容压缩）。
// 返回编码后字节长度，0 失败。输出缓冲区需足够大（建议 len*2+4096）。
size_t png_encode(const uint8_t *pixels, int width, int height,
                  uint8_t *out, size_t out_cap);

#endif
