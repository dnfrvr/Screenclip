from pathlib import Path
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parent
OUT = ROOT.parent / "include" / "logo_image.h"
WIDTH = 640
PINK = (255, 40, 146)

def ink_mask():
    src = Image.open(ROOT / "logo.png").convert("RGBA")
    flat = Image.new("RGBA", src.size, "white")
    flat.alpha_composite(src)
    r, g, b, _ = flat.split()
    ink = ImageChops.invert(ImageChops.lighter(ImageChops.lighter(r, g), b))
    return ink.crop(ink.point(lambda v: 255 if v > 40 else 0).getbbox())

def render(width):
    ink = ink_mask()
    ink = ink.resize((width, round(ink.height * width / ink.width)), Image.LANCZOS)
    logo = Image.new("RGBA", ink.size, PINK + (0,))
    logo.putalpha(ink)
    return logo, 0, ink.height

def main():
    img, pad, text_h = render(WIDTH)
    w, h = img.size
    r, g, b, a = img.split()
    colors = [((rr & 0xF8) << 8) | ((gg & 0xFC) << 3) | (bb >> 3)
              for rr, gg, bb in zip(r.tobytes(), g.tobytes(), b.tobytes())]
    alpha = a.tobytes()
    lines = [
        "// Fichier généré par outils/logo.py — ne pas modifier à la main.",
        "#pragma once",
        "#include <stdint.h>",
        "",
        f"constexpr int kLogoW = {w};        ",
        f"constexpr int kLogoH = {h};",
        f"constexpr int kLogoPad = {pad};       ",
        f"constexpr int kLogoTextH = {text_h};    ",
        "static const uint16_t kLogoColor[] = {",
    ]
    for i in range(0, len(colors), 24):
        lines.append("  " + ",".join(str(c) for c in colors[i:i + 24]) + ",")
    lines += ["};", "static const uint8_t kLogoAlpha[] = {"]
    for i in range(0, len(alpha), 32):
        lines.append("  " + ",".join(str(v) for v in alpha[i:i + 32]) + ",")
    lines.append("};")
    OUT.write_text("\n".join(lines) + "\n")
    print(f"{OUT.name} : {w}x{h} (lettrage {WIDTH}x{text_h})")

if __name__ == "__main__":
    main()
