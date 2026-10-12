# rtos-sensor-node
FreeRTOS sensor node on RP2040. Reads temperature over I2C, samples the supply
rail through the ADC, logs to UART, runs a supervised watchdog, and blinks a
status LED.

## Covers
Embedded Firmware Engineer (entry-level) · RTOS Software Engineer (entry-level) ·
Microcontroller Firmware Engineer · Firmware Design Engineer

## Output
See `docs/sensor-log.csv`. The `sensor` column reports `ok` or `fault` per
sample, so a missing TMP102 is visible in the log rather than silently
producing zeros.

## Quickstart
```bash
# copy build/sensor_node.uf2 onto the Pico (BOOTSEL drag-and-drop)
pip install -r host/requirements.txt
python host/monitor.py --port /dev/ttyACM0 --seconds 30
```
Exits nonzero if fewer than `--min-samples` rows were captured, and reports a
port error instead of raising.

## Task layout
| Task      | Priority | Period | Responsibility                                |
|-----------|----------|--------|-----------------------------------------------|
| watchdog  | 5        | 1s     | feed HW watchdog only when all tasks checked in |
| sensor    | 3        | 1s     | read TMP102 over I2C, sample VREF via ADC      |
| log       | 2        | 2s     | UART log of a mutex-protected snapshot         |
| heartbeat | 1        | 500ms  | status LED on GP25                            |

The watchdog task does not unconditionally kick the hardware watchdog. Each
supervised task sets a flag each cycle, and the watchdog is fed only when every
flag is set — so one hung task causes a reset instead of being masked by an
independent feeder.

## Build
Needs both the pico-sdk and a FreeRTOS kernel tree:
```bash
export PICO_SDK_PATH=/path/to/pico-sdk
export FREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel   # V11.1.0 tested
cmake -G Ninja -S firmware -B build -DPICO_BOARD=pico -DCMAKE_BUILD_TYPE=Release
cmake --build build          # -> build/sensor_node.uf2
```
`firmware/FreeRTOSConfig.h` is checked in and tuned for the Cortex-M0+ port
(`configPRIO_BITS=2`, kernel at the lowest priority, `configTICK_RATE_HZ=1000`,
80 KB heap — task stacks total ~16.3 KB).

## Tests
```bash
pip install pytest
python -m pytest host/test_monitor.py -q
```
Drives the real script over a pty: CSV capture, negative temperatures, legacy
lines without the `sensor` field, noise rejection, dead port, `--min-samples`.

## What works
TMP102 I2C read with bounded timeouts, real ADC sampling of the supply rail,
mutex-protected consistent snapshots, supervised watchdog, LED heartbeat,
pty-based test suite, CI that builds the firmware.

## What doesn't
No CAN bus yet. No SD card logging yet. Task behaviour has not been observed on
real silicon — the firmware is CI-built, not bench-run.

## License
MIT
