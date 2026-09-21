#pragma once

#include "roo_windows/containers/horizontal_layout.h"
#include "roo_windows/containers/vertical_layout.h"

namespace roo_windows_wifi::material3::internal {

/// Detaches borrowed inline widgets before the column itself is destroyed.
class BorrowedColumn : public roo_windows::VerticalLayout {
 public:
  explicit BorrowedColumn(roo_windows::ApplicationContext& context)
      : VerticalLayout(context) {}

  ~BorrowedColumn() override { removeAll(); }
};

/// Detaches borrowed inline widgets before the action row is destroyed.
class BorrowedRow : public roo_windows::HorizontalLayout {
 public:
  explicit BorrowedRow(roo_windows::ApplicationContext& context)
      : HorizontalLayout(context) {}

  ~BorrowedRow() override { removeAll(); }
};

}  // namespace roo_windows_wifi::material3::internal
