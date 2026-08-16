import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import time as time_
from esphome.const import CONF_ID, CONF_TIME_ID

CODEOWNERS = ["@your-github-handle"]
DEPENDENCIES = ["time"]

# This namespace must match the C++ namespace in composite_clock.h
composite_clock_ns = cg.esphome_ns.namespace("composite_clock")
CompositeClockDisplay = composite_clock_ns.class_(
    "CompositeClockDisplay", cg.Component
)

# Enum mirrored from C++ CompositeVideoMode. cv.enum returns the dict value
# for the matched key, so map the YAML names to the C++ enum member names.
MODES = {
    "NTSC": "COMPOSITE_VIDEO_NTSC",
    "PAL": "COMPOSITE_VIDEO_PAL",
}

CONF_MODE = "mode"
CONF_HUE = "hue"
CONF_BRIGHTNESS = "brightness"
CONF_SHOW_SECONDS = "show_seconds"
CONF_BLINK_COLON = "blink_colon"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CompositeClockDisplay),
            cv.GenerateID(CONF_TIME_ID): cv.use_id(time_.RealTimeClock),
            cv.Optional(CONF_MODE, default="NTSC"): cv.enum(MODES, upper=True),
            cv.Optional(CONF_HUE, default=0): cv.int_range(min=0, max=15),
            cv.Optional(CONF_BRIGHTNESS, default=54): cv.int_range(min=0, max=54),
            cv.Optional(CONF_SHOW_SECONDS, default=False): cv.boolean,
            cv.Optional(CONF_BLINK_COLON, default=True): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    time_id = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_id))

    # cv.enum returns an EnumValue whose .enum_value is the C++ enum member
    # name from the MODES mapping above. Emit it as a scoped enum expression.
    cg.add(var.set_mode(cg.RawExpression(
        f"composite_clock::CompositeVideoMode::{config[CONF_MODE].enum_value}"
    )))
    cg.add(var.set_hue(config[CONF_HUE]))
    cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))
    cg.add(var.set_show_seconds(config[CONF_SHOW_SECONDS]))
    cg.add(var.set_blink_colon(config[CONF_BLINK_COLON]))

    # Pull in the ESP32CompositeColorVideo Arduino library from a fork that
    # carries build fixes for ESP-IDF 5.x and Arduino-ESP32 3.x (the upstream
    # marciot/ESP32CompositeColorVideo repo no longer compiles against the
    # framework versions ESPHome 2026.x ships).
    # The library ships a library.properties with sources under src/, so
    # PlatformIO automatically adds that src/ directory to the include path
    # and #include "CompositeGraphics.h" resolves without extra build flags.
    cg.add_library(
        "ESP32CompositeColorVideo",
        None,
        "https://github.com/guruprasadah/ESP32CompositeColorVideo.git",
    )
