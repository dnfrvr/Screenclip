import base64
import gzip
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WEB = ROOT / "web"
OUT = ROOT / "include" / "page_web.h"
DEMO = WEB / "demo.html"


def gifenc():
    js = (WEB / "gifenc.esm.js").read_text(encoding="utf-8")
    match = re.search(r"export\s*\{(.*?)\};?\s*$", js, re.S)
    names = {}
    for part in match.group(1).split(","):
        local, _, public = part.strip().partition(" as ")
        names[public or local] = local
    wanted = ", ".join(f"{name}:{names[name]}" for name in ("GIFEncoder", "quantize", "applyPalette"))
    return "(()=>{" + js[: match.start()] + "window.gifenc={" + wanted + "}})();"


def main():
    body = (WEB / "index.html").read_text(encoding="utf-8").replace("/*GIFENC*/", gifenc())
    font = base64.b64encode((WEB / "vt323.woff2").read_bytes()).decode("ascii")
    body = body.replace("/*VT323*/", font)
    DEMO.write_text(body, encoding="utf-8")
    page = ('<!doctype html><html lang="fr"><head><meta charset="utf-8"><meta name="viewport" '
            'content="width=device-width,initial-scale=1,viewport-fit=cover"></head><body>' + body + "</body></html>")
    data = gzip.compress(page.encode("utf-8"), 9, mtime=0)
    lines = ["// Fichier généré par outils/make_web.py à partir de web/index.html.", "#pragma once", "#include <stdint.h>",
             "#include <stddef.h>", "", "static const uint8_t kPageWeb[] = {"]
    for i in range(0, len(data), 24):
        lines.append("  " + ",".join(str(b) for b in data[i:i + 24]) + ",")
    lines += ["};", "static const size_t kPageWebSize = sizeof(kPageWeb);", ""]
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"{OUT.name} : {len(data) // 1024} Ko compressés")


if __name__ == "__main__":
    main()
