<p align="right">
  <a href="AGENTS.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Repository Guidelines for AI Agents

Standalone **Guitar Kit** for the FoloToy AI Passport (ESP32-C3, 8 MB Flash, no PSRAM,
ESP-IDF 5.5.3). Four tools: chord dictionary, tuner, capo lookup, metronome. Product
details live in [README.md](README.md); this file covers the rules an agent must not
get wrong.

## Firmware artifacts: two types, two flashing paths

This is the #1 way to break things. There are **two different kinds of `.bin`** and
they are **not interchangeable**:

| Artifact | Contents | Written at | Used for |
| --- | --- | --- | --- |
| `build/guitar-kit.bin` (and the dated copy `guitar-kit-YYYYMMDD.bin`) | App only — no bootloader, no partition table | `0x10000`, together with bootloader and partition table | **Personal upgrades only**, via segmented `idf.py flash` |
| `build/guitar-kit-full-YYYYMMDD.bin` (from `tools/merge.sh`) | Merged: bootloader + partition table + app in one image | `0x0`, as a single write | **Store review submissions** and blank devices |

### The incident (do not repeat it)

A review submission of the **app-only** bin was rejected with
"固件无法正常启动，请检查是否能从 0x0 分区刷写的合并固件" ("firmware cannot boot;
check the merged image flashable from 0x0"). The review pipeline writes whatever it
receives at `0x0` — an app-only image flashed there has no bootloader or partition
table and cannot start.

**Rules:**

1. Store review / any 0x0-style pipeline → **always submit the merged image** from
   `tools/merge.sh` (`guitar-kit-full-YYYYMMDD.bin`), never `guitar-kit.bin`.
2. The user's own device is **provisioned** (it carries a protected `cardid`
   partition at `0x356000` with factory identity data). Personal upgrades must use
   **segmented `idf.py flash`** — never flash the merged image onto it.
3. After renaming or moving this directory, run `idf.py fullclean` before build —
   the cached build dir keeps the old absolute path and will fail or flash stale
   images.

## Build, test, flash, submit

```bash
cd firmware
source ~/esp/esp-idf-v5.5.3/export.sh
./tests/run.sh                  # host tests (chords / tuner / capo / metronome) — no hardware needed
idf.py set-target esp32c3       # first time only (regenerates sdkconfig from defaults)
idf.py build                    # emits app bin + dated copy guitar-kit-YYYYMMDD.bin
idf.py -p /dev/cu.usbmodemXXX flash          # personal upgrade (segmented)
tools/merge.sh                  # store submission (merged full image, self-checked)
```

`idf.py set-target` is also required after changing `sdkconfig.defaults` (new Kconfig
options are not picked up by incremental builds).

## Code layout rules

- Pure, hardware-independent logic (chords, tuner pitch, capo math, metronome
  timing) lives in its own module with a host test in `firmware/tests/run.sh` —
  no ESP-IDF or LVGL includes there.
- New tools are added to the `HUB_ITEMS` table in `main/chords_ui.c`; the hub grid
  grows automatically. Wire the page branch, the key handling, and (if animated)
  the 100 ms `tuner_tick`.
- When a page rebuilds, widget pointers are zeroed first — never touch them before
  the page builder recreates them (a NULL label write rebooted the device once).
- LVGL pool is 48 KB; pages rebuild by deleting the old screen **before** building
  the new one. Do not let two screens coexist.
- USB connection on this setup is flaky: after moving hardware or directories,
  confirm `/dev/cu.usbmodem*` exists and retry; a replug fixes a wedged USB-JTAG.

More background: [README.md](README.md) (product, gestures, build details).
