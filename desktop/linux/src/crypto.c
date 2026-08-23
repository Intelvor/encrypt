#include "crypto.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <wchar.h>

// ====== 码点映射表 ======
#define UNICODE_MOD 0x110000

static int g_safe_total = 0;
static int32_t *g_fwd_map = NULL;   // 安全位置 → 码点
static int32_t *g_rev_map = NULL;   // 码点 → 安全位置（-1 表示控制字符）

static int is_control(int code)
{
    return code <= 0x1F || (code >= 0x7F && code <= 0x9F) || (code >= 0xD800 && code <= 0xDFFF);
}

void crypto_init(void)
{
    if (g_fwd_map && g_rev_map) return;

    g_rev_map = (int32_t *)malloc((size_t)UNICODE_MOD * sizeof(int32_t));
    if (!g_rev_map) { fprintf(stderr, "oom rev_map\n"); exit(1); }

    int total = 0;
    for (int i = 0; i < UNICODE_MOD; i++)
        if (!is_control(i)) total++;

    g_fwd_map = (int32_t *)malloc((size_t)total * sizeof(int32_t));
    if (!g_fwd_map) { fprintf(stderr, "oom fwd_map\n"); exit(1); }

    for (int i = 0; i < UNICODE_MOD; i++) g_rev_map[i] = -1;

    int s = 0;
    for (int c = 0; c < UNICODE_MOD; c++)
    {
        if (!is_control(c)) { g_fwd_map[s] = c; g_rev_map[c] = s; s++; }
    }
    g_safe_total = total;
}

// ====== 模拟 JS 的 ToInt32(double) ======
// C# 的实现：NaN/Inf→0，向零截断，取 2^32 正余数，>=2^31 减 2^32
static int32_t to_int32(double d)
{
    if (isnan(d) || isinf(d)) return 0;
    // 向零截断
    if (d >= 0) d = floor(d); else d = ceil(d);
    // mod 2^32
    double mod = 4294967296.0;
    d = fmod(d, mod);
    if (d < 0) d += mod;
    if (d >= 2147483648.0) d -= mod;
    return (int32_t)d;
}

// ====== FNV-1a 32 位哈希（与 C# 版逐位一致） ======
// C# 里 HashKey 用了 double 乘法 + ToInt32，严格模拟
static uint32_t hash_key(const wchar_t *key)
{
    uint32_t h = 0x811c9dc5;
    for (; *key; key++)
    {
        h ^= (uint32_t)*key;
        // h = (uint)ToInt32((double)(int)h * 0x01000193)
        h = (uint32_t)to_int32((double)(int32_t)h * 0x01000193);
    }
    return h;
}

// ====== 与 JS MakeShift 逐位一致 ======
// seed = hash_key(key) 只依赖密钥，与位置无关；由调用方每轮缓存一次，
// 避免逐码点重算 O(|key|) 的 FNV 哈希。
static uint32_t make_shift(uint32_t seed, int pos)
{
    // x = (uint)ToInt32(seed + (double)pos * 0x9E3779B9)
    uint32_t x = (uint32_t)to_int32((double)(int32_t)seed + (double)pos * 0x9E3779B9);
    // x = (uint)ToInt32((double)(int)(x ^ (x >> 16)) * 0x45d9f3b)
    x = (uint32_t)to_int32((double)(int32_t)(x ^ (x >> 16)) * 0x45d9f3b);
    x = (uint32_t)to_int32((double)(int32_t)(x ^ (x >> 16)) * 0x45d9f3b);
    x = (uint32_t)to_int32((double)(int32_t)(x ^ (x >> 16)));
    return x % 256;
}

// ====== 安全移位 ======
static int safe_shift(int pos, int delta)
{
    int p = (pos + delta) % g_safe_total;
    if (p < 0) p += g_safe_total;
    while (is_control(g_fwd_map[p])) p = (p + 1) % g_safe_total;
    return p;
}

// ====== 从 UTF-16 字符串读取码点 ======
// 返回码点，advance 是消耗的 wchar_t 数（1 或 2）
static int read_cp(const wchar_t *s, int idx, int *advance)
{
    wchar_t c = s[idx];
    if (c >= 0xD800 && c <= 0xDBFF && s[idx + 1] >= 0xDC00 && s[idx + 1] <= 0xDFFF)
    {
        *advance = 2;
        return 0x10000 + ((c - 0xD800) << 10) + (s[idx + 1] - 0xDC00);
    }
    *advance = 1;
    return c;
}

// ====== 把码点追加到输出 wchar_t 缓冲区 ======
static void append_cp(wchar_t **buf_ptr, int *cap_ptr, int *len_ptr, int cp)
{
    wchar_t *buf = *buf_ptr;
    int cap = *cap_ptr;
    int len = *len_ptr;
    int need = cp > 0xFFFF ? 2 : 1;
    if (len + need + 1 > cap)
    {
        cap = (cap == 0 ? 64 : cap * 2);
        while (len + need + 1 > cap) cap *= 2;
        buf = (wchar_t *)realloc(buf, (size_t)cap * sizeof(wchar_t));
        if (!buf) { fprintf(stderr, "oom out\n"); exit(1); }
        *buf_ptr = buf;
        *cap_ptr = cap;
    }
    if (cp > 0xFFFF)
    {
        buf[len++] = (wchar_t)(0xD800 + ((cp - 0x10000) >> 10));
        buf[len++] = (wchar_t)(0xDC00 + ((cp - 0x10000) & 0x3FF));
    }
    else
    {
        buf[len++] = (wchar_t)cp;
    }
    buf[len] = 0;
    *len_ptr = len;
}

// ====== 归一化换行符：与浏览器 textarea.value 一致 ======
// 1) \r\n → \n；2) 孤立 \r → \n（顺序不能颠倒，否则 \r\n 会变 \n\n）
static wchar_t *normalize_lf(const wchar_t *s)
{
    int len = (int)wcslen(s);
    wchar_t *out = (wchar_t *)malloc(((size_t)len + 1) * sizeof(wchar_t));
    if (!out) { fprintf(stderr, "oom\n"); exit(1); }
    int j = 0;
    for (int i = 0; i < len; i++)
    {
        if (s[i] == L'\r')
        {
            if (i + 1 < len && s[i + 1] == L'\n') i++; // \r\n → \n
            out[j++] = L'\n';                          // 孤立 \r → \n
        }
        else
        {
            out[j++] = s[i];
        }
    }
    out[j] = 0;
    return out;
}

// ====== 单轮加密 ======
static wchar_t *encrypt_once(const wchar_t *text, const wchar_t *key)
{
    wchar_t *out = NULL;
    int cap = 0, len = 0;
    int text_len = (int)wcslen(text);
    uint32_t seed = hash_key(key);
    int cp = 0;  // 码点序号（代理对算 1 个），保证加解密位置始终一致
    for (int i = 0; i < text_len; )
    {
        int advance;
        int code = read_cp(text, i, &advance);
        int safe_pos = g_rev_map[code];
        if (safe_pos == -1)
        {
            append_cp(&out, &cap, &len, code);
        }
        else
        {
            int shift = (int)make_shift(seed, cp);
            append_cp(&out, &cap, &len, g_fwd_map[safe_shift(safe_pos, shift)]);
        }
        i += advance;
        cp++;
    }
    return out;
}

// ====== 单轮解密 ======
static wchar_t *decrypt_once(const wchar_t *cipher, const wchar_t *key)
{
    wchar_t *out = NULL;
    int cap = 0, len = 0;
    int cipher_len = (int)wcslen(cipher);
    uint32_t seed = hash_key(key);
    int cp = 0;  // 码点序号（代理对算 1 个）
    for (int i = 0; i < cipher_len; )
    {
        int advance;
        int code = read_cp(cipher, i, &advance);
        int safe_pos = g_rev_map[code];
        if (safe_pos == -1)
        {
            append_cp(&out, &cap, &len, code);
        }
        else
        {
            int shift = (int)make_shift(seed, cp);
            append_cp(&out, &cap, &len, g_fwd_map[safe_shift(safe_pos, -shift)]);
        }
        i += advance;
        cp++;
    }
    return out;
}

wchar_t *crypto_encrypt(const wchar_t *text, const wchar_t *key, int rounds)
{
    crypto_init();
    wchar_t *cur = normalize_lf(text);
    for (int r = 0; r < rounds; r++)
    {
        wchar_t *next = encrypt_once(cur, key);
        free(cur);
        cur = next;
    }
    return cur;
}

wchar_t *crypto_decrypt(const wchar_t *cipher, const wchar_t *key, int rounds)
{
    crypto_init();
    wchar_t *cur = normalize_lf(cipher);
    for (int r = 0; r < rounds; r++)
    {
        wchar_t *next = decrypt_once(cur, key);
        free(cur);
        cur = next;
    }
    return cur;
}

// ====== 自检 ======
static void print_escaped(const wchar_t *s)
{
    for (; *s; s++)
    {
        wchar_t c = *s;
        if (c < 0x80)
            putchar((int)c);
        else
            printf("\\u%04X", (unsigned)c);
    }
    putchar('\n');
}

int crypto_selftest(void)
{
    crypto_init();

    // 手动构造测试向量（直接用 C#/JS 版的 ExpectedCt 值）
    // Case 0: "Hello, 世界！" "mimo" 1
    // C# 输出: \u0157\u00C5\u011D\u0105\u00E1\u00D98\u4ED4\u7634\uFFA8
    {
        const wchar_t *pt = L"Hello, \u4e16\u754c\uFF01";
        const wchar_t *key = L"mimo";
        int rounds = 1;
        wchar_t expected[] = { 0x0157, 0x00C5, 0x011D, 0x0105, 0x00E1, 0x00D9, '8', 0x4ED4, 0x7634, 0xFFA8, 0 };
        wchar_t *got = crypto_encrypt(pt, key, rounds);
        if (wcscmp(got, expected) != 0)
        {
            printf("FAIL case 0\n  expected: "); print_escaped(expected);
            printf("  actual  : "); print_escaped(got);
            free(got);
            return 0;
        }
        wchar_t *back = crypto_decrypt(got, key, rounds);
        if (wcscmp(back, pt) != 0)
        {
            printf("FAIL case 0 roundtrip\n");
            free(got); free(back);
            return 0;
        }
        free(got); free(back);
    }

    // Case 1: "Hello, 世界！" "mimo" 3
    // C# 输出: \u0333\u0143\u023D\u01F5\u0183\u01F1h\u5050\u7804\uD800\uDCF6
    {
        const wchar_t *pt = L"Hello, \u4e16\u754c\uFF01";
        const wchar_t *key = L"mimo";
        int rounds = 3;
        wchar_t expected[] = { 0x0333, 0x0143, 0x023D, 0x01F5, 0x0183, 0x01F1, 'h', 0x5050, 0x7804, 0xD800, 0xDCF6, 0 };
        wchar_t *got = crypto_encrypt(pt, key, rounds);
        if (wcscmp(got, expected) != 0)
        {
            printf("FAIL case 1\n  expected: "); print_escaped(expected);
            printf("  actual  : "); print_escaped(got);
            free(got);
            return 0;
        }
        wchar_t *back = crypto_decrypt(got, key, rounds);
        if (wcscmp(back, pt) != 0)
        {
            printf("FAIL case 1 roundtrip\n");
            free(got); free(back);
            return 0;
        }
        free(got); free(back);
    }

    // Case 2: "Hello, 世界！" "123" 2
    {
        const wchar_t *pt = L"Hello, \u4e16\u754c\uFF01";
        const wchar_t *key = L"123";
        int rounds = 2;
        wchar_t expected[] = { 0x01F1, 0x0216, 0x0209, 0x015F, 0x015E, 0x00C3, 0x00CB, 0x4E26, 0x767A, 0xFFBF, 0 };
        wchar_t *got = crypto_encrypt(pt, key, rounds);
        if (wcscmp(got, expected) != 0)
        {
            printf("FAIL case 2\n  expected: "); print_escaped(expected);
            printf("  actual  : "); print_escaped(got);
            free(got);
            return 0;
        }
        wchar_t *back = crypto_decrypt(got, key, rounds);
        if (wcscmp(back, pt) != 0)
        {
            printf("FAIL case 2 roundtrip\n");
            free(got); free(back);
            return 0;
        }
        free(got); free(back);
    }

    // Case 3: 代理对输入（🔐 U+1F510）+ 后续字符，修复前的 bug 场景
    // JS 输出: \u00DE\uD83D\uDD66\u00B3\u00BD\u5316\u5BE7\u6E01\u8CC2\u00F6\u0148Q\u00DA\u00D0
    {
        wchar_t pt[] = { L'A', 0xD83D, 0xDD10, L'B', L' ', 0x52A0, 0x5BC6, 0x6D4B, 0x8BD5, L'-', L'2', L'0', L'2', L'6', 0 };
        const wchar_t *key = L"pass";
        int rounds = 1;
        wchar_t expected[] = { 0x00DE, 0xD83D, 0xDD66, 0x00B3, 0x00BD, 0x5316, 0x5BE7, 0x6E01, 0x8CC2, 0x00F6, 0x0148, L'Q', 0x00DA, 0x00D0, 0 };
        wchar_t *got = crypto_encrypt(pt, key, rounds);
        if (wcscmp(got, expected) != 0)
        {
            printf("FAIL case 3 (proxy input)\n  expected: "); print_escaped(expected);
            printf("  actual  : "); print_escaped(got);
            free(got);
            return 0;
        }
        wchar_t *back = crypto_decrypt(got, key, rounds);
        if (wcscmp(back, pt) != 0)
        {
            printf("FAIL case 3 roundtrip\n");
            free(got); free(back);
            return 0;
        }
        free(got); free(back);
    }

    // Case 4: 代理对输入 2 轮（修复前不可逆）
    // JS 输出: \u015A\uD83D\uDDBC\u0103\u0139\u538C\u5C08\u6EB7\u8DAF\u019E\u023Dr\u0161\u0149
    {
        wchar_t pt[] = { L'A', 0xD83D, 0xDD10, L'B', L' ', 0x52A0, 0x5BC6, 0x6D4B, 0x8BD5, L'-', L'2', L'0', L'2', L'6', 0 };
        const wchar_t *key = L"pass";
        int rounds = 2;
        wchar_t expected[] = { 0x015A, 0xD83D, 0xDDBC, 0x0103, 0x0139, 0x538C, 0x5C08, 0x6EB7, 0x8DAF, 0x019E, 0x023D, L'r', 0x0161, 0x0149, 0 };
        wchar_t *got = crypto_encrypt(pt, key, rounds);
        if (wcscmp(got, expected) != 0)
        {
            printf("FAIL case 4 (proxy 2 rounds)\n  expected: "); print_escaped(expected);
            printf("  actual  : "); print_escaped(got);
            free(got);
            return 0;
        }
        wchar_t *back = crypto_decrypt(got, key, rounds);
        if (wcscmp(back, pt) != 0)
        {
            printf("FAIL case 4 roundtrip\n");
            free(got); free(back);
            return 0;
        }
        free(got); free(back);
    }

    // Case 5: 修复前的具体失败场景 "Hello, 世界！测试文本" 3 轮
    // JS 输出: \u0333\u0143\u023D\u01F5\u0183\u01F1h\u5050\u7804\uD800\uDCF6\u6EF2\u8E39\u66E3\u674A
    {
        const wchar_t *pt = L"Hello, \u4e16\u754c\uFF01\u6d4b\u8bd5\u6587\u672c";
        const wchar_t *key = L"mimo";
        int rounds = 3;
        wchar_t expected[] = { 0x0333, 0x0143, 0x023D, 0x01F5, 0x0183, 0x01F1, 'h', 0x5050, 0x7804, 0xD800, 0xDCF6, 0x6EF2, 0x8E39, 0x66E3, 0x674A, 0 };
        wchar_t *got = crypto_encrypt(pt, key, rounds);
        if (wcscmp(got, expected) != 0)
        {
            printf("FAIL case 5 (long bug scenario)\n  expected: "); print_escaped(expected);
            printf("  actual  : "); print_escaped(got);
            free(got);
            return 0;
        }
        wchar_t *back = crypto_decrypt(got, key, rounds);
        if (wcscmp(back, pt) != 0)
        {
            printf("FAIL case 5 roundtrip\n");
            free(got); free(back);
            return 0;
        }
        free(got); free(back);
    }

    // Case 6: 生僻字（CJK 扩展 B，代理对）3 轮
    // 生僻字：𠀀𠁀𠂀𠃀 加密 2 轮
    // JS 输出: \u75D1\u521B\u5C4D\uFFA6\uD840\uDD70\uD840\uDE3A\uD840\uDD26\uD840\uDE40\uFF1C\u00FC\u01A7\u00E2\u6459\u5D53
    {
        wchar_t pt[] = { 0x751F, 0x50FB, 0x5B57, 0xFF1A, 0xD840, 0xDC00, 0xD840, 0xDC40, 0xD840, 0xDC80, 0xD840, 0xDCC0, 0xFF0C, L'C', L'J', L'K', 0x6269, 0x5C55, 0 };
        const wchar_t *key = L"key";
        int rounds = 2;
        wchar_t expected[] = { 0x75D1, 0x521B, 0x5C4D, 0xFFA6, 0xD840, 0xDD70, 0xD840, 0xDE3A, 0xD840, 0xDD26, 0xD840, 0xDE40, 0xFF1C, 0x00FC, 0x01A7, 0x00E2, 0x6459, 0x5D53, 0 };
        wchar_t *got = crypto_encrypt(pt, key, rounds);
        if (wcscmp(got, expected) != 0)
        {
            printf("FAIL case 6 (rare CJK ext B)\n  expected: "); print_escaped(expected);
            printf("  actual  : "); print_escaped(got);
            free(got);
            return 0;
        }
        wchar_t *back = crypto_decrypt(got, key, rounds);
        if (wcscmp(back, pt) != 0)
        {
            printf("FAIL case 6 roundtrip\n");
            free(got); free(back);
            return 0;
        }
        free(got); free(back);
    }

    // Case 7: CRLF 归一化
    {
        const wchar_t *lf = L"a\nb";
        wchar_t crlf[] = { L'a', L'\r', L'\n', L'b', 0 };
        wchar_t *a = crypto_encrypt(lf, L"k", 1);
        wchar_t *b = crypto_encrypt(crlf, L"k", 1);
        if (wcscmp(a, b) != 0)
        {
            printf("FAIL CRLF normalization\n");
            free(a); free(b);
            return 0;
        }
        wchar_t *back = crypto_decrypt(b, L"k", 1);
        if (wcscmp(back, lf) != 0)
        {
            printf("FAIL CRLF roundtrip\n");
            free(a); free(b); free(back);
            return 0;
        }
        free(a); free(b); free(back);
    }

    // Case 4: 孤立 \r 归一化（与浏览器 textarea 一致：孤立 \r 也视为换行）
    {
        const wchar_t *lf = L"a\nb";
        wchar_t lonecr[] = { L'a', L'\r', L'b', 0 };
        wchar_t *a = crypto_encrypt(lonecr, L"k", 1);
        wchar_t *b = crypto_encrypt(lf, L"k", 1);
        if (wcscmp(a, b) != 0)
        {
            printf("FAIL lone CR normalization\n");
            free(a); free(b);
            return 0;
        }
        free(a); free(b);
    }

    printf("SELFTEST_OK\n");
    return 1;
}

#ifdef SELFTEST
int main(void)
{
    return crypto_selftest() ? 0 : 1;
}
#endif
