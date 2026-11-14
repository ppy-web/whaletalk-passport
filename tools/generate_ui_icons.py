#!/usr/bin/env python3
"""Render the hand-drawn UI symbols as flash-resident LVGL A8 masks.

The artwork is defined on a 24-unit grid and supersampled at the display size.
Run with --check to verify the checked-in header without rewriting it.
"""
import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'main/online_ui_icon_data.h'
SAMPLE = 8


class Icon:
    def __init__(self, width=20, height=20):
        self.width, self.height = width, height
        self.scale = height * SAMPLE / 24
        self.image = Image.new('L', (width * SAMPLE, height * SAMPLE), 0)
        self.draw = ImageDraw.Draw(self.image)

    def point(self, xy):
        return tuple(round(v * self.scale) for v in xy)

    def line(self, points, width=1.9, fill=255):
        points = [self.point(p) for p in points]
        pixels = max(1, round(width * self.scale))
        self.draw.line(points, fill=fill, width=pixels, joint='curve')
        radius = pixels / 2
        for x, y in points:
            self.draw.ellipse((round(x-radius), round(y-radius),
                               round(x+radius), round(y+radius)), fill=fill)

    def rounded_rect(self, box, radius, width=0):
        self.draw.rounded_rectangle(self.point(box), round(radius * self.scale),
                                    outline=255 if width else None,
                                    fill=None if width else 255,
                                    width=max(1, round(width * self.scale)))

    def ellipse(self, box, width=0):
        self.draw.ellipse(self.point(box), outline=255 if width else None,
                          fill=None if width else 255,
                          width=max(1, round(width * self.scale)))

    def arc(self, center, radius, start, end, width=1.9):
        points = [(center[0] + radius * math.cos(math.radians(angle)),
                   center[1] + radius * math.sin(math.radians(angle)))
                  for angle in range(start, end + 1, 3)]
        self.line(points, width)

    def curve(self, p0, p1, p2, p3):
        points = []
        for step in range(17):
            t = step / 16
            points.append(tuple((1-t)**3*p0[i] + 3*(1-t)**2*t*p1[i] +
                                3*(1-t)*t*t*p2[i] + t**3*p3[i]
                                for i in range(2)))
        self.line(points)

    def bytes(self):
        return self.image.resize((self.width, self.height), Image.Resampling.LANCZOS).tobytes()


def link(connected):
    icon = Icon()
    icon.line([(12.2, 18.1), (10.8, 19.5)])
    icon.curve((10.8, 19.5), (9.0, 21.3), (6.0, 21.3), (4.2, 19.5))
    icon.curve((4.2, 19.5), (2.4, 17.7), (2.4, 14.7), (4.2, 12.9))
    icon.line([(4.2, 12.9), (7.0, 10.1)])
    icon.line([(11.8, 5.9), (13.2, 4.5)])
    icon.curve((13.2, 4.5), (15.0, 2.7), (18.0, 2.7), (19.8, 4.5))
    icon.curve((19.8, 4.5), (21.6, 6.3), (21.6, 9.3), (19.8, 11.1))
    icon.line([(19.8, 11.1), (17.0, 13.9)])
    if connected:
        icon.curve((7.0, 10.1), (8.9, 8.2), (12.0, 8.4), (13.9, 10.9))
        icon.curve((17.0, 13.9), (15.1, 15.8), (12.0, 15.6), (10.1, 13.1))
    return icon


def microphone(enabled):
    icon = Icon()
    icon.rounded_rect((8, 3, 16, 15), 4, 1.9)
    icon.arc((12, 11), 7, 0, 180)
    icon.line([(5, 10), (5, 11)])
    icon.line([(19, 10), (19, 11)])
    icon.line([(12, 18), (12, 21)])
    icon.line([(8.5, 21), (15.5, 21)])
    if not enabled:
        # Clear a transparent channel so the slash stays legible on any background.
        icon.line([(3.5, 3.5), (20.5, 20.5)], 5.0, 0)
        icon.line([(3.5, 3.5), (20.5, 20.5)], 1.9)
    return icon


def speaker(icon, right=11):
    icon.line([(2.5, 9.5), (5.8, 9.5), (right, 5.5), (right, 18.5),
               (5.8, 14.5), (2.5, 14.5), (2.5, 9.5)])


def volume(level):
    icon = Icon()
    speaker(icon)
    if level == 0:
        icon.line([(15, 9), (20, 15)])
        icon.line([(20, 9), (15, 15)])
    else:
        icon.arc((11, 12), 5, -45, 45)
        if level > 1:
            icon.arc((11, 12), 9, -45, 45)
    return icon


def settings_icons():
    """Draw the settings symbols on the same grid as the home controls."""
    icons = {}

    system = Icon(32, 32)
    for x, y in ((5, 8), (12, 15.5), (19, 9.5)):
        system.line([(x, 3.5), (x, y-2.5)])
        system.line([(x, y+2.5), (x, 20.5)])
        system.rounded_rect((x-2.1, y-1.7, x+2.1, y+1.7), 1.2)
    icons['system'] = system

    status = Icon(32, 32)
    status.ellipse((3.4, 3.4, 20.6, 20.6), 1.9)
    status.ellipse((10.8, 7, 13.2, 9.4))
    status.line([(12, 12), (12, 17)], 2.2)
    icons['status'] = status

    sound = Icon(32, 32)
    speaker(sound)
    sound.arc((11.5, 12), 5, -48, 48)
    sound.arc((11.5, 12), 9, -48, 48)
    icons['sound'] = sound

    history = Icon(32, 32)
    history.arc((9.7, 17.5), 10.7, -138, -42)
    history.arc((9.7, 17.5), 6.8, -138, -42)
    history.arc((9.7, 17.5), 3.1, -138, -42)
    history.ellipse((8.6, 16.6, 10.8, 18.8))
    history.draw.ellipse(history.point((11.6, 10.8, 23.3, 22.5)), fill=0)
    history.ellipse((12.8, 12, 22.1, 21.3), 1.75)
    history.line([(17.45, 14.2), (17.45, 16.9), (19.1, 17.7)], 1.6)
    icons['connection'] = history

    upload = Icon(32, 32)
    upload.rounded_rect((3.1, 6.1, 20.8, 21), 2.6, 1.9)
    upload.ellipse((6.3, 9.3, 9.4, 12.4))
    upload.line([(4.2, 18.5), (9.5, 14.8), (12.4, 16.9), (15.6, 14.1), (19.5, 17.2)])
    upload.draw.rounded_rectangle(upload.point((12.6, 1.2, 23.6, 13.9)), radius=24, fill=0)
    upload.line([(18.2, 12), (18.2, 3.4)])
    upload.line([(14.8, 6.9), (18.2, 3.4), (21.6, 6.9)])
    icons['upload'] = upload

    home = Icon(32, 32)
    home.line([(2.9, 11.4), (12, 3.7), (21.1, 11.4)])
    home.line([(5.7, 10.4), (5.7, 20.3), (9.6, 20.3), (9.6, 15.1),
               (14.4, 15.1), (14.4, 20.3), (18.3, 20.3), (18.3, 10.4)])
    icons['home'] = home

    return {f'settings_{name}': icon for name, icon in icons.items()}


def make_icons():
    icons = {'link_off': link(False), 'link_on': link(True),
             'mic_off': microphone(False), 'mic_on': microphone(True)}
    icon = Icon(28, 28)
    icon.rounded_rect((1.5, 6.5, 20, 17.5), 2.8, 1.7)
    icon.rounded_rect((21.5, 10, 23, 14), 0.7)
    icons['battery'] = icon
    for name, points in {'up': [(5.5, 15), (12, 8.5), (18.5, 15)],
                         'down': [(5.5, 9), (12, 15.5), (18.5, 9)]}.items():
        icon = Icon()
        icon.line(points, 2.1)
        icons[name] = icon
    icon = Icon()
    icon.ellipse((3.2, 3.2, 20.8, 20.8), 1.9)
    icon.line([(7.5, 12), (10.7, 15.2), (16.8, 8.8)], 1.9)
    icons['ok'] = icon
    icon = Icon()
    icon.rounded_rect((6.4, 6.4, 17.6, 17.6), 2.3)
    icons['stop'] = icon
    for phase in range(3):
        icon = Icon()
        for dot in range(3):
            y = 10 if dot == phase else 12.5
            icon.ellipse((4 + dot*7, y-1.5, 7 + dot*7, y+1.5))
        icons[f'thinking_{phase}'] = icon
    icon = Icon()
    for x, height in ((4, 4), (8, 10), (12, 16), (16, 10), (20, 4)):
        icon.line([(x, 12-height/2), (x, 12+height/2)])
    icons['listen'] = icon
    icons['volume_off'] = volume(0)
    icons['volume_low'] = volume(1)
    icons['volume_high'] = volume(2)
    icon = Icon(28, 20)
    speaker(icon, 10.5)
    icons['volume_level'] = icon
    icons.update(settings_icons())
    return icons


def generate():
    lines = ['/* Generated by tools/generate_ui_icons.py; do not edit by hand. */',
             '#pragma once', '#include "lvgl.h"', '']
    for name, icon in make_icons().items():
        data = icon.bytes()
        lines.append(f'static const uint8_t ui_{name}_pixels[] = {{')
        for i in range(0, len(data), 20):
            lines.append('    ' + ', '.join(f'0x{value:02x}' for value in data[i:i+20]) + ',')
        lines.extend(['};', f'static const lv_image_dsc_t ui_{name}_image = {{',
                      '    .header = { .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_A8,',
                      f'                .w = {icon.width}, .h = {icon.height}, .stride = {icon.width} }},',
                      f'    .data_size = sizeof(ui_{name}_pixels), .data = ui_{name}_pixels,',
                      '};', ''])
    return '\n'.join(lines)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    result = generate()
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text(encoding='utf-8') != result:
            raise SystemExit(f'{OUTPUT} is out of date')
        print('UI icon assets: PASS')
    else:
        OUTPUT.write_text(result, encoding='utf-8', newline='\n')
        print(f'Wrote {OUTPUT}')
