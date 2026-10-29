"""Preserve the native WorldBridge binding list, including accidental duplicates."""
from collections import Counter
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent.parent


def write_native_bindings(output: Path) -> Path:
    source = (ROOT / "src/services/game/world_bridge.cpp").read_text(encoding="utf-8")
    start = source.index("static constexpr binding names[]{")
    table = source[start:source.index("\n        };", start)]
    compiler = shutil.which("clang++") or shutil.which("g++")
    if not compiler:
        raise RuntimeError("A C++ preprocessor (clang++ or g++) is required for the real native binding table")
    for target in (12111, 26100, 26101, 26102):
        # Runtime spellings are unnecessary for the duplicate-key check. 1.21.11
        # actually uses these intermediary spellings, so its full list feeds Java.
        defines = f"#define ONESTEP_MC_NAME(x) x\n#define ONESTEP_MINECRAFT_VERSION {target}\n#define ONESTEP_MINECRAFT_26_1 {int(target >= 26100)}\n"
        parsed = subprocess.check_output([compiler, "-E", "-P", "-x", "c++", "-"], input=defines + table, text=True)
        pairs = re.findall(r'\{"([^"\n]+)",\s*"([^"\n]+)"\}', parsed)
        if not pairs:
            raise AssertionError("Native WorldBridge binding table was empty")
        duplicates = {key: count for key, count in Counter(key for key, _ in pairs).items() if count > 1}
        if duplicates:
            raise AssertionError(f"WorldBridge {target} rejects duplicate native bindings: {duplicates}")
        if target == 12111:
            output.write_text("\n".join(part for pair in pairs for part in pair) + "\n", encoding="utf-8")
    return output
