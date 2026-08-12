// all-main.c - 文本 + 图片 加解密工具（Tab 切换合并版）
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>
#include <shellapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <math.h>
#include "crypto.h"
#include "crypto_img.h"
#include "png.h"
#include "img/image_decode.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
#ifndef ICC_UPDOWN_CLASSES
#define ICC_UPDOWN_CLASSES 0x00000400
#endif
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((void*)-4)
#endif

// ====== i18n ======
enum { LANG_ZH_CN = 0, LANG_ZH_TW = 1, LANG_EN = 2 };
static int g_lang = LANG_ZH_CN;

typedef struct {
    const wchar_t *windowTitle;
    const wchar_t *tabText;
    const wchar_t *tabImage;
    const wchar_t *key;
    const wchar_t *rounds;
    const wchar_t *inputText;
    const wchar_t *outputResult;
    const wchar_t *encrypt;
    const wchar_t *decrypt;
    const wchar_t *copy;
    const wchar_t *swap;
    const wchar_t *clear;
    const wchar_t *ready;
    const wchar_t *enterKey;
    const wchar_t *enterText;
    const wchar_t *encDone;
    const wchar_t *decDone;
    const wchar_t *noCopy;
    const wchar_t *copied;
    const wchar_t *noSwap;
    const wchar_t *swapped;
    const wchar_t *cleared;
    const wchar_t *openImage;
    const wchar_t *savePng;
    const wchar_t *showOrig;
    const wchar_t *imgStatusInit;
    const wchar_t *imgNotValid;
    const wchar_t *imgLoaded;
    const wchar_t *imgEnterKey;
    const wchar_t *imgOpenFirst;
    const wchar_t *imgSaved;
    const wchar_t *imgSaveFail;
    const wchar_t *langLabel;
    const wchar_t *fileFilter;
    const wchar_t *pngFilter;
} LangStrings;

static const LangStrings g_strings[] = {
    {
        L"\u52a0\u89e3\u5bc6\u5de5\u5177",
        L"\u6587\u672c\u52a0\u89e3\u5bc6",
        L"\u56fe\u7247\u52a0\u89e3\u5bc6",
        L"\u5bc6\u94a5",
        L"\u8f6e\u6b21",
        L"\u8f93\u5165\u6587\u672c",
        L"\u8f93\u51fa\u7ed3\u679c",
        L"\u52a0\u5bc6",
        L"\u89e3\u5bc6",
        L"\u590d\u5236",
        L"\u56de\u586b",
        L"\u6e05\u7a7a",
        L"\u5c31\u7eea",
        L"\u8bf7\u8f93\u5165\u5bc6\u94a5",
        L"\u8bf7\u8f93\u5165\u6587\u672c",
        L"\u52a0\u5bc6\u5b8c\u6210",
        L"\u89e3\u5bc6\u5b8c\u6210",
        L"\u6ca1\u6709\u53ef\u590d\u5236\u7684\u5185\u5bb9",
        L"\u5df2\u590d\u5236\u5230\u526a\u8d34\u677f",
        L"\u6ca1\u6709\u53ef\u56de\u586b\u7684\u5185\u5bb9",
        L"\u5df2\u56de\u586b\u5230\u8f93\u5165\u6846",
        L"\u5df2\u6e05\u7a7a",
        L"\u6253\u5f00\u56fe\u7247",
        L"\u4fdd\u5b58 PNG",
        L"\u663e\u793a\u539f\u56fe",
        L"\u70b9\u51fb\u300c\u6253\u5f00\u56fe\u7247\u300d\u6216\u62d6\u5165\u56fe\u7247\u6587\u4ef6\uff08PNG/JPG/BMP\uff09",
        L"\u4e0d\u662f\u6709\u6548\u7684\u56fe\u7247",
        L"\u5df2\u52a0\u8f7d",
        L"\u8bf7\u8f93\u5165\u5bc6\u94a5",
        L"\u8bf7\u5148\u6253\u5f00\u56fe\u7247",
        L"\u5df2\u4fdd\u5b58",
        L"\u4fdd\u5b58\u5931\u8d25",
        L"\u8bed\u8a00",
        L"\u56fe\u7247\u6587\u4ef6 (*.png;*.jpg;*.jpeg;*.bmp;*.gif)\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0PNG \u56fe\u7247 (*.png)\0*.png\0JPEG \u56fe\u7247 (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0BMP \u56fe\u7247 (*.bmp)\0*.bmp\0\u6240\u6709\u6587\u4ef6 (*.*)\0*.*\0",
        L"PNG \u56fe\u7247 (*.png)\0*.png\0"
    },
    {
        L"\u52a0\u89e3\u5bc6\u5de5\u5177",
        L"\u6587\u5b57\u52a0\u89e3\u5bc6",
        L"\u5716\u7247\u52a0\u89e3\u5bc6",
        L"\u5bc6\u94a5",
        L"\u8f2a\u6b21",
        L"\u8f38\u5165\u6587\u5b57",
        L"\u8f38\u51fa\u7d50\u679c",
        L"\u52a0\u5bc6",
        L"\u89e3\u5bc6",
        L"\u8907\u88fd",
        L"\u56de\u586b",
        L"\u6e05\u7a7a",
        L"\u5c31\u7dd2",
        L"\u8acb\u8f38\u5165\u5bc6\u94a5",
        L"\u8acb\u8f38\u5165\u6587\u5b57",
        L"\u52a0\u5bc6\u5b8c\u6210",
        L"\u89e3\u5bc6\u5b8c\u6210",
        L"\u6c92\u6709\u53ef\u8907\u88fd\u7684\u5167\u5bb9",
        L"\u5df2\u8907\u88fd\u5230\u526a\u8cbc\u7c3f",
        L"\u6c92\u6709\u53ef\u56de\u586b\u7684\u5167\u5bb9",
        L"\u5df2\u56de\u586b\u5230\u8f38\u5165\u6846",
        L"\u5df2\u6e05\u7a7a",
        L"\u958b\u555f\u5716\u7247",
        L"\u5132\u5b58 PNG",
        L"\u986f\u793a\u539f\u5716",
        L"\u9ede\u64ca\u300c\u958b\u555f\u5716\u7247\u300d\u6216\u62d6\u5165\u5716\u7247\u6587\u4ef6\uff08PNG/JPG/BMP\uff09",
        L"\u4e0d\u662f\u6709\u6548\u7684\u5716\u7247",
        L"\u5df2\u8f09\u5165",
        L"\u8acb\u8f38\u5165\u5bc6\u94a5",
        L"\u8acb\u5148\u958b\u555f\u5716\u7247",
        L"\u5df2\u5132\u5b58",
        L"\u5132\u5b58\u5931\u6557",
        L"\u8a9e\u8a00",
        L"\u5716\u7247\u6587\u4ef6 (*.png;*.jpg;*.jpeg;*.bmp;*.gif)\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0PNG \u5716\u7247 (*.png)\0*.png\0JPEG \u5716\u7247 (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0BMP \u5716\u7247 (*.bmp)\0*.bmp\0\u6240\u6709\u6587\u4ef6 (*.*)\0*.*\0",
        L"PNG \u5716\u7247 (*.png)\0*.png\0"
    },
    {
        L"Encrypt & Decrypt",
        L"Text Encrypt",
        L"Image Encrypt",
        L"Key",
        L"Rounds",
        L"Input Text",
        L"Output",
        L"Encrypt",
        L"Decrypt",
        L"Copy",
        L"Swap",
        L"Clear",
        L"Ready",
        L"Please enter a key",
        L"Please enter text",
        L"Encryption done",
        L"Decryption done",
        L"Nothing to copy",
        L"Copied to clipboard",
        L"Nothing to swap",
        L"Swapped to input",
        L"Cleared",
        L"Open Image",
        L"Save PNG",
        L"Show Original",
        L"Click \"Open Image\" or drag image file (PNG/JPG/BMP)",
        L"Not a valid image",
        L"Image loaded",
        L"Please enter a key",
        L"Please open an image first",
        L"Saved",
        L"Save failed",
        L"Language",
        L"Image files (*.png;*.jpg;*.jpeg;*.bmp;*.gif)\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0PNG images (*.png)\0*.png\0JPEG images (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0BMP images (*.bmp)\0*.bmp\0All files (*.*)\0*.*\0",
        L"PNG images (*.png)\0*.png\0"
    }
};

#define S (g_strings[g_lang])

static void load_lang(void)
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\EncryptTool", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD val = 0, sz = sizeof(val);
        RegQueryValueExW(hKey, L"Language", NULL, NULL, (LPBYTE)&val, &sz);
        if (val <= LANG_EN) g_lang = (int)val;
        RegCloseKey(hKey);
    }
}

static void save_lang(void)
{
    HKEY hKey;
    DWORD disp;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\EncryptTool", 0, NULL, 0, KEY_WRITE, NULL, &hKey, &disp) == ERROR_SUCCESS)
    {
        DWORD val = (DWORD)g_lang;
        RegSetValueExW(hKey, L"Language", 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        RegCloseKey(hKey);
    }
}

// ====== Tab 控件 ID ======
#define ID_TAB_TEXT     1001
#define ID_TAB_IMAGE    1002
#define ID_LANG_COMBO   1003

// ====== 文本工具控件 ID ======
#define ID_T_ENC     2001
#define ID_T_DEC     2002
#define ID_T_COPY    2003
#define ID_T_SWAP    2004
#define ID_T_CLEAR   2005
#define ID_T_KEY     2006
#define ID_T_ROUNDUD 2007
#define ID_T_ROUND   2008
#define ID_T_INPUT   2009
#define ID_T_OUTPUT  2010
#define ID_T_STATUS  2011
#define ID_T_SPLIT   2012

// ====== 图片工具控件 ID ======
#define ID_I_OPEN    3001
#define ID_I_ENC     3002
#define ID_I_DEC     3003
#define ID_I_SAVE    3004
#define ID_I_KEY     3005
#define ID_I_ROUND   3006
#define ID_I_STATUS  3007
#define ID_I_CHKORIG 3008
#define ID_I_ZOOM    3009

// ====== DPI ======
static int g_dpi = 96;
static int px(int logical) { return MulDiv(logical, g_dpi, 96); }

static HWND g_hwndMain;
static HFONT g_hFont;
static int g_active_tab = 0;   // 0=文本 1=图片
static HWND g_tabText, g_tabImage;
static HWND g_langCombo, g_langLabel;

// 前置声明
static void layout_text(HWND hwnd);
static void layout_image(HWND hwnd);
static void i_update_zoom(void);

// ====== 文本工具 ======
static HWND t_key, t_round, t_roundud, t_input, t_output, t_status, t_split;
static HWND t_btns[5];
static HWND t_lblKey, t_lblRound, t_lblIn, t_lblOut;
static int t_splitY = -1;   // 分割条逻辑位置；-1 表示首次布局时自动设为输入/输出等高
static int t_dragging = 0;
static int t_dragStartY = 0;

// ====== 图片工具 ======
static HWND i_open, i_enc, i_dec, i_save, i_key, i_round, i_status, i_chk, i_zoom;
static HWND i_lblKey, i_lblRound, i_roundud;
static HWND i_pic;
static uint8_t *i_pixels = NULL;
static uint8_t *i_orig = NULL;
static int i_w = 0, i_h = 0;
static int i_dirty = 0;
static int i_show_orig = 0;

// ====== 文本工具：布局逻辑尺寸 ======
#define T_FONT      12
#define T_BTN_H     24
#define T_BTN_W     68
#define T_TITLE_H   16
#define T_MIN_SPLIT 80

// Tab 区高度（顶部偏移）：8 顶边 + 30 Tab高 + 8 间隔
#define TAB_TOP_OFFSET 46

// ====== 图片工具：布局逻辑尺寸 ======
#define I_PAD       8
#define I_BTN_H     30
#define I_CTRL_H    26
#define I_BTN_W_OPEN 90
#define I_BTN_W_MID 70
#define I_BTN_W_SAVE 90
#define I_KEY_W     150
#define I_ROUND_W   50
#define I_LABEL_W   40
#define I_STATUS_H  22
#define I_GAP       8
#define I_FONT      14
#define I_ZOOM_W    200

// ====== 共享工具函数 ======
static void set_text(HWND h, const wchar_t *s) { SetWindowTextW(h, s ? s : L""); }

static wchar_t *get_text(HWND h)
{
    int len = GetWindowTextLengthW(h);
    wchar_t *buf = (wchar_t *)malloc(((size_t)len + 1) * sizeof(wchar_t));
    if (!buf) return NULL;
    GetWindowTextW(h, buf, len + 1);
    return buf;
}

static void set_status(const wchar_t *s)
{
    if (g_active_tab == 0) set_text(t_status, s);
    else set_text(i_status, s);
}

static HFONT make_font(int h)
{
    return CreateFontW(-px(h), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
}

// ====== Tab 切换 ======
static void show_text_tab(void) { ShowWindow(t_key, SW_SHOW); ShowWindow(t_round, SW_SHOW); ShowWindow(t_roundud, SW_SHOW); ShowWindow(t_input, SW_SHOW); ShowWindow(t_output, SW_SHOW); ShowWindow(t_status, SW_SHOW); ShowWindow(t_split, SW_SHOW); ShowWindow(t_lblKey, SW_SHOW); ShowWindow(t_lblRound, SW_SHOW); ShowWindow(t_lblIn, SW_SHOW); ShowWindow(t_lblOut, SW_SHOW); for (int i = 0; i < 5; i++) ShowWindow(t_btns[i], SW_SHOW); }
static void hide_text_tab(void) { ShowWindow(t_key, SW_HIDE); ShowWindow(t_round, SW_HIDE); ShowWindow(t_roundud, SW_HIDE); ShowWindow(t_input, SW_HIDE); ShowWindow(t_output, SW_HIDE); ShowWindow(t_status, SW_HIDE); ShowWindow(t_split, SW_HIDE); ShowWindow(t_lblKey, SW_HIDE); ShowWindow(t_lblRound, SW_HIDE); ShowWindow(t_lblIn, SW_HIDE); ShowWindow(t_lblOut, SW_HIDE); for (int i = 0; i < 5; i++) ShowWindow(t_btns[i], SW_HIDE); }
static void show_image_tab(void) { ShowWindow(i_open, SW_SHOW); ShowWindow(i_enc, SW_SHOW); ShowWindow(i_dec, SW_SHOW); ShowWindow(i_save, SW_SHOW); ShowWindow(i_lblKey, SW_SHOW); ShowWindow(i_key, SW_SHOW); ShowWindow(i_lblRound, SW_SHOW); ShowWindow(i_round, SW_SHOW); ShowWindow(i_roundud, SW_SHOW); ShowWindow(i_status, SW_SHOW); ShowWindow(i_chk, SW_SHOW); ShowWindow(i_zoom, SW_SHOW); ShowWindow(i_pic, SW_SHOW); }
  static void hide_image_tab(void) { ShowWindow(i_open, SW_HIDE); ShowWindow(i_enc, SW_HIDE); ShowWindow(i_dec, SW_HIDE); ShowWindow(i_save, SW_HIDE); ShowWindow(i_lblKey, SW_HIDE); ShowWindow(i_key, SW_HIDE); ShowWindow(i_lblRound, SW_HIDE); ShowWindow(i_round, SW_HIDE); ShowWindow(i_roundud, SW_HIDE); ShowWindow(i_status, SW_HIDE); ShowWindow(i_chk, SW_HIDE); ShowWindow(i_zoom, SW_HIDE); ShowWindow(i_pic, SW_HIDE); }

static void switch_tab(int tab)
{
    g_active_tab = tab;
    // 激活的标签页按钮置灰不可点，另一个可点
    EnableWindow(g_tabText, tab == 1);
    EnableWindow(g_tabImage, tab == 0);
    if (tab == 0) { hide_image_tab(); show_text_tab(); layout_text(g_hwndMain); }
    else { hide_text_tab(); show_image_tab(); layout_image(g_hwndMain); }
    InvalidateRect(g_hwndMain, NULL, TRUE);
}

// ====== 文本工具：转 LF 为 CRLF ======
static wchar_t *to_win_lf(const wchar_t *s)
{
    if (!s || !wcschr(s, L'\n')) return wcsdup(s ? s : L"");
    int cap = (int)(wcslen(s) * 1.1 + 16);
    wchar_t *out = (wchar_t *)malloc((size_t)cap * sizeof(wchar_t));
    if (!out) return NULL;
    int j = 0;
    for (int i = 0; s[i]; i++)
    {
        if (s[i] == L'\n' && (i == 0 || s[i - 1] != L'\r'))
        {
            if (j + 2 >= cap) { cap *= 2; out = (wchar_t *)realloc(out, (size_t)cap * sizeof(wchar_t)); }
            out[j++] = L'\r'; out[j++] = L'\n';
        }
        else out[j++] = s[i];
    }
    out[j] = 0;
    return out;
}

// ====== 文本工具：操作逻辑 ======
static int t_get_rounds(void)
{
    wchar_t *s = get_text(t_round);
    int r = 1;
    if (s && s[0]) r = (int)wcstol(s, NULL, 10);
    free(s);
    if (r < 1) r = 1;
    return r;
}

static void t_transform(int encrypt)
{
    wchar_t *key = get_text(t_key);
    if (!key || !key[0]) { set_status(S.enterKey); free(key); return; }
    wchar_t *text = get_text(t_input);
    if (!text || !text[0]) { set_status(S.enterText); free(key); free(text); return; }
    int rounds = (int)SendMessageW(t_roundud, UDM_GETPOS, 0, 0);
    if (rounds < 1) rounds = 1;
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    wchar_t *result = encrypt ? crypto_encrypt(text, key, rounds) : crypto_decrypt(text, key, rounds);
    QueryPerformanceCounter(&t1);
    wchar_t *win_text = to_win_lf(result);
    set_text(t_output, win_text);
    double ms = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart * 1000.0;
    wchar_t info[160];
    if (ms < 1000.0)
        swprintf(info, 160, L"%s (%d %s) | %.0f ms", encrypt ? S.encDone : S.decDone, rounds, S.rounds, ms);
    else
        swprintf(info, 160, L"%s (%d %s) | %.2f s", encrypt ? S.encDone : S.decDone, rounds, S.rounds, ms / 1000.0);
    set_status(info);
    free(key); free(text); free(result); free(win_text);
}

static void t_copy(void)
{
    wchar_t *out = get_text(t_output);
    if (!out || !out[0]) { set_status(S.noCopy); free(out); return; }
    if (OpenClipboard(g_hwndMain))
    {
        EmptyClipboard();
        size_t sz = (wcslen(out) + 1) * sizeof(wchar_t);
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, sz);
        if (h) { wchar_t *d = (wchar_t *)GlobalLock(h); memcpy(d, out, sz); GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); }
        CloseClipboard();
    }
    set_status(S.copied);
    free(out);
}

static void t_swap(void)
{
    wchar_t *out = get_text(t_output);
    if (!out || !out[0]) { set_status(S.noSwap); free(out); return; }
    set_text(t_input, out);
    set_text(t_output, L"");
    set_status(S.swapped);
    free(out); SetFocus(t_input);
}

static void t_clear(void)
{
    set_text(t_key, L"mimo");
    set_text(t_input, L"");
    set_text(t_output, L"");
    SendMessageW(t_roundud, UDM_SETPOS, 0, MAKELONG(1, 0));
    set_status(S.cleared);
    SetFocus(t_input);
}

// ====== 文本工具：布局 ======
static void layout_text(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;

    int padding = px(8);
    int top_off = px(TAB_TOP_OFFSET);
    int status_h = px(22);
    int splitter_h = px(4);
    int minSplit = px(T_MIN_SPLIT);
    int btn_h = px(T_BTN_H);
    int btn_w = px(T_BTN_W);
    int btn_gap = px(6);
    int label_w = px(34);
    int label_gap = px(4);
    int key_input_w = px(180);
    int round_label_w = px(34);
    int round_w = px(60);
    int round_updown_w = px(16);
    int row_y_gap = px(6);

    // 顶栏第一行内容宽度（密钥+轮次），按钮从其后开始
    int row0_content = padding + label_w + label_gap + key_input_w + px(12)
        + round_label_w + label_gap + round_w + round_updown_w + px(16);
    int btn_start_x = row0_content;
    int btn_row_w = btn_w * 5 + btn_gap * 4;

    int top_row_h = btn_h + row_y_gap;
    int top_bar_h, btn_row1_count = 0, btn_row2_count = 0;

    if (btn_start_x + btn_row_w <= W - padding)
    {
        btn_row1_count = 5;
        top_bar_h = top_row_h;
    }
    else
    {
        btn_row1_count = 0; btn_row2_count = 0;
        int avail_row1 = (W - padding) - btn_start_x;
        int remaining = 5, cur_w = 0;
        for (int i = 0; i < 5 && remaining > 0; i++)
        {
            int add = btn_w + (i > 0 ? btn_gap : 0);
            if (cur_w + add > avail_row1 && btn_row1_count > 0) break;
            cur_w += add; btn_row1_count++; remaining--;
        }
        btn_row2_count = remaining;
        top_bar_h = top_row_h * 2 + row_y_gap;
    }

    int title_h = px(T_TITLE_H);
    int title_gap = px(2);
    int mid_y = top_off + padding + top_bar_h + row_y_gap + title_h + title_gap;
    int mid_h = H - mid_y - padding - status_h - padding;

    // 首次布局：自动计算分割条位置，使输入框/输出框高度相等
    if (t_splitY < 0)
    {
        int eq = (H - status_h - padding - mid_y - splitter_h - 2 * title_gap - title_h + px(2)) / 2;
        int eq_min = px(T_MIN_SPLIT);
        int eq_max = mid_h - eq_min - splitter_h;
        if (eq < eq_min) eq = eq_min;
        if (eq > eq_max) eq = eq_max;
        t_splitY = MulDiv(eq, 96, g_dpi);
    }

    int sp = px(t_splitY);
    int max_split = mid_h - minSplit - splitter_h;
    if (max_split < minSplit) max_split = minSplit;
    if (sp < minSplit) sp = minSplit;
    if (sp > max_split) sp = max_split;
    int splitter_top = mid_y + sp;

    int y = top_off + padding + row_y_gap / 2;
    int x = padding;

    MoveWindow(t_lblKey, x, y, label_w, btn_h, TRUE); x += label_w + label_gap;
    MoveWindow(t_key, x, y, key_input_w, btn_h, TRUE); x += key_input_w + px(12);
    MoveWindow(t_lblRound, x, y, round_label_w, btn_h, TRUE); x += round_label_w + label_gap;
    MoveWindow(t_round, x, y, round_w, btn_h, TRUE);
    MoveWindow(t_roundud, x + round_w, y, round_updown_w, btn_h, TRUE);
    x += round_w + round_updown_w + px(16);

    for (int i = 0; i < btn_row1_count; i++) { MoveWindow(t_btns[i], x, y, btn_w, btn_h, TRUE); x += btn_w + btn_gap; }

    if (btn_row2_count > 0)
    {
        int y2 = y + btn_h + row_y_gap;
        x = padding;
        for (int i = btn_row1_count; i < 5; i++) { MoveWindow(t_btns[i], x, y2, btn_w, btn_h, TRUE); x += btn_w + btn_gap; }
    }

    int title_y = top_off + padding + top_bar_h + row_y_gap;
    MoveWindow(t_lblIn, padding, title_y, W - padding * 2, title_h, TRUE);

    int input_y = title_y + title_h + title_gap;
    MoveWindow(t_input, padding, input_y, W - padding * 2, splitter_top - input_y - px(2), TRUE);

    MoveWindow(t_split, padding, splitter_top, W - padding * 2, splitter_h, TRUE);

    int output_title_y = splitter_top + splitter_h + title_gap;
    MoveWindow(t_lblOut, padding, output_title_y, W - padding * 2, title_h, TRUE);
    int output_y = output_title_y + title_h + title_gap;
    MoveWindow(t_output, padding, output_y, W - padding * 2, (H - status_h - padding) - output_y, TRUE);

    MoveWindow(t_status, padding, H - status_h - padding, W - padding * 2, status_h, TRUE);
}

// ====== 文本工具：创建控件 ======
static void create_text_controls(HWND hwnd)
{
    HMODULE mod = GetModuleHandleW(NULL);
    t_lblIn = CreateWindowExW(0, L"STATIC", S.inputText, WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, px(80), px(T_TITLE_H), hwnd, NULL, mod, NULL);
    t_lblOut = CreateWindowExW(0, L"STATIC", S.outputResult, WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, px(80), px(T_TITLE_H), hwnd, NULL, mod, NULL);
    t_lblKey = CreateWindowExW(0, L"STATIC", S.key, WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, px(34), px(T_BTN_H), hwnd, NULL, mod, NULL);
    t_lblRound = CreateWindowExW(0, L"STATIC", S.rounds, WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, px(34), px(T_BTN_H), hwnd, NULL, mod, NULL);
    t_key = CreateWindowExW(0, L"EDIT", L"mimo", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 0, 0, px(180), px(T_BTN_H), hwnd, (HMENU)(LONG_PTR)ID_T_KEY, mod, NULL);
    t_round = CreateWindowExW(0, L"EDIT", L"1", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 0, 0, px(60), px(T_BTN_H), hwnd, (HMENU)(LONG_PTR)ID_T_ROUND, mod, NULL);
    t_roundud = CreateWindowExW(0, UPDOWN_CLASSW, L"", WS_CHILD | WS_VISIBLE | UDS_ARROWKEYS | UDS_SETBUDDYINT, 0, 0, px(16), px(T_BTN_H), hwnd, (HMENU)(LONG_PTR)ID_T_ROUNDUD, mod, NULL);
    SendMessageW(t_roundud, UDM_SETBUDDY, (WPARAM)t_round, 0);
    SendMessageW(t_roundud, UDM_SETRANGE, 0, MAKELONG(99, 1));
    SendMessageW(t_roundud, UDM_SETPOS, 0, MAKELONG(1, 0));

    int ids[] = { ID_T_ENC, ID_T_DEC, ID_T_COPY, ID_T_SWAP, ID_T_CLEAR };
    const wchar_t *txts[] = { S.encrypt, S.decrypt, S.copy, S.swap, S.clear };
    for (int i = 0; i < 5; i++)
        t_btns[i] = CreateWindowExW(0, L"BUTTON", txts[i], WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, px(T_BTN_W), px(T_BTN_H), hwnd, (HMENU)(LONG_PTR)ids[i], mod, NULL);

    t_input = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, 0, 0, px(100), px(100), hwnd, (HMENU)(LONG_PTR)ID_T_INPUT, mod, NULL);
    t_split = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, 0, 0, px(100), px(4), hwnd, (HMENU)(LONG_PTR)ID_T_SPLIT, mod, NULL);
    t_output = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_WANTRETURN, 0, 0, px(100), px(100), hwnd, (HMENU)(LONG_PTR)ID_T_OUTPUT, mod, NULL);
    t_status = CreateWindowExW(0, L"STATIC", S.ready, WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, px(100), px(22), hwnd, (HMENU)(LONG_PTR)ID_T_STATUS, mod, NULL);
}

// ====== 图片工具：文件读取 ======
static uint8_t *i_read_file(const wchar_t *path, size_t *len_out)
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

static int i_load(const wchar_t *path)
{
    int w, h;
    uint8_t *px = NULL;
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    int rc = image_decode_file(path, &w, &h, &px);
    QueryPerformanceCounter(&t1);
    if (rc != 0) { set_status(S.imgNotValid); return 0; }
    free(i_pixels); free(i_orig);
    i_pixels = px;
    i_orig = (uint8_t *)malloc((size_t)w * h * 4);
    if (i_orig) memcpy(i_orig, px, (size_t)w * h * 4);
    i_w = w; i_h = h;
    i_dirty = 0; i_show_orig = 0;
    SendMessageW(i_chk, BM_SETCHECK, BST_UNCHECKED, 0);
    double ms = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart * 1000.0;
    wchar_t info[160];
    if (ms < 1000.0)
        swprintf(info, 160, L"%d x %d (%d KB) | %.0f ms", w, h, (int)((size_t)w * h * 4 / 1024), ms);
    else
        swprintf(info, 160, L"%d x %d (%d KB) | %.2f s", w, h, (int)((size_t)w * h * 4 / 1024), ms / 1000.0);
    set_status(info);
    InvalidateRect(i_pic, NULL, TRUE);
    i_update_zoom();
    EnableWindow(i_enc, TRUE);
    EnableWindow(i_dec, TRUE);
    EnableWindow(i_save, FALSE);
    return 1;
}

static void i_open_file(void)
{
    OPENFILENAMEW ofn = {0};
    wchar_t file[1024] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFilter = S.fileFilter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) i_load(file);
}

static int i_rounds(void)
{
    int r = (int)SendMessageW(i_roundud, UDM_GETPOS, 0, 0);
    if (r < 1) r = 1;
    if (r > 99) r = 99;
    return r;
}

static void i_transform(int enc)
{
    if (!i_pixels) { set_status(S.imgOpenFirst); return; }
    wchar_t *key = get_text(i_key);
    if (!key || !key[0]) { set_status(S.imgEnterKey); free(key); return; }
    int rounds = i_rounds();
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    if (enc) img_encrypt(i_pixels, key, rounds, i_w, i_h);
    else img_decrypt(i_pixels, key, rounds, i_w, i_h);
    QueryPerformanceCounter(&t1);
    double ms = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart * 1000.0;
    wchar_t info[160];
    if (ms < 1000.0)
        swprintf(info, 160, L"%s (%d) | %d x %d | %.0f ms", enc ? S.encDone : S.decDone, rounds, i_w, i_h, ms);
    else
        swprintf(info, 160, L"%s (%d) | %d x %d | %.2f s", enc ? S.encDone : S.decDone, rounds, i_w, i_h, ms / 1000.0);
    set_status(info);
    i_dirty = 1; i_show_orig = 0;
    SendMessageW(i_chk, BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(i_save, TRUE);
    InvalidateRect(i_pic, NULL, TRUE);
    free(key);
}

static void i_save_file(void)
{
    if (!i_pixels || !i_dirty) return;
    OPENFILENAMEW ofn = {0};
    wchar_t file[1024] = L"";
    wcscpy(file, L"output.png");
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFilter = S.pngFilter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"png";
    if (!GetSaveFileNameW(&ofn)) return;
    size_t cap = (size_t)i_w * i_h * 4 * 2 + 4096;
    uint8_t *png = (uint8_t *)malloc(cap);
    size_t plen = png_encode(i_pixels, i_w, i_h, png, cap);
    if (plen == 0) { set_status(S.imgSaveFail); free(png); return; }
    FILE *f = _wfopen(file, L"wb");
    if (f) { fwrite(png, 1, plen, f); fclose(f); set_status(S.imgSaved); }
    else set_status(S.imgSaveFail);
    free(png);
}

// ====== 图片工具：绘制预览 ======
static void i_draw(HWND hwnd, HDC hdc)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
      if (!i_pixels || i_w <= 0 || i_h <= 0)
      {
        SetTextColor(hdc, RGB(120, 120, 120));
        SetBkMode(hdc, TRANSPARENT);
        HFONT oldFont = (HFONT)SelectObject(hdc, g_hFont);
        const wchar_t *msg = S.imgStatusInit;
        RECT tr = rc;
        DrawTextW(hdc, msg, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        return;
    }

    int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
    double sx = (double)cw / i_w, sy = (double)ch / i_h;
    double s = sx < sy ? sx : sy;
    int dw = (int)(i_w * s), dh = (int)(i_h * s);
    if (dw < 1) dw = 1; if (dh < 1) dh = 1;
    int dx = (cw - dw) / 2, dy = (ch - dh) / 2;

    const uint8_t *src = (i_show_orig && i_orig) ? i_orig : i_pixels;
    size_t npx = (size_t)i_w * i_h;
    uint8_t *bgra = (uint8_t *)malloc(npx * 4);
    if (!bgra) return;
    // 与白色背景（#ffffff）做 alpha 混合：透明像素显示为白，半透明平滑过渡
    for (size_t i = 0; i < npx; i++)
    {
        int a = src[i*4+3];
        int inv = 255 - a;
        bgra[i*4]   = (uint8_t)((src[i*4+2] * a + 255 * inv) / 255);
        bgra[i*4+1] = (uint8_t)((src[i*4+1] * a + 255 * inv) / 255);
        bgra[i*4+2] = (uint8_t)((src[i*4]   * a + 255 * inv) / 255);
        bgra[i*4+3] = 255;
    }
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, NULL);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = i_w;
    bmi.bmiHeader.biHeight = -i_h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(hdc, dx, dy, dw, dh, 0, 0, i_w, i_h, bgra, &bmi, DIB_RGB_COLORS, SRCCOPY);
    free(bgra);
}

// ====== 图片工具：更新状态栏右端缩放信息 ======
static void i_update_zoom(void)
{
    if (!i_zoom) return;
    wchar_t buf[96];
    if (i_pixels && i_w > 0 && i_h > 0 && i_pic)
    {
        RECT rc;
        GetClientRect(i_pic, &rc);
        int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
        double sx = (double)cw / i_w, sy = (double)ch / i_h;
        double s = sx < sy ? sx : sy;
        int pct = (int)(s * 100 + 0.5);
        swprintf(buf, 96, L"%d x %d · %d%%", i_w, i_h, pct);
    }
    else swprintf(buf, 96, L"");
    SetWindowTextW(i_zoom, buf);
}

// ====== 图片工具：布局 ======
static void layout_image(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;

    int pad = px(I_PAD), gap = px(I_GAP);
    int btn_h = px(I_BTN_H), ctrl_h = px(I_CTRL_H), status_h = px(I_STATUS_H);
    int top_y = pad + px(TAB_TOP_OFFSET);
    int label_top = top_y + (btn_h - ctrl_h) / 2;

    int x = pad;
    MoveWindow(i_open, x, top_y, px(I_BTN_W_OPEN), btn_h, TRUE); x += px(I_BTN_W_OPEN) + gap;
    MoveWindow(i_enc, x, top_y, px(I_BTN_W_MID), btn_h, TRUE); x += px(I_BTN_W_MID) + gap;
    MoveWindow(i_dec, x, top_y, px(I_BTN_W_MID), btn_h, TRUE); x += px(I_BTN_W_MID) + gap;
    MoveWindow(i_save, x, top_y, px(I_BTN_W_SAVE), btn_h, TRUE); x += px(I_BTN_W_SAVE) + gap * 2;
    // 密钥：标签 + 输入框
    MoveWindow(i_lblKey, x, label_top, px(34), ctrl_h, TRUE); x += px(34) + px(4);
    MoveWindow(i_key, x, label_top, px(I_KEY_W), ctrl_h, TRUE); x += px(I_KEY_W) + gap;
    // 轮次：标签 + 输入框 + UpDown
    MoveWindow(i_lblRound, x, label_top, px(34), ctrl_h, TRUE); x += px(34) + px(4);
    MoveWindow(i_round, x, label_top, px(I_ROUND_W), ctrl_h, TRUE);
    MoveWindow(i_roundud, x + px(I_ROUND_W), label_top, px(16), ctrl_h, TRUE);
    x += px(I_ROUND_W) + px(16) + gap;
    MoveWindow(i_chk, x, label_top, px(90), ctrl_h, TRUE);

    int pic_top = top_y + btn_h + pad;
    int pic_bottom = H - status_h - pad;
    if (pic_bottom < pic_top + px(40)) pic_bottom = pic_top + px(40);
    MoveWindow(i_pic, pad, pic_top, W - pad * 2, pic_bottom - pic_top, TRUE);

    int status_y = H - status_h - pad;
    int zoom_w = px(I_ZOOM_W);
    MoveWindow(i_status, pad, status_y, W - pad * 2 - zoom_w - gap, status_h, TRUE);
    MoveWindow(i_zoom, W - pad - zoom_w, status_y, zoom_w, status_h, TRUE);
    i_update_zoom();
}

// ====== 图片工具：创建控件 ======
static void create_image_controls(HWND hwnd)
{
    HMODULE mod = GetModuleHandleW(NULL);
    i_open = CreateWindowExW(0, L"BUTTON", S.openImage, WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hwnd, (HMENU)(LONG_PTR)ID_I_OPEN, mod, NULL);
    i_enc = CreateWindowExW(0, L"BUTTON", S.encrypt, WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hwnd, (HMENU)(LONG_PTR)ID_I_ENC, mod, NULL);
    i_dec = CreateWindowExW(0, L"BUTTON", S.decrypt, WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hwnd, (HMENU)(LONG_PTR)ID_I_DEC, mod, NULL);
    i_save = CreateWindowExW(0, L"BUTTON", S.savePng, WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, hwnd, (HMENU)(LONG_PTR)ID_I_SAVE, mod, NULL);
    i_lblKey = CreateWindowExW(0, L"STATIC", S.key, WS_CHILD | SS_LEFT, 0, 0, 10, 10, hwnd, NULL, mod, NULL);
    i_key = CreateWindowExW(0, L"EDIT", L"mimo", WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 10, 10, hwnd, (HMENU)(LONG_PTR)ID_I_KEY, mod, NULL);
    i_lblRound = CreateWindowExW(0, L"STATIC", S.rounds, WS_CHILD | SS_LEFT, 0, 0, 10, 10, hwnd, NULL, mod, NULL);
    i_round = CreateWindowExW(0, L"EDIT", L"1", WS_CHILD | WS_BORDER | ES_NUMBER, 0, 0, 10, 10, hwnd, (HMENU)(LONG_PTR)ID_I_ROUND, mod, NULL);
    i_roundud = CreateWindowExW(0, UPDOWN_CLASSW, L"", WS_CHILD | UDS_ARROWKEYS | UDS_SETBUDDYINT, 0, 0, 10, 10, hwnd, NULL, mod, NULL);
    SendMessageW(i_roundud, UDM_SETBUDDY, (WPARAM)i_round, 0);
    SendMessageW(i_roundud, UDM_SETRANGE, 0, MAKELONG(99, 1));
    SendMessageW(i_roundud, UDM_SETPOS, 0, MAKELONG(1, 0));
    i_chk = CreateWindowExW(0, L"BUTTON", S.showOrig, WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 10, 10, hwnd, (HMENU)(LONG_PTR)ID_I_CHKORIG, mod, NULL);
    i_pic = CreateWindowExW(WS_EX_CLIENTEDGE, L"ImgEncryptPicBox", L"", WS_CHILD, 0, 0, 10, 10, hwnd, NULL, mod, NULL);
    i_status = CreateWindowExW(0, L"STATIC", S.imgStatusInit, WS_CHILD | SS_LEFT, 0, 0, 10, 10, hwnd, (HMENU)(LONG_PTR)ID_I_STATUS, mod, NULL);
    i_zoom = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_RIGHT, 0, 0, 10, 10, hwnd, (HMENU)(LONG_PTR)ID_I_ZOOM, mod, NULL);
}

// ====== 图片预览框窗口过程 ======
static LRESULT CALLBACK PicWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            i_draw(hwnd, hdc);
            EndPaint(hwnd, &ps);
        }
        return 0;
      case WM_SIZE:
          InvalidateRect(hwnd, NULL, TRUE);
          i_update_zoom();
          return 0;
    case WM_ERASEBKGND:
        return 1;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

// ====== 统一字体应用 ======
static void apply_font_all(void)
{
    // Tab
    SendMessageW(g_tabText, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_tabImage, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    // 文本
    SendMessageW(t_lblKey, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(t_lblRound, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(t_lblIn, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(t_lblOut, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(t_key, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(t_round, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(t_roundud, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(t_input, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(t_output, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(t_status, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    for (int i = 0; i < 5; i++) SendMessageW(t_btns[i], WM_SETFONT, (WPARAM)g_hFont, TRUE);
    // 图片
    SendMessageW(i_open, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(i_enc, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(i_dec, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(i_save, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(i_lblKey, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(i_key, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(i_lblRound, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(i_round, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(i_roundud, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(i_chk, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(i_status, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(i_zoom, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(i_pic, WM_SETFONT, (WPARAM)g_hFont, TRUE);
}

static void apply_lang(void)
{
    SetWindowTextW(g_hwndMain, S.windowTitle);
    SetWindowTextW(g_tabText, S.tabText);
    SetWindowTextW(g_tabImage, S.tabImage);
    SetWindowTextW(g_langLabel, S.langLabel);

    set_text(t_lblKey, S.key);
    set_text(t_lblRound, S.rounds);
    set_text(t_lblIn, S.inputText);
    set_text(t_lblOut, S.outputResult);
    set_text(t_btns[0], S.encrypt);
    set_text(t_btns[1], S.decrypt);
    set_text(t_btns[2], S.copy);
    set_text(t_btns[3], S.swap);
    set_text(t_btns[4], S.clear);
    set_text(t_status, S.ready);

    set_text(i_open, S.openImage);
    set_text(i_enc, S.encrypt);
    set_text(i_dec, S.decrypt);
    set_text(i_save, S.savePng);
    set_text(i_lblKey, S.key);
    set_text(i_lblRound, S.rounds);
    set_text(i_chk, S.showOrig);
    set_text(i_status, S.imgStatusInit);
}

// ====== 主窗口过程 ======
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        // Tab
        case ID_TAB_TEXT: switch_tab(0); break;
        case ID_TAB_IMAGE: switch_tab(1); break;
        case ID_LANG_COMBO:
            if (HIWORD(wp) == CBN_SELCHANGE)
            {
                int sel = (int)SendMessageW(g_langCombo, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel <= LANG_EN)
                {
                    g_lang = sel;
                    save_lang();
                    apply_lang();
                    InvalidateRect(g_hwndMain, NULL, TRUE);
                }
            }
            break;
        // 文本
        case ID_T_ENC: t_transform(1); break;
        case ID_T_DEC: t_transform(0); break;
        case ID_T_COPY: t_copy(); break;
        case ID_T_SWAP: t_swap(); break;
        case ID_T_CLEAR: t_clear(); break;
        // 图片
        case ID_I_OPEN: i_open_file(); break;
        case ID_I_ENC: i_transform(1); break;
        case ID_I_DEC: i_transform(0); break;
        case ID_I_SAVE: i_save_file(); break;
        case ID_I_CHKORIG:
            i_show_orig = (SendMessageW(i_chk, BM_GETCHECK, 0, 0) == BST_CHECKED);
            InvalidateRect(i_pic, NULL, TRUE);
            break;
        }
        break;

    case WM_DROPFILES:
        {
            // 拖拽文件放入：切换到图片 Tab 并加载
            HDROP hDrop = (HDROP)wp;
            wchar_t path[1024];
            UINT n = DragQueryFileW(hDrop, 0, path, 1024);
            if (n > 0)
            {
                if (g_active_tab != 1) switch_tab(1);
                i_load(path);
            }
            DragFinish(hDrop);
        }
        break;

    case WM_SIZE:
        {
            RECT cr; GetClientRect(hwnd, &cr);
            int cw = cr.right - cr.left;
            MoveWindow(g_langLabel, cw - px(170), px(12), px(70), px(22), TRUE);
            MoveWindow(g_langCombo, cw - px(96), px(10), px(90), px(200), TRUE);
            if (g_active_tab == 0) layout_text(hwnd);
            else layout_image(hwnd);
        }
        break;

    case WM_DPICHANGED:
        {
            g_dpi = (int)LOWORD(wp);
            if (g_hFont) DeleteObject(g_hFont);
            g_hFont = make_font(g_active_tab == 0 ? T_FONT : I_FONT);
            apply_font_all();
            RECT *prc = (RECT *)lp;
            SetWindowPos(hwnd, NULL, prc->left, prc->top,
                prc->right - prc->left, prc->bottom - prc->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            if (g_active_tab == 0) layout_text(hwnd); else layout_image(hwnd);
        }
        break;

    case WM_MOUSEMOVE:
        if (g_active_tab == 0 && t_dragging)
        {
            int mouse_y = (int)GET_Y_LPARAM(lp);
            int delta_phys = mouse_y - t_dragStartY;
            int delta_logical = MulDiv(delta_phys, 96, g_dpi);
            int new_logical = t_splitY + delta_logical;
            RECT rc2; GetClientRect(hwnd, &rc2);
            int H_logical = MulDiv(rc2.bottom - rc2.top, 96, g_dpi);
            int minSplit_logical = T_MIN_SPLIT;
            int max_logical = H_logical - 22 - 20 - 2 - minSplit_logical - 4;
            if (max_logical < minSplit_logical) max_logical = minSplit_logical;
            if (new_logical < minSplit_logical) new_logical = minSplit_logical;
            if (new_logical > max_logical) new_logical = max_logical;
            if (new_logical != t_splitY)
            {
                t_splitY = new_logical;
                t_dragStartY = mouse_y;
                layout_text(hwnd);
            }
        }
        break;

    case WM_LBUTTONDOWN:
        if (g_active_tab == 0)
        {
            POINT pt = { (int)GET_X_LPARAM(lp), (int)GET_Y_LPARAM(lp) };
            RECT rc; GetClientRect(t_split, &rc);
            MapWindowPoints(t_split, hwnd, (LPPOINT)&rc, 2);
            int hot = px(6);
            RECT hot_rc = { rc.left, rc.top - hot, rc.right, rc.bottom + hot };
            if (pt.x >= hot_rc.left && pt.x <= hot_rc.right && pt.y >= hot_rc.top && pt.y <= hot_rc.bottom)
            {
                t_dragging = 1;
                SetCapture(hwnd);
                t_dragStartY = pt.y;
            }
        }
        break;

    case WM_LBUTTONUP:
        if (t_dragging) { t_dragging = 0; ReleaseCapture(); }
        break;

    case WM_SETCURSOR:
        if (g_active_tab == 0)
        {
            // 悬停分割条时显示调整光标；否则交给系统处理（含边框 resize 光标）
            POINT pt; GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            RECT rc; GetClientRect(t_split, &rc);
            MapWindowPoints(t_split, hwnd, (LPPOINT)&rc, 2);
            int hot = px(6);
            RECT hot_rc = { rc.left, rc.top - hot, rc.right, rc.bottom + hot };
            if (pt.x >= hot_rc.left && pt.x <= hot_rc.right && pt.y >= hot_rc.top && pt.y <= hot_rc.bottom)
            {
                SetCursor(LoadCursorW(NULL, (LPCWSTR)MAKEINTRESOURCEW(IDC_SIZENS)));
                return TRUE;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        return DefWindowProcW(hwnd, msg, wp, lp);

    case WM_DESTROY:
        free(i_pixels); free(i_orig);
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

// ====== DPI 感知 ======
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
    load_lang();

    // --selftest 支持
    for (const wchar_t *p = GetCommandLineW(); *p; p++)
    {
        if (p[0] == L'-' && wcsncmp(p, L"--selftest", 10) == 0 &&
            (p[10] == 0 || p[10] == L' ' || p[10] == L'\t'))
        {
            int r = crypto_selftest() ? 0 : 1;
            ExitProcess((UINT)r);
        }
    }

    int demo = 0;
    for (char *p = lpCmd; p && *p; p++)
        if (strstr(p, "--demo")) demo = 1;

    // 公共控件
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_UPDOWN_CLASSES };
    InitCommonControlsEx(&icc);

    // 注册图片预览类
    WNDCLASSEXW wcPic = {0};
    wcPic.cbSize = sizeof(wcPic);
    wcPic.lpfnWndProc = PicWndProc;
    wcPic.hInstance = hInstance;
    wcPic.hCursor = LoadCursorW(NULL, (LPCWSTR)MAKEINTRESOURCEW(IDC_ARROW));
    wcPic.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wcPic.lpszClassName = L"ImgEncryptPicBox";
    RegisterClassExW(&wcPic);

    // 注册主窗口类
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)MAKEINTRESOURCEW(IDC_ARROW));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"AllEncryptApp";
    wc.hIcon = LoadIconW(hInstance, (LPCWSTR)MAKEINTRESOURCEW(1));
      wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    // 初始 DPI
    {
        typedef UINT(WINAPI *GetDpiForSystem_t)(void);
        HMODULE hU = GetModuleHandleW(L"user32.dll");
        if (hU)
        {
            GetDpiForSystem_t p = (GetDpiForSystem_t)GetProcAddress(hU, "GetDpiForSystem");
            if (p) g_dpi = (int)p();
        }
    }

    int win_w = px(960), win_h = px(720);
    g_hwndMain = CreateWindowExW(0, L"AllEncryptApp", S.windowTitle,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, win_w, win_h,
        NULL, NULL, hInstance, NULL);

    // 接受拖拽文件放入
    DragAcceptFiles(g_hwndMain, TRUE);

    // ===== Tab 按钮 =====
    g_tabText = CreateWindowExW(0, L"BUTTON", S.tabText, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 10, 10, g_hwndMain, (HMENU)(LONG_PTR)ID_TAB_TEXT, hInstance, NULL);
    g_tabImage = CreateWindowExW(0, L"BUTTON", S.tabImage, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 10, 10, g_hwndMain, (HMENU)(LONG_PTR)ID_TAB_IMAGE, hInstance, NULL);

    // ===== 语言选择 =====
    g_langLabel = CreateWindowExW(0, L"STATIC", S.langLabel, WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 10, 10, g_hwndMain, NULL, hInstance, NULL);
    g_langCombo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 10, 200, g_hwndMain, (HMENU)(LONG_PTR)ID_LANG_COMBO, hInstance, NULL);
    SendMessageW(g_langCombo, CB_ADDSTRING, 0, (LPARAM)L"\u7b80\u4f53\u4e2d\u6587");
    SendMessageW(g_langCombo, CB_ADDSTRING, 0, (LPARAM)L"\u7e41\u9ad4\u4e2d\u6587");
    SendMessageW(g_langCombo, CB_ADDSTRING, 0, (LPARAM)L"English");
    SendMessageW(g_langCombo, CB_SETCURSEL, (WPARAM)g_lang, 0);

    // ===== 文本工具控件 =====
    create_text_controls(g_hwndMain);
    // ===== 图片工具控件 =====
    create_image_controls(g_hwndMain);

    // 字体
    g_hFont = make_font(T_FONT);
    apply_font_all();

    // 初始状态
    EnableWindow(i_enc, FALSE);
    EnableWindow(i_dec, FALSE);
    EnableWindow(i_save, FALSE);

    // 初始布局（含 Tab 位置）
    RECT cr; GetClientRect(g_hwndMain, &cr);
    int cw = cr.right - cr.left;
    MoveWindow(g_tabText, px(8), px(8), px(140), px(30), TRUE);
    MoveWindow(g_tabImage, px(156), px(8), px(140), px(30), TRUE);
    MoveWindow(g_langLabel, cw - px(170), px(12), px(70), px(22), TRUE);
            MoveWindow(g_langCombo, cw - px(96), px(10), px(90), px(200), TRUE);
    SendMessageW(g_langLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessageW(g_langCombo, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    switch_tab(0);
    layout_text(g_hwndMain);

    if (demo)
    {
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
            free(i_pixels); free(i_orig);
            i_pixels = px;
            i_orig = (uint8_t *)malloc((size_t)dw * dh * 4);
            if (i_orig) memcpy(i_orig, px, (size_t)dw * dh * 4);
            i_w = dw; i_h = dh;
            i_dirty = 0;
            EnableWindow(i_enc, TRUE);
            EnableWindow(i_dec, TRUE);
            switch_tab(1);
            layout_image(g_hwndMain);
            set_status(S.imgLoaded);
            InvalidateRect(i_pic, NULL, TRUE);
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
