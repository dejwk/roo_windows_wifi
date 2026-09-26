#pragma once

#include "roo_windows/material3/list/list.h"

namespace roo_windows_wifi::material3::internal {

/// Arranges borrowed rows as a full-width expressive segmented list.
class SegmentedList : public roo_windows::material3::List {
 public:
  /// Creates an empty segmented list using @p context.
  explicit SegmentedList(roo_windows::ApplicationContext& context)
      : List(context) {
    setVariant(roo_windows::material3::ListVariant::kExpressive);
    setStyle(roo_windows::material3::ListStyle::kSegmented);
  }

  /// Fills the available width and sizes its height to the visible rows.
  roo_windows::PreferredSize getPreferredSize() const override {
    return {roo_windows::PreferredSize::MatchParentWidth(),
            roo_windows::PreferredSize::WrapContentHeight()};
  }

  /// Insets the section from the surrounding screen edges.
  roo_windows::Margins getMargins() const override {
    return roo_windows::Margins(roo_windows::Scaled(8));
  }
};

}  // namespace roo_windows_wifi::material3::internal
