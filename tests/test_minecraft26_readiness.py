"""Offline tests for the port audit; no downloads, JVM or Minecraft installation."""
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from minecraft_classfile import ClassArchive, java_descriptor, read_class, read_intermediary, read_proguard, remap_descriptor

spec = importlib.util.spec_from_file_location("readiness", Path(__file__).resolve().parents[1] / "scripts/Test-Minecraft26Readiness.py")
readiness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(readiness)
names_spec = importlib.util.spec_from_file_location("names_generator", Path(__file__).resolve().parents[1] / "scripts/Generate-Minecraft261Names.py")
names_generator = importlib.util.module_from_spec(names_spec)
names_spec.loader.exec_module(names_generator)


def class_fixture(name, parent=None, members=(), interfaces=()):
    """Structural classfile fixture, not executable Java bytecode."""
    pool = []
    def utf8(text):
        data = text.encode()
        pool.append(b"\x01" + struct.pack(">H", len(data)) + data)
        return len(pool)
    def klass(text):
        index = utf8(text)
        pool.append(b"\x07" + struct.pack(">H", index))
        return len(pool)
    owner, base = klass(name), klass(parent) if parent else 0
    implemented = [klass(i) for i in interfaces]
    encoded = {"field": [], "method": []}
    for kind, member, descriptor, flags in members:
        encoded[kind].append(struct.pack(">HHHH", flags, utf8(member), utf8(descriptor), 0))
    return (struct.pack(">IHHH", 0xCAFEBABE, 0, 69, len(pool) + 1) + b"".join(pool)
            + struct.pack(">HHHH", 0x401, owner, base, len(implemented))
            + b"".join(struct.pack(">H", i) for i in implemented)
            + b"".join(struct.pack(">H", len(encoded[k])) + b"".join(encoded[k]) for k in ("field", "method"))
            + b"\x00\x00")


class ReadinessTests(unittest.TestCase):
    def test_names_generator_uses_only_supported_baseline(self):
        self.assertEqual(names_generator.VERSIONS, ("1.21.11",))

    def test_native_261_overrides_are_explicit(self):
        self.assertEqual(names_generator.translate("net/minecraft/class_1713", {}, {}), "net/minecraft/world/inventory/ContainerInput")
        self.assertEqual(names_generator.translate("method_2906", {}, {}), "handleContainerInput")
        self.assertEqual(names_generator.translate("field_56967", {}, {}), "itemTransform")
        self.assertEqual(names_generator.translate("(Lnet/minecraft/class_1713;)V", {}, {}), "(Lnet/minecraft/world/inventory/ContainerInput;)V")

    def test_native_261_unmapped_names_cannot_fall_back(self):
        with self.assertRaises(ValueError): names_generator.translate("method_9999999", {}, {})
        with self.assertRaises(ValueError): names_generator.translate("net/minecraft/class_9999999", {}, {})
        with self.assertRaises(ValueError): names_generator.translate("()Lnet/minecraft/class_9999999;", {}, {})

    def test_native_261_generated_table_has_no_identity_default(self):
        table = names_generator.render({"method_123"}, {}, {"method_123": "namedMethod"})
        self.assertIn('template<jni::string_litteral Source> struct symbol;', table)
        self.assertIn('symbol<"method_123">', table)
        self.assertIn('jni::string_litteral("namedMethod")', table)
        self.assertNotIn("return Source", table)

    def test_descriptor_remapping_preserves_arrays_and_java_types(self):
        self.assertEqual(remap_descriptor("([La;Ljava/lang/String;)Lb;", {"a": "game/A", "b": "game/B"}),
                         "([Lgame/A;Ljava/lang/String;)Lgame/B;")
        self.assertEqual(java_descriptor("int[][]"), "[[I")
        self.assertEqual(java_descriptor("game.Outer$Inner[]"), "[Lgame/Outer$Inner;")

    def test_proguard_overloads_ranges_and_constructor(self):
        classes, members = read_proguard("""game.Owner -> a:
    int count -> b
    1:2:void <init>() -> <init>
    7:8:game.Owner call(java.lang.String,int[]):12:13 -> c
    void call(int) -> c
""")
        self.assertEqual(classes, {"a": "game/Owner"})
        self.assertEqual(members["a", "method", "c", "(Ljava/lang/String;[I)La;"],
                         ("game/Owner", "call", "(Ljava/lang/String;[I)Lgame/Owner;"))
        self.assertIn(("a", "method", "c", "(I)V"), members)
        self.assertIn(("a", "method", "<init>", "()V"), members)

    def test_proguard_conflicting_names_fail_closed(self):
        with self.assertRaises(ValueError):
            read_proguard("game.A -> a:\n    int first -> x\n    int second -> x\n")

    def test_classfile_members_and_java25_version(self):
        info = read_class(class_fixture("A", "B", [("field", "f", "I", 8), ("method", "m", "(I)V", 1)], ["I"]))
        self.assertEqual((info.name, info.major, info.parents), ("A", 69, ("B", "I")))
        self.assertEqual(info.members["field", "f", "I"], 8)

    def test_invalid_and_truncated_classfiles(self):
        data = class_fixture("A")
        for malformed in (b"", b"not java", data[:-1], data + b"x", data[:10] + b"\xff" + data[11:]):
            with self.subTest(length=len(malformed)), self.assertRaises(ValueError):
                read_class(malformed)

    def test_inheritance_and_constructor_resolution(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.jar"
            with zipfile.ZipFile(path, "w") as jar:
                jar.writestr("A.class", class_fixture("A", "B"))
                jar.writestr("B.class", class_fixture("B", members=[("method", "m", "()I", 1), ("method", "<init>", "()V", 1)]))
            with ClassArchive(path) as archive:
                self.assertEqual(archive.find("A", "method", "m", "()I"), ("B", 1))
                self.assertIsNone(archive.find("A", "method", "<init>", "()V"))
                self.assertIsNone(archive.get("absent"))

    def test_tiny_namespaces_and_qualified_members(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "tiny.jar"
            with zipfile.ZipFile(path, "w") as jar:
                jar.writestr("mappings/mappings.tiny", "tiny\t2\t0\tofficial\tintermediary\nc\ta\tclass_1\n\tm\t(La;)I\tx\tmethod_1\n")
            classes, members = read_intermediary(path)
            self.assertEqual(classes, {"a": "class_1"})
            self.assertEqual(members["method", "method_1", "(Lclass_1;)I"], [("a", "x", "(La;)I")])
            with zipfile.ZipFile(path, "w") as jar:
                jar.writestr("mappings/mappings.tiny", "tiny\t2\t0\tintermediary\tnamed\n")
            with self.assertRaises(ValueError): read_intermediary(path)

    def test_static_and_descriptor_changes_are_not_success(self):
        binding = {"owner": "A", "kind": "method", "name": "m", "descriptor": "()I", "static": True}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.jar"
            with zipfile.ZipFile(path, "w") as jar:
                jar.writestr("A.class", class_fixture("A", members=[("method", "m", "()I", 1)]))
                jar.writestr("B.class", class_fixture("B", members=[("method", "m", "()F", 8)]))
            with ClassArchive(path) as archive:
                self.assertEqual(readiness.inspect_binding(binding, archive)["status"], "static_changed")
                changed = readiness.inspect_binding({**binding, "owner": "B"}, archive)
                self.assertEqual(changed["status"], "descriptor_changed")
                self.assertEqual(changed["candidate_descriptors"], ["()F"])
                self.assertEqual(readiness.inspect_binding({**binding, "name": "x"}, archive)["status"], "missing_member")
                self.assertEqual(readiness.inspect_binding({**binding, "owner": "C"}, archive)["status"], "missing_class")
                self.assertEqual(readiness.inspect_binding({**binding, "static": False}, archive)["status"], "signature_present")

    def test_projection_uses_actual_declaring_class(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "base.jar"
            with zipfile.ZipFile(path, "w") as jar:
                jar.writestr("a.class", class_fixture("a", "b"))
                jar.writestr("b.class", class_fixture("b", members=[("method", "x", "()I", 1)]))
            with ClassArchive(path) as base:
                result = readiness.project_binding(["class_1", "method_1", "()I"],
                    {"a": "class_1", "b": "class_2"}, {("method", "method_1", "()I"): [("b", "x", "()I")]},
                    {"a": "game/Child", "b": "game/Parent"},
                    {("b", "method", "x", "()I"): ("game/Parent", "count", "()I")}, base)
                self.assertEqual((result["owner"], result["name"]), ("game/Child", "count"))
                with self.assertRaises(ValueError):
                    readiness.project_binding(["class_1", "method_absent", "()I"], {"a": "class_1"}, {}, {}, {}, base)

    def test_fixture_integrity_checks(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture"
            path.write_bytes(b"abc")
            readiness.validate_file(path, "a9993e364706816aba3e25717850c26c9cd0d89d", 3)
            with self.assertRaises(ValueError): readiness.validate_file(path, None, 4)
            with self.assertRaises(ValueError): readiness.validate_file(path, "0" * 40, 3)


if __name__ == "__main__":
    unittest.main()
