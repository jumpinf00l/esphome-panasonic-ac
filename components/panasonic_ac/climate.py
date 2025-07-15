import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, climate, sensor, select, switch
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_ICON,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_DEVICE_CLASS,
    CONF_STATE_CLASS,
    CONF_SORTING_GROUP_ID,  # Added
    CONF_SORTING_WEIGHT,    # Added
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_POWER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_WATT,
)

AUTO_LOAD = ["switch", "sensor", "select"]
DEPENDENCIES = ["uart"]

# Namespaces for the C++ classes
panasonic_ac_ns = cg.esphome_ns.namespace("panasonic_ac")
PanasonicAC = panasonic_ac_ns.class_("PanasonicAC", cg.Component, uart.UARTDevice, climate.Climate)
panasonic_ac_cnt_ns = panasonic_ac_ns.namespace("CNT")
PanasonicACCNT = panasonic_ac_cnt_ns.class_("PanasonicACCNT", PanasonicAC)
panasonic_ac_wlan_ns = panasonic_ac_ns.namespace("WLAN")
PanasonicACWLAN = panasonic_ac_wlan_ns.class_("PanasonicACWLAN", PanasonicAC)

# Custom components for switches and selects
PanasonicACSwitch = panasonic_ac_ns.class_("PanasonicACSwitch", switch.Switch, cg.Component)
PanasonicACSelect = panasonic_ac_ns.class_("PanasonicACSelect", select.Select, cg.Component)

# Configuration keys
CONF_HORIZONTAL_SWING_SELECT = "horizontal_swing_select"
CONF_VERTICAL_SWING_SELECT = "vertical_swing_select"
CONF_OUTSIDE_TEMPERATURE = "outside_temperature"
CONF_INSIDE_TEMPERATURE = "inside_temperature"
CONF_CURRENT_TEMPERATURE_SENSOR = "current_temperature_sensor"
CONF_NANOEX_SWITCH = "nanoex_switch"
CONF_ECO_SWITCH = "eco_switch"
CONF_ECONAVI_SWITCH = "econavi_switch"
CONF_MILD_DRY_SWITCH = "mild_dry_switch"
CONF_CURRENT_POWER_CONSUMPTION = "current_power_consumption"

# Define options for swing selects (assuming these are the supported options)
HORIZONTAL_SWING_OPTIONS = ["auto", "fixed"]
VERTICAL_SWING_OPTIONS = ["auto", "fixed"]

# Schema for the panasonic_ac climate platform
CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PanasonicAC),
        cv.Required(CONF_TYPE): cv.one_of("cnt", "wlan", lower=True),
        cv.Optional(CONF_NAME): cv.string,
        cv.Optional(CONF_ICON): cv.icon,
        cv.Optional(CONF_SORTING_GROUP_ID): cv.string,  # Added here
        cv.Optional(CONF_SORTING_WEIGHT): cv.int_,      # Added here
        cv.Optional("web_server"): cv.Schema({}), # Keep this as a dummy, or remove if not needed for the climate component directly

        cv.Optional(CONF_HORIZONTAL_SWING_SELECT): select.SELECT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(PanasonicACSelect),
                cv.Optional(CONF_NAME): cv.string,
                cv.Optional(CONF_ICON): cv.icon,
                cv.Optional(CONF_SORTING_GROUP_ID): cv.string,  # Added here
                cv.Optional(CONF_SORTING_WEIGHT): cv.int_,      # Added here
                cv.Optional("web_server"): cv.Schema({}),
            }
        ),
        cv.Optional(CONF_VERTICAL_SWING_SELECT): select.SELECT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(PanasonicACSelect),
                cv.Optional(CONF_NAME): cv.string,
                cv.Optional(CONF_ICON): cv.icon,
                cv.Optional(CONF_SORTING_GROUP_ID): cv.string,  # Added here
                cv.Optional(CONF_SORTING_WEIGHT): cv.int_,      # Added here
                cv.Optional("web_server"): cv.Schema({}),
            }
        ),
        cv.Optional(CONF_OUTSIDE_TEMPERATURE): sensor.sensor_schema().extend(
            {
                cv.Optional(CONF_NAME): cv.string,
                cv.Optional(CONF_ICON): cv.icon,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT): UNIT_CELSIUS,
                cv.Optional(CONF_DEVICE_CLASS): DEVICE_CLASS_TEMPERATURE,
                cv.Optional(CONF_STATE_CLASS): STATE_CLASS_MEASUREMENT,
                cv.Optional(CONF_SORTING_GROUP_ID): cv.string,  # Added here
                cv.Optional(CONF_SORTING_WEIGHT): cv.int_,      # Added here
                cv.Optional("web_server"): cv.Schema({}),
            }
        ),
        cv.Optional(CONF_INSIDE_TEMPERATURE): sensor.sensor_schema().extend(
            {
                cv.Optional(CONF_NAME): cv.string,
                cv.Optional(CONF_ICON): cv.icon,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT): UNIT_CELSIUS,
                cv.Optional(CONF_DEVICE_CLASS): DEVICE_CLASS_TEMPERATURE,
                cv.Optional(CONF_STATE_CLASS): STATE_CLASS_MEASUREMENT,
                cv.Optional(CONF_SORTING_GROUP_ID): cv.string,  # Added here
                cv.Optional(CONF_SORTING_WEIGHT): cv.int_,      # Added here
                cv.Optional("web_server"): cv.Schema({}),
            }
        ),
        cv.Optional(CONF_CURRENT_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_NANOEX_SWITCH): switch.SWITCH_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(PanasonicACSwitch),
                cv.Optional(CONF_NAME): cv.string,
                cv.Optional(CONF_ICON): cv.icon,
                cv.Optional(CONF_SORTING_GROUP_ID): cv.string,  # Added here
                cv.Optional(CONF_SORTING_WEIGHT): cv.int_,      # Added here
                cv.Optional("web_server"): cv.Schema({}),
            }
        ),
        cv.Optional(CONF_ECO_SWITCH): switch.SWITCH_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(PanasonicACSwitch),
                cv.Optional(CONF_NAME): cv.string,
                cv.Optional(CONF_ICON): cv.icon,
                cv.Optional(CONF_SORTING_GROUP_ID): cv.string,  # Added here
                cv.Optional(CONF_SORTING_WEIGHT): cv.int_,      # Added here
                cv.Optional("web_server"): cv.Schema({}),
            }
        ),
        cv.Optional(CONF_ECONAVI_SWITCH): switch.SWITCH_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(PanasonicACSwitch),
                cv.Optional(CONF_NAME): cv.string,
                cv.Optional(CONF_ICON): cv.icon,
                cv.Optional(CONF_SORTING_GROUP_ID): cv.string,  # Added here
                cv.Optional(CONF_SORTING_WEIGHT): cv.int_,      # Added here
                cv.Optional("web_server"): cv.Schema({}),
            }
        ),
        cv.Optional(CONF_MILD_DRY_SWITCH): switch.SWITCH_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(PanasonicACSwitch),
                cv.Optional(CONF_NAME): cv.string,
                cv.Optional(CONF_ICON): cv.icon,
                cv.Optional(CONF_SORTING_GROUP_ID): cv.string,  # Added here
                cv.Optional(CONF_SORTING_WEIGHT): cv.int_,      # Added here
                cv.Optional("web_server"): cv.Schema({}),
            }
        ),
        cv.Optional(CONF_CURRENT_POWER_CONSUMPTION): sensor.sensor_schema().extend(
            {
                cv.GenerateID(): cv.declare_id(sensor.Sensor),
                cv.Optional(CONF_NAME): cv.string,
                cv.Optional(CONF_ICON): cv.icon,
                cv.Optional(CONF_UNIT_OF_MEASUREMENT): UNIT_WATT,
                cv.Optional(CONF_DEVICE_CLASS): DEVICE_CLASS_POWER,
                cv.Optional(CONF_STATE_CLASS): STATE_CLASS_MEASUREMENT,
                cv.Optional(CONF_SORTING_GROUP_ID): cv.string,  # Added here
                cv.Optional(CONF_SORTING_WEIGHT): cv.int_,      # Added here
                cv.Optional("web_server"): cv.Schema({}),
            }
        ),
    }
).extend(uart.UART_DEVICE_SCHEMA)

async def to_code(config):
    if config[CONF_TYPE] == "cnt":
        var = cg.new_Pvariable(config[CONF_ID], PanasonicACCNT)
    else:
        var = cg.new_Pvariable(config[CONF_ID], PanasonicACWLAN)

    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await uart.register_uart_device(var, config)

    if CONF_HORIZONTAL_SWING_SELECT in config:
        conf = config[CONF_HORIZONTAL_SWING_SELECT]
        swing_select = await select.new_select(conf, options=HORIZONTAL_SWING_OPTIONS)
        await cg.register_component(swing_select, conf)
        cg.add(var.set_horizontal_swing_select(swing_select))

    if CONF_VERTICAL_SWING_SELECT in config:
        conf = config[CONF_VERTICAL_SWING_SELECT]
        swing_select = await select.new_select(conf, options=VERTICAL_SWING_OPTIONS)
        await cg.register_component(swing_select, conf)
        cg.add(var.set_vertical_swing_select(swing_select))

    if CONF_OUTSIDE_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_OUTSIDE_TEMPERATURE])
        cg.add(var.set_outside_temperature_sensor(sens))

    if CONF_INSIDE_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_INSIDE_TEMPERATURE])
        cg.add(var.set_inside_temperature_sensor(sens))

    if CONF_CURRENT_TEMPERATURE_SENSOR in config:
        sens = await cg.get_variable(config[CONF_CURRENT_TEMPERATURE_SENSOR])
        cg.add(var.set_current_temperature_sensor(sens))

    for s_key in [CONF_ECO_SWITCH, CONF_NANOEX_SWITCH, CONF_MILD_DRY_SWITCH, CONF_ECONAVI_SWITCH]:
        if s_key in config:
            conf = config[s_key]
            a_switch = await switch.new_switch(conf)
            await cg.register_component(a_switch, conf)
            cg.add(getattr(var, f"set_{s_key}")(a_switch))

    if CONF_CURRENT_POWER_CONSUMPTION in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT_POWER_CONSUMPTION])
        cg.add(var.set_current_power_consumption_sensor(sens))
