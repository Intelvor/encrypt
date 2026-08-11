// img-main.c - 图片加解密工具 Win32 GUI（支持 DPI 缩放 + 响应式布局 + 图片预览）
#include <windows.h>
#include <commdlg.h>
#include <wchar.h>
#include <stdio.h>
#include <string.h>
#include "png.h"
#include "crypto_img.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((void*)-4)
#endif

#define ID_BTN_OPEN     1001
#define ID_BTN_ENCRYPT  1002
#define ID_BTN_DECRYPT  1003
#define ID_BTN_SAVE     1004
#define ID_KEY_EDIT     1005
#define ID_ROUND_EDIT   1006
#define ID_STATUS       1007
#define ID_CHK_ORIG     1008
#define ID_ZOOM_INFO    1009

// ====== DPI ======
static int g_dpi = 96;
static int px(int logical) { return MulDiv(logical, g_dpi, 96); }

static HWND g_hwndMain;
static HWND g_btnOpen, g_btnEncrypt, g_btnDecrypt, g_btnSave;
static HWND g_lblKey, g_lblRound, g_keyEdit, g_roundEdit;
static HWND g_picBox, g_status;
static HWND g_zoomInfo;   // 状态栏右端：尺寸 + 缩放比例
static HFONT g_hFont;

// 布局逻辑尺寸（与 DPI 无关，px() 换算）
static const int L_PAD = 8;
static const int L_BTN_H = 30;
static const int L_CTRL_H = 26;
static const int L_BTN_W_OPEN = 90;
static const int L_BTN_W_MID = 70;
static const int L_BTN_W_SAVE = 90;
static const int L_KEY_W = 150;
static const int L_ROUND_W = 50;
static const int L_LABEL_W = 40;
static const int L_STATUS_H = 22;
static const int L_GAP = 8;
static const int L_FONT = 14;

static uint8_t *g_pixels = NULL;
static uint8_t *g_orig = NULL;   // 原图副本（加载时保存，用于对比）
static int g_w = 0, g_h = 0;
static int g_dirty = 0;   // 当前显示的是处理后的图，需保存
static int g_show_orig = 0;  // 预览显示原图（对比用）
static wchar_t g_filename[1024] = L"";
static HWND g_chkOrig;    // 对比原图复选框

static void set_status(const wchar_t *s) { SetWindowTextW(g_status, s ? s : L""); }

// 更新状态栏右端：尺寸 + 缩放比例
static void update_zoom_info(void)
{
    if (!g_zoomInfo) return;
    wchar_t buf[96];
    if (g_pixels && g_w > 0 && g_h > 0 && g_picBox)
    {
        RECT rc;
        GetClientRect(g_picBox, &rc);
        int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
        double sx = (double)cw / g_w, sy = (double)ch / g_h;
        double s = sx < sy ? sx : sy;
        int pct = (int)(s * 100 + 0.5);
        swprintf(buf, 96, L"%d x %d · %d%%", g_w, g_h, pct);
    }
    else
    {
        swprintf(buf, 96, L"");
    }
    SetWindowTextW(g_zoomInfo, buf);
}

static wchar_t *get_text(HWND h)
{
    int len = GetWindowTextLengthW(h);
    wchar_t *buf = (wchar_t *)malloc(((size_t)len + 1) * sizeof(wchar_t));
    if (!buf) return NULL;
    GetWindowTextW(h, buf, len + 1);
    return buf;
}

static int get_rounds(void)
{
    wchar_t *s = get_text(g_roundEdit);
    int r = 1;
    if (s && s[0]) r = (int)wcstol(s, NULL, 10);
    free(s);
    if (r < 1) r = 1;
    if (r > 99) r = 99;
    return r;
}

static HFONT make_font(void)
{
    return CreateFontW(-px(L_FONT), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
}

// 统一所有控件字体（含标签、状态栏、预览框）
static void apply_font(void)
{
    SendMessageW(g_btnOpen, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_btnEncrypt, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_btnDecrypt, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_btnSave, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_lblKey, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_lblRound, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_keyEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_roundEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_chkOrig, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_status, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_zoomInfo, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_picBox, WM_SETFONT, (WPARAM)g_hFont, TRUE);
}

// ====== 布局（随窗口大小 + DPI 自适应） ======
static void layout_full(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;

    int pad = px(L_PAD);
    int gap = px(L_GAP);
    int btn_h = px(L_BTN_H);
    int ctrl_h = px(L_CTRL_H);
    int status_h = px(L_STATUS_H);
    int top_y = pad;
    int label_h = px(L_LABEL_W);   // 标签高度取大些，垂直居中

    // ---- 顶部工具条 ----
    int x = pad;
    MoveWindow(g_btnOpen, x, top_y, px(L_BTN_W_OPEN), btn_h, TRUE);
    x += px(L_BTN_W_OPEN) + gap;

    MoveWindow(g_btnEncrypt, x, top_y, px(L_BTN_W_MID), btn_h, TRUE);
    x += px(L_BTN_W_MID) + gap;

    MoveWindow(g_btnDecrypt, x, top_y, px(L_BTN_W_MID), btn_h, TRUE);
    x += px(L_BTN_W_MID) + gap;

    MoveWindow(g_btnSave, x, top_y, px(L_BTN_W_SAVE), btn_h, TRUE);
    x += px(L_BTN_W_SAVE) + gap * 2;

    // 密钥标签 + 输入
    int label_top = top_y + (btn_h - ctrl_h) / 2;
    MoveWindow(g_lblKey, x, label_top, px(L_LABEL_W), ctrl_h, TRUE);
    x += px(L_LABEL_W);
    MoveWindow(g_keyEdit, x, label_top, px(L_KEY_W), ctrl_h, TRUE);
    x += px(L_KEY_W) + gap;

    // 轮次标签 + 输入
    MoveWindow(g_lblRound, x, label_top, px(L_LABEL_W), ctrl_h, TRUE);
    x += px(L_LABEL_W);
    MoveWindow(g_roundEdit, x, label_top, px(L_ROUND_W), ctrl_h, TRUE);
    x += px(L_ROUND_W) + gap;

    // 对比原图复选框
    MoveWindow(g_chkOrig, x, label_top, px(90), ctrl_h, TRUE);
    x += px(90) + gap;

    // ---- 图片显示区（占剩余空间） ----
    int pic_top = top_y + btn_h + pad;
    int pic_bottom = H - status_h - pad;
    if (pic_bottom < pic_top + px(40)) pic_bottom = pic_top + px(40);
    MoveWindow(g_picBox, pad, pic_top, W - pad * 2, pic_bottom - pic_top, TRUE);

    // ---- 状态栏（左：操作信息，右：尺寸+缩放） ----
    int status_y = H - status_h - pad;
    int zoom_w = px(200);
    MoveWindow(g_status, pad, status_y, W - pad * 2 - zoom_w - gap, status_h, TRUE);
    MoveWindow(g_zoomInfo, W - pad - zoom_w, status_y, zoom_w, status_h, TRUE);
    update_zoom_info();
}

// ====== 图片读取/保存 ======
static uint8_t *read_file(const wchar_t *path, size_t *len_out)
{
    FILE *f = _wfopen(path, L"rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *buf = (uint8_t *)malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)n, f);
    fclose(f);
    *len_out = (size_t)n;
    return buf;
}

static int load_image(const wchar_t *path)
{
    size_t len;
    uint8_t *file = read_file(path, &len);
    if (!file) { set_status(L"无法读取文件"); return 0; }
    int w, h;
    uint8_t *px = NULL;
    int rc = png_decode(file, len, &w, &h, &px);
    free(file);
    if (rc != 0) { set_status(L"不是有效的 PNG 图片"); return 0; }

    free(g_pixels);
    free(g_orig);
    g_pixels = px;
    // 保存原图副本（供对比）
    g_orig = (uint8_t *)malloc((size_t)w * h * 4);
    if (g_orig) memcpy(g_orig, px, (size_t)w * h * 4);
    g_w = w; g_h = h;
    g_dirty = 0;
    g_show_orig = 0;
    if (g_chkOrig) SendMessageW(g_chkOrig, BM_SETCHECK, BST_UNCHECKED, 0);
    wcscpy(g_filename, path);
    wchar_t info[256];
    swprintf(info, 256, L"已加载: %d x %d (%d KB)", w, h, (int)((size_t)w*h*4/1024));
    set_status(info);
    InvalidateRect(g_picBox, NULL, TRUE);
    EnableWindow(g_btnEncrypt, TRUE);
    EnableWindow(g_btnDecrypt, TRUE);
    EnableWindow(g_btnSave, FALSE);
    update_zoom_info();
    return 1;
}

static void do_open(void)
{
    OPENFILENAMEW ofn = {0};
    wchar_t file[1024] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFilter = L"PNG 图片 (*.png)\0*.png\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn))
        load_image(file);
}

static void do_transform(int enc)
{
    if (!g_pixels) { set_status(L"请先打开图片"); return; }
    wchar_t *key = get_text(g_keyEdit);
    if (!key || !key[0]) { set_status(L"请输入密钥"); free(key); return; }
    int rounds = get_rounds();

    if (enc) img_encrypt(g_pixels, key, rounds, g_w, g_h);
    else img_decrypt(g_pixels, key, rounds, g_w, g_h);

    wchar_t info[256];
    swprintf(info, 256, L"%s完成（%d 轮） | %d x %d",
             enc ? L"加密" : L"解密", rounds, g_w, g_h);
    set_status(info);
    g_dirty = 1;
    g_show_orig = 0;
    if (g_chkOrig) SendMessageW(g_chkOrig, BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(g_btnSave, TRUE);
    InvalidateRect(g_picBox, NULL, TRUE);
    update_zoom_info();
    free(key);
}

static void do_save(void)
{
    if (!g_pixels || !g_dirty) return;
    OPENFILENAMEW ofn = {0};
    wchar_t file[1024] = L"";
    wcscpy(file, L"output.png");
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFilter = L"PNG 图片 (*.png)\0*.png\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"png";
    if (!GetSaveFileNameW(&ofn)) return;

    size_t cap = (size_t)g_w * g_h * 4 * 2 + 4096;
    uint8_t *png = (uint8_t *)malloc(cap);
    size_t plen = png_encode(g_pixels, g_w, g_h, png, cap);
    if (plen == 0) { set_status(L"PNG 编码失败"); free(png); return; }
    FILE *f = _wfopen(file, L"wb");
    if (f) { fwrite(png, 1, plen, f); fclose(f); set_status(L"已保存"); }
    else set_status(L"保存失败");
    free(png);
}

// ====== 图片预览 ======
static void draw_image(HWND hwnd, HDC hdc)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
    if (!g_pixels || g_w <= 0 || g_h <= 0)
    {
        // 占位提示（用雅黑字体）
        SetTextColor(hdc, RGB(120, 120, 120));
        SetBkMode(hdc, TRANSPARENT);
        HFONT oldFont = (HFONT)SelectObject(hdc, g_hFont);
        const wchar_t *msg = L"请点击「打开图片」加载 PNG";
        RECT tr = rc;
        DrawTextW(hdc, msg, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        return;
    }

    int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
    double sx = (double)cw / g_w, sy = (double)ch / g_h;
    double s = sx < sy ? sx : sy;
    int dw = (int)(g_w * s), dh = (int)(g_h * s);
    if (dw < 1) dw = 1; if (dh < 1) dh = 1;
    int dx = (cw - dw) / 2, dy = (ch - dh) / 2;

    // 选择显示的像素源（对比模式用原图）
    const uint8_t *src = (g_show_orig && g_orig) ? g_orig : g_pixels;

    // PNG 像素是 RGBA 序，StretchDIBits 按 BGRA 解释（小端）。
    // 转为 32-bit BGRA 临时缓冲，并强制 alpha=255 避免 GDI alpha 混合偏色。
    size_t npx = (size_t)g_w * g_h;
    uint8_t *bgra = (uint8_t *)malloc(npx * 4);
    if (!bgra) return;
    for (size_t i = 0; i < npx; i++)
    {
        bgra[i*4]   = src[i*4+2];   // B
        bgra[i*4+1] = src[i*4+1];   // G
        bgra[i*4+2] = src[i*4];     // R
        bgra[i*4+3] = 255;          // alpha 不透明
    }

    // 高质量平滑缩放
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, NULL);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_w;
    bmi.bmiHeader.biHeight = -g_h;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(hdc, dx, dy, dw, dh, 0, 0, g_w, g_h,
                  bgra, &bmi, DIB_RGB_COLORS, SRCCOPY);
    free(bgra);
}

// 图片显示区独立窗口类（STATIC 不转发 WM_PAINT，必须自定义类）
static LRESULT CALLBACK PicWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            draw_image(hwnd, hdc);
            EndPaint(hwnd, &ps);
        }
        return 0;
    case WM_SIZE:
        // 尺寸变化时强制重绘，缩放比例实时更新
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    case WM_ERASEBKGND:
        return 1; // 避免闪烁
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

// ====== 主窗口 ======
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case ID_BTN_OPEN: do_open(); break;
        case ID_BTN_ENCRYPT: do_transform(1); break;
        case ID_BTN_DECRYPT: do_transform(0); break;
        case ID_BTN_SAVE: do_save(); break;
        case ID_CHK_ORIG:
            g_show_orig = (SendMessageW(g_chkOrig, BM_GETCHECK, 0, 0) == BST_CHECKED);
            InvalidateRect(g_picBox, NULL, TRUE);
            break;
        }
        break;

    case WM_SIZE:
        layout_full(hwnd);
        if (g_picBox) InvalidateRect(g_picBox, NULL, TRUE);
        break;

    case WM_DPICHANGED:
        {
            g_dpi = (int)LOWORD(wp);
            if (g_hFont) DeleteObject(g_hFont);
            g_hFont = make_font();
            apply_font();
            RECT *prc = (RECT *)lp;
            SetWindowPos(hwnd, NULL, prc->left, prc->top,
                prc->right - prc->left, prc->bottom - prc->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            layout_full(hwnd);
        }
        break;

    case WM_DESTROY:
        free(g_pixels);
        free(g_orig);
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

// ====== DPI 感知初始化 ======
static void enable_dpi_awareness(void)
{
    typedef BOOL(WINAPI *SetProcessDpiAwarenessContext_t)(void*);
    typedef BOOL(WINAPI *SetProcessDPIAware_t)(void);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (!hUser32) return;
    SetProcessDpiAwarenessContext_t pC = (SetProcessDpiAwarenessContext_t)
        GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
    if (pC && pC(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return;
    SetProcessDPIAware_t pLegacy = (SetProcessDPIAware_t)
        GetProcAddress(hUser32, "SetProcessDPIAware");
    if (pLegacy) pLegacy();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    enable_dpi_awareness();
    int demo = 0;
    for (char *p = lpCmd; p && *p; p++)
        if (strstr(p, "--demo")) demo = 1;

    WNDCLASSEXW wcPic = {0};
    wcPic.cbSize = sizeof(wcPic);
    wcPic.lpfnWndProc = PicWndProc;
    wcPic.hInstance = hInstance;
    wcPic.hCursor = LoadCursorW(NULL, (LPCWSTR)MAKEINTRESOURCEW(IDC_ARROW));
    wcPic.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcPic.lpszClassName = L"ImgEncryptPicBox";
    RegisterClassExW(&wcPic);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)MAKEINTRESOURCEW(IDC_ARROW));
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"ImgEncryptApp";
    wc.hIcon = LoadIconW(NULL, (LPCWSTR)MAKEINTRESOURCEW(IDI_APPLICATION));
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    // 获取初始 DPI
    {
        typedef UINT(WINAPI *GetDpiForSystem_t)(void);
        HMODULE hU = GetModuleHandleW(L"user32.dll");
        if (hU)
        {
            GetDpiForSystem_t p = (GetDpiForSystem_t)GetProcAddress(hU, "GetDpiForSystem");
            if (p) g_dpi = (int)p();
        }
    }
    // 逻辑尺寸转物理像素创建窗口
    int win_w = px(760), win_h = px(620);
    g_hwndMain = CreateWindowExW(0, L"ImgEncryptApp", L"图片加解密工具",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, win_w, win_h,
        NULL, NULL, hInstance, NULL);

    // 顶部工具条
    g_btnOpen = CreateWindowExW(0, L"BUTTON", L"打开图片", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0, 0, 10, 10, g_hwndMain, (HMENU)(LONG_PTR)ID_BTN_OPEN, hInstance, NULL);
    g_btnEncrypt = CreateWindowExW(0, L"BUTTON", L"加密", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0, 0, 10, 10, g_hwndMain, (HMENU)(LONG_PTR)ID_BTN_ENCRYPT, hInstance, NULL);
    g_btnDecrypt = CreateWindowExW(0, L"BUTTON", L"解密", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0, 0, 10, 10, g_hwndMain, (HMENU)(LONG_PTR)ID_BTN_DECRYPT, hInstance, NULL);
    g_btnSave = CreateWindowExW(0, L"BUTTON", L"保存 PNG", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0, 0, 10, 10, g_hwndMain, (HMENU)(LONG_PTR)ID_BTN_SAVE, hInstance, NULL);

    // 密钥
    g_lblKey = CreateWindowExW(0, L"STATIC", L"密钥:", WS_CHILD|WS_VISIBLE|SS_LEFT,
        0, 0, 10, 10, g_hwndMain, NULL, hInstance, NULL);
    g_keyEdit = CreateWindowExW(0, L"EDIT", L"mimo", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
        0, 0, 10, 10, g_hwndMain, (HMENU)(LONG_PTR)ID_KEY_EDIT, hInstance, NULL);

    // 轮次
    g_lblRound = CreateWindowExW(0, L"STATIC", L"轮次:", WS_CHILD|WS_VISIBLE|SS_LEFT,
        0, 0, 10, 10, g_hwndMain, NULL, hInstance, NULL);
    g_roundEdit = CreateWindowExW(0, L"EDIT", L"1", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_NUMBER,
        0, 0, 10, 10, g_hwndMain, (HMENU)(LONG_PTR)ID_ROUND_EDIT, hInstance, NULL);

    // 对比原图复选框
    g_chkOrig = CreateWindowExW(0, L"BUTTON", L"显示原图", WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        0, 0, 10, 10, g_hwndMain, (HMENU)(LONG_PTR)ID_CHK_ORIG, hInstance, NULL);

    // 图片显示区（自定义类，可绘制）
    g_picBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"ImgEncryptPicBox", L"",
        WS_CHILD|WS_VISIBLE, 0, 0, 10, 10, g_hwndMain, NULL, hInstance, NULL);

    // 状态栏
    g_status = CreateWindowExW(0, L"STATIC", L"请打开一张 PNG 图片",
        WS_CHILD|WS_VISIBLE|SS_LEFT, 0, 0, 10, 10, g_hwndMain,
        (HMENU)(LONG_PTR)ID_STATUS, hInstance, NULL);
    g_zoomInfo = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD|WS_VISIBLE|SS_RIGHT, 0, 0, 10, 10, g_hwndMain,
        (HMENU)(LONG_PTR)ID_ZOOM_INFO, hInstance, NULL);

    // 字体
    g_hFont = make_font();
    apply_font();

    EnableWindow(g_btnEncrypt, FALSE);
    EnableWindow(g_btnDecrypt, FALSE);
    EnableWindow(g_btnSave, FALSE);

    layout_full(g_hwndMain);

    if (demo)
    {
        // 生成演示图：400x300 渐变
        const int dw = 400, dh = 300;
        uint8_t *px = (uint8_t *)malloc((size_t)dw * dh * 4);
        if (px)
        {
            for (int y = 0; y < dh; y++)
                for (int x = 0; x < dw; x++)
                {
                    int i = (y * dw + x) * 4;
                    px[i] = (uint8_t)(x * 255 / dw);
                    px[i+1] = (uint8_t)(y * 255 / dh);
                    px[i+2] = (uint8_t)((x + y) * 255 / (dw + dh));
                    px[i+3] = 255;
                }
            free(g_pixels);
            free(g_orig);
            g_pixels = px;
            g_orig = (uint8_t *)malloc((size_t)dw * dh * 4);
            if (g_orig) memcpy(g_orig, px, (size_t)dw * dh * 4);
            g_w = dw; g_h = dh;
            g_dirty = 0;
            set_status(L"演示图已加载（400 x 300）");
            InvalidateRect(g_picBox, NULL, TRUE);
            EnableWindow(g_btnEncrypt, TRUE);
            EnableWindow(g_btnDecrypt, TRUE);
        }
    }

    ShowWindow(g_hwndMain, nShow);
    UpdateWindow(g_hwndMain);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    ExitProcess((UINT)msg.wParam);
    return 0;
}
