// png.c - 极简 PNG 解码/编码
#include "png.h"
#include "pngz.h"
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

// ---- PNG 解码 ----
int png_decode(const uint8_t *data, size_t len, int *width, int *height, uint8_t **pixels)
{
    if (len < 8) return 1;
    // 签名
    static const uint8_t sig[8] = { 0x89, 'P','N','G', 0x0D, 0x0A, 0x1A, 0x0A };
    if (memcmp(data, sig, 8) != 0) return 1;

    size_t pos = 8;
    int w = 0, h = 0, bit_depth = 0, color_type = 0, interlace = 0;
    // IDAT 数据收集
    uint8_t *idat = NULL;
    size_t idat_len = 0, idat_cap = 0;
    int have_ihdr = 0;

    while (pos + 8 <= len)
    {
        uint32_t clen = rd_be32(data + pos);
        uint32_t ctype = rd_be32(data + pos + 4);
        size_t cstart = pos + 8;
        if (cstart + clen > len) break;

        if (ctype == 0x49484452) // IHDR
        {
            if (clen != 13) { free(idat); return 1; }
            w = (int)rd_be32(data + cstart);
            h = (int)rd_be32(data + cstart + 4);
            bit_depth = data[cstart + 8];
            color_type = data[cstart + 9];
            interlace = data[cstart + 12];
            have_ihdr = 1;
        }
        else if (ctype == 0x49444154) // IDAT
        {
            if (idat_len + clen > idat_cap)
            {
                size_t nc = idat_cap ? idat_cap * 2 : (clen + 4096);
                while (nc < idat_len + clen) nc *= 2;
                uint8_t *ni = (uint8_t *)realloc(idat, nc);
                if (!ni) { free(idat); return 1; }
                idat = ni;
                idat_cap = nc;
            }
            memcpy(idat + idat_len, data + cstart, clen);
            idat_len += clen;
        }
        pos = cstart + clen + 4; // 跳 CRC
    }

    if (!have_ihdr || !idat || w <= 0 || h <= 0) { free(idat); return 1; }
    // 仅支持 bit_depth 8（最常见的 8-bit）
    if (bit_depth != 8) { free(idat); return 1; }
    // 仅支持非隔行
    if (interlace != 0) { free(idat); return 1; }

    // 每行原始字节数（filter byte + 像素数据）
    int channels;
    switch (color_type)
    {
        case 0: channels = 1; break;  // 灰度
        case 2: channels = 3; break;  // RGB
        case 3: channels = 1; break;  // 调色板（每像素1字节索引）
        case 4: channels = 2; break;  // 灰度+alpha
        case 6: channels = 4; break;  // RGBA
        default: free(idat); return 1;
    }
    int bpp = channels; // bit_depth=8，每通道1字节
    int rowbytes = w * channels;
    size_t raw_size = (size_t)(rowbytes + 1) * h;

    uint8_t *raw = (uint8_t *)malloc(raw_size);
    if (!raw) { free(idat); return 1; }
    size_t got = zlib_inflate(idat, idat_len, raw, raw_size);
    free(idat);
    if (got != raw_size) { free(raw); return 1; }

    // 反滤波
    uint8_t *out = (uint8_t *)malloc((size_t)w * h * 4);
    if (!out) { free(raw); return 1; }

    // 调色板（PLTE）与 tRNS
    // 需要先扫描 PLTE/tRNS，但我们没保存。重新解析。
    uint8_t plte[256 * 3];
    int plte_n = 0;
    uint8_t trns[256];
    int trns_n = 0;
    {
        size_t p2 = 8;
        while (p2 + 8 <= len)
        {
            uint32_t clen = rd_be32(data + p2);
            uint32_t ctype = rd_be32(data + p2 + 4);
            size_t cs = p2 + 8;
            if (ctype == 0x504C5445) // PLTE
            {
                plte_n = (int)(clen / 3);
                if (plte_n > 256) plte_n = 256;
                memcpy(plte, data + cs, (size_t)plte_n * 3);
            }
            else if (ctype == 0x74524E53) // tRNS
            {
                trns_n = (int)clen;
                if (trns_n > 256) trns_n = 256;
                memcpy(trns, data + cs, (size_t)trns_n);
            }
            p2 = cs + clen + 4;
        }
    }

    // 逐行反滤波 + 转 RGBA
    // prev_line 保存上一行反滤波重建的结果（filter 2/3/4 需要正确的前一行）
    uint8_t *prev_line = NULL;
    for (int y = 0; y < h; y++)
    {
        const uint8_t *line = raw + (size_t)y * (rowbytes + 1) + 1;
        uint8_t filter = raw[(size_t)y * (rowbytes + 1)];
        // 复原扫描线到临时缓冲区
        uint8_t *cur = (uint8_t *)malloc((size_t)rowbytes);
        if (!cur) { free(prev_line); free(out); free(raw); return 1; }
        uint8_t *prev = prev_line;

        for (int i = 0; i < rowbytes; i++)
        {
            uint8_t x = line[i];
            int a = (i >= bpp) ? cur[i - bpp] : 0;
            int b = (prev) ? prev[i] : 0;
            int c = (prev && i >= bpp) ? prev[i - bpp] : 0;
            int val;
            switch (filter)
            {
                case 0: val = x; break;
                case 1: val = x + a; break;                      // Sub
                case 2: val = x + b; break;                      // Up
                case 3: val = x + (a + b) / 2; break;            // Average
                case 4: {                                        // Paeth
                    int p = a + b - c;
                    int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
                    val = x + (pa <= pb && pa <= pc ? a : (pb <= pc ? b : c));
                    break;
                }
                default: val = x; break;
            }
            cur[i] = (uint8_t)(val & 0xFF);
        }

        // 释放旧 prev_line，保存当前行重建结果作为下一行的 prev
        free(prev_line);
        prev_line = cur;

        // 转 RGBA
        uint8_t *dst = out + (size_t)y * w * 4;
        for (int x = 0; x < w; x++)
        {
            switch (color_type)
            {
                case 0: // 灰度
                    dst[x*4] = dst[x*4+1] = dst[x*4+2] = cur[x];
                    dst[x*4+3] = 255;
                    break;
                case 2: // RGB
                    dst[x*4] = cur[x*3]; dst[x*4+1] = cur[x*3+1]; dst[x*4+2] = cur[x*3+2];
                    dst[x*4+3] = 255;
                    break;
                case 3: // 调色板
                    {
                        int idx = cur[x];
                        if (idx < plte_n)
                        {
                            dst[x*4] = plte[idx*3]; dst[x*4+1] = plte[idx*3+1]; dst[x*4+2] = plte[idx*3+2];
                            dst[x*4+3] = (idx < trns_n) ? trns[idx] : 255;
                        }
                        else { dst[x*4]=dst[x*4+1]=dst[x*4+2]=0; dst[x*4+3]=255; }
                    }
                    break;
                case 4: // 灰度+alpha
                    dst[x*4] = dst[x*4+1] = dst[x*4+2] = cur[x*2];
                    dst[x*4+3] = cur[x*2+1];
                    break;
                case 6: // RGBA
                    dst[x*4] = cur[x*4]; dst[x*4+1] = cur[x*4+1]; dst[x*4+2] = cur[x*4+2]; dst[x*4+3] = cur[x*4+3];
                    break;
            }
        }
        // cur 已保存为 prev_line 供下一行使用，此处不释放
    }

    free(prev_line);
    free(raw);
    *width = w;
    *height = h;
    *pixels = out;
    return 0;
}

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
