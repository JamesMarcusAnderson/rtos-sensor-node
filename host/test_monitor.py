"""Tests for host/monitor.py, driving the real script over a pty."""
import csv
import os
import pathlib
import pty
import subprocess
import sys
import tempfile
import time

REPO = pathlib.Path(__file__).parent.parent
MONITOR = REPO / "host" / "monitor.py"

FW_OUTPUT = (
    "LOG temp=24.50 voltage=3.31 samples=1 sensor=ok\r\n"
    "LOG temp=24.63 voltage=3.29 samples=2 sensor=ok\r\n"
    "LOG temp=24.75 voltage=3.32 samples=3 sensor=ok\r\n"
)


def run_monitor(payload, extra=(), wait=15):
    master, slave = pty.openpty()
    port = os.ttyname(slave)
    out = pathlib.Path(tempfile.mkdtemp()) / "sensor-log.csv"
    proc = subprocess.Popen(
        [sys.executable, str(MONITOR), "--port", port, "--out", str(out),
         "--seconds", "4", *extra],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    time.sleep(1.0)
    if payload:
        os.write(master, payload.encode())
        time.sleep(0.5)
    os.close(master)
    try:
        stdout, stderr = proc.communicate(timeout=wait)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.communicate()
        raise AssertionError("monitor.py hung")
    rows = list(csv.DictReader(out.open())) if out.exists() else None
    return proc.returncode, stdout, stderr, rows


def test_captures_rows_to_csv():
    code, stdout, stderr, rows = run_monitor(FW_OUTPUT)
    assert code == 0, stderr
    assert len(rows) == 3
    assert rows[0]["temp_c"] == "24.50"
    assert rows[0]["voltage"] == "3.31"
    assert rows[0]["samples"] == "1"
    assert rows[0]["sensor"] == "ok"
    assert "captured 3 samples" in stdout


def test_csv_header_is_stable():
    _, _, _, rows = run_monitor(FW_OUTPUT)
    assert list(rows[0]) == ["timestamp", "temp_c", "voltage", "samples", "sensor"]


def test_negative_temperature_parses():
    code, _, _, rows = run_monitor("LOG temp=-4.25 voltage=3.30 samples=1 sensor=ok\r\n")
    assert code == 0
    assert rows[0]["temp_c"] == "-4.25"


def test_legacy_line_without_sensor_field_still_parses():
    code, _, _, rows = run_monitor("LOG temp=24.50 voltage=3.31 samples=1\r\n")
    assert code == 0
    assert rows[0]["sensor"] == ""


def test_noise_lines_are_ignored():
    payload = "boot\r\ngarbage\r\n" + FW_OUTPUT
    code, _, _, rows = run_monitor(payload)
    assert code == 0
    assert len(rows) == 3


def test_dead_port_exits_1_without_a_traceback():
    code, _, stderr, rows = run_monitor("")
    assert code == 1
    assert "Traceback" not in stderr


def test_min_samples_enforced():
    code, _, stderr, _ = run_monitor(FW_OUTPUT, extra=["--min-samples", "10"])
    assert code == 1
    assert "expected at least 10 samples" in stderr


def test_parse_helper():
    import importlib.util
    spec = importlib.util.spec_from_file_location("monitor", MONITOR)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    assert mod.parse("nothing here") is None
    rec = mod.parse("LOG temp=1.00 voltage=2.00 samples=9 sensor=fault")
    assert rec == {"temp_c": "1.00", "voltage": "2.00", "samples": "9", "sensor": "fault"}
