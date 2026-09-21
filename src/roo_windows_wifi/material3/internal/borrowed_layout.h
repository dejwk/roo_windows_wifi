#pragma once

#include "roo_windows/containers/horizontal_layout.h"
#include "roo_windows/containers/scrollable_panel.h"
#include "roo_windows/containers/vertical_layout.h"

namespace roo_windows_wifi::material3::internal {

/// Detaches borrowed inline widgets before the column itself is destroyed.
class BorrowedColumn : public roo_windows::VerticalLayout {
 public:
  explicit BorrowedColumn(roo_windows::ApplicationContext& context)
      : VerticalLayout(context) {}

  ~BorrowedColumn() override { removeAll(); }

  roo_windows::PreferredSize getPreferredSize() const override {
    return {roo_windows::PreferredSize::MatchParentWidth(),
            roo_windows::PreferredSize::WrapContentHeight()};
  }
};

/// Detaches borrowed inline widgets before the action row is destroyed.
class BorrowedRow : public roo_windows::HorizontalLayout {
 public:
  explicit BorrowedRow(roo_windows::ApplicationContext& context)
      : HorizontalLayout(context) {}

  ~BorrowedRow() override { removeAll(); }
};

/// Repaints the bounded viewport after moving rich field/list contents.
/// Text-field interior clips must be recomputed when previously hidden fields
/// enter the viewport; a scroll does not preserve their old dirty subregions.
class FormScroll : public roo_windows::SimpleScrollablePanel {
 public:
  FormScroll(roo_windows::ApplicationContext& context,
             roo_windows::Widget& body)
      : SimpleScrollablePanel(context, body) {}

 protected:
  void onScrollPositionChanged() override { invalidateInterior(); }
};

}  // namespace roo_windows_wifi::material3::internal
