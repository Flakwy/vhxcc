from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tests" / "cc" / "basic.c"


def executable(name):
    candidates = [ROOT / name, ROOT / f"{name}.exe"]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise AssertionError(f"built executable not found: {name}")


def run(command, *, cwd=None, expected=0):
    result = subprocess.run(
        [str(part) for part in command],
        cwd=cwd,
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


class CCompilerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cc = executable("vhx64-none-vxo-cc")
        cls.assembler = executable("vhx64-none-vxo-as")
        cls.linker = executable("vhx64-none-vxo-ld")

    def test_assembly_output_can_be_assembled_and_linked(self):
        with tempfile.TemporaryDirectory() as temporary:
            output_dir = Path(temporary)
            assembly = output_dir / "basic.s"
            obj_from_assembly = output_dir / "from-assembly.vxo"
            merged = output_dir / "merged.vxo"

            run([self.cc, "-S", "-o", assembly, SOURCE])
            generated = assembly.read_text(encoding="utf-8")
            self.assertIn(".global sum", generated)
            self.assertIn("sum:", generated)
            self.assertIn("    add ", generated)
            self.assertIn("    li ", generated)
            self.assertIn("Lcc_0_negate:", generated)

            run([
                self.assembler,
                "-march=vhx64imsa",
                "-c",
                "-o",
                obj_from_assembly,
                assembly,
            ])
            run([self.linker, "-r", "-o", merged, obj_from_assembly])
            self.assertGreater(merged.stat().st_size, 0)

    def test_default_assembly_output_name(self):
        with tempfile.TemporaryDirectory() as temporary:
            run([self.cc, "-S", SOURCE], cwd=temporary)
            self.assertTrue((Path(temporary) / "basic.s").is_file())
            self.assertFalse((Path(temporary) / "basic.vxo").exists())

    def test_preprocessor_macro_is_used_by_compiler(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "macro.c"
            output = Path(temporary) / "macro.s"
            source.write_text("int value(void) { return VALUE; }\n",
                              encoding="utf-8")
            run([self.cc, "-S", "-DVALUE=17", "-o", output, source])
            self.assertIn("17", output.read_text(encoding="utf-8"))

    def test_preprocess_only_writes_preprocessed_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "macro.c"
            output = Path(temporary) / "macro.i"
            source.write_text("#define VALUE 23\nint value(void) { return VALUE; }\n",
                              encoding="utf-8")
            run([self.cc, "-E", "-o", output, source])
            generated = output.read_text(encoding="utf-8")
            self.assertIn("return 23;", generated)
            self.assertNotIn("#define VALUE", generated)

    def test_integer_typedefs_can_be_used_in_function_signatures(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "typedef.c"
            output = Path(temporary) / "typedef.s"
            source.write_text(
                "typedef unsigned long word;\n"
                "word identity(word value) { return value; }\n",
                encoding="utf-8",
            )
            run([self.cc, "-S", "-o", output, source])
            generated = output.read_text(encoding="utf-8")
            self.assertIn(".global identity", generated)
            self.assertIn("identity:", generated)

    def test_stack_locals_function_calls_and_control_flow(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "flow.c"
            assembly = Path(temporary) / "flow.s"
            object_file = Path(temporary) / "flow.vxo"
            merged = Path(temporary) / "flow-merged.vxo"
            executable_file = Path(temporary) / "flow.bin"
            source.write_text(
                "int increment(int value) { return value + 1; }\n"
                "int total(int count) {\n"
                "  int sum = 0;\n"
                "  while (count > 0) {\n"
                "    sum += increment(increment(count));\n"
                "    count -= 1;\n"
                "  }\n"
                "  if (sum == 0) return 7;\n"
                "  return sum;\n"
                "}\n",
                encoding="utf-8",
            )
            run([self.cc, "-S", "-o", assembly, source])
            generated = assembly.read_text(encoding="utf-8")
            self.assertIn("    sd ", generated)
            self.assertIn("    ld ", generated)
            self.assertIn("    brl increment", generated)
            self.assertIn("    la x30, Lcc_", generated)
            self.assertNotIn("    la x30, increment\n", generated)
            self.assertIn("    mveq x31", generated)
            run([self.assembler, "-c", "-o", object_file, assembly])
            self.assertGreater(object_file.stat().st_size, 0)
            run([self.linker, "-r", "-o", merged, object_file])
            self.assertGreater(merged.stat().st_size, 0)
            run([self.linker, "-e", "total", "-o", executable_file, object_file])
            self.assertGreater(executable_file.stat().st_size, 0)

    def test_scalar_types_use_narrow_stack_accesses(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "scalar.c"
            assembly = Path(temporary) / "scalar.s"
            source.write_text(
                "int value(void) {\n"
                "  signed char small = -1;\n"
                "  unsigned short positive = 9;\n"
                "  return small + positive;\n"
                "}\n",
                encoding="utf-8",
            )
            run([self.cc, "-S", "-o", assembly, source])
            generated = assembly.read_text(encoding="utf-8")
            self.assertIn("sb ", generated)
            self.assertIn("lb ", generated)
            self.assertIn("sxtb ", generated)
            self.assertIn("sh ", generated)
            self.assertIn("lh ", generated)
            self.assertIn("zxth ", generated)

    def test_calls_resolve_same_translation_unit_static_functions(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "static-call.c"
            assembly = Path(temporary) / "static-call.s"
            source.write_text(
                "static int helper(int value) { return value; }\n"
                "int public_function(int value) { return helper(value); }\n",
                encoding="utf-8",
            )
            run([self.cc, "-S", "-o", assembly, source])
            generated = assembly.read_text(encoding="utf-8")
            self.assertIn("Lcc_0_helper:", generated)
            self.assertIn("brl Lcc_0_helper", generated)

    def test_brl_assembles_and_links_as_a_call(self):
        with tempfile.TemporaryDirectory() as temporary:
            assembly = Path(temporary) / "call.s"
            assembly.write_text(
                ".text\n"
                ".global entry\n"
                ".global callee\n"
                "entry:\n"
                "  brl callee\n"
                "  brlr\n"
                "callee:\n"
                "  brlr\n",
                encoding="utf-8",
            )
            for arch in ("vhx64imsa", "vhx32im"):
                with self.subTest(arch=arch):
                    object_file = Path(temporary) / f"call-{arch}.vxo"
                    executable_file = Path(temporary) / f"call-{arch}.bin"
                    run([
                        self.assembler, "-march=" + arch, "-c", "-o",
                        object_file, assembly,
                    ])
                    run([
                        self.linker, "-e", "entry", "-o", executable_file,
                        object_file,
                    ])
                    self.assertGreater(executable_file.stat().st_size, 0)

    def test_local_pc12_relocation_relaxes_or_falls_back_by_range(self):
        image_sizes = {}
        with tempfile.TemporaryDirectory() as temporary:
            temporary = Path(temporary)
            for name, padding in (
                ("near", 0),
                ("edge", 2043),
                ("outside", 2044),
                ("far", 2048),
            ):
                assembly = temporary / f"{name}.s"
                object_file = temporary / f"{name}.vxo"
                executable_file = temporary / f"{name}.bin"
                assembly.write_text(
                    ".text\n"
                    ".global entry\n"
                    "entry:\n"
                    "  la pc, target\n"
                    f"  .space {padding}\n"
                    "target:\n"
                    "  nop\n",
                    encoding="utf-8",
                )
                run([self.assembler, "-c", "-o", object_file, assembly])
                run([
                    self.linker, "-e", "entry", "-o", executable_file,
                    object_file,
                ])
                image_sizes[name] = executable_file.stat().st_size

        self.assertEqual(image_sizes["edge"] - image_sizes["near"], 2043)
        self.assertGreater(image_sizes["outside"], image_sizes["edge"] + 1)
        self.assertGreater(image_sizes["far"], image_sizes["outside"])

    def test_recursive_call_preserves_live_expression_temporaries(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "recursive.c"
            assembly = Path(temporary) / "recursive.s"
            object_file = Path(temporary) / "recursive.vxo"
            source.write_text(
                "int factorial(int n) {\n"
                "  if (n <= 1) return 1;\n"
                "  return n * factorial(n - 1);\n"
                "}\n",
                encoding="utf-8",
            )
            run([self.cc, "-S", "-o", assembly, source])
            generated = assembly.read_text(encoding="utf-8")
            self.assertIn("    sd x13,", generated)
            self.assertIn("    ld x13,", generated)
            run([self.assembler, "-c", "-o", object_file, assembly])
            self.assertGreater(object_file.stat().st_size, 0)

    def test_vhx32_uses_32_bit_stack_and_address_sequences(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "32bit.c"
            assembly = Path(temporary) / "32bit.s"
            object_file = Path(temporary) / "32bit.vxo"
            source.write_text(
                "int identity(int value) { return value; }\n"
                "int caller(int value) { int local = identity(value); return local; }\n",
                encoding="utf-8",
            )
            run([self.cc, "-S", "-march=vhx32im", "-o", assembly, source])
            generated = assembly.read_text(encoding="utf-8")
            self.assertIn("    sw ", generated)
            self.assertIn("    lw ", generated)
            self.assertIn("    brl identity", generated)
            run([self.assembler, "-march=vhx32im", "-c", "-o", object_file, assembly])
            self.assertGreater(object_file.stat().st_size, 0)

    def test_object_output_still_works(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "basic.vxo"
            merged = Path(temporary) / "merged.vxo"
            run([self.cc, "-c", "-o", output, SOURCE])
            run([self.linker, "-r", "-o", merged, output])
            self.assertGreater(merged.stat().st_size, 0)

    def test_unsupported_for_loop_fails_with_diagnostic(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "unsupported.c"
            output = Path(temporary) / "unsupported.s"
            source.write_text(
                "int f(void) { for (;;) return 1; }\n",
                encoding="utf-8",
            )
            result = run([self.cc, "-S", "-o", output, source], expected=1)
            self.assertIn("control-flow statements are not implemented", result.stderr)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
