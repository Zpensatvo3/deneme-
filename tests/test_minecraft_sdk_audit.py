"""Regression tests for exhaustive SDK extraction and fail-closed classfile checks."""
import importlib.util
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from minecraft_classfile import ClassArchive
from test_minecraft26_readiness import class_fixture

spec = importlib.util.spec_from_file_location("sdk_audit", Path(__file__).resolve().parents[1] / "scripts/Test-MinecraftSdk.py")
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


class SdkAuditTests(unittest.TestCase):
    def test_exact_release_fixture_rejects_wrong_version_and_modified_jar(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "client-26.1.1.jar"
            path.write_bytes(b"test client fixture")
            metadata = {"id": "26.1.1", "type": "release", "downloads": {"client": {
                "size": path.stat().st_size, "sha1": hashlib.sha1(path.read_bytes()).hexdigest()}}}
            metadata_path = path.parent / "version-26.1.1.json"
            metadata_path.write_text(json.dumps(metadata), encoding="utf-8")
            sdk.validate_release_fixture(path, "26.1.1")
            for key, value in (("id", "26.1.2"), ("type", "snapshot")):
                invalid = {**metadata, key: value}
                metadata_path.write_text(json.dumps(invalid), encoding="utf-8")
                with self.subTest(key=key), self.assertRaises(ValueError):
                    sdk.validate_release_fixture(path, "26.1.1")
            metadata_path.write_text(json.dumps(metadata), encoding="utf-8")
            path.write_bytes(b"other client fixture")
            with self.assertRaises(ValueError): sdk.validate_release_fixture(path, "26.1.1")

    def test_class_comparison_includes_changed_parents_and_interfaces(self):
        with tempfile.TemporaryDirectory() as directory:
            baseline, target = Path(directory) / "baseline.jar", Path(directory) / "target.jar"
            for path in (baseline, target):
                with zipfile.ZipFile(path, "w") as jar:
                    jar.writestr("A.class", class_fixture("A", "Base", interfaces=["Interface"]))
                    jar.writestr("Base.class", class_fixture("Base", members=[
                        ("method", "value", "()I", 1 if path == baseline else 9)]))
                    jar.writestr("Interface.class", class_fixture("Interface", members=[
                        ("method", "name", "()I" if path == baseline else "()J", 1)]))
                    jar.writestr("Unreferenced.class", class_fixture("Unreferenced", members=[
                        ("field", "unused", "I" if path == baseline else "J", 1)]))
            with ClassArchive(target) as archive:
                archive.archives = [archive]
                comparison = sdk.compare_classes([{"owner": "A"}], archive, baseline)
                self.assertEqual(comparison["client_classes_compared"], 3)
                self.assertEqual(comparison["changed_classes"], ["Base", "Interface"])

    def test_dollar_names_nested_bindings_and_temporaries(self):
        classes, members = sdk.declarations('''
KLASS_DECLARATION(Forward, "game/Forward")
BEGIN_KLASS_DEF(Foo$Bar, "game/Foo$Bar")
jni::constructor<jint> constructor{*this};
struct view { jni::field<jni::array<Foo$Bar>, "data"> value; };
auto other = jni::field<jni::array<Foo$Bar>, "data">{*this};
END_KLASS_DEF()
BEGIN_KLASS_MEMBERS(Forward)
jni::method<void, "run", jni::NOT_STATIC, Foo$Bar> run{*this};
END_KLASS_MEMBERS()
''', "maps")
        self.assertEqual(classes, ["maps::Foo$Bar", "maps::Forward"])
        self.assertEqual(members, [("maps::Foo$Bar", "constructor"), ("maps::Foo$Bar", "view::value"), ("maps::Forward", "run")])

    def test_unsupported_or_incomplete_declarations_fail(self):
        for source in ['END_KLASS_DEF()', 'BEGIN_KLASS_DEF(A, "a")',
                       'BEGIN_KLASS_DEF(A, "a") jni::multi_method<void,"x","y"> x; END_KLASS_DEF()',
                       'BEGIN_KLASS_DEF(A, "a") jni::method<void,"x"> declaration(); END_KLASS_DEF()',
                       'BEGIN_KLASS_DEF(A, "a") jni::field<jni::array<A> x; END_KLASS_DEF()']:
            with self.subTest(source=source), self.assertRaises(ValueError): sdk.declarations(source, "maps")

    def test_preprocessor_headers_must_all_be_seen(self):
        with patch.object(sdk, "HEADERS", {"src/example.hpp": "maps"}):
            with self.assertRaises(ValueError): sdk.selected_sources('# 1 "/usr/include/test.hpp"\nignored')
            sources = sdk.selected_sources('# 1 "src/example.hpp"\nselected\n# 1 "/usr/include/test.hpp"\nignored')
            self.assertEqual(sources, {"src/example.hpp": "selected"})

    def test_raw_jni_argument_parser_preserves_macro_descriptors(self):
        self.assertEqual(sdk.arguments('owner, ONESTEP_MC_NAME("method_1"), ONESTEP_MC_NAME("(Ljava/lang/Object;)V")'),
                         ['owner', 'ONESTEP_MC_NAME("method_1")', 'ONESTEP_MC_NAME("(Ljava/lang/Object;)V")'])

    def test_actual_member_static_and_descriptor_are_required(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.jar"
            with zipfile.ZipFile(path, "w") as jar:
                jar.writestr("A.class", class_fixture("A", members=[("method", "is", "(Ljava/lang/Object;)Z", 1), ("field", "f", "I", 8)]))
                jar.writestr("java/lang/Object.class", class_fixture("java/lang/Object"))
            with ClassArchive(path) as archive:
                records = [["method", "correct", "A", "is", "(Ljava/lang/Object;)Z", "0"],
                           ["method", "wrong_type", "A", "is", "(I)Z", "0"],
                           ["field", "wrong_static", "A", "f", "I", "0"],
                           ["class", "missing", "Absent"]]
                self.assertEqual([r["status"] for r in sdk.inspect(records, archive)],
                                 ["present", "missing_member", "wrong_static", "missing_class"])

    def test_missing_descriptor_type_fails_even_if_member_exists(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.jar"
            with zipfile.ZipFile(path, "w") as jar:
                jar.writestr("A.class", class_fixture("A", members=[("method", "value", "()LMissing;", 1)]))
            with ClassArchive(path) as archive:
                result = sdk.inspect([["method", "value", "A", "value", "()LMissing;", "0"]], archive)[0]
                self.assertEqual(result["status"], "missing_descriptor_type")
                self.assertEqual(result["missing_types"], ["Missing"])


if __name__ == "__main__":
    unittest.main()
