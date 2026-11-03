#!/usr/bin/env python3
"""Convert the actual LVGL host renders to PNG; these are synthetic states."""
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1] / 'build/preview'
for path in root.glob('*.ppm'):
    with Image.open(path) as frame:
        frame.save(path.with_suffix('.png'))

states = ('whale-ready', 'whale-listening', 'whale-thinking', 'whale-speaking',
          'mint-ready', 'mint-listening', 'settings', 'whale-setup')
sheet = Image.new('RGB', (1040, 760), '#f0f3f2')
draw = ImageDraw.Draw(sheet)
for index, name in enumerate(states):
    x, y = 16 + (index % 4) * 260, 16 + (index // 4) * 372
    draw.text((x, y), name, fill='#173e3a')
    with Image.open(root / f'{name}.png') as frame:
        sheet.paste(frame, (x, y + 24))
sheet.save(root / 'overview.png')
print(root / 'overview.png')
