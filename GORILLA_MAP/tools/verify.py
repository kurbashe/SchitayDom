#!/usr/bin/env python3
"""Static source/resource verification and optional PBO byte-for-byte verification.

This is deliberately not an Enforce compiler and cannot certify runtime behavior.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

from build import PREFIX, ROOT, PboError, collect_source, decode_pbo, safe_name


def without_comments_and_strings(text: str) -> str:
    # Retain line numbers so a structural error points at the original source.
    token = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"', re.S)
    return token.sub(lambda m: "\n" * m.group().count("\n"), text)


def verify_source(root: Path) -> tuple[list[str], int, int]:
    source = root / "Source" / PREFIX
    files = collect_source(source)
    errors = []
    texts = {}
    for name, data in files.items():
        if Path(name).suffix.lower() not in (".cpp", ".c", ".layout", ".xml", ".json", ".csv", ".imageset", ".rvmat"):
            continue
        try:
            texts[name] = data.decode("utf-8-sig")
        except UnicodeDecodeError:
            errors.append(f"{name}: text file is not UTF-8")
            continue
        if Path(name).suffix.lower() not in (".cpp", ".c", ".layout", ".rvmat"):
            continue
        clean = without_comments_and_strings(texts[name])
        stack = []
        line = 1
        pairs = {"}": "{", ")": "(", "]": "["}
        for char in clean:
            if char == "\n":
                line += 1
            elif char in "{([":
                stack.append((char, line))
            elif char in "})]":
                if not stack or stack[-1][0] != pairs[char]:
                    errors.append(f"{name}:{line}: unmatched {char}")
                    break
                stack.pop()
        if stack:
            errors.append(f"{name}:{stack[-1][1]}: unclosed {stack[-1][0]}")
    all_names = set(files)
    references = 0
    for name, text in texts.items():
        for match in re.finditer(r'"(GorillaMap[/\\][^"\r\n]+)"', text):
            path = match.group(1).replace("\\", "/")
            relative = path[len(PREFIX) + 1:]
            tail = text[match.end():]
            if re.match(r'\s*\+', tail):
                # The UI chooses an icon from this explicit array and builds its
                # texture path. Validate every possible asset, not the prefix.
                expression = re.match(r'\s*\+\s*icon\s*\+\s*"(_ca\.paa)"', tail)
                icon_array = re.search(r'\bm_Icons\s*=\s*\{([^}]+)\}', text)
                if path == "GorillaMap/GUI/textures/marker_" and expression and icon_array:
                    icons = re.findall(r'"([a-z_]+)"', icon_array.group(1))
                    if not icons:
                        errors.append(f"{name}: empty marker texture whitelist")
                    for icon in icons:
                        references += 1
                        expanded = relative + icon + expression.group(1)
                        if expanded not in all_names:
                            errors.append(f"{name}: missing whitelisted texture {PREFIX}/{expanded}")
                else:
                    errors.append(f"{name}: unresolved dynamic resource path {path}; add explicit verification")
                continue
            references += 1
            if relative not in all_names and not (source / relative).is_dir():
                errors.append(f"{name}: missing resource or script directory {path}")
    layout_names = set()
    layouts = 0
    for name, text in texts.items():
        if name.endswith(".layout"):
            layouts += 1
            layout_names.update(re.findall(r'\b\w+WidgetClass\s+(\w+)\s*\{', text))
    for name, text in texts.items():
        if name.endswith(".c"):
            widget_references = re.findall(r'FindAnyWidget\(\s*"([^"\r\n]+)"\s*\)', text)
            if re.search(r'\bWidget\s+W\(string\s+name\)', text):
                widget_references += re.findall(r'\b(?:W|Show|Visible|SetText)\(\s*"([^"\r\n]+)"', text)
            for widget in widget_references:
                if widget not in layout_names:
                    errors.append(f"{name}: FindAnyWidget names missing widget {widget}")
    input_names = set()
    for name, text in texts.items():
        if name.lower().endswith("inputs.xml"):
            try:
                xml = ET.fromstring(text)
                input_names.update(n.attrib["name"] for n in xml.findall("./inputs/actions/input"))
                if not input_names:
                    errors.append(f"{name}: no named input actions")
                for input_node in xml.findall("./preset/input"):
                    if input_node.attrib.get("name") not in input_names:
                        errors.append(f"{name}: preset references undeclared input")
            except (ET.ParseError, KeyError) as exc:
                errors.append(f"{name}: invalid input XML: {exc}")
    for name, text in texts.items():
        if name.endswith(".c"):
            for action in re.findall(r'(?:GetInputByName|LocalPress)\(\s*"(UAGorilla[^"\r\n]+)"', text):
                if action not in input_names:
                    errors.append(f"{name}: missing input declaration {action}")
    for layer in ("3_Game", "4_World", "5_Mission"):
        if not any(name.startswith(f"Scripts/{layer}/") and name.endswith(".c") for name in files):
            errors.append(f"No script source in {layer}")
    if not layouts:
        errors.append("No UI layouts")
    return errors, len(files), references


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--pbo", type=Path, help="Additionally require PBO to match source exactly")
    args = parser.parse_args()
    root = args.root.resolve()
    try:
        errors, count, references = verify_source(root)
        if args.pbo:
            properties, packed = decode_pbo(args.pbo.read_bytes())
            expected = {safe_name(k): v for k, v in collect_source(root / "Source" / PREFIX).items()}
            if properties.get("prefix") != PREFIX:
                errors.append("PBO prefix is not GorillaMap")
            if packed != expected:
                errors.append("PBO contents differ from current source; rebuild")
    except (PboError, OSError) as exc:
        print(f"VERIFY FAILED: {exc}", file=sys.stderr)
        return 1
    if errors:
        for error in errors:
            print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print(f"PASS: {count} source/resource files; {references} virtual path references; XML and widget references; structural delimiters.")
    if args.pbo:
        print("PASS: PBO prefix, SHA-1 and every packaged file match current source.")
    print("Not checked: Enforce compilation, DayZ GUI rendering, network/gameplay runtime.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
