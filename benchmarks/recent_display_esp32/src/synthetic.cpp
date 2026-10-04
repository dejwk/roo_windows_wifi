// Historical comparison of dynamic composition and direct drawing on ESP32.
// Kept structurally identical to the measured derivative of the workspace
// benchmark. BASELINE disables its workspace-only branches for every
// checkpoint; it does not disable the committed optimizations under comparison.
// The churn helper is unused here. All timing uses production buffers.
#define ROO_BENCHMARK_BASELINE
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <new>
#include <vector>

#include "allocations.h"
#include "display_sink.h"

#ifndef ROO_BENCHMARK_BASELINE
#include "roo_display/composition/composition_workspace.h"
#endif
#include "roo_display/composition/rasterizable_stack.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/filter/foreground.h"
#include "roo_display/shape/basic.h"
#include "roo_display/shape/smooth.h"

namespace {
using namespace roo_display;
constexpr int kPrepareSamples = 96;
constexpr int kRenderSamples = 48;
constexpr int kVariants = 12;
uint32_t samples[kPrepareSamples];
volatile uint32_t checksum_sink = 0;

#ifdef ROO_BENCHMARK_BASELINE
const char*& kRevision = benchmark_revision;
#else
constexpr const char* kRevision = "workspace_change";
#endif

size_t FreeHeap() {
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

size_t LargestBlock() {
  return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL |
                                          MALLOC_CAP_8BIT);
}

struct Distribution {
  double mean;
  uint32_t p50;
  uint32_t p95;
  uint32_t p99;
  uint32_t maximum;
};

Distribution Summarize(int count) {
  uint64_t sum = 0;
  for (int i = 0; i < count; ++i) sum += samples[i];
  std::sort(samples, samples + count);
  return {static_cast<double>(sum) / count, samples[count / 2],
          samples[(count - 1) * 95 / 100], samples[(count - 1) * 99 / 100],
          samples[count - 1]};
}

/// Selects the virtual stream path used by the foreground overlay filter.
class BenchStack : public RasterizableStack {
 public:
  /// Creates a benchmark stack with the given output bounds.
  explicit BenchStack(Box bounds) : RasterizableStack(bounds) {}

  /// Selects workspace reuse for subsequent streams.
  void setReuse(bool reuse) { reuse_ = reuse; }

  /// Discards retained workspace capacity between cases.
  void release() {
#ifndef ROO_BENCHMARK_BASELINE
    workspace_.release();
#endif
  }

  /// Opens the selected compiled or raster stream for this output clip.
  std::unique_ptr<PixelStream> createStream(const Box& clip) const override {
#ifndef ROO_BENCHMARK_BASELINE
    if (reuse_) return workspace_.createStream(*this, clip);
#endif
    return RasterizableStack::createStream(clip);
  }

 private:
  bool reuse_ = false;
#ifndef ROO_BENCHMARK_BASELINE
  mutable CompositionWorkspace workspace_;
#endif
};

/// Leaves holes of mixed sizes among live application-like blocks.
/// This is a synthetic condition, not a claim about long-running UI heaps.
class HeapPressure {
 public:
  /// Allocates the synthetic live-block pattern when requested.
  explicit HeapPressure(bool active) {
    if (!active) return;
    for (size_t i = 0; i < blocks_.size(); ++i) {
      blocks_[i] = std::malloc(48 + (i * 197) % 1400);
      if (blocks_[i] == nullptr) std::abort();
    }
    for (size_t i = 0; i < blocks_.size(); ++i) {
      if (i % 3 == 0) continue;
      std::free(blocks_[i]);
      blocks_[i] = nullptr;
    }
  }

  /// Frees every retained block when the case ends.
  ~HeapPressure() {
    for (void* block : blocks_) std::free(block);
  }

 private:
  std::array<void*, 112> blocks_{};
};

// Output clips change over a small repeatable dirty-window cycle.
Box Clip(const Box& bounds, int variant) {
  return Box(variant % 3, variant % 4, bounds.xMax() - variant % 4,
             bounds.yMax() - variant % 3);
}

// Fully consumes the program, honoring long uniform runs exactly as filters do.
uint32_t Consume(PixelStream& stream, int remaining) {
  Color pixels[64];
  uint32_t checksum = 0;
  while (remaining > 0) {
    uint16_t batch = std::min<int>(remaining, 64);
    uint32_t run = 0;
    stream.read(pixels, batch, run);
    if (run >= batch) {
      run = std::min<uint32_t>(run, remaining);
      checksum += pixels[0].asArgb() * run;
      stream.skip(run - batch);
      remaining -= run;
    } else {
      for (int i = 0; i < batch; ++i) checksum += pixels[i].asArgb();
      remaining -= batch;
    }
  }
  checksum_sink = checksum;
  return checksum;
}

void Report(const char* heap, const char* scene, int layers, int pixels,
            const char* mode, const char* phase, int count, size_t allocations,
            size_t retained, uint32_t checksum) {
  Distribution d = Summarize(count);
  Serial.printf(
      "DATA,%s,%s,%s,%d,%d,%s,%s,%d,%.2f,%lu,%lu,%lu,%lu,%u,%u,%u,%u,%lu\n",
      kRevision, heap, scene, layers, pixels, mode, phase, count, d.mean,
      static_cast<unsigned long>(d.p50), static_cast<unsigned long>(d.p95),
      static_cast<unsigned long>(d.p99), static_cast<unsigned long>(d.maximum),
      allocations, retained, FreeHeap(), LargestBlock(),
      static_cast<unsigned long>(checksum));
  Serial.flush();
  delay(10);
}

void RunMode(BenchStack& stack, const char* heap, const char* scene, int layers,
             bool reuse, const uint32_t* expected) {
  const char* mode = reuse ? "warm" : "fresh";
  stack.setReuse(reuse);
  size_t before = FreeHeap();
  for (int i = 0; i < 48; ++i)
    stack.createStream(Clip(stack.extents(), i % kVariants));
#ifndef ROO_BENCHMARK_BASELINE
  if (reuse) {
    // Partition buffers rotate through scratch slots; varied clips can need
    // more warm-up than repetitions of a single geometry. Require four full
    // clip cycles without retained growth, and record how long that took.
    int stable_cycles = 0;
    int cycles = 0;
    for (; cycles < 256 && stable_cycles < 4; ++cycles) {
      size_t previous = FreeHeap();
      for (int i = 0; i < kVariants; ++i) {
        stack.createStream(Clip(stack.extents(), i));
      }
      stable_cycles = FreeHeap() == previous ? stable_cycles + 1 : 0;
    }
    if (stable_cycles < 4) {
      Serial.println("FAIL: workspace capacity did not stabilize");
      std::abort();
    }
    Serial.printf("WARMUP,%s,%s,%s,%d,%d,%d\n", kRevision, heap, scene, layers,
                  stack.extents().area(), 48 + cycles * kVariants);
    Serial.flush();
  }
#endif
  size_t retained = before - FreeHeap();
  heap_trace::Start();
  stack.createStream(Clip(stack.extents(), 0));
  heap_trace::Stop();
  size_t allocations = heap_trace::allocations;
  Serial.printf("MEM,%s,%s,%s,%d,%d,%u,%u,%u\n", kRevision, heap, scene, layers,
                stack.extents().area(), allocations,
                heap_trace::requested_bytes, heap_trace::peak_live_bytes);

  // Allocation/destruction only: no pixels sampled during measurement.
  for (int i = 0; i < kPrepareSamples; ++i) {
    Box clip = Clip(stack.extents(), i % kVariants);
    int64_t start = esp_timer_get_time();
    stack.createStream(clip);
    samples[i] = esp_timer_get_time() - start;
    if (i % 32 == 31) delay(1);
  }
  Report(heap, scene, layers, stack.extents().area(), mode, "prepare_destroy",
         kPrepareSamples, allocations, retained, 0);

  for (int i = 0; i < kPrepareSamples; ++i) {
    int64_t start = esp_timer_get_time();
    heap_trace::Replay(true);
    samples[i] = esp_timer_get_time() - start;
  }
  Report(heap, scene, layers, stack.extents().area(), mode, "allocator_replay",
         kPrepareSamples, allocations, retained, 0);
  for (int i = 0; i < kPrepareSamples; ++i) {
    int64_t start = esp_timer_get_time();
    heap_trace::Replay(false);
    samples[i] = esp_timer_get_time() - start;
  }
  Report(heap, scene, layers, stack.extents().area(), mode, "replay_overhead",
         kPrepareSamples, allocations, retained, 0);

  uint32_t combined = 0;
  for (int i = 0; i < kRenderSamples; ++i) {
    int variant = i % kVariants;
    Box clip = Clip(stack.extents(), variant);
    int64_t start = esp_timer_get_time();
    auto stream = stack.createStream(clip);
    uint32_t checksum = Consume(*stream, clip.area());
    stream.reset();
    samples[i] = esp_timer_get_time() - start;
    if (checksum != expected[variant]) {
      Serial.println("FAIL: stream checksum mismatch");
      std::abort();
    }
    combined += checksum;
    if (i % 8 == 7) delay(1);
  }
  Report(heap, scene, layers, stack.extents().area(), mode,
         "prepare_consume_destroy", kRenderSamples, allocations, retained,
         combined);
  DisplaySink sink;
  Display display(sink);
  for (int i = 0; i < kVariants; ++i) {
    Box clip = Clip(stack.extents(), i);
    sink.reset(true);
    {
      DrawingContext dc(display, clip);
      dc.draw(stack);
    }
    Serial.printf("VERIFY,%s,%s,%s,%d,%d,%d,%u,%u,%u\n", kRevision, heap, scene,
                  layers, stack.extents().area(), i, sink.pixels, sink.windows,
                  sink.checksum);
  }
  for (int i = 0; i < kRenderSamples; ++i) {
    Box clip = Clip(stack.extents(), i % kVariants);
    sink.reset(false);
    int64_t start = esp_timer_get_time();
    {
      DrawingContext dc(display, clip);
      dc.draw(stack);
    }
    samples[i] = esp_timer_get_time() - start;
    if (i % 8 == 7) delay(1);
  }
  Report(heap, scene, layers, stack.extents().area(), mode, "draw",
         kRenderSamples, 0, 0, 0);
  stack.release();
}

// Sources and descriptors are constructed outside all measured regions.
void RunScene(bool pressured, const char* scene, int layers, int width,
              int height) {
  Box bounds(0, 0, width - 1, height - 1);
  BenchStack stack(bounds);
  std::vector<std::unique_ptr<Rasterizable>> sources;
  stack.reserveInputs(layers);
  for (int i = 0; i < layers; ++i) {
    Box clip = bounds;
    if (strcmp(scene, "split") == 0 || strcmp(scene, "rounded") == 0) {
      clip = Box((i * 7) % (width / 3), (i * 3) % (height / 3),
                 width - 1 - (i * 5) % (width / 3),
                 height - 1 - (i * 2) % (height / 3));
    }
    auto raster = MakeRasterizable(bounds, [i](int16_t x, int16_t y) {
      return Color(96, (x * 7 + i * 13) % 256, (y * 11 + i) % 256, 128);
    });
    bool is_mask = (strcmp(scene, "opaque_mask") == 0 ||
                    strcmp(scene, "rounded_mask") == 0) &&
                   i > 0;
    if (is_mask) {
      if (strcmp(scene, "opaque_mask") == 0) {
        sources.emplace_back(new FilledRect(bounds, color::White));
      } else {
        auto mask =
            SmoothFilledRoundRect(bounds.xMin(), bounds.yMin(), bounds.xMax(),
                                  bounds.yMax(), 5.0f, color::White);
        sources.emplace_back(new decltype(mask)(std::move(mask)));
      }
    } else if (strcmp(scene, "uniform") == 0 ||
               (strcmp(scene, "opaque_top") == 0 && i == layers - 1)) {
      Color color =
          strcmp(scene, "opaque_top") == 0 ? color::Blue : Color(0x60805030);
      sources.emplace_back(new FilledRect(bounds, color));
    } else if (strcmp(scene, "rounded") == 0) {
      auto shape = SmoothFilledRoundRect(clip.xMin(), clip.yMin(), clip.xMax(),
                                         clip.yMax(), 5.0f, Color(0x60805030));
      sources.emplace_back(new decltype(shape)(std::move(shape)));
    } else {
      sources.emplace_back(new decltype(raster)(raster));
    }
    stack.addInput(sources.back().get(), clip)
        .withMode(is_mask ? BlendingMode::kDestinationIn
                          : BlendingMode::kSourceOver);
  }
  HeapPressure pressure(pressured);
  uint32_t expected[kVariants];
  for (int i = 0; i < kVariants; ++i) {
    Box clip = Clip(bounds, i);
    auto stream = stack.createStream(clip);
    expected[i] = Consume(*stream, clip.area());
  }
  const char* heap = pressured ? "mixed" : "clean";
  RunMode(stack, heap, scene, layers, false, expected);
#ifndef ROO_BENCHMARK_BASELINE
  RunMode(stack, heap, scene, layers, true, expected);
#endif
  if (!heap_caps_check_integrity_all(true)) std::abort();
}

// Checks sustained churn separately from short latency measurements.
void RunChurn(bool reuse) {
  const Box bounds(0, 0, 31, 23);
  BenchStack stack(bounds);
  FilledRect source(bounds, Color(0x80402010));
  for (int i = 0; i < 4; ++i) {
    stack.addInput(&source, Box(i * 3, i * 2, 31 - i * 2, 23 - i));
  }
  HeapPressure pressure(true);
  stack.setReuse(reuse);
  for (int i = 0; i < 48; ++i) stack.createStream(Clip(bounds, i % kVariants));
  size_t free_before = FreeHeap();
  size_t largest_before = LargestBlock();
  int64_t start = esp_timer_get_time();
  for (int i = 0; i < 20000; ++i) {
    stack.createStream(Clip(bounds, i % kVariants));
    if (i % 256 == 255) delay(1);
  }
  Serial.printf("CHURN,%s,%s,20000,%lld,%u,%u,%u,%u,%d\n", kRevision,
                reuse ? "warm" : "fresh", esp_timer_get_time() - start,
                free_before, FreeHeap(), largest_before, LargestBlock(),
                heap_caps_check_integrity_all(true));
  Serial.flush();
}
}  // namespace

void RunCompositionBenchmark() {
  Serial.printf(
      "BEGIN,%s,chip=%s,cpu_mhz=%u,psram=%u,heap=%u,sdk=%s,compiler=%s\n",
      kRevision, ESP.getChipModel(), getCpuFrequencyMhz(), ESP.getPsramSize(),
      FreeHeap(), ESP.getSdkVersion(), __VERSION__);
  Serial.println(
      "FIELDS,revision,heap,scene,layers,pixels,mode,phase,samples,mean_us,p50_"
      "us,p95_us,p99_us,max_us,allocations,retained_bytes,free_heap,largest_"
      "block,checksum");
  for (bool pressured : {false, true}) {
    for (int width : {32, 160}) {
      RunScene(pressured, "overlap", 1, width, width * 3 / 4);
      for (int layers : {4, 16}) {
        for (const char* scene : {"overlap", "split", "opaque_top"}) {
          RunScene(pressured, scene, layers, width, width * 3 / 4);
        }
      }
      RunScene(pressured, "uniform", 4, width, width * 3 / 4);
      RunScene(pressured, "rounded", 4, width, width * 3 / 4);
      RunScene(pressured, "opaque_mask", 4, width, width * 3 / 4);
      RunScene(pressured, "rounded_mask", 4, width, width * 3 / 4);
    }
  }
  // Sustained churn is covered by the separate workspace benchmark.
#ifndef ROO_BENCHMARK_BASELINE
  RunChurn(true);
#endif
  Serial.printf("END,%s,heap=%u,integrity=%d\n", kRevision, FreeHeap(),
                heap_caps_check_integrity_all(true));
  Serial.flush();
}
