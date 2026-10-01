"""Run bare-metal firmware with the repository's Verilator core."""

from pathlib import Path
import shutil
import subprocess


def prepare_simulator(repo: Path) -> Path:
    core = repo / "hardware/smx/example_tb/core"
    subprocess.run(["make", "-C", str(core), "verilator-build", "smx-luts"], check=True)
    return core


def validate_exit(output: str) -> None:
    if "EXIT SUCCESS" not in output or "EXIT FAILURE" in output or "TEST(S) FAILED" in output:
        raise ValueError("firmware did not report EXIT SUCCESS")


def run_firmware(core: Path, firmware: Path, run_dir: Path,
                 timeout: int = 120, maxcycles: int = 5_000_000) -> str:
    run_dir.mkdir(parents=True, exist_ok=True)
    for name in ("smx_lut1d.hex", "smx_lut2d.hex"):
        shutil.copy2(core / name, run_dir / name)
    result = subprocess.run(
        [str(core / "obj_dir/Vtb_top"), f"+firmware={firmware.resolve()}",
         f"+maxcycles={maxcycles}"],
        cwd=run_dir, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        timeout=timeout,
    )
    (run_dir / "simulation.log").write_text(result.stdout)
    if result.returncode:
        raise RuntimeError(f"Verilator returned {result.returncode}; see {run_dir / 'simulation.log'}")
    validate_exit(result.stdout)
    return result.stdout
