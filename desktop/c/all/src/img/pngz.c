// pngz.c - 极简 zlib inflate 实现
#include "pngz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- 位读取器 ----
typedef struct {
    const uint8_t *data;
    size_t len;
    size_t pos;   // 当前字节
    uint32_t bitbuf;
    int bitcnt;
} BitReader;

static void br_init(BitReader *br, const uint8_t *data, size_t len)
{
    br->data = data;
    br->len = len;
    br->pos = 0;
    br->bitbuf = 0;
    br->bitcnt = 0;
}

static int br_need(BitReader *br, int n)
{
    while (br->bitcnt < n)
    {
        if (br->pos >= br->len) return 0;
        br->bitbuf |= (uint32_t)br->data[br->pos] << br->bitcnt;
        br->pos++;
        br->bitcnt += 8;
    }
    return 1;
}

static uint32_t br_bits(BitReader *br, int n)
{
    uint32_t v = br->bitbuf & ((1u << n) - 1);
    br->bitbuf >>= n;
    br->bitcnt -= n;
    return v;
}

// ---- Huffman 解码表 ----
// 用长度计数 + 符号映射，标准做法
#define MAXBITS 15
#define MAXSYM 288

typedef struct {
    int count[MAXBITS + 1];   // count[len] = 码长 len 的符号数
    int symbol[MAXSYM];       // 按 (len, code) 排序的符号
    int min_len;              // 最短码长
} HuffTable;

// 由码长数组构造 Huffman 表（经典构造）
static void huff_build(HuffTable *ht, const int *lengths, int nsyms)
{
    for (int i = 0; i <= MAXBITS; i++) ht->count[i] = 0;
    ht->min_len = MAXBITS + 1;
    for (int i = 0; i < nsyms; i++)
    {
        int l = lengths[i];
        if (l > 0) { ht->count[l]++; if (l < ht->min_len) ht->min_len = l; }
    }

    // 每个长度组的起始符号索引
    int offset[MAXBITS + 1];
    offset[0] = 0;
    for (int l = 1; l <= MAXBITS; l++) offset[l] = offset[l - 1] + ht->count[l];

    // canonical 码字分配（RFC1951）：code[l] = (code[l-1] + count[l-1]) << 1
    // symbol 数组按长度分组连续存放，组内按符号号升序（= 码字升序）
    int pos[MAXBITS + 1];
    for (int l = 1; l <= MAXBITS; l++) pos[l] = offset[l - 1];
    for (int i = 0; i < nsyms; i++)
    {
        int l = lengths[i];
        if (l > 0) ht->symbol[pos[l]++] = i;
    }
}

// 用 table 解码一个符号，返回符号值；-1 失败
static int huff_decode(BitReader *br, const HuffTable *ht)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= MAXBITS; len++)
    {
        if (!br_need(br, 1)) return -1;
        code |= (int)br_bits(br, 1);
        int count = ht->count[len];
        if (code - first < count)
            return ht->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

// ---- 固定 Huffman 表 ----
static void huff_fixed_lit(HuffTable *ht)
{
    int lens[288];
    for (int i = 0; i < 144; i++) lens[i] = 8;
    for (int i = 144; i < 256; i++) lens[i] = 9;
    for (int i = 256; i < 280; i++) lens[i] = 7;
    for (int i = 280; i < 288; i++) lens[i] = 8;
    huff_build(ht, lens, 288);
}

static void huff_fixed_dist(HuffTable *ht)
{
    int lens[30];
    for (int i = 0; i < 30; i++) lens[i] = 5;
    huff_build(ht, lens, 30);
}

// ---- 长度/距离码扩展 ----
static const int len_base[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258
};
static const int len_extra[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const int dist_base[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
};
static const int dist_extra[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

// ---- 动态 Huffman 表长度码表 ----
// （辅助，实际未使用）

size_t zlib_inflate(const uint8_t *src, size_t src_len, uint8_t *out, size_t out_cap)
{
    BitReader br;
    br_init(&br, src, src_len);

    // zlib 头: 2 字节
    if (src_len < 2) return 0;
    uint8_t cmf = src[0], flg = src[1];
    if ((cmf & 0x0F) != 8) return 0;      // CM=8 (deflate)
    if (((cmf << 8) | flg) % 31 != 0) return 0; // 校验
    br.pos = 2; // 跳过 zlib 头，从压缩数据开始读块

    size_t out_len = 0;
    int final = 0;

    while (!final)
    {
        if (!br_need(&br, 3)) return 0;
        final = (int)br_bits(&br, 1);
        int btype = (int)br_bits(&br, 2);

        if (btype == 0)
        {
            // stored block：跳到字节边界后，读 LEN/NLEN。
            // br_need/br_bits 已把块头字节读入并推进 pos 到 LEN 字节处。
            // 只需清零 bitbuf（丢弃块头剩余填充位）。
            br.bitbuf = 0; br.bitcnt = 0;
            if (br.pos + 4 > br.len) return 0;
            uint16_t len = (uint16_t)(src[br.pos] | (src[br.pos+1] << 8));
            uint16_t nlen = (uint16_t)(src[br.pos+2] | (src[br.pos+3] << 8));
            br.pos += 4;
            if (len != (uint16_t)~nlen) return 0;
            if (br.pos + len > br.len) return 0;
            if (out_len + len > out_cap) return 0;
            memcpy(out + out_len, src + br.pos, len);
            out_len += len;
            br.pos += len;
        }
        else
        {
            HuffTable lit, dist;
            if (btype == 1)
            {
                huff_fixed_lit(&lit);
                huff_fixed_dist(&dist);
            }
            else if (btype == 2)
            {
                if (!br_need(&br, 14)) return 0;
                int hlit = (int)br_bits(&br, 5) + 257;
                int hdist = (int)br_bits(&br, 5) + 1;
                int hclen = (int)br_bits(&br, 4) + 4;
                int code_lengths[19] = {0};
                int order[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
                for (int i = 0; i < hclen; i++)
                {
                    if (!br_need(&br, 3)) return 0;
                    code_lengths[order[i]] = (int)br_bits(&br, 3);
                }
                HuffTable code_ht;
                huff_build(&code_ht, code_lengths, 19);

                // 解码长度序列
                int lens[288 + 32];
                int n = 0;
                while (n < hlit + hdist)
                {
                    int sym = huff_decode(&br, &code_ht);
                    if (sym < 0 || sym > 18) return 0;
                    if (sym < 16) lens[n++] = sym;
                    else if (sym == 16)
                    {
                        if (!br_need(&br, 2)) return 0;
                        int rep = (int)br_bits(&br, 2) + 3;
                        if (n == 0) return 0;
                        int prev = lens[n - 1];
                        for (int k = 0; k < rep; k++) { if (n >= hlit + hdist) return 0; lens[n++] = prev; }
                    }
                    else if (sym == 17)
                    {
                        if (!br_need(&br, 3)) return 0;
                        int rep = (int)br_bits(&br, 3) + 3;
                        for (int k = 0; k < rep; k++) { if (n >= hlit + hdist) return 0; lens[n++] = 0; }
                    }
                    else // 18
                    {
                        if (!br_need(&br, 7)) return 0;
                        int rep = (int)br_bits(&br, 7) + 11;
                        for (int k = 0; k < rep; k++) { if (n >= hlit + hdist) return 0; lens[n++] = 0; }
                    }
                }
                huff_build(&lit, lens, hlit);
                huff_build(&dist, lens + hlit, hdist);
            }
            else return 0; // 保留类型

            // 解码数据
            for (;;)
            {
                int sym = huff_decode(&br, &lit);
                if (sym < 0) return 0;
                if (sym < 256)
                {
                    if (out_len + 1 > out_cap) return 0;
                    out[out_len++] = (uint8_t)sym;
                }
                else if (sym == 256) break; // 块结束
                else
                {
                    int li = sym - 257;
                    if (li < 0 || li >= 29) return 0;
                    int length = len_base[li];
                    if (len_extra[li] > 0)
                    {
                        if (!br_need(&br, len_extra[li])) return 0;
                        length += (int)br_bits(&br, len_extra[li]);
                    }
                    int dsym = huff_decode(&br, &dist);
                    if (dsym < 0 || dsym >= 30) return 0;
                    int distance = dist_base[dsym];
                    if (dist_extra[dsym] > 0)
                    {
                        if (!br_need(&br, dist_extra[dsym])) return 0;
                        distance += (int)br_bits(&br, dist_extra[dsym]);
                    }
                    if (distance > out_len) return 0; // 非法回退
                    if (out_len + length > out_cap) return 0;
                    for (int k = 0; k < length; k++)
                    {
                        out[out_len] = out[out_len - distance];
                        out_len++;
                    }
                }
            }
        }
    }
    return out_len;
}
