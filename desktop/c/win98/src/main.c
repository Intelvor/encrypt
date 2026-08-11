#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include "crypto.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#ifndef ICC_UPDOWN_CLASSES
#define ICC_UPDOWN_CLASSES 0x00000400
#endif
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

typedef struct DPI_AWARENESS_CONTEXT__ *DPI_AWARENESS_CONTEXT;
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)(2))
#endif

#define ID_BTN_ENCRYPT  1001
#define ID_BTN_DECRYPT  1002
#define ID_BTN_COPY     1003
#define ID_BTN_SWAP     1004
#define ID_BTN_CLEAR    1005
#define ID_KEY_EDIT     1006
#define ID_ROUND_UPDOWN 1007
#define ID_ROUND_EDIT   1008
#define ID_INPUT_EDIT   1009
#define ID_OUTPUT_EDIT  1010
#define ID_STATUS       1011
#define ID_SPLITTER     1012

// ====== DPI ======
static int g_dpi = 96;
static int px(int logical) { return MulDiv(logical, g_dpi, 96); }

static HWND g_hwndMain;
static HWND g_lblKey, g_lblRound;
static HWND g_lblInputTitle, g_lblOutputTitle;
static HWND g_hwndKey, g_hwndRoundEdit, g_hwndRoundUpDown;
static HWND g_hwndInput, g_hwndOutput;
static HWND g_hwndSplitter, g_hwndStatus;
static HWND g_btns[5];
static HFONT g_hFont;
static int g_splitterY_Logical = 0;
static int g_minSplit = 80;
static int g_dragging = 0;
static int g_dragStartMouseY = 0;    // 按下时的鼠标 y（物理）
static int g_dragStartSplitterY = 0; // 按下时的 splitter 物理 y（主窗口客户区坐标）

static void set_text(HWND h, const wchar_t *s) { SetWindowTextW(h, s ? s : L""); }

static wchar_t *get_text(HWND h)
{
    int len = GetWindowTextLengthW(h);
    wchar_t *buf = (wchar_t *)malloc(((size_t)len + 1) * sizeof(wchar_t));
    if (!buf) return NULL;
    GetWindowTextW(h, buf, len + 1);
    return buf;
}

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

static HFONT make_font(int pt_px_logical)
{
    int h = -px(pt_px_logical);
    return CreateFontW(h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
}

static const int FONT_SIZE = 12;   // 逻辑像素，14→12 小一档
static const int BTN_H = 24;       // 按钮/输入框高度（逻辑）
static const int BTN_W = 68;       // 按钮宽度
static const int TITLE_H = 16;     // 小标题高度

static void set_status(const wchar_t *s) { SetWindowTextW(g_hwndStatus, s ? s : L""); }

static void do_transform(int encrypt)
{
    wchar_t *key = get_text(g_hwndKey);
    if (!key || !key[0]) { set_status(L"请输入密钥"); free(key); return; }
    wchar_t *text = get_text(g_hwndInput);
    if (!text || !text[0]) { set_status(L"请输入文本"); free(key); free(text); return; }
    int rounds = (int)SendMessageW(g_hwndRoundUpDown, UDM_GETPOS, 0, 0);
    if (rounds < 1) rounds = 1;
    wchar_t *result = encrypt ? crypto_encrypt(text, key, rounds) : crypto_decrypt(text, key, rounds);
    wchar_t *win_text = to_win_lf(result);
    set_text(g_hwndOutput, win_text);
    set_status(encrypt ? L"加密完成" : L"解密完成");
    free(key); free(text); free(result); free(win_text);
}

static void do_copy(void)
{
    wchar_t *out = get_text(g_hwndOutput);
    if (!out || !out[0]) { set_status(L"没有可复制的内容"); free(out); return; }
    if (OpenClipboard(g_hwndMain))
    {
        EmptyClipboard();
        size_t sz = (wcslen(out) + 1) * sizeof(wchar_t);
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, sz);
        if (h) { wchar_t *d = (wchar_t *)GlobalLock(h); memcpy(d, out, sz); GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); }
        CloseClipboard();
    }
    set_status(L"已复制到剪贴板");
    free(out);
}

static void do_swap(void)
{
    wchar_t *out = get_text(g_hwndOutput);
    if (!out || !out[0]) { set_status(L"没有可回填的内容"); free(out); return; }
    set_text(g_hwndInput, out);
    set_text(g_hwndOutput, L"");
    set_status(L"已回填到输入框");
    free(out); SetFocus(g_hwndInput);
}

static void do_clear(void)
{
    set_text(g_hwndKey, L"mimo");
    set_text(g_hwndInput, L"");
    set_text(g_hwndOutput, L"");
    SendMessageW(g_hwndRoundUpDown, UDM_SETPOS, 0, MAKELONG(1, 0));
    set_status(L"已清空");
    SetFocus(g_hwndInput);
}

// ====== 创建所有子控件 ======
static void create_children(HWND hwnd)
{
    HMODULE mod = GetModuleHandleW(NULL);

    // 小标题
    g_lblInputTitle = CreateWindowExW(0, L"STATIC", L"输入文本", WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, px(80), px(TITLE_H), hwnd, NULL, mod, NULL);
    g_lblOutputTitle = CreateWindowExW(0, L"STATIC", L"输出结果", WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, px(80), px(TITLE_H), hwnd, NULL, mod, NULL);

    // 顶栏标签
    g_lblKey = CreateWindowExW(0, L"STATIC", L"密钥", WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, px(34), px(BTN_H), hwnd, NULL, mod, NULL);
    g_lblRound = CreateWindowExW(0, L"STATIC", L"轮次", WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, px(34), px(BTN_H), hwnd, NULL, mod, NULL);

    // 密钥输入
    g_hwndKey = CreateWindowExW(0, L"EDIT", L"mimo", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        0, 0, px(180), px(BTN_H), hwnd, (HMENU)(LONG_PTR)ID_KEY_EDIT, mod, NULL);

    // 轮次 + UpDown
    g_hwndRoundEdit = CreateWindowExW(0, L"EDIT", L"1", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        0, 0, px(60), px(BTN_H), hwnd, (HMENU)(LONG_PTR)ID_ROUND_EDIT, mod, NULL);
    g_hwndRoundUpDown = CreateWindowExW(0, UPDOWN_CLASSW, L"", WS_CHILD | WS_VISIBLE | UDS_ARROWKEYS | UDS_SETBUDDYINT,
        0, 0, px(16), px(BTN_H), hwnd, (HMENU)(LONG_PTR)ID_ROUND_UPDOWN, mod, NULL);
    SendMessageW(g_hwndRoundUpDown, UDM_SETBUDDY, (WPARAM)g_hwndRoundEdit, 0);
    SendMessageW(g_hwndRoundUpDown, UDM_SETRANGE, 0, MAKELONG(99, 1));
    SendMessageW(g_hwndRoundUpDown, UDM_SETPOS, 0, MAKELONG(1, 0));

    // 按钮
    int btn_ids[] = { ID_BTN_ENCRYPT, ID_BTN_DECRYPT, ID_BTN_COPY, ID_BTN_SWAP, ID_BTN_CLEAR };
    const wchar_t *btn_texts[] = { L"加密", L"解密", L"复制", L"回填", L"清空" };
    for (int i = 0; i < 5; i++)
    {
        g_btns[i] = CreateWindowExW(0, L"BUTTON", btn_texts[i], WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, px(BTN_W), px(BTN_H), hwnd, (HMENU)(LONG_PTR)btn_ids[i], mod, NULL);
    }

    // 输入框（多行）
    g_hwndInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        0, 0, px(100), px(100), hwnd, (HMENU)(LONG_PTR)ID_INPUT_EDIT, mod, NULL);

    // 分割条
    g_hwndSplitter = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        0, 0, px(100), px(4), hwnd, (HMENU)(LONG_PTR)ID_SPLITTER, mod, NULL);

    // 输出框（多行只读）
    g_hwndOutput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_WANTRETURN,
        0, 0, px(100), px(100), hwnd, (HMENU)(LONG_PTR)ID_OUTPUT_EDIT, mod, NULL);

    // 状态栏
    g_hwndStatus = CreateWindowExW(0, L"STATIC", L"就绪", WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, px(100), px(22), hwnd, (HMENU)(LONG_PTR)ID_STATUS, mod, NULL);

    // 统一字体
    g_hFont = make_font(FONT_SIZE);
    HWND all[] = {
        g_lblKey, g_lblRound, g_lblInputTitle, g_lblOutputTitle,
        g_hwndKey, g_hwndRoundEdit, g_hwndRoundUpDown,
        g_btns[0], g_btns[1], g_btns[2], g_btns[3], g_btns[4],
        g_hwndInput, g_hwndOutput, g_hwndStatus
    };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++)
        SendMessageW(all[i], WM_SETFONT, (WPARAM)g_hFont, TRUE);
}

// ====== 布局（顶栏自适应折行）======
static void layout_full(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;

    int padding = px(8);
    int status_h = px(22);
    int splitter_h = px(4);
    int minSplit = px(g_minSplit);

    // === 顶栏：先算顶栏第一行总宽 ===
    int btn_h = px(BTN_H);
    int btn_w = px(BTN_W);
    int btn_gap = px(6);
    int label_w = px(34);
    int label_gap = px(4);
    int key_input_w = px(180);
    int round_label_w = px(34);
    int round_w = px(60);
    int round_updown_w = px(16);
    int row_y_gap = px(6);   // 每行之间的垂直间隙

    // 顶栏第 0 行：密钥标签 + 密钥输入框 + 轮次标签 + 轮次框 + UpDown
    int row0_content = padding
        + label_w + label_gap
        + key_input_w + px(12)
        + round_label_w + label_gap
        + round_w + round_updown_w
        + px(16);

    // 按钮区域起始 x（顶栏第 0 行按钮起始 x = row0_content 末尾 + px(16)）
    int btn_start_x = row0_content;
    int btn_row_w = btn_w * 5 + btn_gap * 4;  // 5 个按钮一行的总宽度

    // 总可用宽度
    int usable_w = W - padding * 2;

    // 顶栏高度：看按钮是否一行放得下
    int top_row_h = btn_h + row_y_gap;  // 每行高度
    int top_bar_h;
    int btn_row1_count = 0;   // 第 0 行放几个按钮
    int btn_row2_count = 0;   // 第 1 行放几个按钮（0 = 没有第二行）

    if (btn_start_x + btn_row_w <= W - padding)
    {
        // 一行放得下
        btn_row1_count = 5;
        top_bar_h = top_row_h;  // 顶栏 = 一行
    }
    else
    {
        // 一行放不下，按钮折行
        btn_row1_count = 0; btn_row2_count = 0;
        int avail_row1 = (W - padding) - btn_start_x;
        int remaining = 5;
        // 第 0 行按钮：看能塞几个
        int cur_w = 0;
        for (int i = 0; i < 5 && remaining > 0; i++)
        {
            int add = btn_w + (i > 0 ? btn_gap : 0);
            if (cur_w + add > avail_row1 && btn_row1_count > 0) break;
            cur_w += add;
            btn_row1_count++;
            remaining--;
        }
        btn_row2_count = remaining;
        top_bar_h = top_row_h * 2 + row_y_gap;  // 两行
    }

    // === 小标题（顶栏下方） ===
    int title_h = px(TITLE_H);
    int title_gap = px(2);

    // === 中间区域 ===
    int mid_y = padding + top_bar_h + row_y_gap + title_h + title_gap;
    int mid_h = H - mid_y - padding - status_h - padding;

    int sp = px(g_splitterY_Logical);
    int max_split = mid_h - minSplit - splitter_h;
    if (max_split < minSplit) max_split = minSplit;
    if (sp < minSplit) sp = minSplit;
    if (sp > max_split) sp = max_split;

    int splitter_top = mid_y + sp;
    int output_top = splitter_top + splitter_h;

    // ================== 开始摆放 ==================
    int y = padding + row_y_gap / 2;

    // ---- 顶栏第 0 行：密钥 + 轮次 + (部分或全部按钮) ----
    int x = padding;
    MoveWindow(g_lblKey, x, y, label_w, btn_h, TRUE);
    x += label_w + label_gap;
    MoveWindow(g_hwndKey, x, y, key_input_w, btn_h, TRUE);
    x += key_input_w + px(12);

    MoveWindow(g_lblRound, x, y, round_label_w, btn_h, TRUE);
    x += round_label_w + label_gap;
    MoveWindow(g_hwndRoundEdit, x, y, round_w, btn_h, TRUE);
    MoveWindow(g_hwndRoundUpDown, x + round_w, y, round_updown_w, btn_h, TRUE);
    x += round_w + round_updown_w + px(16);

    // 第 0 行按钮
    for (int i = 0; i < btn_row1_count; i++)
    {
        MoveWindow(g_btns[i], x, y, btn_w, btn_h, TRUE);
        x += btn_w + btn_gap;
    }

    // ---- 顶栏第 1 行：剩余按钮（如果有） ----
    if (btn_row2_count > 0)
    {
        int y2 = y + btn_h + row_y_gap;
        x = padding;
        for (int i = btn_row1_count; i < 5; i++)
        {
            MoveWindow(g_btns[i], x, y2, btn_w, btn_h, TRUE);
            x += btn_w + btn_gap;
        }
    }

    // ---- 小标题 ----
    int title_y = padding + top_bar_h + row_y_gap;
    MoveWindow(g_lblInputTitle, padding, title_y, W - padding * 2, title_h, TRUE);

    // ---- 分割条上方：输入框 ----
    int input_y = title_y + title_h + title_gap;
    int input_h_real = splitter_top - input_y - px(2);
    MoveWindow(g_hwndInput, padding, input_y, W - padding * 2, input_h_real, TRUE);

    // ---- 分割条 ----
    MoveWindow(g_hwndSplitter, padding, splitter_top, W - padding * 2, splitter_h, TRUE);

    // ---- 输出小标题 ----
    int output_title_y = splitter_top + splitter_h + title_gap;
    MoveWindow(g_lblOutputTitle, padding, output_title_y, W - padding * 2, title_h, TRUE);

    // ---- 输出框 ----
    int output_y = output_title_y + title_h + title_gap;
    int output_h_real = (H - status_h - padding) - output_y;
    MoveWindow(g_hwndOutput, padding, output_y, W - padding * 2, output_h_real, TRUE);

    // ---- 状态栏 ----
    MoveWindow(g_hwndStatus, padding, H - status_h - padding, W - padding * 2, status_h, TRUE);
}

// ====== WndProc ======
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case ID_BTN_ENCRYPT: do_transform(1); break;
        case ID_BTN_DECRYPT: do_transform(0); break;
        case ID_BTN_COPY: do_copy(); break;
        case ID_BTN_SWAP: do_swap(); break;
        case ID_BTN_CLEAR: do_clear(); break;
        }
        break;

    case WM_SIZE:
        layout_full(hwnd);
        break;

    case WM_DPICHANGED:
    {
        int new_dpi = (int)LOWORD(wp);
        if (new_dpi != g_dpi)
        {
            g_dpi = new_dpi;
            if (g_hFont) DeleteObject(g_hFont);
            g_hFont = make_font(FONT_SIZE);
            HWND all[] = {
                g_lblKey, g_lblRound, g_lblInputTitle, g_lblOutputTitle,
                g_hwndKey, g_hwndRoundEdit, g_hwndRoundUpDown,
                g_btns[0], g_btns[1], g_btns[2], g_btns[3], g_btns[4],
                g_hwndInput, g_hwndOutput, g_hwndStatus
            };
            for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++)
                if (all[i]) SendMessageW(all[i], WM_SETFONT, (WPARAM)g_hFont, TRUE);

            RECT *prc = (RECT *)lp;
            SetWindowPos(hwnd, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            layout_full(hwnd);
        }
        break;
    }

    case WM_MOUSEMOVE:
    {
        if (!g_dragging)
        {
            POINT pt = { (int)GET_X_LPARAM(lp), (int)GET_Y_LPARAM(lp) };
            RECT rc; GetClientRect(g_hwndSplitter, &rc);
            MapWindowPoints(g_hwndSplitter, hwnd, (LPPOINT)&rc, 2);
            int hot = px(6);
            RECT hot_rc = { rc.left, rc.top - hot, rc.right, rc.bottom + hot };
            BOOL inside = (pt.x >= hot_rc.left && pt.x <= hot_rc.right && pt.y >= hot_rc.top && pt.y <= hot_rc.bottom);
            SetCursor(LoadCursorW(NULL, inside ? IDC_SIZENS : IDC_ARROW));
            break;
        }

        if (g_dragging)
        {
            int mouse_y = (int)GET_Y_LPARAM(lp);
            int delta_phys = mouse_y - g_dragStartMouseY;

            // 物理像素 delta → 逻辑像素 delta（与 layout_full 里 px() 反向对应）
            int delta_logical = MulDiv(delta_phys, 96, g_dpi);
            int new_logical = g_splitterY_Logical + delta_logical;

            // 边界保护：minSplit 和 max_split 都用逻辑像素
            int minSplit_logical = g_minSplit;  // g_minSplit 本身就是逻辑值 80
            // max_split = mid_h - minSplit - splitter_h
            // mid_h 在 layout_full 里 = H - mid_y - padding - status_h - padding
            // 但 mid_y 我们不知道...所以用简化版：new_logical 夹在 [minSplit, 大一点]
            // 其实最简单：夹在 [minSplit, H 对应的逻辑高度 - minSplit - 留一点]
            RECT rc2; GetClientRect(hwnd, &rc2);
            int H_logical = MulDiv(rc2.bottom - rc2.top, 96, g_dpi);
            // 逻辑坐标里：上限 = 总高 - 状态栏 - 小标题 - minSplit 下限 - 分割条
            // status_h、TITLE_H 等也用逻辑值
            int max_logical = H_logical - 22 - 20 - 2 - minSplit_logical - 4;
            if (max_logical < minSplit_logical) max_logical = minSplit_logical;

            if (new_logical < minSplit_logical) new_logical = minSplit_logical;
            if (new_logical > max_logical) new_logical = max_logical;

            if (new_logical != g_splitterY_Logical)
            {
                g_splitterY_Logical = new_logical;
                g_dragStartMouseY = mouse_y;        // 更新起点（等价于把 splitter"粘"回鼠标）
                // 不用更新 g_dragStartSplitterY，因为我们不用它了
                layout_full(hwnd);
            }
        }
        break;
    }
    case WM_LBUTTONDOWN:
    {
        POINT pt = { (int)GET_X_LPARAM(lp), (int)GET_Y_LPARAM(lp) };
        RECT rc; GetClientRect(g_hwndSplitter, &rc);
        MapWindowPoints(g_hwndSplitter, hwnd, (LPPOINT)&rc, 2);
        int hot = px(6);
        RECT hot_rc = { rc.left, rc.top - hot, rc.right, rc.bottom + hot };
        if (pt.x >= hot_rc.left && pt.x <= hot_rc.right && pt.y >= hot_rc.top && pt.y <= hot_rc.bottom)
        {
            g_dragging = 1;
            SetCapture(hwnd);
            SetCursor(LoadCursorW(NULL, IDC_SIZENS));
            g_dragStartMouseY = pt.y;
            g_dragStartSplitterY = rc.top;  // splitter 当前物理 y
        }
        break;
    }
    case WM_LBUTTONUP:
        if (g_dragging)
        {
            g_dragging = 0;
            ReleaseCapture();
            SetCursor(LoadCursorW(NULL, IDC_ARROW));
        }
        break;

    case WM_DESTROY:
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

// ====== DPI 感知初始化（WinMain 最开头调用）======
static void enable_dpi_awareness(void)
{
    typedef BOOL(WINAPI *SetProcessDpiAwarenessContext_t)(DPI_AWARENESS_CONTEXT);
    typedef BOOL(WINAPI *SetProcessDPIAware_t)(void);

    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (!hUser32) return;

    // 先试 Win10 1703+ 的 Per-Monitor V2
    SetProcessDpiAwarenessContext_t pC = (SetProcessDpiAwarenessContext_t)
        GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
    if (pC && pC(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return;

    // 再试 Win8.1 的 Per-Monitor V1
    SetProcessDPIAware_t pLegacy = (SetProcessDPIAware_t)
        GetProcAddress(hUser32, "SetProcessDPIAware");
    if (pLegacy) pLegacy();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    // ⚠️ DPI 感知必须在任何 GDI/窗口类操作之前
    enable_dpi_awareness();

    // 命令行参数 --selftest（手写解析，避免引入 SHELL32 依赖）
    for (const wchar_t *p = GetCommandLineW(); *p; p++)
    {
        if (p[0] == L'-' && wcsncmp(p, L"--selftest", 10) == 0 &&
            (p[10] == 0 || p[10] == L' ' || p[10] == L'\t'))
        {
            int r = crypto_selftest() ? 0 : 1;
            ExitProcess((UINT)r);
        }
    }

    // 公共控件
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_UPDOWN_CLASSES };
    InitCommonControlsEx(&icc);

    // 注册窗口类
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"TextEncryptApp";
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    // 创建主窗口（逻辑尺寸 720x560，系统按当前 DPI 放大）
    g_hwndMain = CreateWindowExW(0, L"TextEncryptApp", L"文本加解密工具",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 720, 560,
        NULL, NULL, hInstance, NULL);

    // 获取真实 DPI
    {
        typedef UINT(WINAPI *GetDpiForWindow_t)(HWND);
        HMODULE hU = GetModuleHandleW(L"user32.dll");
        if (hU)
        {
            GetDpiForWindow_t p = (GetDpiForWindow_t)GetProcAddress(hU, "GetDpiForWindow");
            if (p) g_dpi = (int)p(g_hwndMain);
        }
    }

    // 创建所有子控件
    create_children(g_hwndMain);

    ShowWindow(g_hwndMain, nShow);
    UpdateWindow(g_hwndMain);

    // 首次分割条位置：中间（逻辑像素）
    RECT rc; GetClientRect(g_hwndMain, &rc);
    int H = rc.bottom - rc.top;
    int padding = px(8), status_h = px(22), splitter_h = px(4), minSplit = px(g_minSplit);
    int top_bar_h = px(BTN_H) + px(12);  // 估算一行顶栏 + 小标题
    int mid_y = padding + top_bar_h + px(6) + px(TITLE_H) + px(2);
    int mid_h = H - mid_y - padding - status_h - padding;
    int half = mid_h / 2 - px(10);  // 留一点给小标题
    if (half < minSplit) half = minSplit;
    g_splitterY_Logical = MulDiv(half, 96, g_dpi);
    layout_full(g_hwndMain);

    // 消息循环
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    ExitProcess((UINT)msg.wParam);  // 显式退出，兼容 -nostartfiles 无 CRT 清理的构建
    return 0;
}

