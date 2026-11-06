from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def executable(name):
    candidates = [ROOT / name, ROOT / f"{name}.exe"]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise AssertionError(f"built executable not found: {name}")


def run(command, *, expected=0):
    result = subprocess.run(
        [str(part) for part in command],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if result.returncode != expected:
        raise AssertionError(
            f"command returned {result.returncode}, expected {expected}: "
            f"{command}\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


class LinkerTargetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.assembler = executable("vhx64-none-vxo-as")
        cls.linker = executable("vhx64-none-vxo-ld")

    def make_object(self, directory, arch="vhx64imsa", source_text=None):
        source = directory / "entry.s"
        object_file = directory / "entry.vxo"
        if source_text is None:
            source_text = ".global start\nstart:\n    nop\n"
        source.write_text(source_text, encoding="utf-8")
        run([
            self.assembler,
            f"-march={arch}",
            "-c",
            "-o",
            object_file,
            source,
        ])
        return object_file

    def test_vxo_target_selects_width_and_storage_endianness(self):
        expected_modes = {
            "vxo32-littlevhx": (0, b"\x01\x00"),
            "vxo64-littlevhx": (2, b"\x01\x00"),
            "vxo32-bigvhx": (1, b"\x00\x01"),
            "vxo64-bigvhx": (3, b"\x00\x01"),
        }
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            object_file = self.make_object(directory)
            for target, (mode, version) in expected_modes.items():
                with self.subTest(target=target):
                    linked = directory / f"{target}.vxo"
                    roundtrip = directory / f"{target}-roundtrip.vxo"
                    run([
                        self.linker,
                        "--oformat",
                        target,
                        "-e",
                        "start",
                        "-o",
                        linked,
                        object_file,
                    ])
                    data = linked.read_bytes()
                    self.assertEqual(data[4], mode)
                    self.assertEqual(data[6:8], version)
                    run([self.linker, "-r", "-o", roundtrip, linked])
                    self.assertGreater(roundtrip.stat().st_size, 16)

    def test_binary_output_starts_with_entry_trampoline(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            object_file = self.make_object(directory)
            binary = directory / "entry.bin"
            run([
                self.linker,
                "--oformat",
                "binary",
                "-e",
                "start",
                "-o",
                binary,
                object_file,
            ])
            data = binary.read_bytes()
            self.assertEqual(len(data), 36)
            self.assertNotEqual(data[:24], b"\0" * 24)
            self.assertEqual(data[-4:], b"\x00\x00\x01\x00")

    def test_binary_requires_an_entry_symbol(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            object_file = self.make_object(directory)
            result = run([
                self.linker,
                "--oformat",
                "binary",
                "-o",
                directory / "missing-entry.bin",
                object_file,
            ], expected=1)
            self.assertIn("binary entry symbol is undefined", result.stderr)

    def test_vxo_storage_endianness_does_not_reorder_section_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            object_file = self.make_object(
                directory,
                source_text=(
                    ".data\n"
                    "    .byte 0x11, 0x22, 0x33, 0x44\n"
                    ".text\n"
                    ".global start\n"
                    "start:\n"
                    "    nop\n"
                ),
            )
            linked = directory / "big-storage.vxo"
            run([
                self.linker,
                "--oformat",
                "vxo64-bigvhx",
                "-e",
                "start",
                "-o",
                linked,
                object_file,
            ])
            self.assertIn(b"\x11\x22\x33\x44", linked.read_bytes())

    def test_vhx32_binary_uses_32_bit_entry_trampoline(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            object_file = self.make_object(directory, arch="vhx32imsa")
            binary = directory / "entry32.bin"
            run([
                self.linker,
                "--oformat",
                "binary",
                "-e",
                "start",
                "-o",
                binary,
                object_file,
            ])
            data = binary.read_bytes()
            self.assertGreaterEqual(len(data), 20)
            self.assertEqual(data[-4:], b"\x00\x00\x01\x00")

    def test_unknown_output_format_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            object_file = self.make_object(directory)
            result = run([
                self.linker,
                "--oformat",
                "not-a-target",
                "-o",
                directory / "invalid.vxo",
                object_file,
            ], expected=1)
            self.assertIn("unsupported output format", result.stderr)
