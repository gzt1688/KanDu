# -*- coding: utf-8 -*-
"""从收款码大图裁出微信/支付宝两个码，输出到 resources/img/。

原图 1170x642，左右两个二维码 + 下方文字，中间有白色分隔。
处理：左右切分 -> 去白边 -> 补 14px 静区 -> 等比缩放 -> 输出 PNG。

资源最终编进 exe（qrc），绿色版要单文件分发，所以宽度压到 360：
模块约 40px，100% 缩放下手机可正常识别。
用法：在 resources/img 目录下执行  python prep_donate.py
"""
import os
from PIL import Image, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "donate_raw.png")
PAD = 14   # 四周补白：二维码静区，不能省
MAXW = 360


def trim(img, bg=(255, 255, 255)):
    """按背景色裁掉四周空白。"""
    diff = ImageChops.difference(img, Image.new(img.mode, img.size, bg))
    # 容忍抗锯齿噪点：先阈值化再取 bbox
    diff = diff.convert("L").point(lambda p: 255 if p > 12 else 0)
    box = diff.getbbox()
    return img.crop(box) if box else img


def pad(img, p=PAD, bg=(255, 255, 255)):
    out = Image.new(img.mode, (img.width + p * 2, img.height + p * 2), bg)
    out.paste(img, (p, p))
    return out


def main():
    src = Image.open(SRC).convert("RGB")
    w, h = src.size
    half = w // 2

    # 多切 30px 覆盖中缝，避免把相邻码的边缘裁进来
    parts = (
        ("donate_wechat.png", src.crop((0, 0, half + 30, h))),
        ("donate_alipay.png", src.crop((half - 30, 0, w, h))),
    )

    report = ["source %dx%d" % (w, h)]
    for name, img in parts:
        img = pad(trim(img))
        if img.width > MAXW:
            img = img.resize((MAXW, round(img.height * MAXW / img.width)),
                             Image.LANCZOS)
        # 码区是纯黑白、中间是彩色 LOGO，128 色量化后肉眼无损，
        # 但 PNG 体积从 ~330KB 降到 ~144KB（绿色版单 exe 分发，值得）。
        img = img.quantize(colors=128, method=Image.MEDIANCUT, dither=Image.NONE)
        path = os.path.join(HERE, name)
        img.save(path, "PNG", optimize=True)
        report.append("%s  %dx%d  %d KB" % (name, img.width, img.height,
                                           os.path.getsize(path) // 1024))

    with open(os.path.join(HERE, "_report.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report) + "\n")


main()
