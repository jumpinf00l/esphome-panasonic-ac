# <a name="its-a-fork">It's a fork</a>:
This is jumpinf00l's super-dodgy hack to add a few nicer sensors and selects, it's only really public so that my ESPHome can reach it to allow for ease of change tracking, but I guess you can use it. No, I don't intend to do a PR because, as previously mentioned, these are super-dodgy hacks and intended only for jumpinf00l. This is specifically to support features of Panasonic CS-Z25XKRW and CS-Z50XKRW, but these changes should be pretty universal

## <a name="key--changes">Key changes</a>:
<b><a name="0.2.x-fix_deprecated_schema">0.2.x - Fix Deprecated Schema Warnings</a></b>
 - Fixed deprecated schema warnings when compiling
   - Shamelessly copied from PRs on the main repo

<b><a name="0.1.x-fan_modes">0.1.x - Change from fan speed to Home Assistant style fan mode</a></b>
 - Fan speed is now a Home Assistant style fan mode
   - This allows the thermostat entity in Apple Home to show fan speeds. See [Fan Mode](#fan-mode) for mapping and notes
   - This has no effect on Google Home. See [Fan Mode Notes](#fan-mode-notes) for notes on this
 - Added Select entities for setting Mode, Fan mode, and Preset separate to the Climate entity
   - These are useful for building custom cards or changing the fan mode via the ESPHome device's web UI
   - These are currently achieved through super-dodgy invervals which run every 1 second. Awful for performance, awful for logging, awful for WiFi/API, but they work for now. See [Wishlist](#wishlist) for a potential fix for this
 - Added Daily Energy as a measurement of power usage over the day. Not strictly part of this custom component and more for documentation alongside, but neat and adds functionality for use with the Home Assistant Energy Dashboard
 - Added inside temperature as an entity rather than relying on the attribute of the climate entity
   - More consisitent for Home Assistant dashboard design
   - The inside temperature attribute remains on the climate entity, so can be referenced either way
 - Added entities for filter maintenance
   - Again, not necessarily part of the custom component, but a neat addition which you can use to trigger an automation to remind you to clean the filter at least a few times in its life
   - Entities:
     - Sensor: Total Runtime (since the ESPHome device started tracking it of course, this isn't a value provided by the AC unit itself to my knowledge unfortunately)
     - Sensor: Filter remaining hours
       - Set the 'initial_filter_remaining_hours' substitution to something useful to get a time remaining indication of the filter in hours
     - Sensor: Filter remaining percent
       - Set the 'initial_filter_remaining_hours' substitution to something useful to get a percentage remaining indication of the filter in percent
     - Reset button to mark the filter as cleaned and return it to 100%
       - Also press this if changing the 'initial_filter_remaining_hours' substitution

## <a name="fan-mode">Fan Mode</a>:
| Home Assistant / ESPHome Fan Mode  | Panasonic AC Speed | Apple Home                               | Google Home |
| ---------------------------------- | ------------------ | ---------------------------------------- | ----------- |
| Auto                               | Auto               | N/A                                      | N/A         |
| Diffuse                            | 1                  | N/A                                      | N/A         |
| Low                                | 2                  | 33.3% (bottom third of bar including 0%) | N/A         |
| Medium                             | 3                  | 66.6% (middle third of bar)              | N/A         |
| High                               | 4                  | 100% (top third of bar                   | N/A         |
| Focus                              | 5                  | N/A                                      | N/A         |

### <a name="fan-mode-notes">Fan Mode Notes</a>: 
 - Apple Home will only show three speeds on the thermostat entity: Low, Medium, and High which are speeds 2, 3, and 4
   - I've had some dodgy experiences with setting these fan speeds in Apple Home where subsequent changes to the Home Assistant climate entity fan no longer change on the Panasonic AC unit, so YMMV
 - Google Home will not show fan speeds on the thermostat entity at all
    - There is a clear reason for this and is unfortunately the trade-off between two 'exceptionally Google' style issues. Home Assistant exposes its climate entities as Google Home thermostat devices rather than a Google Home ac_unit devices because:
      - Google Home ac_unit devices do not support heat mode
      - Google Home thermostat devices do not support fan modes
    - See [Wishlist](#wishlist) for a potential workaround through adding a fan entity to the ESPHome device which can be exposed to Apple Home and Google Home as a fan device

## <a name="wishlist">Wishlist</a>:
There is no ETA on any item on this list, nor an guarantee that these items will ever be available
 - ~Fix deprecation notices when compiling~ ✅ [0.2.x - Fix Deprecated Schema Warnings](#0.2.x-fix_deprecated_schema)
 - Add activity attribute to the climate component
 - Set Presets as a Home Assistant style options, similar to work completed on custom fan speed to fan mode change
   - Align presets with the Panasonic IR remote:
     - Set Eco as a preset rather than a switch
     - Set Quiet as a fan speed rather than a preset
 - Incorporate fan mode and mode Select components as options under the Climate component
   - This will neaten the code up, neaten the logs up, reduce Wi-Fi/API activity, and improve performance
   - Investigate an option for adding custom names to the fan modes in the Select component in the case that the user doesn't want "1 - Diffuse" and for example wants "Diffuse" or "1" instead
 - Add iAuto-X as a preset, switch, or button
 - Rename horizontal swing/vane modes to align front-end naming conventions
   - Low importance because my Panasonic AC units don't support horizontal swing/vane adjustment (they're manual/povo-spec adjustment louvres)

## <a name="general-notes">General Notes</a>:
 - This has only been tested using CNT mode. WLAN mode is untested but should work similarly
 - This is a zero-ecosystem household; there is no love for or preference over Apple Home or Google Home, nor the products of either of these companies

## <a name="example">Example</a>:
Here's a working cut-down example (add your own esphome, wifi, etc sections):

```
substitutions:
  initial_filter_remaining_hours: "500.0" # hours until filter needs cleaning

esp32:
  board: esp32-c3-devkitm-1
  framework:
    type: arduino

external_components:
  - source: github://jumpinf00l/esphome-panasonic-ac
    components: [panasonic_ac]
    refresh: 120min # or whatever

uart:
  tx_pin: GPIO7
  rx_pin: GPIO6
  id: ac_uart
  baud_rate: 9600
  parity: EVEN

globals:
  - id: glob_runtime_minutes
    type: float
    restore_value: yes
    initial_value: "0.0"
  - id: glob_filter_remaining_hours
    type: float
    restore_value: yes
    initial_value: "${initial_filter_remaining_hours}"
  - id: glob_filter_remaining_percent
    type: float
    restore_value: yes
    initial_value: "100.0"

climate:
  - platform: panasonic_ac
    id: panasonic_ac_id
    name: "Climate"
    icon: mdi:thermostat
    type: cnt
    vertical_swing_select:
      name: "Vertical Swing Mode"
      icon: mdi:arrow-up-down
    outside_temperature:
      name: "Outside Temperature"
      icon: mdi:thermometer
    inside_temperature:
      name: "Inside Temperature"
      icon: mdi:thermometer
    nanoex_switch:
      name: "NanoeX"
      icon: mdi:shimmer
    eco_switch:
      name: "Eco"
      icon: mdi:leaf
    current_power_consumption:
      name: "Current Power"
      id: power
      icon: mdi:flash

sensor:
  - platform: uptime
    name: "Uptime"
  - platform: wifi_signal
    name: "WiFi Signal"
    icon: mdi:signal
    update_interval: 120s  
  - platform: internal_temperature
    name: "Controller Temperature"
    icon: mdi:thermometer
  - platform: total_daily_energy
    name: "Daily Energy"
    icon: mdi:lightning-bolt
    power_id: power
    id: energy
    unit_of_measurement: 'kWh'
    state_class: total_increasing
    device_class: energy
    accuracy_decimals: 3
    filters:
      # Multiplication factor from W to kW is 0.001
      - multiply: 0.001

button:
  - platform: template
    name: "Filter Remaining Reset"
    icon: mdi:air-filter
    id: filter_remaining_reset
    web_server:
      sorting_group_id: maintenance_group
      sorting_weight: 40
    on_press:
      then:
        - lambda: |-
            id(glob_filter_remaining_hours) = ${initial_filter_remaining_hours};
            id(glob_filter_remaining_percent) = 100.0;
            id(filter_remaining_hours).publish_state(id(glob_filter_remaining_hours));
            id(filter_remaining_percent).publish_state(id(glob_filter_remaining_percent));

select:
  - platform: template
    name: "Fan Mode"
    icon: mdi:fan
    id: fan_mode_select
    web_server:
      sorting_group_id: climate_group
      sorting_weight: 40
    options:
      - "Auto"
      - "1 - Diffuse"
      - "2 - Low"
      - "3 - Medium"
      - "4 - High"
      - "5 - Focus"
    initial_option: "Auto"
    optimistic: true
    set_action:
      - lambda: |-
          auto call = id(panasonic_ac_id).make_call();
          if (x == "Auto") {
            call.set_fan_mode(climate::CLIMATE_FAN_AUTO);
          } else if (x == "1 - Diffuse") {
            call.set_fan_mode(climate::CLIMATE_FAN_DIFFUSE);
          } else if (x == "2 - Low") {
            call.set_fan_mode(climate::CLIMATE_FAN_LOW);
          } else if (x == "3 - Medium") {
            call.set_fan_mode(climate::CLIMATE_FAN_MEDIUM);
          } else if (x == "4 - High") {
            call.set_fan_mode(climate::CLIMATE_FAN_HIGH);
          } else if (x == "5 - Focus") {
            call.set_fan_mode(climate::CLIMATE_FAN_FOCUS);
          }
          call.perform();
  - platform: template
    name: "Mode"
    icon: mdi:order-alphabetical-ascending
    id: mode_select
    options:
      - Heat/Cool
      - Heat
      - Cool
      - Dry
      - Fan Only
      - "Off"
    initial_option: "Off"
    optimistic: true
    set_action:
      - lambda: |-
          auto call = id(panasonic_ac_id).make_call();
          if (x == "Heat/Cool") {
            call.set_mode(climate::CLIMATE_MODE_HEAT_COOL);
          } else if (x == "Heat") {
            call.set_mode(climate::CLIMATE_MODE_HEAT);
          } else if (x == "Cool") {
            call.set_mode(climate::CLIMATE_MODE_COOL);
          } else if (x == "Dry") {
            call.set_mode(climate::CLIMATE_MODE_DRY);
          } else if (x == "Fan Only") {
            call.set_mode(climate::CLIMATE_MODE_FAN_ONLY);
          } else if (x == "Off") {
            call.set_mode(climate::CLIMATE_MODE_OFF);
          }
          call.perform();
  - platform: template
    name: "Preset Mode"
    icon: mdi:format-list-bulleted
    id: preset_mode_select
    options:
      - "Normal"
      - "Powerful"
      - "Quiet"
    initial_option: "Normal"
    optimistic: true
    set_action:
      - lambda: |-
          auto call = id(panasonic_ac_id).make_call();
          if (x == "Normal") {
            call.set_preset("Normal");
          } else if (x == "Powerful") {
            call.set_preset("Powerful");
          } else if (x == "Quiet") {
            call.set_preset("Quiet");
          }
          call.perform();

interval:
  - interval: 1s
    then:
      - lambda: |-
          auto current_fan_mode = id(panasonic_ac_id);
          // This is a workaround: Check if custom_fan_mode has a value
          if (current_fan_mode->custom_fan_mode.has_value()) {
              std::string fan_mode = current_fan_mode->custom_fan_mode.value();  // Extract string from the optional
              if (fan_mode == "1") {
                  id(fan_mode_select).publish_state("1");
              } else if (fan_mode == "2") {
                  id(fan_mode_select).publish_state("2");
              } else if (fan_mode == "3") {
                  id(fan_mode_select).publish_state("3");
              } else if (fan_mode == "4") {
                  id(fan_mode_select).publish_state("4");
              } else if (fan_mode == "5") {
                  id(fan_mode_select).publish_state("5");
              } else if (fan_mode == "Automatic") {
                  id(fan_mode_select).publish_state("Automatic");
              }
          }
  - interval: 1s
    then:
      - lambda: |-
          auto current_mode = id(panasonic_ac_id).mode;
          if (current_mode == climate::CLIMATE_MODE_HEAT_COOL) {
            id(mode_select).publish_state("Heat/Cool");
          } else if (current_mode == climate::CLIMATE_MODE_HEAT) {
            id(mode_select).publish_state("Heat");
          } else if (current_mode == climate::CLIMATE_MODE_COOL) {
            id(mode_select).publish_state("Cool");
          } else if (current_mode == climate::CLIMATE_MODE_DRY) {
            id(mode_select).publish_state("Dry");
          } else if (current_mode == climate::CLIMATE_MODE_FAN_ONLY) {
            id(mode_select).publish_state("Fan Only");
          } else if (current_mode == climate::CLIMATE_MODE_OFF) {
            id(mode_select).publish_state("Off");
          }
  - interval: 1s
    then:
      - lambda: |-
          auto current_climate = id(panasonic_ac_id);
          // This is a workaround: Check if custom_preset has a value
          if (current_climate->custom_preset.has_value()) {
              std::string preset_mode = current_climate->custom_preset.value();  // Extract string from the optional
              if (preset_mode == "Normal") {
                  id(preset_mode_select).publish_state("Normal");
              } else if (preset_mode == "Powerful") {
                  id(preset_mode_select).publish_state("Powerful");
              } else if (preset_mode == "Quiet") {
                  id(preset_mode_select).publish_state("Quiet");
              }
          }
  - interval: 60s
    then:
      - if:
          condition:
            lambda: |-
              return id(panasonic_ac_id).mode != CLIMATE_MODE_OFF;
          then:
            - lambda: |-
                id(glob_runtime_minutes) += 1.0f;
                id(glob_filter_remaining_hours) = std::max(0.0f, id(glob_filter_remaining_hours) - 1.0f / 60.0f);
                id(glob_filter_remaining_percent) = (id(glob_filter_remaining_hours) / ${initial_filter_remaining_hours}) * 100.0;
                id(filter_remaining_hours).publish_state(id(glob_filter_remaining_hours));
                id(filter_remaining_percent).publish_state(id(glob_filter_remaining_percent));
```

---

# Overview

An open source alternative for the Panasonic wi-fi adapter that works locally without the cloud.

# Features

* Control your AC locally via Home Assistant, MQTT or directly
* Instantly control the AC without any delay like in the Comfort Cloud app
* Receive live reports and state from the AC
* Uses the UART interface on the AC instead of the IR interface
* Provides a drop-in replacement for the Panasonic DNSK-P11 and the CZ-TACG1 wifi module

# Supported hardware

This library works with both the CN-CNT port and the CN-WLAN port. CN-WLAN is only available on newer units. Either port can be used on units that have both ports regardless of the usage of the other port (ie. it is possible to leave the DNSK-P11 connected to CN-WLAN and connect the ESP to CN-CNT). 

Works on the ESP8266 but ESP32 is preferred for the multiple hardware serial ports.

# Requirements

* ESP32 (or ESP8266) ([supported by ESPHome](https://esphome.io/#devices))
* 5V to 3.3V bi-directional Logic Converter (minimum 2 channels, available as pre-soldered prototyping boards)
* Female-Female Jumper cables
* Soldering iron
* Wires to solder from Logic converter to ESP
* Heat shrink
* ESPHome 2022.5.0 or newer
* Home Assistant 2021.8.0 or newer

# Notes

* **Make sure to disconnect mains power before opening your AC, the mains contacts are exposed and can be touched by accident!**
* **Do not connect your ESP32/ESP8266 directly to the AC, the AC uses 5V while the ESPs use 3.3V!**
* **While installation is fairly straightforward I do not take any responsibility for any damage done to you or your AC during installation**
* The DNSK-P11 and the CZ-TACG1 use different types of connectors, make sure to connect to the correct one

# Software installation

This software installation guide assumes some familiarity with ESPHome.

* Pull this repository or copy the `ac.yaml.example` from the root folder
* Rename the `ac.yaml.example` to `ac.yaml`
* Uncomment the `type` field depending on which AC protocol you want to use
* Adjust the `ac.yaml` to your needs
* Connect your ESP
* Run `esphome ac.yaml run` and choose your serial port (or do this via the Home Assistant UI)
* If you see the handshake messages being sent (DNSK-P11) or polling requests being sent (CZ-TACG1) in the log you are good to go
* Disconnect the ESP and continue with hardware installation

## Setting supported features

Since Panasonic ACs support different features you can comment out the lines at the bottom of your `ac.yaml`:

```
  # Enable as needed
  # eco_switch:
  #   name: Panasonic AC Eco Switch
  # nanoex_switch:
  #   name: Panasonic AC NanoeX Switch
  # mild_dry_switch:
  #   name: Panasonic AC Mild Dry Switch
  # econavi_switch:
  #   name: Econavi switch
  # current_power_consumption:
  #   name: Panasonic AC Power Consumption
```

In order to find out which features are supported by your AC, check the remote that came with it. Please note that eco switch and mild dry switch are not supported on DNSK-P11.

**Enabling unsupported features can lead to undefined behavior and may damage your AC. Make sure to check your remote or manual first.**
**current_power_consumption is just as ESTIMATED value by the AC**

## Upgrading from 1.x to 2.x

[Upgrade instructions](README.UPGRADING.md)

# Hardware installation

[Hardware installation for DNSK-P11](README.DNSKP11.md)

[Hardware installation for CZ-TACG1](README.CZTACG1.md)
