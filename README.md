# 加解密工具

一个跨平台的文本 + 图片加解密工具。密钥 + 轮次加密，网页版、桌面版（C）、Android 版算法互通。

> **定位说明**：本工具是**轻量级**加解密，面向日常防窥、内容混淆等场景；
> 不是现代密码学意义上的加密（无 KDF/盐/认证），**请勿用于保护敏感数据**。

## 功能

- **文本加解密**：密钥 → 伪随机偏移序列 → 逐字符移位，支持多轮
- **图片加解密**：密钥 + 轮次 → 像素乱序 + 字节替换，支持 PNG/JPG/BMP/GIF
- 各平台算法完全一致，加密结果互通

## 目录结构

```
encrypt/
├── build/                 # 统一构建产物目录（gitignore）
│   ├── encrypt.exe        # C 桌面版
│   └── encrypt.apk        # Android 版
├── web/                   # 网页版（浏览器打开即用）
│   └── index.html         # 单一源页面：文本 + 图片（含 Android 平台特性检测）
├── desktop/
│   ├── c/all/             # 桌面版（C, Win32 GUI）：文本 + 图片
│   └── cuix/              # 实验性重构版（core-ui .uix，需独立 core-ui 仓库，未随主仓库分发）
├── android/               # Android 版（WebView 壳）
│   ├── app/               # 应用源码（Java + assets + res）
│   ├── sdk/               # 本地 Android SDK（gitignore，需自行放置）
│   └── build-android.bat
├── docs/                  # GitHub Pages（web/index.html 的拷贝）
├── scripts/
│   ├── build-all.bat      # 一键构建 C + Android
│   ├── sync-pages.bat     # 同步 web/index.html → docs/
│   └── sync-android.bat   # 同步 web/index.html → android/app/assets/
└── .github/               # issue 模板
```

## 页面同步（重要）

`web/index.html` 是网页版与 Android 版的**唯一源文件**（Android 专属逻辑通过特性检测
内联，web 上自动闲置）。

```bat
scripts\sync-pages.bat        :: web → docs/（GitHub Pages）
scripts\sync-android.bat      :: web → android/app/assets/（Android APK 内嵌页）
```

`android\build-android.bat` 在打包前会自动同步 assets，无需手动执行 sync-android.bat；
手动执行仅用于单独提交该变更（git 会记录 asset 变化）。

## 构建

所有构建产物统一输出到根目录 `build/`。

### 桌面版（C）

需要 [TDM-GCC](https://jmeubank.github.io/tdm-gcc/)（或任意 MinGW-w64）。
脚本会依次尝试 `%GCC%` 环境变量、PATH 中的 `gcc`，最后回退到已知安装路径。

```bat
cd desktop\c\all
build-all.bat          :: 输出 build\encrypt.exe
```

### Android 版

需要 JDK（`JAVA_HOME` 已设置），并把 Android SDK 放到 `android\sdk`（仓库不携带 SDK，
该目录已被 gitignore；`sdk-tools` 同理）。SDK 需含 `build-tools\34.0.0` 与 `platforms\android-34`。

```bat
cd android
build-android.bat      :: 输出 build\encrypt.apk
```

签名 keystore 放在仓库外（`%USERPROFILE%\.android\encrypt.keystore`），首次运行按脚本
提示生成；密码可用环境变量 `KEYSTORE_PASS` 覆盖默认值。

### 一键构建

```bat
scripts\build-all.bat [c|android|all]
```

## 算法

文本：`makeShift(key, pos)` 用**码点序号**（代理对算 1 个），保证多轮加密可逆。

图片：每轮 = 像素置换（Fisher-Yates）+ 字节 XOR（仅 RGB，保留 alpha），密钥哈希 + 轮次种子。

## 兼容性

- 桌面版：Windows XP+（图片版依赖 GDI+，XP 自带）
- Android：API 24+（Android 7.0+）

## License

MIT
