# KanDu 简洁看图软件（C++ 版）

原生 C++ / Qt 6.11.2 (MinGW) 重写版，纯绿色免安装，启动快（实测 ~0.9 秒）。

## 快速开始

- **绿色版**：`dist\KanDu.exe`（连同 `dist\` 下所有文件），双击即用，可整体拷贝到任意目录分发。
- **压缩包**：`KanDu-绿色版.zip`（约 13.6 MB，解压后即 `dist\` 内容）。
- **开发版**：`build\KanDu.exe`（含完整 Qt 部署，供调试）。

## 功能

- 支持格式：JPG/JPEG/JFIF、PNG、GIF、BMP、ICO、SVG（Qt 插件）；
  WebP、TIFF 由 Windows 系统 WIC 解码（Win10/11 自带），打开即用。
- 窗口按图片比例贴合显示，无黑边。
- 单实例：已打开 KanDu 时再双击其他图片，会在同一窗口打开（覆盖前一张），不会新开窗口。
- 滚轮缩放（以光标为锚点）、左键拖拽平移、双击切换置顶（动作可在设置中改）。
- 标题栏图标按钮：左旋 / 右旋 / 水平镜像 / 垂直镜像 / 裁剪 / 覆盖保存 / 设置，
  以及置顶图钉、最小化 / 最大化 / 关闭。
- 裁剪：进入后拖动 8 个手柄调整选区，Enter 应用、Esc 取消、右键退出。
- 覆盖保存 / 另存为；WebP/TIFF/SVG 等 Qt 无写回能力的格式会自动转存 PNG。
- 上下张浏览（自然排序）、全屏（F11）、复制到剪贴板（Ctrl+C）、拖入文件打开。
- 深色 / 浅色双主题，图片背景色可自定义。
- 设置对话框：鼠标、显示、关于（V1.0，2026-9-16 / QQ 363899135）三页；「设置为默认看图软件」注册用户级文件关联（HKCU，免管理员）。
- 设置持久化：`%APPDATA%\KanDu\KanDu.ini`。

## 快捷键

| 功能 | 按键 |
|---|---|
| 打开 | Ctrl+O |
| 覆盖保存 / 另存为 | Ctrl+S / Ctrl+Shift+S |
| 右旋 / 左旋 | R / Shift+R |
| 水平 / 垂直镜像 | H / V |
| 裁剪 | C（Enter 应用） |
| 适应窗口 / 实际大小 | 0 / 1（或 F / Ctrl+1） |
| 放大 / 缩小 | +/-（或 Ctrl+= / Ctrl+-） |
| 上一张 / 下一张 | ← ↑ PgUp / → ↓ PgDn Space |
| 全屏 | F11，Esc 退出 |
| 窗口置顶 | T（或图钉按钮） |
| 复制图片 | Ctrl+C |
| 设置 | 标题栏齿轮按钮 |

## 构建（开发者）

环境：Qt 6.11.2 mingw_64 + MinGW 13.1 + CMake（`D:\Qt`）。

```bat
build.bat   :: 配置 → 编译 → windeployqt → 生成 dist\ 绿色版 → 可选打包 zip
```

自测：`build\kandu_selftest.exe <测试图> <结果文件>`（自动化 UI 交互验证，
覆盖打开 / 贴合 / 旋转 / 镜像 / 置顶 / 裁剪 / 保存 / 主题 / 设置对话框 / 图钉渲染，全部通过）。

## 目录结构

```
KanDuCpp/
├── src/            # 源码（icons/settings/imageview/titlebar/mainwindow/settingsdialog/defaultapp/wicdecode/singleton）
├── tests/          # 自动化自测
├── build/          # 编译产物（开发版）
├── dist/           # 绿色分发版
└── build.bat       # 一键构建
```
