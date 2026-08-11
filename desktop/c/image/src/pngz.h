// pngz.h - 极简 zlib inflate（解压）实现，用于 PNG 解码
// 支持 stored、fixed、dynamic Huffman 三种块类型，输出解压后字节
#ifndef PNGZ_H
#define PNGZ_H

#include <stddef.h>
#include <stdint.h>

// 输入压缩数据，输出解压数据。返回解压后长度，失败返回 0。
size_t zlib_inflate(const uint8_t *src, size_t src_len, uint8_t *out, size_t out_cap);

#endif
