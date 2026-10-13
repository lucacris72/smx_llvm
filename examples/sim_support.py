"""Run bare-metal firmware with the repository's Verilator core."""

from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess


def run_logged(command: list[str], log: Path, output: Path | None = None) -> None:
    log.parent.mkdir(parents=True, exist_ok=True)
    with ExitStack() as files:
        stream = files.enter_context(log.open("a"))
        stdout = files.enter_context(output.open("w")) if output is not None else stream
        stream.write(f"$ {shlex.join(command)}\n")
        stream.flush()
        result = subprocess.run(command, stdout=stdout, stderr=stream)
    if result.returncode:
        detail = "\n".join(log.read_text(errors="replace").splitlines()[-30:])
        raise RuntimeError(f"command failed ({result.returncode}); see {log}\n{detail}")


def prepare_simulator(repo: Path, log: Path | None = None) -> Path:
    core = repo / "hardware/smx/example_tb/core"
    command = ["make", "-C", str(core), "verilator-build", "smx-luts"]
    if log is None:
        subprocess.run(command, check=True)
    else:
        run_logged(command, log)
    return core


def prepare_event_simulator(core: Path, build: Path) -> Path:
    target = build / "simulator"
    stamp = target / "signature.json"
    driver = Path(__file__).with_name("sim_event_main.cpp")
    signature = {name: hashlib.sha256(path.read_bytes()).hexdigest()
                 for name, path in {
                     "rtl_model": core / "obj_dir/Vtb_top",
                     "hardware_driver": core / "sim_main.cpp",
                     "event_driver": driver,
                 }.items()}
    if stamp.exists() and json.loads(stamp.read_text()) == signature and (target / "obj_dir/Vtb_top").exists():
        return target
    # Only the C++ driver is rebuilt; the generated RTL model is unchanged.
    shutil.copytree(core / "obj_dir", target / "obj_dir", dirs_exist_ok=True)
    run_logged([
        "make", "-s", "-C", str(target / "obj_dir"), "-f", "Vtb_top.mk", "Vtb_top",
        "VM_USER_CLASSES=sim_event_main", f"VM_USER_DIR={driver.parent}",
        f'VM_USER_CFLAGS=-O2 -std=c++20 -I"{core}"',
    ], build / "simulator-build.log")
    stamp.write_text(json.dumps(signature, indent=2) + "\n")
    return target


def validate_exit(output: str) -> None:
    if "EXIT SUCCESS" not in output or "EXIT FAILURE" in output or "TEST(S) FAILED" in output:
        raise ValueError("firmware did not report EXIT SUCCESS")


def run_firmware(core: Path, firmware: Path, run_dir: Path,
                 timeout: int = 120, maxcycles: int = 5_000_000, quiet: bool = False) -> str:
    run_dir.mkdir(parents=True, exist_ok=True)
    for name in ("smx_lut1d.hex", "smx_lut2d.hex"):
        shutil.copy2(core / name, run_dir / name)
    log = run_dir / "simulation.log"
    if not quiet:
        print(f"Simulating {firmware.name}; log: {log}", flush=True)
    try:
        result = subprocess.run(
            [str(core / "obj_dir/Vtb_top"), f"+firmware={firmware.resolve()}",
             f"+maxcycles={maxcycles}"],
            cwd=run_dir, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as error:
        output = error.stdout or ""
        if isinstance(output, bytes):
            output = output.decode(errors="replace")
        log.write_text(output + f"\nTIMEOUT after {timeout} seconds\n")
        raise RuntimeError(f"Verilator timed out; see {log}") from error
    log.write_text(result.stdout)
    if result.returncode:
        raise RuntimeError(f"Verilator returned {result.returncode}; see {log}")
    validate_exit(result.stdout)
    return result.stdout
