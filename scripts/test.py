#!/usr/bin/env python3
"""Run logic and real FastLED rendering tests without lamp hardware."""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument(
    "--fastled-dir", type=Path,
    default=ROOT / ".pio/libdeps/nanoatmega328/FastLED",
    help="Path to the installed FastLED library (for custom PlatformIO build directories)",
)
args = parser.parse_args()
fastled = args.fastled_dir.resolve() / "src"
if not (fastled / "FastLED.h").is_file():
    parser.error("FastLED is missing; run 'pio pkg install --environment nanoatmega328' first")

with tempfile.TemporaryDirectory(prefix="lampe-tests-") as output:
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    flags = [
        "-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror", "-pedantic",
        "-fsanitize=undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer",
        "-I", str(ROOT / "lib/LampLogic"),
        "-I", str(ROOT / "lib/Programs"),
    ]
    suites = {
        "test_logic": ([], ["tests/test_logic.cpp", "lib/Programs/ProgramMenu.cpp"]),
        "test_rendering": (
            [
                "-DFASTLED_STUB_IMPL=1",
                # Treat external headers as system headers; keep our own warnings strict.
                "-isystem", str(fastled),
                "-isystem", str(fastled / "platforms/stub"),
                "-I", str(ROOT / "lib/Lampe"),
                # Discard unused library functions that depend on hardware/runtime I/O.
                "-ffunction-sections", "-fdata-sections",
                "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections",
            ],
            ["tests/test_rendering.cpp", "tests/fastled_colors.cpp",
             "lib/Programs/Programs.cpp", "lib/Programs/ProgramMenu.cpp"],
        ),
    }
    for name, (extra_flags, sources) in suites.items():
        executable = Path(output) / name
        subprocess.run(
            compiler + flags + extra_flags
            + [str(ROOT / source) for source in sources]
            + ["-o", str(executable)],
            check=True,
        )
        subprocess.run([str(executable)], check=True)
