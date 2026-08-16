#include "composite_clock.h"

#include "esphome/core/log.h"

// ESP32CompositeColorVideo library (Arduino library pulled in via cg.add_library).
// The library is not registered as a PlatformIO component with a clean include
// path, so we add its "src" directory explicitly.
#include "CompositeGraphics.h"
#include "CompositeColorOutput.h"

namespace esphome {
namespace composite_clock {

static const char *const TAG = "composite_clock";

// Library framebuffer is 336x240 (half-resolution). We render into the full
// buffer; the library doubles each pixel horizontally when sending the frame.
static constexpr int SCREEN_W = CompositeColorOutput::XRES;  // 336
static constexpr int SCREEN_H = CompositeColorOutput::YRES;  // 240

// Seven-segment encoding: bit 0 = top, going clockwise, bit 6 = middle.
//   0
// 5   1
//   6
// 4   2
//   3
static const uint8_t SEGMENTS[10] = {
    0b1111110,  // 0
    0b0110000,  // 1
    0b1101101,  // 2
    0b1111001,  // 3
    0b0110011,  // 4
    0b1011011,  // 5
    0b1011111,  // 6
    0b1110000,  // 7
    0b1111111,  // 8
    0b1111011,  // 9
};

void CompositeClockDisplay::setup() {
  ESP_LOGCONFIG(TAG, "Setting up composite clock display...");

  // Construct the graphics + composite output objects. We keep them as void*
  // in the header so the Arduino library headers don't leak into other
  // translation units.
  graphics_ = new CompositeGraphics(SCREEN_W, SCREEN_H);
  composite_ = new CompositeColorOutput(mode_ == COMPOSITE_VIDEO_NTSC
                                            ? CompositeColorOutput::NTSC
                                            : CompositeColorOutput::PAL);

  auto *g = static_cast<CompositeGraphics *>(graphics_);
  auto *c = static_cast<CompositeColorOutput *>(composite_);

  g->init();
  c->init();

  // Clear the framebuffer to black once so the first frame is clean.
  Color::setHue(0);
  g->begin(0);
  g->end();
  c->sendFrameHalfResolution(&g->frame);

  ESP_LOGCONFIG(TAG, "Composite clock ready (mode=%s, %dx%d).",
                mode_ == COMPOSITE_VIDEO_NTSC ? "NTSC" : "PAL", SCREEN_W, SCREEN_H);
}

void CompositeClockDisplay::loop() {
  if (time_ == nullptr) {
    return;
  }

  ESPTime now = time_->now();
  if (!now.is_valid()) {
    return;
  }

  // Decide how often to redraw. With seconds shown we redraw every second;
  // otherwise once per minute (and once when settings change).
  int unit = show_seconds_ ? now.second : now.minute;

  if (!dirty_ && unit == last_unit_) {
    // Nothing changed; keep sending the existing frame so the TV keeps a
    // stable signal. The library's video output is driven by a DMA timer, so
    // we just need to keep the framebuffer pointer live.
    auto *g = static_cast<CompositeGraphics *>(graphics_);
    auto *c = static_cast<CompositeColorOutput *>(composite_);
    c->sendFrameHalfResolution(&g->frame);
    return;
  }

  dirty_ = false;
  last_unit_ = unit;

  render_(now);
}

void CompositeClockDisplay::render_(const ESPTime &now) {
  auto *g = static_cast<CompositeGraphics *>(graphics_);
  auto *c = static_cast<CompositeColorOutput *>(composite_);

  // Clear to black and set the hue for everything we draw this frame.
  Color::setHue(hue_);
  g->begin(0);

  // Layout: digits are large seven-segment glyphs centred on screen.
  // We pick a digit size that fits comfortably and leaves a margin.
  const int digit_count = show_seconds_ ? 6 : 4;  // HH:MM(:SS)
  const int colon_count = show_seconds_ ? 2 : 1;

  // Digit geometry. Width is ~half of height for a classic 7-seg look.
  const int digit_h = 110;
  const int digit_w = digit_h / 2;            // 55
  const int seg_thickness = digit_h / 8;     // ~13
  const int colon_w = seg_thickness * 2;     // narrow colon column
  const int h_gap = seg_thickness;            // gap between digits
  const int colon_gap = seg_thickness * 2;    // gap around colon

  const int total_w = digit_count * digit_w + (digit_count - 1) * h_gap +
                      colon_count * colon_w + 2 * colon_count * colon_gap;
  const int start_x = (SCREEN_W - total_w) / 2;
  const int start_y = (SCREEN_H - digit_h) / 2;

  // Brightness doubles as opacity: 0 = invisible, 54 = full intensity.
  const uint8_t b = brightness_;

  int x = start_x;
  auto advance = [&](int w, int gap) {
    x += w + gap;
  };

  // HH
  draw_digit_(now.hour / 10, x, start_y, digit_w, digit_h, seg_thickness, b);
  advance(digit_w, h_gap);
  draw_digit_(now.hour % 10, x, start_y, digit_w, digit_h, seg_thickness, b);
  advance(digit_w, colon_gap);

  // Colon
  bool colon_on = !blink_colon_ || (now.second % 2 == 0);
  draw_colon_(x + colon_w / 2, start_y, digit_h, seg_thickness, colon_on ? b : 0);
  advance(colon_w, colon_gap);

  // MM
  draw_digit_(now.minute / 10, x, start_y, digit_w, digit_h, seg_thickness, b);
  advance(digit_w, h_gap);
  draw_digit_(now.minute % 10, x, start_y, digit_w, digit_h, seg_thickness, b);

  if (show_seconds_) {
    advance(digit_w, colon_gap);
    draw_colon_(x + colon_w / 2, start_y, digit_h, seg_thickness, colon_on ? b : 0);
    advance(colon_w, colon_gap);
    draw_digit_(now.second / 10, x, start_y, digit_w, digit_h, seg_thickness, b);
    advance(digit_w, h_gap);
    draw_digit_(now.second % 10, x, start_y, digit_w, digit_h, seg_thickness, b);
  }

  g->end();
  c->sendFrameHalfResolution(&g->frame);
}

void CompositeClockDisplay::draw_digit_(int digit, int dx, int dy, int dw, int dh, int t,
                                        uint8_t brightness) {
  if (digit < 0 || digit > 9) {
    return;
  }
  auto *g = static_cast<CompositeGraphics *>(graphics_);
  uint8_t segs = SEGMENTS[digit];

  // Segment coordinates. Horizontal segments are t tall, vertical segments
  // are t wide. We inset by t so segments don't quite touch the corners,
  // giving the classic seven-segment look.
  const int x0 = dx + t;
  const int x1 = dx + dw - t;
  const int y0 = dy + t;
  const int y1 = dy + dh / 2 - t / 2;
  const int y2 = dy + dh / 2 + t / 2;
  const int y3 = dy + dh - t;
  const int hw = x1 - x0;  // horizontal segment length
  const int vh = y1 - y0;  // vertical segment length (top half)
  const int vh2 = y3 - y2; // vertical segment length (bottom half)

  // bit 0 = top, 1 = top-right, 2 = bottom-right, 3 = bottom,
  // 4 = bottom-left, 5 = top-left, 6 = middle
  if (segs & (1 << 0)) g->fillRect(x0, y0, hw, t, brightness);          // top
  if (segs & (1 << 1)) g->fillRect(x1, y0, t, vh, brightness);          // top-right
  if (segs & (1 << 2)) g->fillRect(x1, y2, t, vh2, brightness);         // bottom-right
  if (segs & (1 << 3)) g->fillRect(x0, y3, hw, t, brightness);          // bottom
  if (segs & (1 << 4)) g->fillRect(x0, y2, t, vh2, brightness);         // bottom-left
  if (segs & (1 << 5)) g->fillRect(x0, y0, t, vh, brightness);         // top-left
  if (segs & (1 << 6)) g->fillRect(x0, y1, hw, t, brightness);         // middle
}

void CompositeClockDisplay::draw_colon_(int cx, int dy, int dh, int t, uint8_t brightness) {
  if (brightness == 0) {
    return;
  }
  auto *g = static_cast<CompositeGraphics *>(graphics_);
  // Two dots, each a small square, vertically centred on the digit.
  const int dot = t;
  const int top_y = dy + dh / 3 - dot / 2;
  const int bot_y = dy + 2 * dh / 3 - dot / 2;
  g->fillRect(cx - dot / 2, top_y, dot, dot, brightness);
  g->fillRect(cx - dot / 2, bot_y, dot, dot, brightness);
}

void CompositeClockDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "Composite Clock Display:");
  ESP_LOGCONFIG(TAG, "  Mode: %s", mode_ == COMPOSITE_VIDEO_NTSC ? "NTSC" : "PAL");
  ESP_LOGCONFIG(TAG, "  Hue: %u", hue_);
  ESP_LOGCONFIG(TAG, "  Brightness: %u", brightness_);
  ESP_LOGCONFIG(TAG, "  Show seconds: %s", show_seconds_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  Blink colon: %s", blink_colon_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  Resolution: %dx%d", SCREEN_W, SCREEN_H);
}

}  // namespace composite_clock
}  // namespace esphome
