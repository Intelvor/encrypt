// png.c - PNG 编码（GDI+，带 deflate 压缩）
#include "png.h"
#include <windows.h>
#include <objidl.h>
#include <stdlib.h>
#include <string.h>

static const GUID my_IID_IUnknown = {0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
static const GUID my_IID_IStream = {0x0000000C,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

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
#define ImageLockModeRead 0x00000001

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

// IStream vtable (MinGW objidl.h 可能不完整，手写保证兼容)
typedef struct {
    IStreamVtbl *lpVtbl;
    LONG ref;
    uint8_t *buf;
    size_t cap;
    size_t pos;
    size_t used;
} MemStream;

static HRESULT STDMETHODCALLTYPE ms_QueryInterface(IStream *This, REFIID riid, void **ppv)
{
    if (!memcmp(riid, &my_IID_IUnknown, sizeof(*riid)) || !memcmp(riid, &my_IID_IStream, sizeof(*riid)))
    { *ppv = This; This->lpVtbl->AddRef(This); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE ms_AddRef(IStream *This) { return ++((MemStream*)This)->ref; }
static ULONG STDMETHODCALLTYPE ms_Release(IStream *This) { return --((MemStream*)This)->ref; }

static HRESULT STDMETHODCALLTYPE ms_Read(IStream *This, void *pv, ULONG cb, ULONG *pcbRead)
{
    MemStream *s = (MemStream*)This;
    ULONG avail = (ULONG)(s->used > s->pos ? s->used - s->pos : 0);
    ULONG n = cb < avail ? cb : avail;
    memcpy(pv, s->buf + s->pos, n);
    s->pos += n;
    if (pcbRead) *pcbRead = n;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ms_Write(IStream *This, const void *pv, ULONG cb, ULONG *pcbWritten)
{
    MemStream *s = (MemStream*)This;
    if (s->pos + cb > s->cap)
    {
        size_t nc = (s->pos + cb) * 2;
        uint8_t *nb = (uint8_t *)realloc(s->buf, nc);
        if (!nb) { if (pcbWritten) *pcbWritten = 0; return E_OUTOFMEMORY; }
        s->buf = nb; s->cap = nc;
    }
    memcpy(s->buf + s->pos, pv, cb);
    s->pos += cb;
    if (s->pos > s->used) s->used = s->pos;
    if (pcbWritten) *pcbWritten = cb;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ms_Seek(IStream *This, LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
{
    MemStream *s = (MemStream*)This;
    LONGLONG np;
    if (dwOrigin == STREAM_SEEK_SET) np = dlibMove.QuadPart;
    else if (dwOrigin == STREAM_SEEK_CUR) np = (LONGLONG)s->pos + dlibMove.QuadPart;
    else np = (LONGLONG)s->used + dlibMove.QuadPart;
    if (np < 0) np = 0;
    s->pos = (size_t)np;
    if (plibNewPosition) { plibNewPosition->QuadPart = (ULONGLONG)np; }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ms_SetSize(IStream *This, ULARGE_INTEGER libNewSize)
{
    MemStream *s = (MemStream*)This;
    size_t ns = (size_t)libNewSize.QuadPart;
    if (ns > s->cap)
    {
        uint8_t *nb = (uint8_t *)realloc(s->buf, ns);
        if (!nb) return E_OUTOFMEMORY;
        s->buf = nb; s->cap = ns;
    }
    if (ns < s->used) s->used = ns;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ms_CopyTo(IStream *This, IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE ms_Commit(IStream *This, DWORD grfCommitFlags) { return S_OK; }
static HRESULT STDMETHODCALLTYPE ms_Revert(IStream *This) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE ms_LockRegion(IStream *This, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE ms_UnlockRegion(IStream *This, ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE ms_Stat(IStream *This, STATSTG *pstatstg, DWORD grfStatFlag)
{
    memset(pstatstg, 0, sizeof(*pstatstg));
    pstatstg->cbSize.QuadPart = (ULONGLONG)((MemStream*)This)->used;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE ms_Clone(IStream *This, IStream **ppstm) { *ppstm = NULL; return E_NOTIMPL; }

static IStreamVtbl g_memStreamVtbl = {
    ms_QueryInterface, ms_AddRef, ms_Release,
    ms_Read, ms_Write, ms_Seek, ms_SetSize,
    ms_CopyTo, ms_Commit, ms_Revert,
    ms_LockRegion, ms_UnlockRegion, ms_Stat, ms_Clone
};

static const CLSID g_pngClsid = {0x557CF406,0x1A04,0x11D3,{0x9A,0x73,0x00,0x00,0xF8,0x1E,0xF3,0x2E}};

size_t png_encode(const uint8_t *pixels, int width, int height, uint8_t *out, size_t out_cap)
{
    GdiplusStartupInputT gdi_input = { 1, NULL, 0, 0 };
    ULONG_PTR token = 0;
    if (GdiplusStartup(&token, &gdi_input, NULL) != 0) return 0;

    void *bitmap = NULL;
    if (GdipCreateBitmapFromScan0(width, height, 0, PixelFormat32bppARGB, NULL, &bitmap) != 0 || !bitmap)
    { GdiplusShutdown(token); return 0; }

    GpRectT rect = { 0, 0, width, height };
    GpBitmapDataT bmpdata;
    memset(&bmpdata, 0, sizeof(bmpdata));
    if (GdipBitmapLockBits(bitmap, &rect, ImageLockModeWrite, PixelFormat32bppARGB, &bmpdata) != 0)
    { GdipDisposeImage(bitmap); GdiplusShutdown(token); return 0; }

    int src_row = width * 4;
    for (int y = 0; y < height; y++)
    {
        uint8_t *dst_row = (uint8_t*)bmpdata.Scan0 + (LONG)y * bmpdata.Stride;
        const uint8_t *src_row_ptr = pixels + (size_t)y * src_row;
        for (int x = 0; x < width; x++)
        {
            dst_row[x*4]   = src_row_ptr[x*4+2]; // B <- R
            dst_row[x*4+1] = src_row_ptr[x*4+1]; // G
            dst_row[x*4+2] = src_row_ptr[x*4];   // R <- B
            dst_row[x*4+3] = src_row_ptr[x*4+3]; // A
        }
    }
    GdipBitmapUnlockBits(bitmap, &bmpdata);

    MemStream ms;
    ms.lpVtbl = &g_memStreamVtbl;
    ms.ref = 1;
    ms.cap = (size_t)width * height + 4096;
    ms.buf = (uint8_t *)malloc(ms.cap);
    ms.pos = 0;
    ms.used = 0;
    if (!ms.buf) { GdipDisposeImage(bitmap); GdiplusShutdown(token); return 0; }

    size_t result = 0;
    if (GdipSaveImageToStream(bitmap, (IStream*)&ms, &g_pngClsid, NULL) == 0 && ms.used <= out_cap)
    {
        memcpy(out, ms.buf, ms.used);
        result = ms.used;
    }

    free(ms.buf);
    GdipDisposeImage(bitmap);
    GdiplusShutdown(token);
    return result;
}