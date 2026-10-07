#!/usr/bin/env python3
"""parse_skin.py — разбирает MAIN/VestaMMCskin.ai (Illustrator/PDF) на экраны и ассеты.

- 800x480 артборды = экраны, рендерятся в _ai/screens/screen_XX.png.
- меньшие артборды = отдельные ассеты, рендерятся в _ai/assets/asset_XX.png и
  сопоставляются с файлами скина (MAIN/MMC27) по размеру, при совпадении — по хэшу.
Итог печатается в консоль (index/type/size/match).
"""
import os

import fitz

AI = r"e:\GitHub\YaMaps\MAIN\VestaMMCskin.ai"
MMC27 = r"e:\GitHub\YaMaps\MAIN\MMC27"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "_ai")
SCREENS = os.path.join(OUT, "screens")
ASSETS = os.path.join(OUT, "assets")


def main():
    os.makedirs(SCREENS, exist_ok=True)
    os.makedirs(ASSETS, exist_ok=True)
    d = fitz.open(AI)
    print("artboards:", d.page_count)
    print("index\ttype\tw\th")
    for i in range(d.page_count):
        p = d[i]
        w, h = p.rect.width, p.rect.height
        pix = p.get_pixmap(matrix=fitz.Matrix(1, 1), alpha=False)
        if w >= 790 and h >= 470:
            pix.save(os.path.join(SCREENS, "screen_%02d.png" % i))
            print("%d\tscreen\t%.0f\t%.0f" % (i, w, h))
        else:
            pix.save(os.path.join(ASSETS, "asset_%02d.png" % i))
            print("%d\tasset\t%.0f\t%.0f" % (i, w, h))
    d.close()


if __name__ == "__main__":
    main()
