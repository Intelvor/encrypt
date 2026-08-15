// png.c - PNG 编码（GDI+ deflate 优先，fallback 无压缩 zlib，兼容 Win XP）
#include "png.h"
#include <windows.h>
#include <objidl.h>
#include <stdlib.h>
#include <string.h>

// GDI+ flat API
#if defined(_WIN64)
typedef unsigned long long ULONG_PTR_T;
#else
typedef unsigned long ULONG_PTR_T;
#endif
typedef int Status;
typedef struct { int X, Y, Width, Height; } GpRectT;

__declspec(dllimport) Status WINAPI GdiplusStartup(ULONG_PTR*, const void*, const void*);
__declspec(dllimport) void WINAPI GdiplusShutdown(ULONG_PTR);
__declspec(dllimport) Status WINAPI GdipCreateBitmapFromScan0(int, int, int, unsigned int, void*, void**);
__declspec(dllimport) Status WINAPI GdipBitmapLockBits(void*, const GpRectT*, unsigned int, unsigned int, void*);
__declspec(dllimport) Status WINAPI GdipBitmapUnlockBits(void*, void*);
__declspec(dllimport) Status WINAPI GdipDisposeImage(void*);
__declspec(dllimport) Status WINAPI GdipSaveImageToStream(void*, IStream*, const CLSID*, const void*);

#define PixelFormat32bppARGB 0x0026200A
#define ImageLockModeWrite 0x00000002

typedef struct {
    unsigned int Width;
    unsigned int Height;
    int Stride;
    unsigned int PixelFormat;
    void *Scan0;
    ULONG_PTR_T Reserved;
} GpBitmapDataT;

typedef struct {
    unsigned int GdiplusVersion;
    void *DebugEventCallback;
    int SuppressBackgroundThread;
    int SuppressExternalCodecs;
} GdiplusStartupInputT;

static const CLSID g_pngClsid = {0x557CF406,0x1A04,0x11D3,{0x9A,0x73,0x00,0x00,0xF8,0x1E,0xF3,0x2E}};

static ULONG_PTR g_gdiplus_token = 0;
static int g_gdiplus_ok = 0;

static void gdiplus_ensure(void)
{
    if (g_gdiplus_ok) return;
    GdiplusStartupInputT gdi_input = { 1, NULL, 0, 0 };
    if (GdiplusStartup(&g_gdiplus_token, &gdi_input, NULL) == 0)
        g_gdiplus_ok = 1;
}

static size_t stream_to_mem(IStream *stm, uint8_t **out_buf)
{
    STATSTG stat;
    if (stm->lpVtbl->Stat(stm, &stat, STATFLAG_NONAME) != S_OK) return 0;
    size_t len = (size_t)stat.cbSize.QuadPart;
    if (len == 0) return 0;
    uint8_t *buf = (uint8_t *)malloc(len);
    if (!buf) return 0;
    LARGE_INTEGER zero;
    zero.QuadPart = 0;
    stm->lpVtbl->Seek(stm, zero, STREAM_SEEK_SET, NULL);
    ULONG read = 0;
    HRESULT hr = stm->lpVtbl->Read(stm, buf, (ULONG)len, &read);
    if (hr != S_OK || read != len) { free(buf); return 0; }
    *out_buf = buf;
    return len;
}

// ====== Fallback: 无压缩 zlib PNG（XP 兼容） ======
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
static void wr_be32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v; }

static size_t write_chunk(uint8_t *out, uint32_t ctype, const uint8_t *cdata, size_t clen)
{
    size_t o = 0;
    wr_be32(out + o, (uint32_t)clen); o += 4;
    wr_be32(out + o, ctype); o += 4;
    if (clen > 0) { memcpy(out + o, cdata, clen); o += clen; }
    uint32_t crc = 0xFFFFFFFFu;
    crc = crc_update(crc, out + 4, 4 + clen);
    wr_be32(out + o, crc ^ 0xFFFFFFFFu); o += 4;
    return o;
}

static size_t png_encode_raw(const uint8_t *pixels, int width, int height, uint8_t *out, size_t out_cap)
{
    crc_init();
    int rowbytes = width * 4;
    size_t raw_size = (size_t)(rowbytes + 1) * height;
    size_t est = 8 + 25 + 12 + raw_size + raw_size / 65535 * 5 + 6 + 12;
    if (est > out_cap) return 0;

    size_t o = 0;
    static const uint8_t sig[8] = { 0x89, 'P','N','G', 0x0D, 0x0A, 0x1A, 0x0A };
    memcpy(out + o, sig, 8); o += 8;

    uint8_t ihdr[13];
    wr_be32(ihdr, (uint32_t)width);
    wr_be32(ihdr + 4, (uint32_t)height);
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    o += write_chunk(out + o, 0x49484452, ihdr, 13);

    uint8_t *raw = (uint8_t *)malloc(raw_size);
    if (!raw) return 0;
    for (int y = 0; y < height; y++)
    {
        raw[(size_t)y * (rowbytes + 1)] = 0;
        memcpy(raw + (size_t)y * (rowbytes + 1) + 1, pixels + (size_t)y * rowbytes, rowbytes);
    }

    size_t comp_cap = raw_size + raw_size / 16 + 4096;
    uint8_t *comp = (uint8_t *)malloc(comp_cap);
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
        comp[c++] = (uint8_t)(last ? 1 : 0);
        comp[c++] = (uint8_t)(blk & 0xFF);
        comp[c++] = (uint8_t)((blk >> 8) & 0xFF);
        comp[c++] = (uint8_t)(~(blk & 0xFF) & 0xFF);
        comp[c++] = (uint8_t)(~((blk >> 8) & 0xFF) & 0xFF);
        if (c + blk > comp_cap) { free(raw); free(comp); return 0; }
        memcpy(comp + c, raw + off, blk);
        c += blk; off += blk;
    }
    uint32_t a1 = 1, a2 = 0;
    for (size_t i = 0; i < raw_size; i++) { a1 = (a1 + raw[i]) % 65521; a2 = (a2 + a1) % 65521; }
    uint32_t adler = (a2 << 16) | a1;
    comp[c++] = (uint8_t)(adler >> 24);
    comp[c++] = (uint8_t)((adler >> 16) & 0xFF);
    comp[c++] = (uint8_t)((adler >> 8) & 0xFF);
    comp[c++] = (uint8_t)(adler & 0xFF);
    free(raw);

    o += write_chunk(out + o, 0x49444154, comp, c);
    free(comp);
    o += write_chunk(out + o, 0x49454E44, NULL, 0);
    return o;
}

// ====== 主编码：GDI+ 优先，失败 fallback ======
size_t png_encode(const uint8_t *pixels, int width, int height, uint8_t *out, size_t out_cap)
{
    // 尝试 GDI+ 压缩编码
    gdiplus_ensure();
    if (g_gdiplus_ok)
    {
        void *bitmap = NULL;
        if (GdipCreateBitmapFromScan0(width, height, 0, PixelFormat32bppARGB, NULL, &bitmap) == 0 && bitmap)
        {
            GpRectT rect = { 0, 0, width, height };
            GpBitmapDataT bmpdata;
            memset(&bmpdata, 0, sizeof(bmpdata));
            if (GdipBitmapLockBits(bitmap, &rect, ImageLockModeWrite, PixelFormat32bppARGB, &bmpdata) == 0)
            {
                int src_row = width * 4;
                for (int y = 0; y < height; y++)
                {
                    uint8_t *dst_row = (uint8_t*)bmpdata.Scan0 + (LONG)y * bmpdata.Stride;
                    const uint8_t *src_row_ptr = pixels + (size_t)y * src_row;
                    for (int x = 0; x < width; x++)
                    {
                        dst_row[x*4]   = src_row_ptr[x*4+2];
                        dst_row[x*4+1] = src_row_ptr[x*4+1];
                        dst_row[x*4+2] = src_row_ptr[x*4];
                        dst_row[x*4+3] = src_row_ptr[x*4+3];
                    }
                }
                GdipBitmapUnlockBits(bitmap, &bmpdata);

                IStream *stm = NULL;
                if (CreateStreamOnHGlobal(NULL, TRUE, &stm) == S_OK && stm)
                {
                    if (GdipSaveImageToStream(bitmap, stm, &g_pngClsid, NULL) == 0)
                    {
                        uint8_t *buf = NULL;
                        size_t len = stream_to_mem(stm, &buf);
                        if (len > 0 && len <= out_cap)
                        {
                            memcpy(out, buf, len);
                            free(buf);
                            stm->lpVtbl->Release(stm);
                            GdipDisposeImage(bitmap);
                            return len;
                        }
                        free(buf);
                    }
                    stm->lpVtbl->Release(stm);
                }
            }
            GdipDisposeImage(bitmap);
        }
    }

    // Fallback: 无压缩 zlib
    return png_encode_raw(pixels, width, height, out, out_cap);
}