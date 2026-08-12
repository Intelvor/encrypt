// image_decode.c - 统一图片解码（GDI+ flat API 静态链接）
// PNG/JPG/BMP/GIF/TIFF 全部用 GDI+（Windows XP+ 系统自带 gdiplus.dll）
#include "image_decode.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ====== GDI+ flat API 声明（纯 C，链接 gdiplus.lib） ======
// GDI+ 类型
#if defined(_WIN64)
typedef unsigned long long ULONG_PTR_T;
#else
typedef unsigned long ULONG_PTR_T;
#endif
typedef struct { int X, Y, Width, Height; } GpRectT;
typedef struct {
    unsigned int Width;
    unsigned int Height;
    int Stride;
    unsigned int PixelFormat;
    void *Scan0;
    ULONG_PTR_T Reserved;
} GpBitmapDataT;

typedef int Status;

// flat API 原型（stdcall）
typedef Status (WINAPI *GdiplusStartup_t)(ULONG_PTR*, const void*, const void*);
typedef void (WINAPI *GdiplusShutdown_t)(ULONG_PTR);
typedef Status (WINAPI *GdipCreateBitmapFromFile_t)(const WCHAR*, void**);
typedef Status (WINAPI *GdipGetImageWidth_t)(void*, unsigned int*);
typedef Status (WINAPI *GdipGetImageHeight_t)(void*, unsigned int*);
typedef Status (WINAPI *GdipBitmapLockBits_t)(void*, const GpRectT*, unsigned int, unsigned int, GpBitmapDataT*);
typedef Status (WINAPI *GdipBitmapUnlockBits_t)(void*, GpBitmapDataT*);
typedef Status (WINAPI *GdipDisposeImage_t)(void*);

static ULONG_PTR gdiplus_token = 0;
static int gdiplus_ok = 0;

// 显式链接 gdiplus.lib 的 flat 函数（MinGW 提供）
__declspec(dllimport) Status WINAPI GdiplusStartup(ULONG_PTR*, const void*, const void*);
__declspec(dllimport) void WINAPI GdiplusShutdown(ULONG_PTR);
__declspec(dllimport) Status WINAPI GdipCreateBitmapFromFile(const WCHAR*, void**);
__declspec(dllimport) Status WINAPI GdipGetImageWidth(void*, unsigned int*);
__declspec(dllimport) Status WINAPI GdipGetImageHeight(void*, unsigned int*);
__declspec(dllimport) Status WINAPI GdipBitmapLockBits(void*, const GpRectT*, unsigned int, unsigned int, GpBitmapDataT*);
__declspec(dllimport) Status WINAPI GdipBitmapUnlockBits(void*, GpBitmapDataT*);
__declspec(dllimport) Status WINAPI GdipDisposeImage(void*);

#define PixelFormat32bppARGB 0x0026200A
#define ImageLockModeRead 0x00000001

// GdiplusStartupInput：GdiplusVersion=1, callback=NULL, suppress=FALSE, FALSE
// 必须用结构体（含指针）声明，以适配指针宽度：
//   x86 = 16 字节，x64 = 24 字节；若用固定 16 字节数组，x64 下越界导致初始化失败
typedef struct {
    unsigned int GdiplusVersion;
    void *DebugEventCallback;
    int SuppressBackgroundThread;
    int SuppressExternalCodecs;
} GdiplusStartupInputT;
static GdiplusStartupInputT gdi_input = { 1, NULL, 0, 0 };

static void gdiplus_init(void)
{
    if (gdiplus_ok) return;
    if (GdiplusStartup(&gdiplus_token, &gdi_input, NULL) == 0)
        gdiplus_ok = 1;
}

// 用 GDI+ 解码，输出 RGBA
static int decode_gdiplus(const wchar_t *path, int *width, int *height, uint8_t **pixels)
{
    gdiplus_init();
    if (!gdiplus_ok) return 1;

    void *bitmap = NULL;
    if (GdipCreateBitmapFromFile(path, &bitmap) != 0 || !bitmap) return 1;

    unsigned int w = 0, h = 0;
    GdipGetImageWidth(bitmap, &w);
    GdipGetImageHeight(bitmap, &h);
    if (w == 0 || h == 0 || w > 100000 || h > 100000) { GdipDisposeImage(bitmap); return 1; }

    GpRectT rect;
    rect.X = 0; rect.Y = 0; rect.Width = (int)w; rect.Height = (int)h;
    GpBitmapDataT bmpdata;
    memset(&bmpdata, 0, sizeof(bmpdata));

    if (GdipBitmapLockBits(bitmap, &rect, ImageLockModeRead, PixelFormat32bppARGB, &bmpdata) != 0)
    {
        GdipDisposeImage(bitmap);
        return 1;
    }
    if (bmpdata.Scan0 == NULL || bmpdata.Stride == 0)
    {
        GdipBitmapUnlockBits(bitmap, &bmpdata);
        GdipDisposeImage(bitmap);
        return 1;
    }

    uint8_t *out = (uint8_t *)malloc((size_t)w * h * 4);
    if (!out) { GdipBitmapUnlockBits(bitmap, &bmpdata); GdipDisposeImage(bitmap); return 1; }

    uint8_t *src = (uint8_t *)bmpdata.Scan0;
    for (unsigned int y = 0; y < h; y++)
    {
        uint8_t *row = src + (LONG)y * bmpdata.Stride;
        for (unsigned int x = 0; x < w; x++)
        {
            uint8_t b = row[x * 4];
            uint8_t g = row[x * 4 + 1];
            uint8_t r = row[x * 4 + 2];
            uint8_t a = row[x * 4 + 3];
            size_t o = ((size_t)y * w + x) * 4;
            out[o] = r; out[o + 1] = g; out[o + 2] = b; out[o + 3] = a;
        }
    }

    GdipBitmapUnlockBits(bitmap, &bmpdata);
    GdipDisposeImage(bitmap);

    *width = (int)w;
    *height = (int)h;
    *pixels = out;
    return 0;
}

// 统一解码入口（全部走 GDI+，含 PNG/JPG/BMP/GIF/TIFF）
int image_decode_file(const wchar_t *path, int *width, int *height, uint8_t **pixels)
{
    // PNG 也交给 GDI+（GDI+ 原生支持 PNG 解码），无需自带解码器
    return decode_gdiplus(path, width, height, pixels);
}
