#!/usr/bin/env python3
"""Regenerate the device's Chinese UI and provisioning address font subsets."""
from pathlib import Path
import os
import subprocess
import argparse
import re

ROOT = Path(__file__).resolve().parents[1]
BUILT_IN_FONTS = ROOT / 'managed_components/lvgl__lvgl/scripts/built_in_font'
source = "\n".join(p.read_text(encoding="utf-8") for p in (ROOT / "main").glob("online_*.c"))
symbols = "".join(sorted({ch for ch in source if ord(ch) > 127}))
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()


def build_font(name, size, bpp, font_source, required, *options, line_metrics=None):
    output = ROOT / f"assets/fonts/{name}.c"
    if args.check:
        existing = {int(n, 16) for n in re.findall(r"U\+([0-9A-Fa-f]+)", output.read_text(encoding="utf-8"))}
        missing = {ord(ch) for ch in required} - existing
        if missing:
            raise SystemExit(f"Missing {name} glyphs: " + "".join(chr(n) for n in sorted(missing)))
        print(f"{name}: {len(required)} characters covered")
        return
    subprocess.run([
        "npx.cmd" if os.name == "nt" else "npx", "--yes", "lv_font_conv@1.5.3",
        "--size", str(size), "--bpp", str(bpp), "--format", "lvgl",
        "--font", str(font_source),
        "--symbols", required, *options,
        "--lv-include", "lvgl.h", "--lv-font-name", name,
        "-o", str(output),
    ], check=True)
    generated = output.read_text(encoding="utf-8").replace(str(font_source), "<font-source>").replace(str(ROOT), "<repo>")
    if line_metrics is not None:
        line_height, base_line = line_metrics
        generated = re.sub(r"(?<=\.line_height = )\d+", str(line_height), generated)
        generated = re.sub(r"(?<=\.base_line = )\d+", str(base_line), generated)
    output.write_text(generated, encoding="utf-8")


source_han = Path(os.environ.get("ONLINE_FONT_SOURCE", BUILT_IN_FONTS / "SourceHanSansSC-Normal.otf"))
for size in (12, 16):
    build_font(f"mouthy_bean_zh_{size}", size, 2, source_han, symbols, "--range", "0x20-0x7e",
               "--no-compress", "--no-kerning", "--lv-fallback", "lv_font_montserrat_14")

# Subsetting drops ascenders and descenders. Keep LVGL's full 20px font baseline.
build_font("whaletalk_address_20", 20, 4, BUILT_IN_FONTS / "Montserrat-Medium.ttf", ".0123456789",
           "--no-compress", "--no-prefilter", "--force-fast-kern-format", line_metrics=(22, 4))
