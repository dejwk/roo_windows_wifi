# ESP32 Wi-Fi UI composition benchmark

Measured on 2026-10-04 using the actual Material 3 `WifiSettingsFlow`: scroll
settings to **Add network**, invoke it, scroll the form down, open **Security**,
scroll its menu down and up, close it, and scroll the form to the bottom and back
to the top. The retained editor/menu sequence contains 104 repaint steps.

For the preceding committed `roo_display` optimizations, see the
[six-revision ESP32 comparison](recent_display_esp32/README.md), which includes
both the synthetic composition suite and this Wi-Fi journey.

**Reusable compiler storage did not reduce allocations in this flow.** The
separate diagnostic observed zero workspace acquisitions and zero workspace or
ordinary compiled `RasterizableStack` stream openings. It observed only three
geometry compiler runs in its entire 316-frame sequence. These are different
counters: a compiler run does not necessarily use a workspace.

The filter also has direct rectangle/span paths and a small-window path that
bypasses stream creation. Compiler storage reuse cannot remove allocations on
those paths. This result substantially limits how broadly the synthetic
composition benchmark should be generalized to ordinary UI scrolling.

## Hardware and method

- ESP32-D0WD-V3 revision 3.1, 240 MHz, 4 MB flash, no PSRAM.
- PlatformIO/pioarduino platform 55.03.35, Arduino-ESP32 3.3.5, ESP-IDF
  `v5.5.1-931-g9bb7aa84fe`, GCC 14.2.0; production drawing buffers, `-DNDEBUG`,
  32 KB Arduino loop stack, SDK light heap poisoning enabled.
- 320 × 240, default 100% widget scale. An RGB565 display sink advertises no
  hardware blending or blitting, matching the example's ILI9341 capabilities.
  It counts output instead of transmitting pixels or storing a framebuffer.
- Actual widgets, controller, layout, invalidation, rounded clipping, shadows,
  text rendering, and incremental painting. The controller uses `roo_wifi`'s
  deterministic in-memory test backend, with Wi-Fi disabled.
- Semantic click callbacks and 8-pixel `scrollTo()` steps drive the flow. This
  excludes touch polling, press/release feedback, fling animation, radio work,
  and physical display transfers. Scrollbars are forced visible during scrolling.
- Timings cover the scroll/click action plus one complete `Application::refresh()`.
  Serial output, a watchdog-friendly delay, pixel hashing, and allocation
  recording are outside timed passes. C++ `new`/`delete` counts and requested
  bytes come from separate trace passes; they do not count arbitrary C `malloc`.
- Each firmware rendered 1,980 frames. Reported timing uses 15 repetitions after
  warm-up. Allocation counts use the final trace repetition. Every corresponding
  frame in the three trace passes (316 frame positions) matched output pixel
  count, address-window count, and ordered color/coordinate checksum across all
  three builds. This checks equivalence, not an independent visual golden.

The three builds use the same Wi-Fi UI (`roo_windows_wifi` revision `2468b6e`)
and local dependencies (`roo_wifi` revision `bfe6b38`). The existing local
`roo_io` FatFs include-order fix is required by this SDK.

| Build | Composition implementation |
| --- | --- |
| `baseline` | `roo_display` `b985f1168f45354276a58f9e9904585b93f64c48`, `roo_windows` `8a61b433051e29aa8c0f1ce721e05e0aa5de733c` |
| `fresh` | Working workspace implementation, with the temporary `OverlayStack` copy calling ordinary `RasterizableStack::createStream()` |
| `reuse` | Working workspace implementation and its `roo_windows` integration |

The baseline already includes the preceding mask optimizations. This comparison
concerns the compiler-storage change.

## Measurements

Median milliseconds per repaint; allocation columns are identical in all three
builds. Frame geometry varies within a scroll, so each row pools matching steps
across the measured repetitions.

| Action | Baseline ms | New, no reuse ms | New, reuse ms | C++ allocations/frame, median (range) |
| --- | ---: | ---: | ---: | ---: |
| Form scroll toward Security | 32.99 | 33.13 | 30.94 | 42 (40–64) |
| Open Security | 118.55 | 111.54 | 131.82 | 89 |
| Security menu down | 83.18 | 78.63 | 76.80 | 103 (82–136) |
| Security menu up | 83.31 | 79.10 | 77.38 | 103 (82–126) |
| Close Security | 43.36 | 43.73 | 41.16 | 57 |
| Continue form down | 35.42 | 35.59 | 32.91 | 48 (35–63) |
| Form up | 33.95 | 34.11 | 31.75 | 42 (35–64) |

The complete 104-frame repeated sequence used **7,501 allocations requesting
218,392 bytes in every build**. Median summed CPU elapsed time was 6.016 s,
5.809 s, and 5.613 s respectively. This sum excludes time between frames.
The different linked images exhibit mixed timing changes, including slower menu
opening with reuse enabled. **These differences do not establish a benefit from
avoiding allocations: the workspace was never acquired.** Their cause was not
isolated by this benchmark.

The only allocation differences occurred during initial settings painting and
its two scroll frames: each new build made one additional 4-byte allocation in
each of those three frames. Every later traced frame had identical allocation,
requested-byte, and free counts across all builds.

Heap observations were identical for all three builds:

| Checkpoint | Free heap bytes | Largest free block bytes |
| --- | ---: | ---: |
| Before UI construction | 235,008 | 110,580 |
| After first complete journey | 198,508 | 110,580 |
| After warm-up | 198,508 | 110,580 |
| After all repetitions | 198,508 | 110,580 |
| After UI destruction | 233,996 | 110,580 |

All heap integrity checks passed. There was no accumulating heap loss or decrease
in largest free block during the repetitions. The common 1,012-byte difference
after destruction was not investigated; it is separate from repeated-frame
heap stability and should not be described as proof of complete teardown.

## Allocator-only diagnostic

A separate, untimed diagnostic adds counters to a **temporary** copy of
`composition_workspace.cpp` and captures allocation sizes and lifetimes for two
representative scrolling frames. It then replays the trace 128 times using
`malloc`/`free`, subtracting median empty-replay loop cost.

| Frame (downward step 12) | Allocations | Requested bytes | Allocator replay, net | Original reuse frame | Approximate fraction |
| --- | ---: | ---: | ---: | ---: | ---: |
| Security menu | 115 | 3,348 | 2.877 ms | 79.892 ms | 3.6% |
| Form | 35 | 920 | 0.934 ms | 26.794 ms | 3.5% |

This estimates allocation/free cost; it does not include constructors,
destructors, or rendering. The diagnostic has additional static trace storage,
so its heap layout differs from the timing firmware. One menu-frame allocation
survived the capture boundary: its synthetic replay lifetime was ended at the
boundary, without freeing the real UI object. Pre-existing objects' frees are
not replayed. The form trace needed no such closure. These limits prevent
treating replay as an exact decomposition of frame time.

Final counters after 316 diagnostic frames: `acquires=0`,
`workspace_streams=0`, `fresh_streams=0`, `compiler_runs=3`.

For this workload, profiling the remaining menu painting cost is a better next
step than expanding compiler-storage reuse. The synthetic benchmark's benefits
remain relevant to workloads that actually open compiled composition streams.

## Reproduction

The preparation script reads local roo repositories, archives the two pinned
baseline revisions, and creates isolated PlatformIO projects. Temporary library
manifests remove registry dependencies while preserving build scripts and local
source links. It does not modify the canonical repositories or upload firmware.
It expects the workspace implementation in the local source tree and an installed
ESP32 PlatformIO toolchain. Use new output directories on each invocation.

```sh
python3 benchmarks/prepare_composition_workspace_esp32.py \
  ~/Documents/Arduino/roo /tmp/wifi-ui-benchmark
pio run -d /tmp/wifi-ui-benchmark/baseline -j 6
pio run -d /tmp/wifi-ui-benchmark/fresh -j 6
pio run -d /tmp/wifi-ui-benchmark/reuse -j 6
```

Each project selects `huge_app.csv`: the complete UI firmware is about 1.39 MB
and exceeds the default 1.25 MB application partition. To upload and record a
variant, run the following with that variant's name; the capture script opens
`/dev/ttyUSB0` at 115200 baud, resets the flashed application, and waits for `END`:

```sh
pio run -d /tmp/wifi-ui-benchmark/reuse -t upload
python3 benchmarks/capture_composition_workspace_esp32.py \
  /tmp/wifi-ui-benchmark/reuse_serial.log
```

After saving all three `<variant>_serial.log` files:

```sh
python3 benchmarks/analyze_composition_workspace_esp32.py /tmp/wifi-ui-benchmark
```

For the separate diagnostic, `--probe` applies the supplied patch only to the
new temporary display copy and a separate sketch. Build and capture **reuse** in
that fixture; the ordinary projects in the first fixture remain unchanged:

```sh
python3 benchmarks/prepare_composition_workspace_esp32.py \
  ~/Documents/Arduino/roo /tmp/wifi-ui-probe --probe
pio run -d /tmp/wifi-ui-probe/reuse -j 6 -t upload
python3 benchmarks/capture_composition_workspace_esp32.py \
  /tmp/wifi-ui-probe/probe_serial.log
```

Saved data: [all measured frames](results/composition_workspace_esp32_2026-10-04.csv),
[heap observations](results/composition_workspace_esp32_2026-10-04_heap.csv), and
[diagnostic serial output](results/composition_workspace_esp32_2026-10-04_probe.log).
The diagnostic timings are intentionally excluded from the comparison CSV.
