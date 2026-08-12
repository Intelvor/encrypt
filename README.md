# 加解密工具

一个跨平台的文本 + 图片加解密工具。密钥 + 轮次加密，网页版、桌面版（C）、Android 版算法互通。

## 功能

- **文本加解密**：密钥 → 伪随机偏移序列 → 逐字符移位，支持多轮
- **图片加解密**：密钥 + 轮次 → 像素乱序 + 字节替换，支持 PNG/JPG/BMP/GIF
- 各平台算法完全一致，加密结果互通

## 目录结构

```
encrypt/
├── build/                 # 统一构建产物目录（gitignore）
│   ├── encrypt-x86.exe    # C 桌面版 32 位
│   ├── encrypt-x64.exe    # C 桌面版 64 位
│   └── encrypt.apk        # Android 版
├── web/                   # 网页版（浏览器打开即用）
│   ├── index.html         # 文本 + 图片
│   └── legacy/            # 旧版单工具
├── desktop/
│   └── c/                 # 桌面版（C, Win32 GUI）
│       └── all/           # 文本 + 图片
├── android/               # Android 版（WebView 壳）
│   ├── app/               # 应用源码（Java + assets + res）
│   ├── sdk/               # Android SDK（gitignore）
│   └── build-android.bat
├── docs/                  # 文档
└── scripts/               # 构建脚本
```

## 构建

所有构建产物统一输出到根目录 `build/`。

### 桌面版（C）

需要 [TDM-GCC](https://jmeubank.github.io/tdm-gcc/)（或任意 MinGW-w64）。

```bat
cd desktop\c\all
build-all.bat          :: 输出 build\encrypt-x86.exe（32位）和 build\encrypt-x64.exe（64位）
```

### Android 版

需要 JDK（JAVA_HOME 已设置）和 SDK（已随仓库提供于 `android/sdk`，或自行配置）。

```bat
cd android
build-android.bat      :: 输出 build\encrypt.apk
```

## 算法

文本：`makeShift(key, pos)` 用**码点序号**（代理对算 1 个），保证多轮加密可逆。

图片：每轮 = 像素置换（Fisher-Yates）+ 字节 XOR（仅 RGB，保留 alpha），密钥哈希 + 轮次种子。

## 兼容性

- 桌面版：Windows XP+（图片版依赖 GDI+，XP 自带）
- Android：API 24+（Android 7.0+）

## License

MIT
