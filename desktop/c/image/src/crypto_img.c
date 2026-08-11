// crypto_img.c - 图片加解密算法（与 HTML 版逐位互通）
// 设计：每轮 = 像素置换(Fisher-Yates) + 字节XOR；密钥哈希 + 轮次种子
#include "crypto_img.h"
#include <math.h>
#include <wchar.h>
#include <stdlib.h>

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
static void round_transform(uint8_t *px, uint32_t *perm, int npx, uint32_t seed, int enc)
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
    // 收集 XOR 字节（消耗 rng）
    uint8_t *xv = (uint8_t *)malloc((size_t)nbyte);
    if (!xv) return;
    for (int i = 0; i < nbyte; i++) xv[i] = (uint8_t)(rng_next() & 0xFF);

    uint8_t *out = (uint8_t *)malloc((size_t)nbyte);
    if (!out) { free(xv); return; }

    if (enc)
    {
        // 先 XOR（仅 RGB，跳过 alpha）
        for (int i = 0; i < nbyte; i += 4)
        {
            px[i] ^= xv[i];
            px[i+1] ^= xv[i+1];
            px[i+2] ^= xv[i+2];
        }
        // 再置换 out[perm[i]] = px[i]（整像素含 alpha）
        for (int i = 0; i < npx; i++)
        {
            int d = (int)perm[i] * 4, s = i * 4;
            out[d]=px[s]; out[d+1]=px[s+1]; out[d+2]=px[s+2]; out[d+3]=px[s+3];
        }
        for (int i = 0; i < nbyte; i++) px[i] = out[i];
    }
    else
    {
        // 先逆置换 out[i] = px[perm[i]]
        for (int i = 0; i < npx; i++)
        {
            int s = (int)perm[i] * 4, d = i * 4;
            out[d]=px[s]; out[d+1]=px[s+1]; out[d+2]=px[s+2]; out[d+3]=px[s+3];
        }
        for (int i = 0; i < nbyte; i++) px[i] = out[i];
        // 再逆 XOR（仅 RGB）
        for (int i = 0; i < nbyte; i += 4)
        {
            px[i] ^= xv[i];
            px[i+1] ^= xv[i+1];
            px[i+2] ^= xv[i+2];
        }
    }
    free(out);
    free(xv);
}

static void img_transform(uint8_t *px, const wchar_t *key, int rounds, int w, int h, int enc)
{
    int npx = w * h;
    uint32_t *perm = (uint32_t *)malloc((size_t)npx * sizeof(uint32_t));
    if (!perm) return;
    uint32_t seed_base = hash_key(key);

    if (enc)
    {
        for (int r = 0; r < rounds; r++)
        {
            uint32_t seed = (seed_base ^ ((uint32_t)(r + 1) * B));
            round_transform(px, perm, npx, seed, 1);
        }
    }
    else
    {
        for (int r = rounds - 1; r >= 0; r--)
        {
            uint32_t seed = (seed_base ^ ((uint32_t)(r + 1) * B));
            round_transform(px, perm, npx, seed, 0);
        }
    }
    free(perm);
}

void img_encrypt(uint8_t *px, const wchar_t *key, int rounds, int w, int h)
{
    img_transform(px, key, rounds, w, h, 1);
}

void img_decrypt(uint8_t *px, const wchar_t *key, int rounds, int w, int h)
{
    img_transform(px, key, rounds, w, h, 0);
}
