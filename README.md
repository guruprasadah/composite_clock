# composite_clock

An ESPHome custom component that displays the current time — in 24-hour format,
as large seven-segment digits centred on the screen — over ESP32 composite
video, using (an updated personal fork of) the
[ESP32CompositeColorVideo](https://github.com/guruprasadah/ESP32CompositeColorVideo)
library by Marcio Teixeira (a colour-capable fork of bitluni's
ESP32CompositeVideo).

The colour (hue) and opacity/brightness are configurable, and can be changed at
runtime from Home Assistant or from lambdas.

## Wiring

For an Adafruit HUZZAH32 (or any ESP32 with a DAC):

| ESP32 pin | RCA plug |
|-----------|----------|
| `GND`     | outer barrel |
| `GPIO25` (DAC1) | centre pin |

Plug the other end of the RCA cable into the yellow (video) jack of your TV or
monitor. Select `mode: NTSC` or `mode: PAL` to match your region.

## Installation
Reference it with `external_components`:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/guruprasadah/composite_clock
```
## Configuration

```yaml
composite_clock:
  id: my_clock
  time_id: sntp_time   # required - any ESPHome time source (sntp, homeassistant, ...)
  mode: NTSC           # NTSC or PAL (default: NTSC)
  hue: 4               # 0-15, default 0 (white / B&W)
  brightness: 54       # 0-54, default 54 (full intensity)
  show_seconds: false  # show HH:MM:SS instead of HH:MM (default: false)
  blink_colon: true     # blink the colon once per second (default: true)
```

### Configuration variables

- **`time_id`** (**Required**, ID): The ID of an ESPHome `time` component
  (e.g. `sntp`, `homeassistant`, `ds1307`, ...). The clock reads the current
  time from this source every frame.
- **`mode`** (*Optional*, string): `NTSC` or `PAL`. Defaults to `NTSC`.
- **`hue`** (*Optional*, int 0–15): Colour hue. `0` is black & white; other
  values cycle through the 16 Atari-style hues (greens, blues, reds, etc.).
  Defaults to `0`.
- **`brightness`** (*Optional*, int 0–54): Brightness / opacity of the digits.
  `0` is invisible (black), `54` is full intensity. Defaults to `54`.
- **`show_seconds`** (*Optional*, boolean): Show `HH:MM:SS` instead of `HH:MM`.
  Defaults to `false`.
- **`blink_colon`** (*Optional*, boolean): Blink the colon once per second.
  Defaults to `true`.

## Changing colour/brightness at runtime

The `set_hue(uint8_t)` and `set_brightness(uint8_t)` methods are public, so you
can drive them from a template `number` (or any automation). See
`example-clock.yaml` for a complete example, but in short:

```yaml
number:
  - platform: template
    name: "Clock Hue"
    min_value: 0
    max_value: 15
    step: 1
    initial_value: 4
    on_value:
      then:
        - lambda: 'id(my_clock).set_hue((uint8_t) x);'

  - platform: template
    name: "Clock Brightness"
    min_value: 0
    max_value: 54
    step: 1
    initial_value: 54
    on_value:
      then:
        - lambda: 'id(my_clock).set_brightness((uint8_t) x);'
```

## How it works

- The component creates a `CompositeGraphics` (336×240 half-resolution
  framebuffer) and a `CompositeColorOutput` (NTSC or PAL) from the
  ESP32CompositeColorVideo library in `setup()`.
- On every `loop()` it reads the current time from the configured `time`
  source. If the displayed unit (minute, or second when `show_seconds` is on)
  has changed — or a setting changed — it redraws the frame; otherwise it just
  re-sends the existing framebuffer so the TV keeps a stable signal.
- The digits are drawn as classic seven-segment glyphs using
  `CompositeGraphics::fillRect()`, sized to roughly fill the centre of the
  screen. The colour is set via `Color::setHue(hue)` and the per-pixel
  brightness is the `brightness` value passed to `fillRect()` — which is why
  `brightness` doubles as opacity (0 = black/invisible).
- The finished frame is pushed to the DAC with
  `CompositeColorOutput::sendFrameHalfResolution(&graphics.frame)`.

## Limitations / notes

- ESP32 only. The library uses the ESP32 DAC + I²S/DMA for the video signal,
  so this component will not build for ESP8266 or other platforms.
- The composite video output runs from an interrupt/DMA continuously, so the
  ESP32 is effectively dedicated to driving the display while powered on.
- `hue` and `brightness` map directly onto the library's 16-hue × 16-brightness
  colour model (the library accepts a 0–54 brightness range for bitluni
  compatibility, internally quantised to 16 levels).
