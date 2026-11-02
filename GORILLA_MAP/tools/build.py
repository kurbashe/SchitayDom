#!/usr/bin/env python3
"""Reproducible, uncompressed PBO + release ZIP; Python 3 standard library only.

This packages source scripts; it does not invoke the Enforce compiler or sign PBOs.
PBO format cross-checked against armake src/build.c and src/unpack.c:
https://github.com/KoffeinFlummi/armake
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import struct
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PREFIX = "GorillaMap"
VERS = 0x56657273
HEADER = struct.Struct("<5I")
ALLOWED = {".c", ".cpp", ".layout", ".xml", ".paa", ".tga", ".csv", ".json", ".rvmat", ".imageset", ".edds"}


class PboError(ValueError):
    pass


def safe_name(name: str) -> str:
    normalized = name.replace("\\", "/")
    parts = PurePosixPath(normalized).parts
    if not parts or normalized.startswith("/") or any(p in ("", ".", "..") for p in normalized.split("/")):
        raise PboError(f"Unsafe PBO path: {name!r}")
    if any(c in normalized for c in '\x00:*?"<>|') or any(ord(c) < 32 for c in normalized):
        raise PboError(f"Invalid PBO path: {name!r}")
    return "\\".join(parts)


def encode_pbo(files: dict[str, bytes], prefix: str = PREFIX) -> bytes:
    safe_name(prefix)
    entries = sorted(((safe_name(k), v) for k, v in files.items()), key=lambda p: p[0].lower())
    if len({n.lower() for n, _ in entries}) != len(entries):
        raise PboError("Case-insensitive duplicate filename")
    body = bytearray(b"\0" + HEADER.pack(VERS, 0, 0, 0, 0))
    body.extend(b"prefix\0" + prefix.encode("utf-8") + b"\0\0")
    for name, data in entries:
        body.extend(name.encode("utf-8") + b"\0")
        body.extend(HEADER.pack(0, len(data), 0, 0, len(data)))
    body.extend(b"\0" + HEADER.pack(0, 0, 0, 0, 0))
    for _, data in entries:
        body.extend(data)
    return bytes(body) + b"\0" + hashlib.sha1(body).digest()


def decode_pbo(blob: bytes) -> tuple[dict[str, str], dict[str, bytes]]:
    """Strict bounded reader for this packer's uncompressed PBO variant."""
    if len(blob) < 43 or blob[-21] != 0:
        raise PboError("Missing PBO checksum trailer")
    data = blob[:-21]
    if hashlib.sha1(data).digest() != blob[-20:]:
        raise PboError("PBO SHA-1 mismatch")
    pos = 0

    def string() -> str:
        nonlocal pos
        end = data.find(b"\0", pos)
        if end < 0 or end - pos > 4096:
            raise PboError("Invalid or unterminated PBO string")
        try:
            result = data[pos:end].decode("utf-8")
        except UnicodeDecodeError as exc:
            raise PboError("Non-UTF-8 PBO string") from exc
        pos = end + 1
        return result

    def header() -> tuple[int, int, int, int, int]:
        nonlocal pos
        if pos + HEADER.size > len(data):
            raise PboError("Truncated PBO header")
        result = HEADER.unpack_from(data, pos)
        pos += HEADER.size
        return result

    if string() or header() != (VERS, 0, 0, 0, 0):
        raise PboError("Expected PBO version extension")
    properties = {}
    while True:
        key = string()
        if not key:
            break
        if key in properties:
            raise PboError("Duplicate PBO property")
        properties[key] = string()
    entries = []
    names = set()
    while True:
        name = string()
        method, original, reserved, timestamp, size = header()
        if not name:
            if any((method, original, reserved, timestamp, size)):
                raise PboError("Invalid terminal PBO header")
            break
        safe_name(name)
        if name.lower() in names:
            raise PboError("Duplicate PBO entry")
        names.add(name.lower())
        if method or original not in (0, size) or reserved:
            raise PboError("Unsupported compressed or invalid PBO entry")
        entries.append((name, size))
        if len(entries) > 100_000:
            raise PboError("Too many PBO entries")
    result = {}
    for name, size in entries:
        if size > len(data) - pos:
            raise PboError("PBO entry exceeds data boundary")
        result[name] = data[pos:pos + size]
        pos += size
    if pos != len(data):
        raise PboError("Unclaimed bytes after PBO data")
    return properties, result


def collect_source(source: Path) -> dict[str, bytes]:
    if not source.is_dir() or not (source / "config.cpp").is_file():
        raise PboError(f"Missing source directory/config.cpp: {source}")
    files = {}
    for file in sorted(source.rglob("*")):
        if file.is_symlink():
            raise PboError(f"Symlinks are not packaged: {file}")
        if file.is_file() and file.suffix.lower() in ALLOWED:
            files[file.relative_to(source).as_posix()] = file.read_bytes()
    return files


def zip_entry(archive: zipfile.ZipFile, name: str, data: bytes) -> None:
    info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o100644 << 16
    archive.writestr(info, data)


def build(root: Path, output: Path) -> tuple[Path, Path]:
    source = root / "Source" / PREFIX
    if output.resolve() == source.resolve() or source.resolve() in output.resolve().parents:
        raise PboError("Output directory must stay outside the PBO source tree")
    files = collect_source(source)
    mod = root / "mod.cpp"
    if not mod.is_file():
        raise PboError(f"Missing launcher metadata: {mod}")
    pbo_bytes = encode_pbo(files)
    properties, decoded = decode_pbo(pbo_bytes)
    expected = {safe_name(k): v for k, v in files.items()}
    if properties.get("prefix") != PREFIX or decoded != expected:
        raise PboError("PBO round-trip verification failed")
    output.mkdir(parents=True, exist_ok=True)
    addon = output / "@GorillaMap" / "addons" / "gorillamap.pbo"
    addon.parent.mkdir(parents=True, exist_ok=True)
    addon.write_bytes(pbo_bytes)
    (addon.parent.parent / "mod.cpp").write_bytes(mod.read_bytes())
    keys = addon.parent.parent / "keys"
    keys.mkdir(exist_ok=True)
    key_readme = (
        "Unsigned development PBO. Before use with verifySignatures=2, sign with your own "
        "DSUtils private key, place the .bisign next to the PBO, and install only the public "
        ".bikey in the server keys directory. Never distribute the .biprivatekey.\n"
    ).encode()
    (keys / "README.txt").write_bytes(key_readme)
    manifest = {
        "prefix": PREFIX,
        "pbo_sha256": hashlib.sha256(pbo_bytes).hexdigest(),
        "files": {name: hashlib.sha256(value).hexdigest() for name, value in sorted(files.items())},
        "status": "Packaged source; Enforce compilation and in-game validation required",
        "signed": False,
    }
    manifest_bytes = (json.dumps(manifest, indent=2, ensure_ascii=False) + "\n").encode()
    (output / "build-manifest.json").write_bytes(manifest_bytes)
    release = output / "GORILLA_MAP.zip"
    with zipfile.ZipFile(release, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        zip_entry(archive, "@GorillaMap/addons/gorillamap.pbo", pbo_bytes)
        zip_entry(archive, "@GorillaMap/mod.cpp", mod.read_bytes())
        zip_entry(archive, "@GorillaMap/keys/README.txt", key_readme)
        zip_entry(archive, "build-manifest.json", manifest_bytes)
        for folder in ("Source", "ServerProfile", "docs", "tools"):
            for file in sorted((root / folder).rglob("*")):
                if file.is_file() and not file.is_symlink() and "__pycache__" not in file.parts and file.suffix != ".pyc":
                    if file.suffix.lower() in (".biprivatekey", ".pem", ".key"):
                        raise PboError(f"Refusing to distribute private key file: {file}")
                    zip_entry(archive, file.relative_to(root).as_posix(), file.read_bytes())
        for name in ("README.md", "LICENSE", "mod.cpp"):
            if (root / name).is_file():
                zip_entry(archive, name, (root / name).read_bytes())
    return addon, release


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    output = args.output.resolve() if args.output else root / "dist"
    try:
        addon, release = build(root, output)
    except (OSError, PboError) as exc:
        print(f"BUILD FAILED: {exc}", file=sys.stderr)
        return 1
    print(f"PBO: {addon}")
    print(f"ZIP: {release}")
    print("PBO structure, prefix, contents and checksum verified; not an Enforce compilation.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
