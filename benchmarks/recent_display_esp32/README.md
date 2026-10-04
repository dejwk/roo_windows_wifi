# ESP32 comparison of recent roo_display optimizations

Measured on 2026-10-04, separately from the reusable compiler-workspace experiment.
Every build uses committed `roo_display` sources; none includes the staged
`CompositionWorkspace` implementation. `roo_windows` stays at `8a61b43`,
`roo_windows_wifi` at `2468b6e`, and `roo_wifi` at `bfe6b38`.

## Results

The range is `c243c4f` → `b985f11`; the checkpoint immediately before the mask
change is `e745df9`.

The cumulative UI improvement is **6.8%** in this firmware: median summed CPU
repaint time falls from **5.894 s to 5.494 s** per 104-step editor/menu journey.
Security-menu scrolling improves most; form scrolling is essentially unchanged.
The final mask commit alone lowers the sum by **2.3%**, with mixed per-action
changes. This is much smaller than its targeted opaque-mask microbenchmark win.

Median CPU milliseconds per repaint (five timed repetitions; corresponding scroll
steps pooled). Positive percentages below mean less time.

| Action | Starting revision | Before mask change | Latest | Overall reduction |
| --- | ---: | ---: | ---: | ---: |
| Form toward Security | 31.57 | 31.16 | 31.97 | -1.3% |
| Open Security | 116.20 | 112.33 | 106.43 | 8.4% |
| Security menu down | 82.60 | 77.41 | 73.66 | 10.8% |
| Security menu up | 82.44 | 77.30 | 74.25 | 9.9% |
| Close Security | 41.40 | 40.70 | 41.30 | 0.2% |
| Continue form down | 33.62 | 33.26 | 34.03 | -1.2% |
| Form up | 32.62 | 32.23 | 32.98 | -1.1% |

| Checkpoint | Median journey CPU seconds | Range over five journeys |
| --- | ---: | ---: |
| `c243c4f` | 5.8940 | 5.8900–5.8940 |
| `dbdea8e` | 5.8916 | 5.8893–5.8917 |
| `c61724a` | 5.8408 | 5.8382–5.8409 |
| `6b348f3` | 5.7066 | 5.7065–5.7071 |
| `e745df9` | 5.6217 | 5.6215–5.6218 |
| `b985f11` | 5.4935 | 5.4935–5.4938 |

Every warm journey still makes **7,501 C++ allocations requesting 218,392 bytes**.
The reduction in CPU time therefore is not a reduction in repeated allocation
count or requested bytes. Individual linked-image differences should not be
assigned wholly to an optimization whose path is absent from the workload.
The earlier workspace probe found no compiled raster-stack stream openings in
this flow; `roo_windows` also uses rounded overlays/exclusion spans rather than
an explicit stack of `DestinationIn` masks for ordinary rounded clipping.

Initial settings painting does benefit from compiler allocation reductions:

| Initial frame (untimed trace) | Starting allocations / bytes | Latest allocations / bytes |
| --- | ---: | ---: |
| settings 0 | 97 / 4506 | 82 / 3274 |
| settings_down 1 | 80 / 4038 | 65 / 2806 |
| settings_down 2 | 80 / 4038 | 65 / 2806 |
| add_network 0 | 75 / 2880 | 75 / 2880 |

### Synthetic improvements and limits

All numbers below are clean-heap median microseconds. Dimensions are nominal
scene bounds; twelve dirty clips vary within those bounds. Stream totals include
preparation, consumption, and destruction. Direct drawing includes drawing
context creation/destruction and output-sink calls.

| Change | Case / measured phase | Before µs | After µs | Time reduction |
| --- | --- | ---: | ---: | ---: |
| Pruning and uniform runs | 16 opaque-top layers, 160×120; stream total | 196,726 | 1,048 | 99.5% |
| Pruning and uniform runs | 4 uniform layers, 160×120; stream total | 27,720 | 684 | 97.5% |
| Reserve/partition reuse | 16 overlapping layers, 32×24; prepare/destroy | 1,063 | 376 | 64.6% |
| Uniform fills/opacity | 4 uniform layers, 160×120; direct draw | 17,276 | 13 | 99.9% |
| Uniform fills/opacity | 16 opaque-top layers, 160×120; direct draw | 5,464 | 8 | 99.9% |
| Mask optimization | 3 opaque masks + source, 160×120; stream total | 17,022 | 8,179 | 52.0% |
| Mask optimization | 3 opaque masks + source, 160×120; direct draw | 46,029 | 28,728 | 37.6% |
| Mask optimization | 3 rounded masks + source, 160×120; stream total | 19,679 | 17,376 | 11.7% |
| Mask optimization | 3 rounded masks + source, 160×120; direct draw | 40,885 | 36,942 | 9.6% |
| Mask optimization | 3 rounded masks + source, 32×24; stream total | 1,345 | 1,356 | -0.8% |
| Mask optimization | 4 rounded SourceOver layers, 160×120; direct draw | 16,806 | 23,395 | -39.2% |

The last row is a **regression**, not a win: ordinary rounded `SourceOver`
painting becomes **39.2% slower** across the mask checkpoint (16.806 → 23.395 ms).
It repeats under the mixed-size heap (16.805 → 23.395 ms), with matching output.
The mixed-size condition repeats the workload in the same firmware; it is not an
independent relink. An earlier checkpoint also shows a similar slowdown in this
otherwise unchanged drawing path (23.062 ms at `c61724a`, then 16.573 ms at
`6b348f3`). Code placement or instruction-cache effects are plausible, but this
experiment does **not** isolate the cause.

A [follow-up investigation](regression/README.md) confirms a strong code-placement
effect: relocating unchanged current drawing code restores **16.580 ms**, with
matching output. Runtime disabling of the new mask checks saves only about
**0.1 ms** in the diagnostic probe; instruction stalls account for almost all of
its placement-dependent slowdown. The full Wi-Fi journey changes by only 0.36%
under this relocation. These results are linked-firmware comparisons, not pure
measurements of algorithmic changes.

Small rounded masks show no useful speedup. Ordinary varying translucent content
also gets no broad benefit: the 16-layer 160×120 overlap stream total moves from
206.890 to 212.834 ms across the whole range (2.9% slower). Four split translucent
layers move from 41.697 to 43.167 ms (3.5% slower), despite cheaper preparation.
Full clean/mixed results, allocation replay controls, percentiles, and emitted
window counts are retained in the CSV files.

Allocation changes are independently visible in the untimed traces:

- Reserving compiler capacity/retaining full-coverage partitions cuts the
  16-layer overlap preparation from **74 allocations to 26**, and peak requested
  storage from **7,152 to 5,148 bytes**.
- Allocating only surviving input buffers cuts the 16-layer opaque-top peak from
  **4,836 to 696 bytes** (85.6% less), with the same **11 allocations**. Requested
  bytes over the complete trace fall from **4,906 to 766**.
- These figures describe temporary compiler/stream storage, not retained
  workspace capacity. No reusable compiler workspace is present in any build.

The practical priority remains painting cost in the real UI. Compiler allocation
and uniform/occluded-scene improvements are strong targeted wins; eliminating
more compiler allocations would not address this flow's steady-state scrolling.
Follow-up measurements confirm substantial sensitivity to linked-code placement.

## Verification and artifacts

All six firmware builds and both suites completed: **1,320 synthetic timing rows**,
**5,640 UI frames**, **264 memory traces**, and **3,168 drawing checks**.
All 44 stream cases and all 316 traced UI frame positions match across revisions.
Drawing checks match except the 40 documented old visibility-fix cases; those
match across all five later revisions. All heap integrity checks passed.

UI free heap remains **179,660 bytes**, with largest free block **110,580 bytes**,
from the first complete journey through warm-up and all repetitions. After UI
teardown it is **215,148 bytes**, versus **216,160** before construction: the same
uninvestigated 1,012-byte initial/teardown difference seen in the workspace test.
There is no accumulating loss during the repetitions; this is not proof of
complete teardown.

- [Synthetic timings](results/2026-10-04/synthetic.csv)
- [UI frames and allocations](results/2026-10-04/wifi.csv)
- [Compiler memory traces](results/2026-10-04/memory.csv)
- [Drawing equivalence checks](results/2026-10-04/verify.csv)
- [UI heap checkpoints](results/2026-10-04/heaps.csv)
- [Complete selected-case summary](results/2026-10-04/summary.txt)
- [Dependency source hashes](results/2026-10-04/dependencies.json)
- [Firmware/log hashes and completion records](results/2026-10-04/firmware.json)

## Checkpoints

| Revision | Changes since preceding checkpoint |
| --- | --- |
| `c243c4f` | Starting point: bounded scratch, fallback above 16 inputs, reusable input descriptors |
| `dbdea8e` | Fast stream skipping, pruning layers hidden by replacing inputs, propagation of uniform runs; raster visibility/Background correctness fixes |
| `c61724a` | Reserve known compiler capacities; avoid rebuilding partitions for full-coverage layers |
| `6b348f3` | Allocate pixel buffers only for surviving inputs; ignore empty inputs in natural bounds |
| `e745df9` | Fill uniform output without materializing pixels/visiting tiles; propagate conservative opacity |
| `b985f11` | Avoid redundant mask sampling in composition and blending filters |

These are cumulative groups, not single-commit ablations. Documentation and test
commits between checkpoints do not change runtime behavior.

## Method

ESP32-D0WD-V3 revision 3.1, 240 MHz, 4 MB flash, no PSRAM; Arduino-ESP32
3.3.5 / IDF `v5.5.1-931-g9bb7aa84fe`, GCC 14.2.0, `-Os`, `-DNDEBUG`,
32 KiB loop stack, SDK light heap poisoning. Both suites share one firmware,
but the board resets before each suite. Firmware images and build logs are kept
in the temporary experiment directory, not committed.

The synthetic suite retains the earlier benchmark's 32×24 and 160×120 scenes,
1/4/16 inputs, twelve changing output clips, and clean/mixed-size heaps. It adds
three `DestinationIn` masks over a varying source (opaque rectangle or rounded
rectangle), and measures direct drawing as well as stream construction and
consumption. Each preparation/allocator-replay case uses 96 samples; each stream
consumption/direct-drawing case uses 48. Reported synthetic latencies are medians
unless otherwise stated. Allocation traces are separate and describe one
unclipped stream construction/destruction: C++ allocation count, requested bytes,
and peak simultaneous requested bytes (excluding allocator headers).

The UI suite uses the actual retained Wi-Fi widget tree and the same deterministic
controller backend as the workspace test. Semantic clicks open Add network and
Security; 8-pixel scroll steps visit both boundaries of the Security menu and
editor. Five timed repetitions follow warm-up, each with 104 repaint steps.
Three separate trace passes record C++ allocations and output checksums. Timed
passes exclude instrumentation and serial output. Entering Add network is verified during the initial trace; reported UI timings
cover the subsequent repeated editor/menu journey. The output device advertises
RGB565 without hardware blending/blitting but does not transfer pixels to an LCD.
These are CPU repaint times, not physical display frame rates. Uniform fills
still require pixel transfer on real hardware; large sink-only speedup factors
should not be interpreted as proportional improvements in LCD frame rate.

Checksums compare streams plus emitted pixel/color-coordinate pairs; drawing
checksums are independent of batching order, allowing optimized fills to change
address-window counts. They are equivalence checks, not independent visual
goldens, and do not reconstruct final pixels after overlapping writes. The initial
revision predates `852a8e7`: it emits zero-alpha colored pixels at rounded-mask
edges, whereas later revisions correctly skip them in visible drawing mode.
The 40 affected direct-drawing comparisons are excluded from speedup claims;
stream output is unaffected by this visibility fix.

The combined firmware's static heap-trace buffers differ from the previous
workspace experiment. Compare revisions within this experiment; do not attribute
small differences from the previous report to a library change. Linked-code
placement can affect ESP32 timings. Large changes consistently tied to a path
are stronger evidence than a few percent movement in an unrelated path.

## Reproduction

Use the installed local PlatformIO ESP32 platform and roo repositories with the
same `roo_io` FatFs include-order fix as the workspace benchmark. Other local
library source hashes are recorded with the results. `prepare.py` archives the
six display revisions and fixed windows revision, and uses local dependencies
without registry downloads. It leaves source repositories untouched.

```sh
python3 prepare.py ~/Documents/Arduino/roo /tmp/display-history
~/.platformio/penv/bin/python run.py /tmp/display-history
python3 analyze.py /tmp/display-history
```

`run.py` explicitly flashes `/dev/ttyUSB0` and runs both suites per revision,
resetting the board between suites. It can resume at a named checkpoint, e.g.
`run.py /tmp/display-history e745df9`. It archives each firmware before preparing
the next build. It does not save or restore pre-existing board firmware.

To validate and summarize the checked-in data without hardware:

```sh
python3 analyze.py results/2026-10-04
```

The shared allocation hooks and display sink are in `src/`. The synthetic source
retains the structure of the prior workspace harness, with workspace branches
disabled and churn unused, to preserve the measured code arrangement.
