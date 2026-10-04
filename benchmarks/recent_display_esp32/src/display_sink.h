#pragma once
#include "roo_display.h"
#include "roo_display/internal/color_format.h"
#include "roo_logging.h"
using roo_display::BlendingMode;
using roo_display::Color;
// RGB565 output without hardware blending/blitting, like the example's ILI9341.
// An untimed pass hashes each pixel with its destination to check equivalence.
// Timed passes only count emitted pixels, excluding hashing and transport cost.
class DisplaySink : public roo_display::DisplayDevice {
 public:
  /// Creates an output with the example display dimensions and capabilities.
  DisplaySink() : DisplayDevice(320, 240), format_(roo_display::Rgb565()) {}

  const ColorFormat& getColorFormat() const override { return format_; }

  /// Starts a frame, enabling pixel verification only in untimed passes.
  void reset(bool verify) {
    verify_ = verify;
    pixels = 0;
    windows = 0;
    checksum = 2166136261u;
  }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    CHECK(x0 <= x1 && y0 <= y1 && x1 < 320 && y1 < 240);
    left_ = x_ = x0;
    right_ = x1;
    y_ = y0;
    ++windows;
  }

  void write(Color* colors, uint32_t count) override {
    pixels += count;
    if (!verify_) return;
    for (uint32_t i = 0; i < count; ++i) hash(colors[i]);
  }

  void fill(Color color, uint32_t count) override {
    pixels += count;
    if (!verify_) return;
    for (uint32_t i = 0; i < count; ++i) hash(color);
  }

  void writePixels(BlendingMode mode, Color* colors, int16_t* xs, int16_t* ys,
                   uint16_t count) override {
    for (uint16_t i = 0; i < count; ++i) {
      setAddress(xs[i], ys[i], xs[i], ys[i], mode);
      write(colors + i, 1);
    }
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* xs, int16_t* ys,
                  uint16_t count) override {
    for (uint16_t i = 0; i < count; ++i) {
      setAddress(xs[i], ys[i], xs[i], ys[i], mode);
      fill(color, 1);
    }
  }

  void writeRects(BlendingMode mode, Color* colors, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    for (uint16_t i = 0; i < count; ++i) {
      setAddress(x0[i], y0[i], x1[i], y1[i], mode);
      fill(colors[i], (x1[i] - x0[i] + 1) * (y1[i] - y0[i] + 1));
    }
  }

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    for (uint16_t i = 0; i < count; ++i) {
      setAddress(x0[i], y0[i], x1[i], y1[i], mode);
      fill(color, (x1[i] - x0[i] + 1) * (y1[i] - y0[i] + 1));
    }
  }

  uint32_t pixels = 0;
  uint32_t windows = 0;
  uint32_t checksum = 0;

 private:
  void hash(Color color) {
    // Permit different tile/fill batching while checking emitted color/address
    // pairs. Pixel count is compared separately; this is not a visual golden.
    uint32_t sample = color.asArgb() ^ ((y_ * 320 + x_ + 1) * 2654435761u);
    sample ^= sample >> 16;
    sample *= 0x7feb352du;
    sample ^= sample >> 15;
    sample *= 0x846ca68bu;
    checksum += sample ^ (sample >> 16);
    if (++x_ > right_) {
      x_ = left_;
      ++y_;
    }
  }

  roo_display::internal::ColorFormatImpl<
      roo_display::Rgb565, roo_io::ByteOrder::kBigEndian,
      roo_display::ColorPixelOrder::kMsbFirst>
      format_;
  bool verify_ = false;
  int left_ = 0;
  int right_ = 0;
  int x_ = 0;
  int y_ = 0;
};
