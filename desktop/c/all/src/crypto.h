#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stddef.h>

// 初始化码点映射表（程序启动时调用一次）
void crypto_init(void);

// 加密 / 解密。输入输出都是 UTF-16 (wchar_t) 字符串，与网页版互通。
// rounds: 加密轮次；返回的缓冲区调用者负责 free()。
wchar_t *crypto_encrypt(const wchar_t *text, const wchar_t *key, int rounds);
wchar_t *crypto_decrypt(const wchar_t *cipher, const wchar_t *key, int rounds);

// 命令行自检：验证与 C# 版测试向量一致，以及 CRLF 归一化
int crypto_selftest(void);

#endif
