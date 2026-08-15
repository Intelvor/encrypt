// png.c - PNG 编码（GDI+，带 deflate 压缩，兼容 Win XP）
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

// 从 IStream 读取全部字节到 malloc 的缓冲区，返回长度
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

size_t png_encode(const uint8_t *pixels, int width, int height, uint8_t *out, size_t out_cap)
{
    gdiplus_ensure();
    if (!g_gdiplus_ok) return 0;

    void *bitmap = NULL;
    if (GdipCreateBitmapFromScan0(width, height, 0, PixelFormat32bppARGB, NULL, &bitmap) != 0 || !bitmap)
        return 0;

    GpRectT rect = { 0, 0, width, height };
    GpBitmapDataT bmpdata;
    memset(&bmpdata, 0, sizeof(bmpdata));
    if (GdipBitmapLockBits(bitmap, &rect, ImageLockModeWrite, PixelFormat32bppARGB, &bmpdata) != 0)
    { GdipDisposeImage(bitmap); return 0; }

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

    // 用系统 COM 创建 IStream（XP 兼容）
    IStream *stm = NULL;
    if (CreateStreamOnHGlobal(NULL, TRUE, &stm) != S_OK || !stm)
    { GdipDisposeImage(bitmap); return 0; }

    size_t result = 0;
    if (GdipSaveImageToStream(bitmap, stm, &g_pngClsid, NULL) == 0)
    {
        uint8_t *buf = NULL;
        size_t len = stream_to_mem(stm, &buf);
        if (len > 0 && len <= out_cap)
        {
            memcpy(out, buf, len);
            result = len;
        }
        free(buf);
    }

    stm->lpVtbl->Release(stm);
    GdipDisposeImage(bitmap);
    return result;
}