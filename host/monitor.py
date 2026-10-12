#!/usr/bin/env python3
"""Capture LOG lines from the sensor node over USB serial into a CSV.

Exit status:
    0  at least --min-samples rows were captured
    1  otherwise, including a dead port
"""
import argparse
import csv
import re
import sys
import time
from datetime import datetime, timezone

import serial

# Matches: LOG temp=24.50 voltage=3.31 samples=1 sensor=ok
# The trailing sensor= field is optional so older firmware still parses.
LINE = re.compile(r"LOG temp=(-?[\d.]+) voltage=(-?[\d.]+) samples=(\d+)"
                  r"(?: sensor=(\w+))?")


def now():
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def parse(line):
    m = LINE.search(line)
    if not m:
        return None
    return {"temp_c": m.group(1), "voltage": m.group(2), "samples": m.group(3),
            "sensor": m.group(4) or ""}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--seconds", type=int, default=30)
    ap.add_argument("--out", default="sensor-log.csv")
    ap.add_argument("--min-samples", type=int, default=1)
    args = ap.parse_args()

    rows = []
    port_error = None
    try:
        ser = serial.Serial(args.port, 115200, timeout=2)
    except (serial.SerialException, OSError) as exc:
        print(f"cannot open {args.port}: {exc}", file=sys.stderr)
        return 1

    t0 = time.time()
    with ser, open(args.out, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["timestamp", "temp_c", "voltage", "samples", "sensor"])
        while time.time() - t0 < args.seconds:
            try:
                line = ser.readline().decode(errors="ignore").strip()
            except (serial.SerialException, OSError) as exc:
                port_error = str(exc)
                print(f"port error: {exc}", file=sys.stderr)
                break
            rec = parse(line)
            if not rec:
                continue
            ts = now()
            writer.writerow([ts, rec["temp_c"], rec["voltage"], rec["samples"], rec["sensor"]])
            rows.append(rec)
            print(f"{ts}  temp={rec['temp_c']}C  v={rec['voltage']}V"
                  f"  sensor={rec['sensor'] or '-'}")

    print(f"captured {len(rows)} samples -> {args.out}")
    if len(rows) < args.min_samples:
        print(f"expected at least {args.min_samples} samples", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
