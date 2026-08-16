#pragma once

#include "esphome/core/component.h"
#include "esphome/core/time.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace composite_clock {

/// Composite video standard to use.
enum CompositeVideoMode { COMPOSITE_VIDEO_NTSC, COMPOSITE_VIDEO_PAL };

/// ESPHome component that renders the current time (24-hour format) as large
/// seven-segment digits in the centre of a composite-video signal, using the
/// ESP32CompositeColorVideo library.
///
/// The colour is selected with `hue` (0-15) and the intensity / opacity with
/// `brightness` (0-54, where 0 is black/invisible and 54 is full intensity).
class CompositeClockDisplay : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  /// Source of the current time (e.g. sntp, homeassistant, ...).
  void set_time(time::RealTimeClock *time) {
    time_ = time;
    dirty_ = true;
  }

  void set_mode(CompositeVideoMode mode) {
    mode_ = mode;
    dirty_ = true;
  }

  /// Colour hue, 0-15 (0 = white / black & white).
  void set_hue(uint8_t hue) {
    hue_ = hue;
    dirty_ = true;
  }

  /// Brightness / opacity, 0-54 (0 = invisible, 54 = full intensity).
  void set_brightness(uint8_t brightness) {
    brightness_ = brightness;
    dirty_ = true;
  }

  /// Show seconds (HH:MM:SS) instead of just HH:MM.
  void set_show_seconds(bool show_seconds) {
    show_seconds_ = show_seconds;
    dirty_ = true;
  }

  /// Blink the colon once per second.
  void set_blink_colon(bool blink_colon) {
    blink_colon_ = blink_colon;
    dirty_ = true;
  }

 protected:
  void draw_digit_(int digit, int dx, int dy, int dw, int dh, int t, uint8_t brightness);
  void draw_colon_(int cx, int dy, int dh, int t, uint8_t brightness);
  void render_(const ESPTime &now);

  time::RealTimeClock *time_{nullptr};
  CompositeVideoMode mode_{COMPOSITE_VIDEO_NTSC};
  uint8_t hue_{0};          // 0-15
  uint8_t brightness_{54};  // 0-54 (opacity / brightness)
  bool show_seconds_{false};
  bool blink_colon_{true};

  // Opaque pointers to the library types (CompositeGraphics / CompositeColorOutput).
  // Kept as void* so the Arduino library headers only need to be included from
  // the .cpp, keeping this header lightweight and platform-agnostic.
  void *graphics_{nullptr};
  void *composite_{nullptr};

  // Redraw bookkeeping.
  bool dirty_{true};
  int last_unit_{-1};  // last minute (or second) that was drawn
};

}  // namespace composite_clock
}  // namespace esphome
