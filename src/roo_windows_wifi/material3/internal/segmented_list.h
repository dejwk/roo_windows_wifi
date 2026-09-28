#pragma once

#include "roo_windows/containers/vertical_layout.h"
#include "roo_windows/core/application_context.h"
#include "roo_windows/core/margins_mixin.h"
#include "roo_windows/material3/list/list.h"
#include "roo_windows/material3/typography.h"
#include "roo_windows/widgets/text_label.h"

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
};

class CaptionedSegmentedList : public roo_windows::VerticalLayout {
 public:
  explicit CaptionedSegmentedList(roo_windows::ApplicationContext& context,
                                  roo::string_view caption)
      : VerticalLayout(context),
        caption_(context, caption,
                 roo_windows::material3::text_style_title_small()),
        list_(context) {
    add(caption_);
    add(list_);
    caption_.setMargins(roo_windows::MarginSize::k8dp, roo_windows::MarginSize::k8dp);
  }

  SegmentedList& list() { return list_; }

  roo_windows::PreferredSize getPreferredSize() const override {
    return {roo_windows::PreferredSize::MatchParentWidth(),
            roo_windows::PreferredSize::WrapContentHeight()};
  }

 private:
  roo_windows::MarginsMixin<roo_windows::StringViewLabel> caption_;
  SegmentedList list_;
};

}  // namespace roo_windows_wifi::material3::internal
