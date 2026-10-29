"""Regression fixtures for cleanup guardrails; no game, network or production data."""
from pathlib import Path
import json
import re
import runpy
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
from repository_hygiene import validate_repository, validate_markdown
validate_project_entries = runpy.run_path(str(ROOT / "scripts/Test-Infrastructure.py"))["validate_project_entries"]

# Captured before source/class renaming. Intentionally independent of the catalog
# and settings manifest: removing an entry from both must still fail this test.
BASELINE_IDS = """
friends_manager click_gui panic anti_bot
legit_click_aim_assist blatant_click_aim_assist aim_assist triggerbot
watermark arraylist
chams china_hat wings cape sky trails hit_particles jump_circles player_esp item_esp storage_esp block_esp radar spotify_hud
full_bright jump_reset shield_breaker auto_totem pearl_macro
sprint no_jump_delay
fast_place fast_exp
pot_refill mouse_tweaks vape_aim_assist
""".split()


class RepositoryHygieneTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="onestep-hygiene-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.write("resource.h", "#define IDR_TEST 101\n")
        self.write("assets.rc", 'IDR_TEST RCDATA "assets/test.bin"\n')
        self.write("assets/test.bin", "resource fixture")

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path

    def test_empty_source_is_rejected_but_vendor_archive_and_cache_are_excluded(self):
        for name in ("include/glm/empty.inl", "server/node_modules/x/empty.js",
                     "server/archive/wolf/history.js", "server/.credentials/local.json"):
            self.write(name, "")
        self.assertEqual(validate_repository(self.root), [])
        self.write("src/empty.cpp", " \n\t")
        self.assertTrue(any("Empty first-party" in e for e in validate_repository(self.root)))

    def test_quoted_includes_follow_real_include_roots(self):
        self.write("include/imgui/imgui.h", "// vendor fixture")
        self.write("src/a.cpp", '#include "imgui/imgui.h"\n')
        self.assertEqual(validate_repository(self.root), [])
        self.write("src/a.cpp", '#include "removed.hpp"\n')
        self.assertTrue(any("Broken quoted include" in e for e in validate_repository(self.root)))

    def test_static_and_dynamic_relative_js_imports(self):
        self.write("config-service/helper.mjs", "export const value = 1;")
        self.write("config-service/worker.js", "import {value} from './helper.mjs';\nimport('node:crypto');")
        self.assertEqual(validate_repository(self.root), [])
        self.write("config-service/worker.js", "await import('./removed.mjs');")
        self.assertTrue(any("Broken JS import" in e for e in validate_repository(self.root)))

    def test_resources_include_generated_classes_and_reject_missing_or_duplicate_ids(self):
        self.write("assets.rc", 'IDR_TEST RCDATA "assets\\\\test.bin"\n')
        self.assertEqual(validate_repository(self.root), [])
        self.write("assets.rc", 'IDR_TEST RCDATA "assets/Bridge.class"\n')
        self.assertTrue(any("Missing resource file" in e for e in validate_repository(self.root)))
        self.write("assets/Bridge.class", "generated class fixture")
        self.assertEqual(validate_repository(self.root), [])
        self.write("assets.rc", 'IDR_TEST RCDATA "assets/test.bin"\nIDR_TEST RCDATA "assets/test.bin"\n')
        self.assertTrue(any("Duplicate resource ID" in e for e in validate_repository(self.root)))
        self.write("assets.rc", 'IDR_MISSING RCDATA "assets/test.bin"\n')
        self.assertTrue(any("Undefined resource ID" in e for e in validate_repository(self.root)))

    def test_markdown_files_headings_unicode_and_code_examples(self):
        self.write("docs/guide.md", "# Başlangıç\n\n## Test\n\n## Test\n")
        path = self.write("README.md", "[guide](docs/guide.md#başlangıç) [repeat](docs/guide.md#test-1) "
                          "[web](https://example.invalid/skip)\n```md\n[x](not-a-real-file)\n```\n")
        self.assertEqual(validate_markdown(path), [])
        self.write("README.md", "[bad](docs/guide.md#missing) [gone](gone.md)")
        self.assertEqual(len(validate_markdown(path)), 2)

    def test_all_36_module_ids_and_order_are_preserved(self):
        catalog = (ROOT / "src/modules/module_catalog.cpp").read_text(encoding="utf-8")
        ids = re.findall(r'\{\s*"([a-z0-9_]+)"\s*,\s*&', catalog)
        self.assertEqual(len(BASELINE_IDS), 36)
        self.assertEqual(ids, BASELINE_IDS)
        manifest = json.loads((ROOT / "src/modules/settings_manifest.json").read_text(encoding="utf-8"))
        self.assertEqual({entry["id"] for entry in manifest["modules"]}, set(BASELINE_IDS))

    def test_solution_and_client_build_paths(self):
        self.write("OneStep.slnx", '<Solution><Project Path="OneStep.vcxproj"/></Solution>')
        self.write("OneStep.vcxproj", '<Project><ClCompile Include="src/source.cpp"/></Project>')
        self.write("src/source.cpp", "// source fixture")
        self.assertEqual(validate_repository(self.root), [])
        self.write("OneStep.vcxproj", '<Project><ClCompile Include="removed.cpp"/></Project>')
        self.assertTrue(any("Missing build file" in e for e in validate_repository(self.root)))

    def test_duplicate_build_entries_are_not_hidden_by_set_comparison(self):
        item = ("ClCompile", "src/source.cpp")
        self.assertEqual(validate_project_entries([item], [item]), [])
        self.assertIn("Duplicate project/filter entries", validate_project_entries([item, item], [item]))
        self.assertIn("Duplicate project/filter entries", validate_project_entries([item], [item, item]))
        self.assertTrue(validate_project_entries([item], []))


if __name__ == "__main__":
    unittest.main()
