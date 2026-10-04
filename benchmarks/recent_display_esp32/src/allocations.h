#pragma once
#include <Arduino.h>
namespace allocation_count {
extern bool enabled;
extern size_t calls, bytes, frees;
extern TaskHandle_t task;
}  // namespace allocation_count
namespace heap_trace {
extern size_t allocations, requested_bytes, peak_live_bytes;
/// Starts an untimed, task-local C++ allocation trace.
void Start();
/// Stops tracing and requires all captured allocations to have been freed.
void Stop();
/// Replays lifetimes or measures loop overhead without allocator calls.
void Replay(bool allocate);
}  // namespace heap_trace
extern const char* benchmark_revision;
/// Runs the synthetic suite once, reporting serial CSV records.
void RunCompositionBenchmark();
/// Runs the retained Wi-Fi flow once, reporting serial CSV records.
void RunWifiBenchmark();
