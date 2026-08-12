// crypto_img.c - 图片加解密算法（与 HTML 版逐位互通）
// 设计：每轮 = 像素置换(Fisher-Yates) + 字节XOR；密钥哈希 + 轮次种子
#include "crypto_img.h"
#include <math.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>

#define A 0x6D2B79F5u
#define B 0x9E3779B9u

// 模拟 JS 的 ToInt32(double)：与文本版逐位一致（floor/ceil + fmod）
static int32_t to_int32(double d)
{
    if (isnan(d) || isinf(d)) return 0;
    if (d >= 0) d = floor(d); else d = ceil(d);
    double mod = 4294967296.0;
    d = fmod(d, mod);
    if (d < 0) d += mod;
    if (d >= 2147483648.0) d -= mod;
    return (int32_t)d;
}

// FNV-1a 32 位哈希，与 JS hashKey 逐位一致（乘法走 double，模拟 JS 舍入）
static uint32_t hash_key(const wchar_t *key)
{
    uint32_t h = 0x811c9dc5u;
    for (; *key; key++)
    {
        h ^= (uint32_t)*key;
        h = (uint32_t)to_int32((double)(int32_t)h * 0x01000193);
    }
    return h;
}

// 确定性 PRNG（xorshift32，与 JS 逐位一致，纯移位+XOR 无乘法）
static uint32_t rng_state;
static uint32_t rng_next(void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

// 单轮：enc=1 加密，0 解密。就地修改 px。
// 只变换 RGB，保持 alpha 不变（避免浏览器对半透明像素做预乘导致 RGB 漂移）
// 性能优化：复用 caller 提供的缓冲区（xvbuf/outbuf），避免每轮 malloc/free；
// 用 32 位整型一次读写一个像素（4 字节），并批量生成 XOR 字节。
static void round_transform(uint8_t *px, uint32_t *perm, int npx,
                            uint8_t *xv, uint8_t *out, uint32_t seed, int enc)
{
    int nbyte = npx * 4;
    rng_state = seed;

    // 生成置换（消耗 rng）
    for (int i = 0; i < npx; i++) perm[i] = (uint32_t)i;
    for (int i = npx - 1; i > 0; i--)
    {
        uint32_t j = rng_next() % (uint32_t)(i + 1);
        uint32_t t = perm[i]; perm[i] = perm[j]; perm[j] = t;
    }
    // 收集 XOR 字节（消耗 rng）：必须逐字节生成以保持与 JS 版互通
    for (int i = 0; i < nbyte; i++) xv[i] = (uint8_t)(rng_next() & 0xFF);

    uint32_t *px32 = (uint32_t *)px;
    uint32_t *out32 = (uint32_t *)out;
    uint32_t *xv32 = (uint32_t *)xv;

    if (enc)
    {
        // 先 XOR（仅 RGB，跳过 alpha）：按像素处理，alpha 位保持不变
        for (int i = 0; i < npx; i++)
        {
            uint32_t v = px32[i];
            uint32_t k = xv32[i];                 // k = RGBA 随机字节
            uint32_t nc = v ^ k;                  // 异或整像素（含 alpha）
            nc = (nc & 0x00FFFFFFu) | (v & 0xFF000000u); // 恢复 alpha 原值
            px32[i] = nc;
        }
        // 再置换 out[perm[i]] = px[i]（整像素含 alpha）
        for (int i = 0; i < npx; i++)
            out32[perm[i]] = px32[i];
        // 回写
        memcpy(px32, out32, (size_t)nbyte);
    }
    else
    {
        // 先逆置换 out[i] = px[perm[i]]
        for (int i = 0; i < npx; i++)
            out32[i] = px32[perm[i]];
        memcpy(px32, out32, (size_t)nbyte);
        // 再逆 XOR（仅 RGB）
        for (int i = 0; i < npx; i++)
        {
            uint32_t v = px32[i];
            uint32_t k = xv32[i];
            uint32_t nc = v ^ k;
            nc = (nc & 0x00FFFFFFu) | (v & 0xFF000000u);
            px32[i] = nc;
        }
    }
}

static void img_transform(uint8_t *px, const wchar_t *key, int rounds, int w, int h, int enc)
{
    int npx = w * h;
    int nbyte = npx * 4;
    uint32_t *perm = (uint32_t *)malloc((size_t)npx * sizeof(uint32_t));
    uint8_t *xv = (uint8_t *)malloc((size_t)nbyte);
    uint8_t *out = (uint8_t *)malloc((size_t)nbyte);
    if (!perm || !xv || !out) { free(perm); free(xv); free(out); return; }
    uint32_t seed_base = hash_key(key);

    if (enc)
    {
        for (int r = 0; r < rounds; r++)
        {
            uint32_t seed = (seed_base ^ ((uint32_t)(r + 1) * B));
            round_transform(px, perm, npx, xv, out, seed, 1);
        }
    }
    else
    {
        for (int r = rounds - 1; r >= 0; r--)
        {
            uint32_t seed = (seed_base ^ ((uint32_t)(r + 1) * B));
            round_transform(px, perm, npx, xv, out, seed, 0);
        }
    }
    free(perm);
    free(xv);
    free(out);
}

void img_encrypt(uint8_t *px, const wchar_t *key, int rounds, int w, int h)
{
    img_transform(px, key, rounds, w, h, 1);
}

void img_decrypt(uint8_t *px, const wchar_t *key, int rounds, int w, int h)
{
    img_transform(px, key, rounds, w, h, 0);
}
