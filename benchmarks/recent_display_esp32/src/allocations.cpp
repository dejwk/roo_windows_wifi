#include "allocations.h"

#include <algorithm>
#include <cstdlib>
#include <new>
namespace allocation_count {
bool enabled = false;
size_t calls = 0;
size_t bytes = 0;
size_t frees = 0;
TaskHandle_t task = nullptr;
}  // namespace allocation_count

namespace heap_trace {
// Capture one untimed C++ allocation/free trace. Timing disables recording;
// replay uses malloc/free directly, without compiler work or child
// constructors.
constexpr size_t kSlots = 256;
constexpr size_t kEvents = 2048;
struct Event {
  size_t size;
  uint16_t slot;
};
Event events[kEvents];
void* slots[kSlots]{};
size_t length = 0;
size_t allocations = 0;
size_t requested_bytes = 0;
size_t peak_live_bytes = 0;
size_t live_bytes = 0;
size_t slot_sizes[kSlots]{};
bool enabled = false;
TaskHandle_t task = nullptr;

void Record(void* ptr, size_t size) {
  if (!enabled || xTaskGetCurrentTaskHandle() != task) return;
  size_t slot = 0;
  if (size != 0) {
    while (slot < kSlots && slots[slot] != nullptr) ++slot;
    ++allocations;
    requested_bytes += size;
    live_bytes += size;
    peak_live_bytes = std::max(peak_live_bytes, live_bytes);
    if (slot < kSlots) slot_sizes[slot] = size;
  } else {
    while (slot < kSlots && slots[slot] != ptr) ++slot;
    if (slot == kSlots) return;  // Storage predating the capture.
    live_bytes -= slot_sizes[slot];
  }
  if (slot == kSlots || length == kEvents) std::abort();
  slots[slot] = size != 0 ? ptr : nullptr;
  events[length++] = {size, static_cast<uint16_t>(slot)};
}

void Start() {
  length = 0;
  allocations = 0;
  requested_bytes = peak_live_bytes = live_bytes = 0;
  std::fill_n(slots, kSlots, nullptr);
  task = xTaskGetCurrentTaskHandle();
  enabled = true;
}

void Stop() {
  enabled = false;
  for (void* slot : slots) {
    if (slot != nullptr) std::abort();
  }
}

// Replays sizes and lifetimes in order, also offering a loop-overhead control.
__attribute__((noinline)) void Replay(bool allocate) {
  for (size_t i = 0; i < length; ++i) {
    const Event& event = events[i];
    if (event.size != 0) {
      slots[event.slot] = allocate ? std::malloc(event.size) : nullptr;
      if (allocate && slots[event.slot] == nullptr) std::abort();
    } else {
      if (allocate) std::free(slots[event.slot]);
      slots[event.slot] = nullptr;
    }
  }
}
}  // namespace heap_trace

void* operator new(size_t size) {
  size = std::max<size_t>(size, 1);
  void* ptr = std::malloc(size);
  if (ptr == nullptr) std::abort();
  heap_trace::Record(ptr, size);
  if (allocation_count::enabled &&
      xTaskGetCurrentTaskHandle() == allocation_count::task) {
    ++allocation_count::calls;
    allocation_count::bytes += size;
  }
  return ptr;
}
void* operator new[](size_t size) { return ::operator new(size); }
void* operator new(size_t size, const std::nothrow_t&) noexcept {
  return ::operator new(size);
}
void* operator new[](size_t size, const std::nothrow_t&) noexcept {
  return ::operator new(size);
}
void operator delete(void* ptr) noexcept {
  if (ptr != nullptr) {
    heap_trace::Record(ptr, 0);
    if (allocation_count::enabled &&
        xTaskGetCurrentTaskHandle() == allocation_count::task)
      ++allocation_count::frees;
  }
  std::free(ptr);
}
void operator delete[](void* ptr) noexcept { ::operator delete(ptr); }
void operator delete(void* ptr, size_t) noexcept { ::operator delete(ptr); }
void operator delete[](void* ptr, size_t) noexcept { ::operator delete(ptr); }
void operator delete(void* ptr, const std::nothrow_t&) noexcept {
  ::operator delete(ptr);
}
void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
  ::operator delete(ptr);
}
