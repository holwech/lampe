#!/usr/bin/env python3
"""Replay the checked-in 100 Hz features without requiring the original audio."""
import argparse
import csv
import gzip
import hashlib
import io
import json
from pathlib import Path
import subprocess

from compare import summarize


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--fixtures", type=Path, default=Path(__file__).parent / "recordings")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    versions = json.loads(subprocess.check_output([str(args.executable.resolve()), "--info"], text=True))
    results = {"versions": versions, "captures": {}}
    for case in json.loads((args.fixtures / "manifest.json").read_text()):
        source = args.fixtures / case["file"]
        assert hashlib.sha256(source.read_bytes()).hexdigest() == case["sha256"]
        with gzip.open(source, "rt") as file:
            rows = list(csv.DictReader(file))
        assert len(rows) == case["windows"]
        assert [int(r["time_ms"]) for r in rows] == list(range(10, 10 * len(rows) + 1, 10))
        methods = {}
        for method in case["methods"]:
            header = "time_ms,peak\n" if method == "lamp-onsets" else "time_ms,peak,onset\n"
            recording = header + "".join(f"{r['time_ms']},{r['peak']}" +
                                        (f",{r[method]}" if method != "lamp-onsets" else "") + "\n" for r in rows)
            runs = {}
            for initial in ["default", "100", "150"]:
                command = [str(args.executable.resolve())] + ([] if initial == "default" else [initial])
                output = subprocess.check_output(command, input=recording, text=True)
                predictions = [{k: float(v) for k, v in r.items()} for r in csv.DictReader(io.StringIO(output))]
                assert len(predictions) == len(rows)
                assert [r["onset"] for r in predictions] == [float(r["lamp-onsets"]) for r in rows], "Lamp onset implementation changed; review feature provenance"
                runs[initial] = summarize(predictions, case["reference_bpm"])
            methods[method] = runs
        results["captures"][case["name"]] = methods
        print(case["name"], "replayed", flush=True)
    with args.output.open("x") as file:
        json.dump(results, file, indent=2)
        file.write("\n")


if __name__ == "__main__":
    main()
