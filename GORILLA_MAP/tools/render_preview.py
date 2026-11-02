#!/usr/bin/env python3
"""Offline design preview of native layout over a neutral placeholder map.

Requires Pillow. This is NOT a screenshot or verification of rendering in DayZ.
"""
from copy import deepcopy
from pathlib import Path
import argparse
from PIL import Image, ImageDraw, ImageFont
from layout_model import parse_layout, extent

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'Source/GorillaMap'


def preview(output, width=1673, height=940):
    root = parse_layout(SOURCE / 'GUI/layouts/main.layout')
    row = parse_layout(SOURCE / 'GUI/layouts/marker_row.layout')
    examples = {
        'ServerRows': [('Электрозаводск', 'home'), ('Торговая зона', 'pin')],
        'ZoneRows': [('Опасная зона', 'radiation')],
        'BuildRows': [],
        'PersonalRows': [('Мой лагерь', 'camp'), ('Склад', 'home')],
        'GroupRows': [('Место встречи', 'star')],
    }
    counts = {'ServerRows': 'ServerCount', 'ZoneRows': 'ZoneCount', 'BuildRows': 'BuildCount', 'PersonalRows': 'PersonalCount', 'GroupRows': 'GroupCount'}
    for container, entries in examples.items():
        node = root.find(container)
        root.find(counts[container]).properties['text'] = [str(len(entries))]
        for label, icon in entries:
            copy = deepcopy(row)
            copy.find('RowTitle').properties['text'] = [label]
            copy.find('RowIcon').properties['image0'] = [f'GorillaMap/GUI/textures/marker_{icon}_ca.paa']
            node.children.append(copy)
    root.find('Coordinates').properties['text'] = ['9037 / 2174']
    root.find('Status').properties['text'] = ['GORILLA MAP  /  ЧЕРНОРУССИЯ']
    # Neutral diagram background deliberately avoids implying an in-game screenshot.
    image = Image.new('RGBA', (width, height), '#1b292b')
    draw = ImageDraw.Draw(image)
    for x in range(0, width, 128):
        draw.line((x, 0, x, height), fill='#334348', width=1)
    for y in range(0, height, 128):
        draw.line((0, y, width, y), fill='#334348', width=1)
    font_path = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
    font = ImageFont.truetype(font_path, 24)
    draw.text((width * .43, height * .46), 'ЧЕРНОРУССИЯ / MAPWIDGET', font=font, fill='#71918f')
    draw.text((width * .43, height * .51), 'Фон карты предоставляет игра', font=ImageFont.truetype(font_path, 18), fill='#71918f')

    def render(node, parent_box):
        if not node.number('visible', 1):
            return
        parent_x, parent_y, parent_w, parent_h = parent_box
        pos = node.properties.get('position', ['0', '0'])
        x = parent_x + float(pos[0]) * (1 if node.number('hexactpos') else parent_w)
        y = parent_y + float(pos[1]) * (1 if node.number('vexactpos') else parent_h)
        w, h = extent(node, parent_w, parent_h)
        box = (x, y, w, h)
        if node.kind == 'ImageWidgetClass' and w > 0 and h > 0:
            path = node.properties.get('image0', [''])[0]
            texture = ROOT / 'Source' / Path(path).with_suffix('.png')
            if texture.is_file():
                asset = Image.open(texture).convert('RGBA').resize((max(1, round(w)), max(1, round(h))), Image.Resampling.LANCZOS)
                rgba = [float(n) for n in node.properties.get('color', ['1','1','1','1'])]
                channels = asset.split()
                asset = Image.merge('RGBA', [channels[i].point(lambda n, factor=rgba[i]: round(n*factor)) for i in range(4)])
                image.alpha_composite(asset, (round(x), round(y)))
        if node.kind in ('TextWidgetClass','MultilineTextWidgetClass','ButtonWidgetClass'):
            text = node.properties.get('text', [''])[0]
            if text:
                size = int(node.number('exact text size', 16))
                text_font = ImageFont.truetype(font_path, max(8,size))
                bounds = draw.textbbox((0,0), text, font=text_font)
                tw, th = bounds[2]-bounds[0], bounds[3]-bounds[1]
                tx = x
                if node.properties.get('text halign') == ['center']: tx += (w-tw)/2
                ty = y + (h-th)/2 - bounds[1]
                rgba = [round(float(n)*255) for n in node.properties.get('color', ['.91','.95','1','1'])]
                draw.text((tx,ty), text, font=text_font, fill=tuple(rgba))
        children = sorted(node.children, key=lambda c: c.number('priority'))
        offset = 0
        for child in children:
            if node.kind == 'GridSpacerWidgetClass':
                render(child, (x,y+offset,w,max(h,parent_h)))
                if child.number('visible', 1): offset += extent(child,w,parent_h)[1] + node.number('Margin')
            else:
                render(child, box)

    render(root,(0,0,width,height))
    # Visible caption explicitly identifies the limits of this artifact.
    draw.rectangle((width*.29,height-40,width,height),fill='#101923')
    draw.text((width*.30,height-31), 'МАКЕТ ПО LAYOUT • НЕ СКРИНШОТ DAYZ • ПРИМЕРЫ МЕТОК', font=ImageFont.truetype(font_path,14),fill='#a7c1d7')
    output.parent.mkdir(parents=True,exist_ok=True)
    image.convert('RGB').save(output)
    print(output)


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'docs/UI_PREVIEW.png')
    args=parser.parse_args()
    preview(args.output)
