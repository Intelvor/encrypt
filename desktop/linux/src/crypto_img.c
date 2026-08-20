// crypto_img.c - 图片加解密算法（Linux 版，使用 pthreads）
// 设计：每轮 = 像素置换(Fisher-Yates) + 字节XOR；密钥哈希 + 轮次种子
#include "crypto_img.h"
#include <math.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define A 0x6D2B79F5u
#define B 0x9E3779B9u

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

#ifndef ENCRYPT_PARALLEL_THRESHOLD
#define ENCRYPT_PARALLEL_THRESHOLD (256 * 256)
#endif

#define MAX_POOL_THREADS 32

typedef struct {
    int start;
    int end;
    const uint32_t *perm;
    const uint32_t *xv32;
    uint32_t *px32;
    uint32_t *out32;
    int enc;
    int ready;
    int done;
} TransformWork;

typedef struct {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond_work;
    pthread_cond_t cond_done;
    TransformWork work;
    int stop;
} PoolThread;

static PoolThread g_pool[MAX_POOL_THREADS];
static int g_pool_size = 0;

static void do_chunk(TransformWork *w)
{
    if (w->enc)
    {
        for (int i = w->start; i < w->end; i++)
        {
            uint32_t v = w->px32[i];
            uint32_t k = w->xv32[i];
            uint32_t nc = (v ^ k) & 0x00FFFFFFu;
            w->out32[w->perm[i]] = nc | (v & 0xFF000000u);
        }
    }
    else
    {
        for (int i = w->start; i < w->end; i++)
            w->out32[i] = w->px32[w->perm[i]];
    }
}

static void *pool_thread_proc(void *arg)
{
    PoolThread *pt = (PoolThread *)arg;
    for (;;)
    {
        pthread_mutex_lock(&pt->mutex);
        while (!pt->work.ready && !pt->stop)
            pthread_cond_wait(&pt->cond_work, &pt->mutex);
        if (pt->stop)
        {
            pthread_mutex_unlock(&pt->mutex);
            break;
        }
        pt->work.ready = 0;
        pthread_mutex_unlock(&pt->mutex);

        do_chunk(&pt->work);

        pthread_mutex_lock(&pt->mutex);
        pt->work.done = 1;
        pthread_cond_signal(&pt->cond_done);
        pthread_mutex_unlock(&pt->mutex);
    }
    return NULL;
}

static void shutdown_pool(void)
{
    for (int i = 0; i < g_pool_size; i++)
    {
        pthread_mutex_lock(&g_pool[i].mutex);
        g_pool[i].stop = 1;
        pthread_cond_signal(&g_pool[i].cond_work);
        pthread_mutex_unlock(&g_pool[i].mutex);
        pthread_join(g_pool[i].thread, NULL);
        pthread_mutex_destroy(&g_pool[i].mutex);
        pthread_cond_destroy(&g_pool[i].cond_work);
        pthread_cond_destroy(&g_pool[i].cond_done);
    }
    g_pool_size = 0;
}

static void init_pool(void)
{
    if (g_pool_size) return;
    int hw = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (hw < 1) hw = 1;
    if (hw > MAX_POOL_THREADS) hw = MAX_POOL_THREADS;
    g_pool_size = hw;
    for (int i = 0; i < hw; i++)
    {
        g_pool[i].stop = 0;
        g_pool[i].work.ready = 0;
        g_pool[i].work.done = 0;
        pthread_mutex_init(&g_pool[i].mutex, NULL);
        pthread_cond_init(&g_pool[i].cond_work, NULL);
        pthread_cond_init(&g_pool[i].cond_done, NULL);
        pthread_create(&g_pool[i].thread, NULL, pool_thread_proc, &g_pool[i]);
    }
    atexit(shutdown_pool);
}

static void parallel_apply(uint32_t *px32, uint32_t *perm, uint32_t *xv32,
                           uint32_t *out32, int npx, int enc)
{
    int threads = g_pool_size;
    if (threads > npx / 4096) threads = npx / 4096;
    if (threads < 2) threads = 2;

    int base = npx / threads;
    int extra = npx % threads;
    int cur = 0;
    int dispatched = threads - 1;

    for (int t = 0; t < dispatched; t++)
    {
        int count = base + (t < extra ? 1 : 0);
        pthread_mutex_lock(&g_pool[t].mutex);
        g_pool[t].work = (TransformWork){cur, cur + count, perm, xv32, px32, out32, enc, 1, 0};
        pthread_cond_signal(&g_pool[t].cond_work);
        pthread_mutex_unlock(&g_pool[t].mutex);
        cur += count;
    }

    TransformWork main_work = {cur, npx, perm, xv32, px32, out32, enc, 0, 0};
    do_chunk(&main_work);

    for (int t = 0; t < dispatched; t++)
    {
        pthread_mutex_lock(&g_pool[t].mutex);
        while (!g_pool[t].work.done)
            pthread_cond_wait(&g_pool[t].cond_done, &g_pool[t].mutex);
        g_pool[t].work.done = 0;
        pthread_mutex_unlock(&g_pool[t].mutex);
    }
}

static void round_transform(uint8_t *px, uint32_t *perm, int npx,
                             uint8_t *xv, uint8_t *out, uint32_t seed, int enc)
{
    int nbyte = npx * 4;
    rng_state = seed;

    for (int i = 0; i < npx; i++) perm[i] = (uint32_t)i;
    for (int i = npx - 1; i > 0; i--)
    {
        uint32_t j = rng_next() % (uint32_t)(i + 1);
        uint32_t t = perm[i]; perm[i] = perm[j]; perm[j] = t;
    }
    for (int i = 0; i < nbyte; i++) xv[i] = (uint8_t)(rng_next() & 0xFF);

    uint32_t *px32 = (uint32_t *)px;
    uint32_t *out32 = (uint32_t *)out;
    uint32_t *xv32 = (uint32_t *)xv;

    if (npx < ENCRYPT_PARALLEL_THRESHOLD)
    {
        if (enc)
        {
            for (int i = 0; i < npx; i++)
            {
                uint32_t v = px32[i];
                uint32_t k = xv32[i];
                uint32_t nc = (v ^ k) & 0x00FFFFFFu;
                out32[perm[i]] = nc | (v & 0xFF000000u);
            }
        }
        else
        {
            for (int i = 0; i < npx; i++)
                out32[i] = px32[perm[i]];
        }
        memcpy(px32, out32, (size_t)nbyte);
        if (!enc)
        {
            for (int i = 0; i < npx; i++)
            {
                uint32_t v = px32[i];
                uint32_t k = xv32[i];
                uint32_t nc = (v ^ k) & 0x00FFFFFFu;
                px32[i] = nc | (v & 0xFF000000u);
            }
        }
        return;
    }

    parallel_apply(px32, perm, xv32, out32, npx, enc);
    memcpy(px32, out32, (size_t)nbyte);
    if (!enc)
    {
        for (int i = 0; i < npx; i++)
        {
            uint32_t v = px32[i];
            uint32_t k = xv32[i];
            uint32_t nc = (v ^ k) & 0x00FFFFFFu;
            px32[i] = nc | (v & 0xFF000000u);
        }
    }
}

static void img_transform(uint8_t *px, const wchar_t *key, int rounds, int w, int h, int enc)
{
    init_pool();

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
