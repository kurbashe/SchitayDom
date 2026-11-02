"""Small offline parser for this project's native layouts, not an engine parser."""
from dataclasses import dataclass, field
from pathlib import Path
import re
import shlex


@dataclass
class Widget:
    kind: str
    name: str
    properties: dict = field(default_factory=dict)
    children: list = field(default_factory=list)

    def find(self, name):
        if self.name == name:
            return self
        for child in self.children:
            found = child.find(name)
            if found:
                return found
        return None

    def number(self, key, default=0):
        return float(self.properties.get(key, [default])[0])


HEADER = re.compile(r'(\w+WidgetClass)\s+(\w+)\s*\{')


def parse_layout(path):
    lines = [line.strip() for line in Path(path).read_text().splitlines() if line.strip() and not line.strip().startswith('//')]

    def widget(index):
        match = HEADER.fullmatch(lines[index])
        if not match:
            raise ValueError(f'Expected widget header: {lines[index]}')
        node = Widget(*match.groups())
        index += 1
        while lines[index] != '}':
            if lines[index] == '{':
                index += 1
                while lines[index] != '}':
                    child, index = widget(index)
                    node.children.append(child)
                index += 1
            else:
                parts = shlex.split(lines[index])
                node.properties[parts[0]] = parts[1:]
                index += 1
        return node, index + 1

    root, end = widget(0)
    if end != len(lines):
        raise ValueError('Unexpected data after root')
    return root


def extent(node, parent_width, parent_height):
    size = node.properties.get('size', ['1', '1'])
    width = float(size[0]) * (1 if node.number('hexactsize') else parent_width)
    height = float(size[1]) * (1 if node.number('vexactsize') else parent_height)
    if node.kind == 'GridSpacerWidgetClass' and node.number('Size To Content V'):
        visible = [child for child in node.children if child.number('visible', 1)]
        height = sum(extent(child, width, parent_height)[1] for child in visible)
        height += max(0, len(visible) - 1) * node.number('Margin')
    return width, height
