#!/usr/bin/env python3
"""Emit native engine telemetry for comparison with the WebAssembly build."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
fastled = ROOT / ".pio/libdeps/nanoatmega328/FastLED/src"
with tempfile.TemporaryDirectory(prefix="lampe-trace-") as temp:
    binary = Path(temp) / "trace"
    command = shlex.split(os.environ.get("CXX", "c++")) + [
        "-std=c++17", "-O2", "-DFASTLED_STUB_IMPL=1", "-ffunction-sections", "-fdata-sections",
        "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections",
        "-isystem", str(fastled), "-isystem", str(fastled / "platforms/stub"),
    ]
    for library in ("LampEngine", "LampLogic", "Programs", "Telemetry"):
        command += ["-I", str(ROOT / "lib" / library)]
    command += [str(ROOT / path) for path in (
        "tests/bridge_trace.cpp", "simulator/bridge.cpp", "simulator/fastled_colors.cpp",
        "lib/LampEngine/LampEngine.cpp", "lib/Programs/Programs.cpp", "lib/Programs/ProgramMenu.cpp")]
    subprocess.run(command + ["-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
