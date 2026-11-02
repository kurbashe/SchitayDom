#!/usr/bin/env python3
"""Meaningful format, integrity, safety and reproducibility checks for build.py."""
import hashlib
from pathlib import Path
import tempfile
import unittest
import zipfile

from build import PboError, build, decode_pbo, encode_pbo


def prepare_project(root):
    for prefix, metadata in (("GorillaMap", "mod.cpp"), ("GorillaMapServer", "mod_server.cpp")):
        source = root / "Source" / prefix
        source.mkdir(parents=True, exist_ok=True)
        (source / "config.cpp").write_text("class CfgPatches {};\n")
        (root / metadata).write_text('name="Test";\n')



class PackagingTests(unittest.TestCase):
    def test_round_trip_binary_text_and_empty_file(self):
        files = {"config.cpp": b"class CfgPatches {};\n", "GUI/test.paa": bytes(range(256)), "Scripts/empty.c": b""}
        blob = encode_pbo(files)
        properties, decoded = decode_pbo(blob)
        self.assertEqual(properties, {"prefix": "GorillaMap"})
        self.assertEqual(decoded, {k.replace("/", "\\"): v for k, v in files.items()})
        self.assertEqual(blob[-20:], hashlib.sha1(blob[:-21]).digest())

    def test_matches_fixed_pbo_header_format(self):
        blob = encode_pbo({"a.c": b"hello"})
        self.assertTrue(blob.startswith(b"\0sreV" + bytes(16) + b"prefix\0GorillaMap\0\0a.c\0"))
        self.assertEqual(blob[-26:-21], b"hello")

    def test_payload_corruption_fails(self):
        blob = bytearray(encode_pbo({"a.c": b"original"}))
        blob[-22] ^= 1
        with self.assertRaisesRegex(PboError, "SHA-1"):
            decode_pbo(bytes(blob))

    def test_truncation_fails(self):
        blob = encode_pbo({"a.c": b"hello"})
        for cut in (0, 20, len(blob) - 1, len(blob) - 21):
            with self.subTest(cut=cut), self.assertRaises(PboError):
                decode_pbo(blob[:cut])

    def test_unsafe_paths_and_case_collisions_fail(self):
        for path in ("../secret", "/absolute", "C:/secret", "GUI/../secret", "bad\0name", "a//b"):
            with self.subTest(path=path), self.assertRaises(PboError):
                encode_pbo({path: b""})
        with self.assertRaises(PboError):
            encode_pbo({"Scripts/Test.c": b"a", "scripts/test.c": b"b"})

    def test_input_order_does_not_change_pbo(self):
        self.assertEqual(encode_pbo({"z.c": b"z", "a.c": b"a"}), encode_pbo({"a.c": b"a", "z.c": b"z"}))

    def test_release_repeatable_and_has_no_signature(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prepare_project(root)
            _, release = build(root, root / "dist")
            first = release.read_bytes()
            build(root, root / "dist")
            self.assertEqual(first, release.read_bytes())
            with zipfile.ZipFile(release) as archive:
                self.assertIn("@GorillaMap/addons/gorillamap.pbo", archive.namelist())
                self.assertIn("@GorillaMapServer/addons/gorillamap_server.pbo", archive.namelist())
                self.assertFalse(any(n.endswith((".bisign", ".biprivatekey")) for n in archive.namelist()))
                self.assertIsNone(archive.testzip())

    def test_symlink_source_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prepare_project(root)
            source = root / "Source/GorillaMap"
            (source / "escaped.c").symlink_to(root / "mod.cpp")
            with self.assertRaisesRegex(PboError, "Symlinks"):
                build(root, root / "dist")

    def test_valid_hash_does_not_hide_invalid_header(self):
        blob = encode_pbo({"a.c": b"hello"})
        body = bytearray(blob[:-21])
        # Corrupt the empty terminal header; recompute checksum to exercise parsing.
        body[-6] = 1
        malformed = bytes(body) + b"\0" + hashlib.sha1(body).digest()
        with self.assertRaises(PboError):
            decode_pbo(malformed)

    def test_output_inside_source_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            with self.assertRaisesRegex(PboError, "outside"):
                build(root, root / "Source/GorillaMap/output")

    def test_private_key_is_not_distributed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prepare_project(root)
            source = root / "Source/GorillaMap"
            docs = root / "docs"
            docs.mkdir()
            (docs / "secret.biprivatekey").write_bytes(b"not-a-real-key")
            with self.assertRaisesRegex(PboError, "private key"):
                build(root, root / "dist")

    def test_separate_addon_prefixes_and_payloads(self):
        import json
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prepare_project(root)
            client = root / "Source/GorillaMap/client.c"
            server = root / "Source/GorillaMapServer/server.c"
            client.write_text("class GMapClient {};")
            server.write_text("class GMapService {};")
            _, release = build(root, root / "dist")
            with zipfile.ZipFile(release) as archive:
                cp, cf = decode_pbo(archive.read("@GorillaMap/addons/gorillamap.pbo"))
                sp, sf = decode_pbo(archive.read("@GorillaMapServer/addons/gorillamap_server.pbo"))
                self.assertEqual(cp["prefix"], "GorillaMap")
                self.assertEqual(sp["prefix"], "GorillaMapServer")
                self.assertIn("client.c", cf)
                self.assertNotIn("server.c", cf)
                self.assertIn("server.c", sf)
                self.assertNotIn("client.c", sf)
                manifest = json.loads(archive.read("build-manifest.json"))
                self.assertEqual(set(manifest["addons"]), {"GorillaMap", "GorillaMapServer"})
                for prefix, folder, filename in (("GorillaMap", "@GorillaMap", "gorillamap.pbo"), ("GorillaMapServer", "@GorillaMapServer", "gorillamap_server.pbo")):
                    self.assertEqual(manifest["addons"][prefix]["pbo_sha256"], hashlib.sha256(archive.read(f"{folder}/addons/{filename}")).hexdigest())

    def test_server_authority_cannot_leak_into_client_package(self):
        for forbidden in ("GMapService", "GMapConfig", "GMapStore", "GMapServerRPC", "GorillaMapServer"):
            with self.subTest(forbidden=forbidden), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                prepare_project(root)
                (root / "Source/GorillaMap/accident.c").write_text(f"class {forbidden} {{}};")
                with self.assertRaisesRegex(PboError, "server-only dependency"):
                    build(root, root / "dist")

    def test_server_package_cannot_contain_client_gui(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prepare_project(root)
            gui = root / "Source/GorillaMapServer/GUI"
            gui.mkdir()
            (gui / "accident.layout").write_text("FrameWidgetClass Root {}")
            with self.assertRaisesRegex(PboError, "must not contain GUI"):
                build(root, root / "dist")

    def test_missing_server_source_is_not_a_complete_release(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "Source/GorillaMap"
            source.mkdir(parents=True)
            (source / "config.cpp").write_text("")
            with self.assertRaisesRegex(PboError, "GorillaMapServer"):
                build(root, root / "dist")


if __name__ == "__main__":
    unittest.main(verbosity=2)
