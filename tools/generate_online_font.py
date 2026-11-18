#!/usr/bin/env python3
"""Regenerate the device's Chinese UI and provisioning address font subsets."""
from pathlib import Path
import os
import subprocess
import argparse
import re

ROOT = Path(__file__).resolve().parents[1]
LVGL = ROOT / 'managed_components/lvgl__lvgl'
BUILT_IN_FONTS = LVGL / 'scripts/built_in_font'
source = "\n".join(p.read_text(encoding="utf-8") for p in (ROOT / "main").glob("online_*.c"))
symbols = "".join(sorted({ch for ch in source if ord(ch) > 127}))
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()


def build_font(name, size, bpp, font_source, required, *options, line_metrics=None, fallback=None):
    output = ROOT / f"assets/fonts/{name}.c"
    if args.check:
        existing = {int(n, 16) for n in re.findall(r"U\+([0-9A-Fa-f]+)", output.read_text(encoding="utf-8"))}
        missing = {ord(ch) for ch in required} - existing
        if missing:
            raise SystemExit(f"Missing {name} glyphs: " + "".join(chr(n) for n in sorted(missing)))
        print(f"{name}: {len(required)} characters covered")
        return
    if fallback:
        options += ("--lv-fallback", fallback)
    subprocess.run([
        "npx.cmd" if os.name == "nt" else "npx", "--yes", "lv_font_conv@1.5.3",
        "--size", str(size), "--bpp", str(bpp), "--format", "lvgl",
        "--font", str(font_source),
        "--symbols", required, *options,
        "--lv-include", "lvgl.h", "--lv-font-name", name,
        "-o", str(output),
    ], check=True)
    generated = output.read_text(encoding="utf-8").replace(str(font_source), "<font-source>").replace(str(ROOT), "<repo>")
    if fallback:
        generated = generated.replace('#endif\n', f'#endif\n\nLV_FONT_DECLARE({fallback});\n', 1)
    if line_metrics is not None:
        line_height, base_line = line_metrics
        generated = re.sub(r"(?<=\.line_height = )\d+", str(line_height), generated)
        generated = re.sub(r"(?<=\.base_line = )\d+", str(base_line), generated)
    output.write_text(generated, encoding="utf-8")


def build_ssid_fallback():
    """Keep the original non-ASCII fallback glyphs and their kerning with any successor."""
    source = (LVGL / 'src/font/lv_font_montserrat_14.c').read_text(encoding='utf-8')

    def array(name):
        return re.search(rf'\b{name}\[\]\s*=\s*\{{(.*?)\n\}};', source, re.S)[1]

    def numbers(name):
        return [int(n) for n in re.findall(r'-?\d+', array(name))]

    def emit_array(ctype, name, values):
        rows = [', '.join(str(n) for n in values[i:i + 16]) for i in range(0, len(values), 16)]
        return f'static const {ctype} {name}[] = {{\n' + ''.join(f'    {row},\n' for row in rows) + '};'

    # The pinned LVGL font has contiguous printable ASCII, then a sparse
    # Unicode map. Leave ASCII IDs for kerning, with zeroed glyph descriptors.
    cmaps = array('cmaps')
    ascii_start, first = (int(n) for n in re.findall(r'\.glyph_id_start = (\d+)', cmaps))
    if ascii_start != 1 or first != 96 or '.bitmap_format = 0' not in source:
        raise ValueError('Unexpected LVGL fallback layout')
    glyphs = re.findall(r'\{\.bitmap_index = (\d+),([^}]+)\}', array('glyph_dsc'))[first:]
    base = int(glyphs[0][0])
    bitmaps = re.sub(r'/\*.*?\*/', '', array('glyph_bitmap'), flags=re.S)
    bitmap_bytes = re.findall(r'0x[0-9a-fA-F]+', bitmaps)[base:]
    descriptors = [f'{{.bitmap_index = {int(offset) - base},{fields}}}' for offset, fields in glyphs]
    left = [0] * first + numbers('kern_left_class_mapping')[first:]
    right = numbers('kern_right_class_mapping')
    class_values = numbers('kern_class_values')
    right_count = int(re.search(r'\.right_class_cnt\s*=\s*(\d+)', source)[1])
    used_left = sorted(set(left) - {0})
    compact_left = {old: new for new, old in enumerate(used_left, start=1)}
    compact_values = [n for old in used_left for n in class_values[(old - 1) * right_count:old * right_count]]
    output = '\n\n'.join([
        '/* Generated from LVGL Montserrat 14 by tools/generate_online_font.py.\n'
        ' * Montserrat and Font Awesome 5 are licensed under SIL OFL 1.1. See NOTICE. */\n'
        '#include "lvgl.h"',
        emit_array('uint8_t', 'glyph_bitmap', bitmap_bytes),
        'static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {\n'
        f'    [{first}] = ' + ',\n    '.join(descriptors) + ',\n};',
        f'static const uint16_t unicode_list_1[] = {{{array("unicode_list_1")}\n}};',
        f'static const lv_font_fmt_txt_cmap_t cmaps[] = {{{cmaps}\n}};',
        emit_array('uint8_t', 'kern_left_class_mapping', [compact_left.get(n, 0) for n in left]),
        emit_array('uint8_t', 'kern_right_class_mapping', right),
        emit_array('int8_t', 'kern_class_values', compact_values),
        f'''static const lv_font_fmt_txt_kern_classes_t kern_classes = {{
    .class_pair_values = kern_class_values,
    .left_class_mapping = kern_left_class_mapping,
    .right_class_mapping = kern_right_class_mapping,
    .left_class_cnt = {len(used_left)},
    .right_class_cnt = {right_count},
}};''',
        '''static const lv_font_fmt_txt_dsc_t font_dsc = {
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_classes,
    .kern_scale = 16,
    .cmap_num = 2,
    .bpp = 4,
    .kern_classes = 1,
};

const lv_font_t whaletalk_ssid_symbols = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = 16,
    .base_line = 3,
    .underline_position = -1,
    .underline_thickness = 1,
    .dsc = &font_dsc,
};''',
    ])
    (ROOT / 'assets/fonts/whaletalk_ssid_symbols.c').write_text(output + '\n', encoding='utf-8')


source_han = Path(os.environ.get("ONLINE_FONT_SOURCE", BUILT_IN_FONTS / "SourceHanSansSC-Normal.otf"))
if not args.check:
    build_ssid_fallback()
printable_ascii = "".join(chr(n) for n in range(0x20, 0x7f))
for size in (12, 16):
    build_font(f"mouthy_bean_zh_{size}", size, 2, source_han, symbols + printable_ascii if args.check else symbols,
               "--range", "0x20-0x7e", "--no-compress", "--no-kerning", fallback="whaletalk_ssid_symbols")

# Subsetting drops ascenders and descenders. Keep LVGL's full 20px font baseline.
build_font("whaletalk_address_20", 20, 4, BUILT_IN_FONTS / "Montserrat-Medium.ttf", ".0123456789",
           "--no-compress", "--no-prefilter", "--force-fast-kern-format", line_metrics=(22, 4))
