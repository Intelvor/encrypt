// png.c - 极简 PNG 编码（解码由 GDI+ 处理）
#include "png.h"
#include <stdlib.h>
#include <string.h>

// ---- CRC32 ----
static uint32_t crc_table[256];
static int crc_init_done = 0;
static void crc_init(void)
{
    if (crc_init_done) return;
    for (uint32_t n = 0; n < 256; n++)
    {
        uint32_t c = n;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc_table[n] = c;
    }
    crc_init_done = 1;
}
static uint32_t crc_update(uint32_t crc, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
        crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

// ---- 大端读写 ----
static uint32_t rd_be32(const uint8_t *p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static void wr_be32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v; }

// ---- PNG 编码 ----
// 用最简单方案：每行 filter=0（None），用 zlib stored 块（无压缩）。
// 这样无需实现 deflate 压缩，且完全有效。
// 构造 zlib 流：header 0x78 0x01，然后 stored deflate 块。
static size_t write_chunk(uint8_t *out, uint32_t ctype, const uint8_t *cdata, size_t clen)
{
    size_t o = 0;
    wr_be32(out + o, (uint32_t)clen); o += 4;
    wr_be32(out + o, ctype); o += 4;
    memcpy(out + o, cdata, clen); o += clen;
    uint32_t crc = 0xFFFFFFFFu;
    crc = crc_update(crc, out + 4, 4 + clen);
    wr_be32(out + o, crc ^ 0xFFFFFFFFu); o += 4;
    return o;
}

size_t png_encode(const uint8_t *pixels, int width, int height, uint8_t *out, size_t out_cap)
{
    crc_init();
    size_t o = 0;
    // 签名
    static const uint8_t sig[8] = { 0x89, 'P','N','G', 0x0D, 0x0A, 0x1A, 0x0A };
    memcpy(out + o, sig, 8); o += 8;

    // IHDR
    uint8_t ihdr[13];
    wr_be32(ihdr, (uint32_t)width);
    wr_be32(ihdr + 4, (uint32_t)height);
    ihdr[8] = 8;   // bit depth
    ihdr[9] = 6;   // color type RGBA
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    o += write_chunk(out + o, 0x49484452, ihdr, 13);

    // IDAT: zlib stored 块
    int rowbytes = width * 4;
    size_t raw_size = (size_t)(rowbytes + 1) * height;
    uint8_t *raw = (uint8_t *)malloc(raw_size);
    if (!raw) return 0;
    for (int y = 0; y < height; y++)
    {
        raw[(size_t)y * (rowbytes + 1)] = 0; // filter None
        memcpy(raw + (size_t)y * (rowbytes + 1) + 1, pixels + (size_t)y * rowbytes, rowbytes);
    }

    // zlib 压缩：stored 块（无压缩，BTYPE=00）。
    // zlib header 0x78 0x01（deflate, no preset dictionary）。
    size_t cap = raw_size + raw_size / 16 + 4096;
    uint8_t *comp = (uint8_t *)malloc(cap);
    if (!comp) { free(raw); return 0; }

    size_t c = 0;
    comp[c++] = 0x78; comp[c++] = 0x01;
    size_t off = 0;
    while (off < raw_size)
    {
        size_t blk = raw_size - off;
        int last = 0;
        if (blk > 65535) blk = 65535;
        else last = 1;
        // 块头：1 字节 (BFINAL=last, BTYPE=00) + LEN(2) + NLEN(2)
        uint8_t hdr = (uint8_t)(last ? 1 : 0);
        comp[c++] = hdr;
        comp[c++] = (uint8_t)(blk & 0xFF);
        comp[c++] = (uint8_t)((blk >> 8) & 0xFF);
        comp[c++] = (uint8_t)(~(blk & 0xFF) & 0xFF);
        comp[c++] = (uint8_t)(~((blk >> 8) & 0xFF) & 0xFF);
        if (c + blk > cap) { free(raw); free(comp); return 0; }
        memcpy(comp + c, raw + off, blk);
        c += blk;
        off += blk;
    }
    // Adler32
    uint32_t a1 = 1, a2 = 0;
    for (size_t i = 0; i < raw_size; i++) { a1 = (a1 + raw[i]) % 65521; a2 = (a2 + a1) % 65521; }
    uint32_t adler = (a2 << 16) | a1;
    if (c + 4 > cap) { free(raw); free(comp); return 0; }
    comp[c++] = (uint8_t)(adler >> 24);
    comp[c++] = (uint8_t)((adler >> 16) & 0xFF);
    comp[c++] = (uint8_t)((adler >> 8) & 0xFF);
    comp[c++] = (uint8_t)(adler & 0xFF);

    free(raw);
    o += write_chunk(out + o, 0x49444154, comp, c);
    free(comp);

    // IEND
    o += write_chunk(out + o, 0x49454E44, NULL, 0);

    return o;
}
