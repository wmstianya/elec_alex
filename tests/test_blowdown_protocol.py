"""Compile the paired production control cores against one deterministic valve."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run():
    compiler = shutil.which(os.environ.get("HOST_CC", "gcc"))
    if not compiler:
        raise RuntimeError("Set HOST_CC to a native GCC executable.")
    master = ROOT / "UART-elec/SYSTEM/system_control"
    slave = ROOT / "UART/SYSTEM/system_control"
    sources = [ROOT / "tests/test_blowdown_protocol.c", master / "union_blowdown_core.c"]
    sources += [slave / name for name in ["water_level_core.c", "blowdown_core.c", "slave_water_control.c"]]
    with tempfile.TemporaryDirectory(prefix="blowdown-protocol-") as output:
        executable = Path(output) / "protocol-tests.exe"
        subprocess.run([compiler, "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
                        "-I", str(master), "-I", str(slave), *map(str, sources), "-o", str(executable)], check=True)
        return subprocess.run([str(executable)]).returncode


if __name__ == "__main__":
    raise SystemExit(run())
