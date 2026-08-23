# Linux GTK3 版本加解密工具

基于 GTK3 的 Linux 桌面版，与网页版、Windows 版算法完全互通。

## 依赖

- GCC
- GTK3 开发库
- pkg-config

### 安装依赖

```bash
# Debian/Ubuntu
sudo apt install build-essential libgtk-3-dev pkg-config

# Fedora
sudo dnf install gcc gtk3-devel pkgconfig

# Arch
sudo pacman -S base-devel gtk3 pkgconf
```

## 构建

```bash
cd desktop/linux
make
```

产物位于 `../../build/linux/encrypt-gtk`。

## 运行

```bash
../../build/linux/encrypt-gtk
```

## 功能

- **文本加解密**：密钥 → 伪随机偏移 → 逐字符移位
- **图片加解密**：密钥 + 轮次 · 像素乱序 + 字节替换
- 支持 PNG / JPG / BMP / GIF 格式
- 支持拖拽图片文件
- 多语言切换（简体中文 / 繁体中文 / English）

## 目录结构

```
linux/
├── Makefile
├── README.md
└── src/
    ├── main.c          # GTK3 界面
    ├── crypto.c        # 文本加解密算法（与 Windows 版一致）
    ├── crypto.h
    ├── crypto_img.c    # 图片加解密算法（pthreads 版）
    └── crypto_img.h
```
