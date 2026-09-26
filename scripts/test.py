#!/usr/bin/env python3
"""Compile and run hardware-independent firmware tests with the host C++ compiler."""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix="lampe-tests-") as output:
    executable = Path(output) / "test_logic"
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    subprocess.run(
        compiler + [
            "-std=c++11", "-Wall", "-Wextra", "-Werror", "-pedantic",
            "-fsanitize=undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer",
            "-I", str(ROOT / "lib/LampLogic"),
            "-I", str(ROOT / "lib/Programs"),
            str(ROOT / "tests/test_logic.cpp"),
            str(ROOT / "lib/Programs/ProgramMenu.cpp"),
            "-o", str(executable),
        ],
        check=True,
    )
    subprocess.run([str(executable)], check=True)
