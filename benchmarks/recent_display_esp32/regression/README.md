# Rounded SourceOver regression investigation

Follow-up to the [six-checkpoint ESP32 comparison](../README.md), measured on
2026-10-04 with the same board, compiler, dependencies, CPU clock, and flash
settings. All variants start from committed `roo_display` revision `b985f11`;
the staged compiler workspace is excluded.

The original 16.806 → 23.395 ms slowdown is real for those linked firmware
images. The investigation identifies **code-layout-dependent instruction stalls**
as the dominant mechanism. Disabling the new opaque-mask checks at runtime
recovers only about 0.1 ms. Relocating unchanged drawing code recovers several
milliseconds, with essentially unchanged executed instruction counts and matching
output. The mask optimization does not add enough work to explain the regression.

## Runtime and placement controls

The diagnostic probe draws the same four translucent rounded rectangles in the
same twelve dirty clips. Each measurement group has 48 draws. The new mask
optimization can be enabled or disabled in one firmware through a runtime gate
at its three raster-stack call sites. Disabling it uses the general raster path;
there are no DestinationIn layers in this scene. It does not undo unrelated
changes elsewhere in the mask commit.

The placement control predeclares the actual `readColorRect` ELF text section
at the beginning of `rasterizable_stack.cpp`. This changes section ordering in
the object. It can also prepend 0, 16, 64, or 256 bytes of **unexecuted** padding
to that section. It changes function addresses, not the rendering algorithm or
its inputs. The `readColorRect` function body remains 1,769 bytes. Related functions in
that object also move. In the zero-padding control, the rounded-shape
classifier retains its address; positive padding also shifts later code.

| Layout | `readColorRect` address | Checks on (ms) | Checks off (ms) | Instructions, on | Stall cycles, on |
| --- | --- | ---: | ---: | ---: | ---: |
| hotnorm | `0x400dc2b8` | 25.497 | 25.404 | 2,327,849.0 | 2,316,033.0 |
| hot0000 | `0x400db08c` | 16.070 | 15.972 | 2,327,850.5 | 78,546.5 |
| hot0016 | `0x400db09c` | 16.054 | 15.957 | 2,327,850.5 | 75,648.0 |
| hot0064 | `0x400db0cc` | 17.397 | 17.302 | 2,327,850.5 | 395,131.5 |
| hot0256 | `0x400db188` | 18.998 | 18.898 | 2,327,849.0 | 776,676.0 |

The initial instrumented image, `runtime_gate`, gives 25.495 ms with checks and
25.403 ms without them. Against `hot0000`, paired samples save a mean 9.364 ms
while instruction stalls fall by 2,237,758 cycles, or 9.324 ms at 240 MHz:
**99.6% of the elapsed-time difference**. Mean completed instructions differ by
only 0.125 instructions per draw, within interrupt variation. A second original
layout, `hotnorm`, uses a seven-character revision label matching the relocated
variants and confirms the slow result.

This also explains why the earlier checkpoint series showed a similar jump
at `c61724a` followed by recovery at `6b348f3`, despite unchanged direct-drawing
algorithms across that step. Revision-to-revision timing differences combine
algorithm changes with placement effects.

### What the counters establish

Counter 0 records completed instructions (`XTPERF_CNT_INSN`, mask `0x8dff`).
Counter 1 records instruction-related stalls (`XTPERF_CNT_I_STALL`). Successive
on/off pairs select masks `0x1ff` (all), `0x3f` (fetch/busy/external stalls),
`0x40` (fast L32R), and `0x180` (iterative multiply/divide). Arithmetic stalls
stay near 17,604 cycles; the large reduction is in the non-arithmetic group.
These masks can overlap: their measurements must not be added together.

Counters run on the Arduino loop core and include interrupts. They start just
before the timing call and stop just after the second timing call. Serial output
is outside both measured regions. They are stall-cycle measurements, **not a
direct count of external flash-cache misses**. Together with the controlled
relocation, they support instruction-fetch/cache placement as the explanation;
they do not identify a particular cache set, replacement pattern, or evicted line.
Espressif documents this binary-layout sensitivity and the option of placing
selected hot functions in IRAM in its
[ESP-IDF 5.5.1 performance guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32/api-guides/performance/speed.html).

An earlier negative control inserted padding in an unrelated function that
linked *after* the hot code. It did not move the drawing functions and did not
change their timing. That control is not evidence against placement sensitivity.

## Original suites without profiling

The original benchmark sources were restored byte-for-byte (except the revision
label), with all counter instrumentation and runtime gates removed. The only
rendering-source change was predeclaring the `readColorRect` section, with no
padding. Its body remains 1,773 bytes and moves from `0x400dc2cc` to `0x400db088`;
the rounded-shape classifier remains at `0x400ecd10`.

| Workload | Original `b985f11` image | Relocated identical algorithm |
| --- | ---: | ---: |
| Rounded SourceOver draw, clean heap | 23.395 ms | 16.580 ms |
| Rounded SourceOver draw, mixed heap | 23.395 ms | 16.580 ms |
| Rounded SourceOver stream total, clean heap | 29.950 ms | 29.952 ms |
| Opaque masks, direct draw, clean heap | 28.728 ms | 25.173 ms |
| Rounded masks, direct draw, clean heap | 36.942 ms | 32.858 ms |
| Wi-Fi journey, median over five repetitions | 5.493530 s | 5.513414 s |

Relocation alone reduces the regressed rounded draw by **29.1%**, fully
recovering the gap to the pre-mask image's 16.806 ms. The Wi-Fi journey changes
by only **+0.36%**. This is not a general application speedup or a proposal to
ship this particular layout.

Both original suites completed: 220 synthetic timing rows and 940 UI rows.
All 44 stream checksums, 528 drawing checks, 316 traced UI frames, and allocation
traces match the original `b985f11` run. UI heap checkpoints pass integrity
checks and remain stable across repetitions. Pixel checks retain the limitations
of the parent experiment: they check emitted output, not an independent visual
golden. Each warm UI journey still makes 7,501 allocations requesting 218,392 bytes.

The full relocated [synthetic results](results/relocated_synthetic.csv),
[UI results](results/relocated_wifi.csv), [drawing checks](results/relocated_verify.csv),
[memory traces](results/relocated_memory.csv), and
[heap checkpoints](results/relocated_heaps.csv) are retained alongside the
[validation summary](results/summary.txt).


## Implications

- Keep the mask optimization: its targeted mask cases benefit, and its runtime
  predicate has small overhead in ordinary SourceOver drawing.
- Treat the measured regression as a real firmware performance issue, while
  avoiding attributing its full magnitude to added algorithmic work or allocator
  overhead. Repeating samples or changing heap contents in the same image does
  not test sensitivity to linking.
- Include multiple controlled layouts when evaluating small renderer changes,
  then measure the actual application firmware as well. Keep functional output
  checks separate from timing.
- A robust follow-up would reduce the hot renderer's instruction footprint or
  evaluate selective IRAM placement, including its RAM cost and impact on other
  scenes. Neither remedy has been implemented or validated here. Arbitrary
  padding is a diagnostic control and should not become a library fix.

## Artifacts and reproduction

The [probe patch](probe.patch) contains the exact instrumentation and runtime
gate used for the measurements. Its unused tail-padding function is retained
to reproduce the measured code arrangement. The
[counter samples](results/samples.csv), [output checks](results/verify.csv), and
[firmware hashes and function addresses](results/firmware.json) are saved.
There are 2,304 counter samples and 576 matching output checks across six probe
images. ELF files, binaries, and complete serial logs remain in the temporary
experiment directory `/tmp/roo_rounded_regression`.

From the parent benchmark directory, prepare a **new** isolated directory:

```sh
python3 prepare.py ~/Documents/Arduino/roo /tmp/rounded-regression
~/.platformio/penv/bin/python regression/run.py /tmp/rounded-regression
python3 regression/analyze.py /tmp/rounded-results /tmp/rounded-regression
```

The runner builds and flashes all six probe layouts, then runs both original
suites with unchanged rendering code relocated in flash. It replaces the
connected ESP32 firmware without backup/restore and does not modify source
repositories. An optional final argument resumes at a named variant.

To validate the checked-in counters and both suites without hardware:

```sh
python3 regression/analyze.py regression/results
```
