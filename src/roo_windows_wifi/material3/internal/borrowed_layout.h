#pragma once

#include "roo_windows/containers/flex_layout.h"
#include "roo_windows/containers/horizontal_layout.h"

namespace roo_windows_wifi::material3::internal {

/// Column flex layout that fills its parent width and wraps its content height.
class FlexColumn : public roo_windows::FlexLayout {
 public:
  explicit FlexColumn(roo_windows::ApplicationContext& context,
                      int16_t gap = 0)
      : FlexLayout(context, roo_windows::FlexDirection::kColumn) {
    setGap(gap);
  }

  roo_windows::PreferredSize getPreferredSize() const override {
    return {roo_windows::PreferredSize::MatchParentWidth(),
            roo_windows::PreferredSize::WrapContentHeight()};
  }
};

/// Action row; Panel handles detaching its borrowed children.
using BorrowedRow = roo_windows::HorizontalLayout;

}  // namespace roo_windows_wifi::material3::internal
