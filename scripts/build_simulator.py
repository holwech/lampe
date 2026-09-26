#!/usr/bin/env python3
"""Compile the actual lamp engine and FastLED color math to WebAssembly."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--fastled-dir", type=Path, default=ROOT / ".pio/libdeps/nanoatmega328/FastLED")
args = parser.parse_args()
compiler = os.environ.get("EMXX") or shutil.which("em++")
if not compiler:
    parser.error("em++ is missing. Install the SDK version in .emscripten-version and source emsdk_env.sh (see README).")
fastled = args.fastled_dir.resolve() / "src"
if not (fastled / "FastLED.h").is_file():
    parser.error("FastLED is missing. Run: uv run --locked pio pkg install --environment nanoatmega328")
output = ROOT / "web/public/generated"
output.mkdir(parents=True, exist_ok=True)
exports = ["reset", "select", "brightness", "button", "advance", "frame", "frame_size", "audio_data", "audio_size", "program_count", "frame_interval_us", "program_name", "program_uses_audio"]
command = [compiler, "-std=c++17", "-O2", "-DFASTLED_STUB_IMPL=1",
           "-ffunction-sections", "-fdata-sections", "-isystem", str(fastled),
           "-isystem", str(fastled / "platforms/stub")]
for library in ("LampEngine", "LampLogic", "Programs", "Telemetry"):
    command += ["-I", str(ROOT / "lib" / library)]
command += [str(ROOT / path) for path in (
    "simulator/bridge.cpp", "simulator/fastled_colors.cpp", "lib/LampEngine/LampEngine.cpp",
    "lib/Programs/Programs.cpp", "lib/Programs/ProgramMenu.cpp")]
command += ["-sMODULARIZE=1", "-sEXPORT_ES6=1", "-sENVIRONMENT=web,node",
            "-sFILESYSTEM=0", "-sALLOW_MEMORY_GROWTH=1",
            "-sEXPORTED_FUNCTIONS=" + str(["_lamp_" + name for name in exports]),
            "-sEXPORTED_RUNTIME_METHODS=['HEAPU8','UTF8ToString']",
            "-o", str(output / "lamp.js")]
subprocess.run(command, check=True, cwd=ROOT)
print("Built shared C++ lamp engine → web/public/generated/lamp.wasm")
