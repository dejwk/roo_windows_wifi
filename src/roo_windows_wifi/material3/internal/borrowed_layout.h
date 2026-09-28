#pragma once

#include "roo_windows/containers/horizontal_layout.h"
#include "roo_windows/containers/vertical_layout.h"

namespace roo_windows_wifi::material3::internal {

/// Column that fills the available width and wraps its content height.
class BorrowedColumn : public roo_windows::VerticalLayout {
 public:
  explicit BorrowedColumn(roo_windows::ApplicationContext& context)
      : VerticalLayout(context) {}

  roo_windows::PreferredSize getPreferredSize() const override {
    return {roo_windows::PreferredSize::MatchParentWidth(),
            roo_windows::PreferredSize::WrapContentHeight()};
  }
};

/// Action row; Panel handles detaching its borrowed children.
using BorrowedRow = roo_windows::HorizontalLayout;

}  // namespace roo_windows_wifi::material3::internal
