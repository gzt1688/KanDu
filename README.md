# KanDu

A clean, fast, minimalist image viewer for Windows.

## Features

- Fast startup — native C++/Qt, launches in ~1 second
- Portable — no installation, no admin rights
- Fit-to-window display with no black bars
- Wheel zoom (anchor at cursor), drag to pan, double-click to pin on top
- Single instance — opening another image reuses the current window
- Quick edit — rotate, flip, crop, overwrite-save or save as
- Formats: JPG, PNG, GIF, BMP, ICO, SVG, WebP, TIFF
- Light & dark themes, custom image background color
- Set as default image viewer (per-user, no admin needed)

## Usage

Download the release ZIP, extract anywhere and run `KanDu.exe`.

## Shortcuts

| Action | Key |
|---|---|
| Open | `Ctrl+O` |
| Save / Save as | `Ctrl+S` / `Ctrl+Shift+S` |
| Rotate right / left | `R` / `Shift+R` |
| Mirror horizontal / vertical | `H` / `V` |
| Crop | `C` (`Enter` to apply) |
| Fit window / 100% | `0` / `1` |
| Zoom in / out | `+` / `-` |
| Previous / next | `←` / `→` |
| Fullscreen | `F11` |
| Always on top | `T` |

## Build

Requires Qt 6.11 (MinGW) + CMake on Windows.

```bat
build.bat
```

## Free & open source

KanDu is **free and open source**: no ads, no in-app purchases, no bundling,
no background uploads, and no limits on any feature. Feel free to build, modify
and redistribute it.

The About dialog (right-click menu / Settings → About) includes WeChat and
Alipay QR codes if you'd like to support development. It is entirely voluntary
and never affects your use of the software.

## Contact

QQ: 363899135
