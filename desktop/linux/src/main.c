// main.c - Linux GTK3 版本：文本 + 图片加解密工具
#include <gtk/gtk.h>
#include <gdk/gdkpixbuf.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <locale.h>
#include <time.h>
#include "crypto.h"
#include "crypto_img.h"

// ====== 多语言 ======
enum { LANG_ZH_CN = 0, LANG_ZH_TW = 1, LANG_EN = 2 };
static int g_lang = LANG_EN;

static int detect_system_lang(void)
{
    const gchar * const *langs = g_get_language_names();
    if (langs && langs[0])
    {
        const gchar *l = langs[0];
        if (g_str_has_prefix(l, "zh"))
        {
            if (g_str_has_prefix(l, "zh-TW") || g_str_has_prefix(l, "zh-HK") ||
                g_str_has_prefix(l, "zh-MO") || g_str_has_prefix(l, "zh-Hant"))
                return LANG_ZH_TW;
            return LANG_ZH_CN;
        }
    }
    return LANG_EN;  // 其他语言一律回退 English
}

typedef struct {
    const char *windowTitle;
    const char *tabText;
    const char *tabImage;
    const char *key;
    const char *rounds;
    const char *inputText;
    const char *outputResult;
    const char *encrypt;
    const char *decrypt;
    const char *copy;
    const char *swap;
    const char *clear;
    const char *paste;
    const char *ready;
    const char *enterKey;
    const char *enterText;
    const char *encDone;
    const char *decDone;
    const char *noCopy;
    const char *copied;
    const char *noSwap;
    const char *swapped;
    const char *cleared;
    const char *openImage;
    const char *savePng;
    const char *showOrig;
    const char *imgStatusInit;
    const char *imgNotValid;
    const char *imgLoaded;
    const char *imgEnterKey;
    const char *imgOpenFirst;
    const char *imgSaved;
    const char *imgSaveFail;
    const char *langLabel;
} LangStrings;

static const LangStrings g_strings[] = {
    { // zh-CN
        "加解密工具",
        "文本加解密", "图片加解密",
        "密钥", "轮次", "输入文本", "输出结果",
        "加密", "解密", "复制", "回填", "清空", "粘贴",
        "就绪",
        "请输入密钥", "请输入文本",
        "加密完成", "解密完成",
        "没有可复制的内容", "已复制到剪贴板",
        "没有可回填的内容", "已回填到输入框",
        "已清空",
        "打开图片", "保存 PNG", "显示原图",
        "点击「打开图片」或拖入图片文件（PNG/JPG/BMP）",
        "不是有效的图片",
        "已加载",
        "请输入密钥",
        "请先打开图片",
        "已保存",
        "保存失败",
        "语言"
    },
    { // zh-TW
        "加解密工具",
        "文字加解密", "圖片加解密",
        "密鑰", "輪次", "輸入文字", "輸出結果",
        "加密", "解密", "複製", "回填", "清空", "貼上",
        "就緒",
        "請輸入密鑰", "請輸入文字",
        "加密完成", "解密完成",
        "沒有可複製的內容", "已複製到剪貼簿",
        "沒有可回填的內容", "已回填到輸入框",
        "已清空",
        "開啟圖片", "儲存 PNG", "顯示原圖",
        "點擊「開啟圖片」或拖入圖片檔案（PNG/JPG/BMP）",
        "不是有效的圖片",
        "已載入",
        "請輸入密鑰",
        "請先開啟圖片",
        "已儲存",
        "儲存失敗",
        "語言"
    },
    { // en
        "Encrypt & Decrypt",
        "Text Encrypt", "Image Encrypt",
        "Key", "Rounds", "Input Text", "Output",
        "Encrypt", "Decrypt", "Copy", "Swap", "Clear", "Paste",
        "Ready",
        "Please enter a key", "Please enter text",
        "Encryption done", "Decryption done",
        "Nothing to copy", "Copied to clipboard",
        "Nothing to swap", "Swapped to input",
        "Cleared",
        "Open Image", "Save PNG", "Show Original",
        "Click \"Open Image\" or drag image file (PNG/JPG/BMP)",
        "Not a valid image",
        "Image loaded",
        "Please enter a key",
        "Please open an image first",
        "Saved",
        "Save failed",
        "Language"
    }
};

#define S (g_strings[g_lang])

// ====== 全局控件 ======
static GtkWidget *g_window;
static GtkNotebook *g_notebook;

// 文本工具
static GtkWidget *t_key, *t_round, *t_input, *t_output, *t_status;
static GtkWidget *t_btn_enc, *t_btn_dec, *t_btn_copy, *t_btn_swap, *t_btn_clear, *t_btn_paste;

// 图片工具
static GtkWidget *i_key, *i_round, *i_status, *i_image, *i_scroll;
static GtkWidget *i_btn_open, *i_btn_enc, *i_btn_dec, *i_btn_save, *i_btn_copy, *i_btn_clear;
static GtkWidget *i_chk_orig;
static GdkPixbuf *i_pixbuf = NULL;
static GdkPixbuf *i_orig_pixbuf = NULL;
static int i_w = 0, i_h = 0;
static int i_dirty = 0;
static int i_show_orig = 0;

// ====== 状态缓存（语言切换时重建） ======
enum { ST_NONE = 0, ST_READY, ST_TEXT_OP, ST_IMG_INIT, ST_IMG_LOADED, ST_IMG_OP };
enum { SMSG_NONE = 0, SMSG_ENTER_KEY, SMSG_ENTER_TEXT, SMSG_NO_COPY, SMSG_COPIED,
       SMSG_NO_SWAP, SMSG_SWAPPED, SMSG_CLEARED, SMSG_IMG_NOT_VALID, SMSG_IMG_OPEN_FIRST,
       SMSG_IMG_ENTER_KEY, SMSG_IMG_SAVE_FAIL, SMSG_IMG_SAVED, SMSG_IMG_LOADED, SMSG_NO_PASTE, SMSG_PASTED };
static struct {
    int type;
    int msg;
    int is_enc;
    int rounds;
    double ms;
    int w, h, kb;
} g_st = { ST_READY, 0, 0, 0, 0, 0, 0, 0 };

// ====== 工具函数 ======
static wchar_t *utf8_to_wchar(const char *utf8)
{
    if (!utf8) return NULL;
    int cap = strlen(utf8) + 1;
    wchar_t *out = (wchar_t *)malloc((size_t)cap * sizeof(wchar_t));
    if (!out) return NULL;
    int len = 0;
    const unsigned char *s = (const unsigned char *)utf8;
    while (*s) {
        unsigned int cp;
        if (*s < 0x80) { cp = *s++; }
        else if ((*s & 0xE0) == 0xC0) { cp = (*s++ & 0x1F) << 6; cp |= (*s++ & 0x3F); }
        else if ((*s & 0xF0) == 0xE0) { cp = (*s++ & 0x0F) << 12; cp |= (*s++ & 0x3F) << 6; cp |= (*s++ & 0x3F); }
        else { cp = (*s++ & 0x07) << 18; cp |= (*s++ & 0x3F) << 12; cp |= (*s++ & 0x3F) << 6; cp |= (*s++ & 0x3F); }
        out[len++] = (wchar_t)cp;
    }
    out[len] = 0;
    return out;
}

static char *wchar_to_utf8(const wchar_t *w)
{
    if (!w) return NULL;
    int cap = (int)wcslen(w) * 4 + 1;
    char *out = (char *)malloc((size_t)cap);
    if (!out) return NULL;
    int len = 0;
    for (; *w; w++) {
        unsigned int cp = (unsigned int)*w;
        if (cp < 0x80) { out[len++] = (char)cp; }
        else if (cp < 0x800) { out[len++] = 0xC0 | (cp >> 6); out[len++] = 0x80 | (cp & 0x3F); }
        else if (cp < 0x10000) { out[len++] = 0xE0 | (cp >> 12); out[len++] = 0x80 | ((cp >> 6) & 0x3F); out[len++] = 0x80 | (cp & 0x3F); }
        else { out[len++] = 0xF0 | (cp >> 18); out[len++] = 0x80 | ((cp >> 12) & 0x3F); out[len++] = 0x80 | ((cp >> 6) & 0x3F); out[len++] = 0x80 | (cp & 0x3F); }
    }
    out[len] = 0;
    return out;
}

static void set_status(GtkWidget *status, const char *msg)
{
    gtk_label_set_text(GTK_LABEL(status), msg);
}

static const char *smsg_text(int msg)
{
    switch (msg)
    {
    case SMSG_ENTER_KEY:      return S.enterKey;
    case SMSG_ENTER_TEXT:     return S.enterText;
    case SMSG_NO_COPY:        return S.noCopy;
    case SMSG_COPIED:         return S.copied;
    case SMSG_NO_SWAP:        return S.noSwap;
    case SMSG_SWAPPED:        return S.swapped;
    case SMSG_CLEARED:        return S.cleared;
    case SMSG_IMG_NOT_VALID:  return S.imgNotValid;
    case SMSG_IMG_OPEN_FIRST: return S.imgOpenFirst;
    case SMSG_IMG_ENTER_KEY:  return S.imgEnterKey;
    case SMSG_IMG_SAVE_FAIL:  return S.imgSaveFail;
    case SMSG_IMG_SAVED:      return S.imgSaved;
    case SMSG_IMG_LOADED:     return S.imgLoaded;
    case SMSG_NO_PASTE:       return S.noCopy;
    case SMSG_PASTED:         return S.paste;
    default:                  return S.ready;
    }
}

static void rebuild_status(void)
{
    GtkWidget *target = (g_notebook && gtk_notebook_get_current_page(g_notebook) == 0) ? t_status : i_status;
    switch (g_st.type)
    {
    case ST_READY:
        set_status(target, S.ready);
        break;
    case ST_TEXT_OP:
        {
            char info[256];
            const char *op = g_st.is_enc ? S.encDone : S.decDone;
            if (g_st.ms < 1000.0)
                snprintf(info, sizeof(info), "%s (%d %s) | %.0f ms", op, g_st.rounds, S.rounds, g_st.ms);
            else
                snprintf(info, sizeof(info), "%s (%d %s) | %.2f s", op, g_st.rounds, S.rounds, g_st.ms / 1000.0);
            set_status(t_status, info);
        }
        break;
    case ST_IMG_INIT:
        set_status(i_status, S.imgStatusInit);
        break;
    case ST_IMG_LOADED:
        {
            char info[256];
            if (g_st.ms < 1000.0)
                snprintf(info, sizeof(info), "%d x %d (%d KB) | %.0f ms", g_st.w, g_st.h, g_st.kb, g_st.ms);
            else
                snprintf(info, sizeof(info), "%d x %d (%d KB) | %.2f s", g_st.w, g_st.h, g_st.kb, g_st.ms / 1000.0);
            set_status(i_status, info);
        }
        break;
    case ST_IMG_OP:
        {
            char info[256];
            const char *op = g_st.is_enc ? S.encDone : S.decDone;
            if (g_st.ms < 1000.0)
                snprintf(info, sizeof(info), "%s (%d) | %d x %d | %.0f ms", op, g_st.rounds, g_st.w, g_st.h, g_st.ms);
            else
                snprintf(info, sizeof(info), "%s (%d) | %d x %d | %.2f s", op, g_st.rounds, g_st.w, g_st.h, g_st.ms / 1000.0);
            set_status(i_status, info);
        }
        break;
    default:
        if (g_st.msg) set_status(target, smsg_text(g_st.msg));
        break;
    }
}

static double get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

// ====== 文本加解密 ======
static void on_text_encrypt(GtkWidget *widget, gpointer data)
{
    const char *key_utf8 = gtk_entry_get_text(GTK_ENTRY(t_key));
    if (!key_utf8 || !key_utf8[0]) {
        g_st.type = ST_NONE; g_st.msg = SMSG_ENTER_KEY;
        set_status(t_status, S.enterKey);
        return;
    }
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(t_input));
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buf, &start);
    gtk_text_buffer_get_end_iter(buf, &end);
    char *text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
    if (!text || !text[0]) {
        g_st.type = ST_NONE; g_st.msg = SMSG_ENTER_TEXT;
        set_status(t_status, S.enterText);
        g_free(text);
        return;
    }

    wchar_t *wtext = utf8_to_wchar(text);
    wchar_t *wkey = utf8_to_wchar(key_utf8);
    int rounds = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(t_round));
    if (rounds < 1) rounds = 1;

    double t0 = get_time_ms();
    wchar_t *result = crypto_encrypt(wtext, wkey, rounds);
    double dt = get_time_ms() - t0;

    char *result_utf8 = wchar_to_utf8(result);
    GtkTextBuffer *out_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(t_output));
    gtk_text_buffer_set_text(out_buf, result_utf8 ? result_utf8 : "", -1);

    char info[256];
    if (dt < 1000.0)
        snprintf(info, sizeof(info), "%s (%d %s) | %.0f ms", S.encDone, rounds, S.rounds, dt);
    else
        snprintf(info, sizeof(info), "%s (%d %s) | %.2f s", S.encDone, rounds, S.rounds, dt / 1000.0);
    set_status(t_status, info);
    g_st.type = ST_TEXT_OP; g_st.msg = SMSG_NONE; g_st.is_enc = 1; g_st.rounds = rounds; g_st.ms = dt;

    free(wtext); free(wkey); free(result);
    g_free(text); g_free(result_utf8);
}

static void on_text_decrypt(GtkWidget *widget, gpointer data)
{
    const char *key_utf8 = gtk_entry_get_text(GTK_ENTRY(t_key));
    if (!key_utf8 || !key_utf8[0]) {
        g_st.type = ST_NONE; g_st.msg = SMSG_ENTER_KEY;
        set_status(t_status, S.enterKey);
        return;
    }
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(t_input));
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buf, &start);
    gtk_text_buffer_get_end_iter(buf, &end);
    char *text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
    if (!text || !text[0]) {
        g_st.type = ST_NONE; g_st.msg = SMSG_ENTER_TEXT;
        set_status(t_status, S.enterText);
        g_free(text);
        return;
    }

    wchar_t *wtext = utf8_to_wchar(text);
    wchar_t *wkey = utf8_to_wchar(key_utf8);
    int rounds = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(t_round));
    if (rounds < 1) rounds = 1;

    double t0 = get_time_ms();
    wchar_t *result = crypto_decrypt(wtext, wkey, rounds);
    double dt = get_time_ms() - t0;

    char *result_utf8 = wchar_to_utf8(result);
    GtkTextBuffer *out_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(t_output));
    gtk_text_buffer_set_text(out_buf, result_utf8 ? result_utf8 : "", -1);

    char info[256];
    if (dt < 1000.0)
        snprintf(info, sizeof(info), "%s (%d %s) | %.0f ms", S.decDone, rounds, S.rounds, dt);
    else
        snprintf(info, sizeof(info), "%s (%d %s) | %.2f s", S.decDone, rounds, S.rounds, dt / 1000.0);
    set_status(t_status, info);
    g_st.type = ST_TEXT_OP; g_st.msg = SMSG_NONE; g_st.is_enc = 0; g_st.rounds = rounds; g_st.ms = dt;

    free(wtext); free(wkey); free(result);
    g_free(text); g_free(result_utf8);
}

static void on_text_copy(GtkWidget *widget, gpointer data)
{
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(t_output));
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buf, &start);
    gtk_text_buffer_get_end_iter(buf, &end);
    char *text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
    if (!text || !text[0]) {
        g_st.type = ST_NONE; g_st.msg = SMSG_NO_COPY;
        set_status(t_status, S.noCopy);
        g_free(text);
        return;
    }
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, text, -1);
    g_st.type = ST_NONE; g_st.msg = SMSG_COPIED;
    set_status(t_status, S.copied);
    g_free(text);
}

static void on_text_swap(GtkWidget *widget, gpointer data)
{
    GtkTextBuffer *out_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(t_output));
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(out_buf, &start);
    gtk_text_buffer_get_end_iter(out_buf, &end);
    char *text = gtk_text_buffer_get_text(out_buf, &start, &end, FALSE);
    if (!text || !text[0]) {
        g_st.type = ST_NONE; g_st.msg = SMSG_NO_SWAP;
        set_status(t_status, S.noSwap);
        g_free(text);
        return;
    }
    GtkTextBuffer *in_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(t_input));
    gtk_text_buffer_set_text(in_buf, text, -1);
    gtk_text_buffer_set_text(out_buf, "", -1);
    g_st.type = ST_NONE; g_st.msg = SMSG_SWAPPED;
    set_status(t_status, S.swapped);
    g_free(text);
}

static void on_text_clear(GtkWidget *widget, gpointer data)
{
    gtk_entry_set_text(GTK_ENTRY(t_key), "mimo");
    GtkTextBuffer *in_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(t_input));
    gtk_text_buffer_set_text(in_buf, "", -1);
    GtkTextBuffer *out_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(t_output));
    gtk_text_buffer_set_text(out_buf, "", -1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(t_round), 1);
    g_st.type = ST_READY; g_st.msg = SMSG_NONE;
    set_status(t_status, S.cleared);
}

static void on_text_paste(GtkWidget *widget, gpointer data)
{
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    char *text = gtk_clipboard_wait_for_text(clipboard);
    if (!text) {
        g_st.type = ST_NONE; g_st.msg = SMSG_NO_COPY;
        set_status(t_status, S.noCopy);
        return;
    }
    GtkTextBuffer *in_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(t_input));
    gtk_text_buffer_set_text(in_buf, text, -1);
    g_st.type = ST_NONE; g_st.msg = SMSG_PASTED;
    set_status(t_status, S.paste);
    g_free(text);
}

// ====== 图片加解密 ======
static void update_image_display(void)
{
    if (!i_pixbuf) {
        gtk_widget_set_visible(i_image, FALSE);
        return;
    }
    gtk_widget_set_visible(i_image, TRUE);

    GdkPixbuf *display = i_show_orig && i_orig_pixbuf ? i_orig_pixbuf : i_pixbuf;

    // 按滚动窗口可用尺寸缩放
    GtkAllocation alloc;
    gtk_widget_get_allocation(i_scroll, &alloc);
    int max_w = alloc.width - 4;
    int max_h = alloc.height - 4;
    if (max_w < 1) max_w = 1;
    if (max_h < 1) max_h = 1;

    int src_w = gdk_pixbuf_get_width(display);
    int src_h = gdk_pixbuf_get_height(display);

    if (src_w > max_w || src_h > max_h) {
        double scale_x = (double)max_w / src_w;
        double scale_y = (double)max_h / src_h;
        double scale = scale_x < scale_y ? scale_x : scale_y;
        int new_w = (int)(src_w * scale);
        int new_h = (int)(src_h * scale);
        if (new_w < 1) new_w = 1;
        if (new_h < 1) new_h = 1;
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple(display, new_w, new_h, GDK_INTERP_BILINEAR);
        gtk_image_set_from_pixbuf(GTK_IMAGE(i_image), scaled);
        g_object_unref(scaled);
    } else {
        gtk_image_set_from_pixbuf(GTK_IMAGE(i_image), display);
    }
}

static void on_image_open(GtkWidget *widget, gpointer data)
{
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        S.openImage,
        GTK_WINDOW(g_window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_filter_add_mime_type(filter, "image/jpeg");
    gtk_file_filter_add_mime_type(filter, "image/bmp");
    gtk_file_filter_add_mime_type(filter, "image/gif");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            GError *error = NULL;
            GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(filename, &error);
            if (pixbuf) {
                if (i_pixbuf) g_object_unref(i_pixbuf);
                if (i_orig_pixbuf) g_object_unref(i_orig_pixbuf);
                i_pixbuf = pixbuf;
                i_orig_pixbuf = gdk_pixbuf_copy(pixbuf);
                i_w = gdk_pixbuf_get_width(pixbuf);
                i_h = gdk_pixbuf_get_height(pixbuf);
                i_dirty = 0;
                i_show_orig = 0;
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(i_chk_orig), FALSE);
                update_image_display();

                char info[256];
                snprintf(info, sizeof(info), "%s: %d x %d", S.imgLoaded, i_w, i_h);
                set_status(i_status, info);
                g_st.type = ST_IMG_LOADED; g_st.msg = SMSG_NONE;
                g_st.w = i_w; g_st.h = i_h;
                g_st.kb = (int)((size_t)i_w * i_h * 4 / 1024);
                g_st.ms = 0;

                gtk_widget_set_sensitive(i_btn_enc, TRUE);
                gtk_widget_set_sensitive(i_btn_dec, TRUE);
                gtk_widget_set_sensitive(i_btn_save, FALSE);
            } else {
                g_st.type = ST_NONE; g_st.msg = SMSG_IMG_NOT_VALID;
                set_status(i_status, S.imgNotValid);
                g_error_free(error);
            }
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
}

static void on_image_transform(GtkWidget *widget, gpointer data)
{
    int enc = GPOINTER_TO_INT(data);
    if (!i_pixbuf) {
        g_st.type = ST_NONE; g_st.msg = SMSG_IMG_OPEN_FIRST;
        set_status(i_status, S.imgOpenFirst);
        return;
    }
    const char *key_utf8 = gtk_entry_get_text(GTK_ENTRY(i_key));
    if (!key_utf8 || !key_utf8[0]) {
        g_st.type = ST_NONE; g_st.msg = SMSG_IMG_ENTER_KEY;
        set_status(i_status, S.imgEnterKey);
        return;
    }

    int rounds = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(i_round));
    if (rounds < 1) rounds = 1;

    // 获取像素数据
    GdkPixbuf *pixbuf = i_show_orig && i_orig_pixbuf ? i_orig_pixbuf : i_pixbuf;
    int npx = i_w * i_h;
    uint8_t *pixels = gdk_pixbuf_get_pixels(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);

    // 转换为RGBA格式
    uint8_t *rgba = (uint8_t *)malloc((size_t)npx * 4);
    if (!rgba) return;

    for (int y = 0; y < i_h; y++) {
        uint8_t *row = pixels + y * rowstride;
        for (int x = 0; x < i_w; x++) {
            uint8_t *src = row + x * n_channels;
            uint8_t *dst = rgba + (y * i_w + x) * 4;
            dst[0] = src[0]; // R
            dst[1] = src[1]; // G
            dst[2] = src[2]; // B
            dst[3] = n_channels == 4 ? src[3] : 255; // A
        }
    }

    wchar_t *wkey = utf8_to_wchar(key_utf8);
    double t0 = get_time_ms();
    if (enc)
        img_encrypt(rgba, wkey, rounds, i_w, i_h);
    else
        img_decrypt(rgba, wkey, rounds, i_w, i_h);
    double dt = get_time_ms() - t0;
    free(wkey);

    // 更新像素
    for (int y = 0; y < i_h; y++) {
        uint8_t *row = pixels + y * rowstride;
        for (int x = 0; x < i_w; x++) {
            uint8_t *src = rgba + (y * i_w + x) * 4;
            uint8_t *dst = row + x * n_channels;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            if (n_channels == 4) dst[3] = src[3];
        }
    }
    free(rgba);

    i_dirty = 1;
    i_show_orig = 0;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(i_chk_orig), FALSE);
    update_image_display();
    gtk_widget_set_sensitive(i_btn_save, TRUE);

    char info[256];
    if (dt < 1000.0)
        snprintf(info, sizeof(info), "%s (%d) | %d x %d | %.0f ms",
                 enc ? S.encDone : S.decDone, rounds, i_w, i_h, dt);
    else
        snprintf(info, sizeof(info), "%s (%d) | %d x %d | %.2f s",
                 enc ? S.encDone : S.decDone, rounds, i_w, i_h, dt / 1000.0);
    set_status(i_status, info);
    g_st.type = ST_IMG_OP; g_st.msg = SMSG_NONE;
    g_st.is_enc = enc; g_st.rounds = rounds; g_st.ms = dt; g_st.w = i_w; g_st.h = i_h;
}

static void on_image_save(GtkWidget *widget, gpointer data)
{
    if (!i_pixbuf || !i_dirty) return;

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        S.savePng,
        GTK_WINDOW(g_window),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "PNG images");
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "output.png");

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            GError *error = NULL;
            if (gdk_pixbuf_save(i_pixbuf, filename, "png", &error, NULL)) {
                g_st.type = ST_NONE; g_st.msg = SMSG_IMG_SAVED;
                set_status(i_status, S.imgSaved);
            } else {
                g_st.type = ST_NONE; g_st.msg = SMSG_IMG_SAVE_FAIL;
                set_status(i_status, S.imgSaveFail);
                g_error_free(error);
            }
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
}

static void on_image_copy(GtkWidget *widget, gpointer data)
{
    if (!i_pixbuf) return;
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_image(clipboard, i_pixbuf);
    g_st.type = ST_NONE; g_st.msg = SMSG_COPIED;
    set_status(i_status, S.copied);
}

static void on_image_clear(GtkWidget *widget, gpointer data)
{
    if (i_pixbuf) { g_object_unref(i_pixbuf); i_pixbuf = NULL; }
    if (i_orig_pixbuf) { g_object_unref(i_orig_pixbuf); i_orig_pixbuf = NULL; }
    i_w = i_h = 0;
    i_dirty = 0;
    i_show_orig = 0;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(i_chk_orig), FALSE);
    update_image_display();
    gtk_widget_set_sensitive(i_btn_enc, FALSE);
    gtk_widget_set_sensitive(i_btn_dec, FALSE);
    gtk_widget_set_sensitive(i_btn_save, FALSE);
    g_st.type = ST_IMG_INIT; g_st.msg = SMSG_NONE;
    set_status(i_status, S.cleared);
}

static void on_orig_toggled(GtkWidget *widget, gpointer data)
{
    i_show_orig = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(i_chk_orig));
    // 显示原图时禁用加解密按钮，防止误操作
    gtk_widget_set_sensitive(i_btn_enc, !i_show_orig && i_pixbuf);
    gtk_widget_set_sensitive(i_btn_dec, !i_show_orig && i_pixbuf);
    update_image_display();
}

// ====== 拖拽支持 ======
static void on_drag_data_received(GtkWidget *widget, GdkDragContext *context,
                                  gint x, gint y, GtkSelectionData *data,
                                  guint info, guint time, gpointer user_data)
{
    gchar **uris = gtk_selection_data_get_uris(data);
    if (uris) {
        for (int i = 0; uris[i]; i++) {
            char *filename = g_filename_from_uri(uris[i], NULL, NULL);
            if (filename) {
                GError *error = NULL;
                GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(filename, &error);
                if (pixbuf) {
                    if (i_pixbuf) g_object_unref(i_pixbuf);
                    if (i_orig_pixbuf) g_object_unref(i_orig_pixbuf);
                    i_pixbuf = pixbuf;
                    i_orig_pixbuf = gdk_pixbuf_copy(pixbuf);
                    i_w = gdk_pixbuf_get_width(pixbuf);
                    i_h = gdk_pixbuf_get_height(pixbuf);
                    i_dirty = 0;
                    i_show_orig = 0;
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(i_chk_orig), FALSE);
                    update_image_display();

                    char info[256];
                    snprintf(info, sizeof(info), "%s: %d x %d", S.imgLoaded, i_w, i_h);
                    set_status(i_status, info);
                    g_st.type = ST_IMG_LOADED; g_st.msg = SMSG_NONE;
                    g_st.w = i_w; g_st.h = i_h;
                    g_st.kb = (int)((size_t)i_w * i_h * 4 / 1024);
                    g_st.ms = 0;

                    gtk_widget_set_sensitive(i_btn_enc, TRUE);
                    gtk_widget_set_sensitive(i_btn_dec, TRUE);
                    gtk_widget_set_sensitive(i_btn_save, FALSE);
                } else {
                    g_st.type = ST_NONE; g_st.msg = SMSG_IMG_NOT_VALID;
                    set_status(i_status, S.imgNotValid);
                    g_error_free(error);
                }
                g_free(filename);
                break;
            }
        }
        g_strfreev(uris);
    }
    gtk_drag_finish(context, TRUE, FALSE, time);
}

// ====== 语言切换 ======
static void on_lang_changed(GtkComboBox *combo, gpointer data)
{
    int sel = gtk_combo_box_get_active(combo);
    if (sel >= 0 && sel <= LANG_EN) {
        g_lang = sel;
        // 更新所有标签
        gtk_window_set_title(GTK_WINDOW(g_window), S.windowTitle);
        gtk_notebook_set_tab_label_text(g_notebook, gtk_notebook_get_nth_page(g_notebook, 0), S.tabText);
        gtk_notebook_set_tab_label_text(g_notebook, gtk_notebook_get_nth_page(g_notebook, 1), S.tabImage);
        gtk_button_set_label(GTK_BUTTON(t_btn_enc), S.encrypt);
        gtk_button_set_label(GTK_BUTTON(t_btn_dec), S.decrypt);
        gtk_button_set_label(GTK_BUTTON(t_btn_copy), S.copy);
        gtk_button_set_label(GTK_BUTTON(t_btn_swap), S.swap);
        gtk_button_set_label(GTK_BUTTON(t_btn_clear), S.clear);
        gtk_button_set_label(GTK_BUTTON(t_btn_paste), S.paste);
        gtk_button_set_label(GTK_BUTTON(i_btn_open), S.openImage);
        gtk_button_set_label(GTK_BUTTON(i_btn_enc), S.encrypt);
        gtk_button_set_label(GTK_BUTTON(i_btn_dec), S.decrypt);
        gtk_button_set_label(GTK_BUTTON(i_btn_save), S.savePng);
        gtk_button_set_label(GTK_BUTTON(i_btn_copy), S.copy);
        gtk_button_set_label(GTK_BUTTON(i_btn_clear), S.clear);
        // 重建状态栏（保留当前操作状态，仅切换语言文本）
        rebuild_status();
    }
}

static void on_scroll_size_allocate(GtkWidget *widget, GtkAllocation *alloc, gpointer data)
{
    (void)widget; (void)data;
    if (i_pixbuf && alloc->width > 1 && alloc->height > 1)
        update_image_display();
}

// ====== 创建界面 ======
static GtkWidget* create_text_tab(void)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

    // 密钥和轮次行
    GtkWidget *hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl_key = gtk_label_new(S.key);
    t_key = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(t_key), "mimo");
    gtk_entry_set_width_chars(GTK_ENTRY(t_key), 20);
    GtkWidget *lbl_round = gtk_label_new(S.rounds);
    t_round = gtk_spin_button_new_with_range(1, 99, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(t_round), 1);
    gtk_box_pack_start(GTK_BOX(hbox1), lbl_key, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), t_key, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), lbl_round, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), t_round, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 0);

    // 按钮行
    GtkWidget *hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    t_btn_enc = gtk_button_new_with_label(S.encrypt);
    t_btn_dec = gtk_button_new_with_label(S.decrypt);
    t_btn_copy = gtk_button_new_with_label(S.copy);
    t_btn_swap = gtk_button_new_with_label(S.swap);
    t_btn_clear = gtk_button_new_with_label(S.clear);
    t_btn_paste = gtk_button_new_with_label(S.paste);
    gtk_box_pack_start(GTK_BOX(hbox2), t_btn_enc, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2), t_btn_dec, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2), t_btn_copy, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2), t_btn_swap, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2), t_btn_clear, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2), t_btn_paste, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

    // 输入文本
    GtkWidget *lbl_in = gtk_label_new(S.inputText);
    gtk_label_set_xalign(GTK_LABEL(lbl_in), 0);
    gtk_box_pack_start(GTK_BOX(vbox), lbl_in, FALSE, FALSE, 0);
    t_input = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(t_input), GTK_WRAP_WORD_CHAR);
    GtkWidget *scroll_in = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_in), GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(scroll_in), t_input);
    gtk_widget_set_size_request(scroll_in, -1, 150);
    gtk_box_pack_start(GTK_BOX(vbox), scroll_in, TRUE, TRUE, 0);

    // 输出文本
    GtkWidget *lbl_out = gtk_label_new(S.outputResult);
    gtk_label_set_xalign(GTK_LABEL(lbl_out), 0);
    gtk_box_pack_start(GTK_BOX(vbox), lbl_out, FALSE, FALSE, 0);
    t_output = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(t_output), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(t_output), FALSE);
    GtkWidget *scroll_out = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_out), GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(scroll_out), t_output);
    gtk_widget_set_size_request(scroll_out, -1, 150);
    gtk_box_pack_start(GTK_BOX(vbox), scroll_out, TRUE, TRUE, 0);

    // 状态栏
    t_status = gtk_label_new(S.ready);
    gtk_label_set_xalign(GTK_LABEL(t_status), 0);
    gtk_box_pack_start(GTK_BOX(vbox), t_status, FALSE, FALSE, 0);

    // 信号连接
    g_signal_connect(t_btn_enc, "clicked", G_CALLBACK(on_text_encrypt), NULL);
    g_signal_connect(t_btn_dec, "clicked", G_CALLBACK(on_text_decrypt), NULL);
    g_signal_connect(t_btn_copy, "clicked", G_CALLBACK(on_text_copy), NULL);
    g_signal_connect(t_btn_swap, "clicked", G_CALLBACK(on_text_swap), NULL);
    g_signal_connect(t_btn_clear, "clicked", G_CALLBACK(on_text_clear), NULL);
    g_signal_connect(t_btn_paste, "clicked", G_CALLBACK(on_text_paste), NULL);

    return vbox;
}

static GtkWidget* create_image_tab(void)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

    // 按钮行
    GtkWidget *hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    i_btn_open = gtk_button_new_with_label(S.openImage);
    i_btn_enc = gtk_button_new_with_label(S.encrypt);
    i_btn_dec = gtk_button_new_with_label(S.decrypt);
    i_btn_save = gtk_button_new_with_label(S.savePng);
    i_btn_copy = gtk_button_new_with_label(S.copy);
    i_btn_clear = gtk_button_new_with_label(S.clear);
    gtk_box_pack_start(GTK_BOX(hbox1), i_btn_open, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), i_btn_enc, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), i_btn_dec, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), i_btn_save, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), i_btn_copy, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), i_btn_clear, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 0);

    // 密钥和轮次
    GtkWidget *hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl_key = gtk_label_new(S.key);
    i_key = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(i_key), "mimo");
    gtk_entry_set_width_chars(GTK_ENTRY(i_key), 20);
    GtkWidget *lbl_round = gtk_label_new(S.rounds);
    i_round = gtk_spin_button_new_with_range(1, 99, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(i_round), 1);
    i_chk_orig = gtk_check_button_new_with_label(S.showOrig);
    gtk_box_pack_start(GTK_BOX(hbox2), lbl_key, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2), i_key, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2), lbl_round, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2), i_round, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2), i_chk_orig, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

    // 图片预览
    i_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(i_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(i_scroll), GTK_SHADOW_ETCHED_IN);
    i_image = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(i_scroll), i_image);
    g_signal_connect(i_scroll, "size-allocate", G_CALLBACK(on_scroll_size_allocate), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), i_scroll, TRUE, TRUE, 0);

    // 状态栏
    i_status = gtk_label_new(S.imgStatusInit);
    gtk_label_set_xalign(GTK_LABEL(i_status), 0);
    gtk_box_pack_start(GTK_BOX(vbox), i_status, FALSE, FALSE, 0);

    // 信号连接
    g_signal_connect(i_btn_open, "clicked", G_CALLBACK(on_image_open), NULL);
    g_signal_connect(i_btn_enc, "clicked", G_CALLBACK(on_image_transform), GINT_TO_POINTER(1));
    g_signal_connect(i_btn_dec, "clicked", G_CALLBACK(on_image_transform), GINT_TO_POINTER(0));
    g_signal_connect(i_btn_save, "clicked", G_CALLBACK(on_image_save), NULL);
    g_signal_connect(i_btn_copy, "clicked", G_CALLBACK(on_image_copy), NULL);
    g_signal_connect(i_btn_clear, "clicked", G_CALLBACK(on_image_clear), NULL);
    g_signal_connect(i_chk_orig, "toggled", G_CALLBACK(on_orig_toggled), NULL);

    // 拖拽支持
    gtk_drag_dest_set(vbox, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
    GtkTargetList *targets = gtk_target_list_new(NULL, 0);
    gtk_target_list_add_uri_targets(targets, 0);
    gtk_drag_dest_set_target_list(vbox, targets);
    gtk_target_list_unref(targets);
    g_signal_connect(vbox, "drag-data-received", G_CALLBACK(on_drag_data_received), NULL);

    // 初始状态
    gtk_widget_set_sensitive(i_btn_enc, FALSE);
    gtk_widget_set_sensitive(i_btn_dec, FALSE);
    gtk_widget_set_sensitive(i_btn_save, FALSE);

    return vbox;
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    gtk_init(&argc, &argv);
    g_lang = detect_system_lang();
    crypto_init();

    // 主窗口
    g_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_window), S.windowTitle);
    gtk_window_set_default_size(GTK_WINDOW(g_window), 800, 600);
    g_signal_connect(g_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(g_window), vbox);

    // 语言选择
    GtkWidget *hbox_lang = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *lbl_lang = gtk_label_new(S.langLabel);
    GtkWidget *lang_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(lang_combo), "简体中文");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(lang_combo), "繁體中文");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(lang_combo), "English");
    gtk_combo_box_set_active(GTK_COMBO_BOX(lang_combo), g_lang);
    gtk_box_pack_end(GTK_BOX(hbox_lang), lang_combo, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(hbox_lang), lbl_lang, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_lang, FALSE, FALSE, 4);
    g_signal_connect(lang_combo, "changed", G_CALLBACK(on_lang_changed), NULL);

    // Notebook
    g_notebook = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(g_notebook), TRUE, TRUE, 0);

    GtkWidget *text_tab = create_text_tab();
    GtkWidget *image_tab = create_image_tab();
    gtk_notebook_append_page(g_notebook, text_tab, gtk_label_new(S.tabText));
    gtk_notebook_append_page(g_notebook, image_tab, gtk_label_new(S.tabImage));

    gtk_widget_show_all(g_window);
    gtk_main();

    return 0;
}
