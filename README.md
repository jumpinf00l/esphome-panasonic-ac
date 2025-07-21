# <a name="its-a-fork">It's a Fork</a>
This is jumpinf00l's super-dodgy hack to add a few nicer sensors and selects, it's only really public so that my ESPHome can reach it to allow for ease of change tracking, but I guess you can use it. No, I don't intend to do a PR because, as previously mentioned, these are super-dodgy hacks and intended only for jumpinf00l. This is specifically to support features of Panasonic CS-Z25XKRW and CS-Z50XKRW, but these changes should be pretty universal

# <a name="key-changes">Key changes</a>
 - Preset is now Home Assistant style preset and contains the same options as the IR remote
   - Quiet is now a fan mode
   - Eco is now a preset
     - The optional entity under the climate component remains if you wish to use it
   - See [Presets](#presets) for mapping
 - Fan speed is now a Home Assistant style fan mode
   - This allows the thermostat entity in Apple Home to set Low, Medium, and High fan speeds
   - This has no effect on Google Home
     - See [Fan Mode Notes](#fan-mode-notes) for notes on this
   - See [Fan Mode](#fan-mode) for mapping
 - Added inside temperature as an optional sub-entity under the climate entity rather than relying on the attribute of the climate entity
   - More consistent for Home Assistant dashboard design
   - The inside temperature attribute remains on the climate entity, so can be referenced either way

## <a name="presets">Presets</a>
| Home Assistant / ESPHome Preset | Panasonic Preset |
| ------------------------------- | ---------------- |
| None                            | [Blank]          |
| Boost                           | Powerful         |
| Eco                             | Eco              |

### <a name="preset-notes">Preset Notes</a>
 - Changing the preset to any value will change the fan mode to Auto, the same as changing the preset from the IR remote

## <a name="fan-mode">Fan Mode</a>
| Home Assistant / ESPHome Fan Mode  | Panasonic AC Speed | Apple Home                               | Google Home |
| ---------------------------------- | ------------------ | ---------------------------------------- | ----------- |
| Auto                               | Auto               | N/A                                      | N/A         |
| Quiet                              | Quiet              | N/A                                      | N/A         |
| Diffuse                            | 1                  | N/A                                      | N/A         |
| Low                                | 2                  | 33.3% (bottom third of bar including 0%) | N/A         |
| Medium                             | 3                  | 66.6% (middle third of bar)              | N/A         |
| High                               | 4                  | 100% (top third of bar                   | N/A         |
| Focus                              | 5                  | N/A                                      | N/A         |

### <a name="fan-mode-notes">Fan Mode Notes</a>
 - Apple Home will only show three speeds on the thermostat device: Low, Medium, and High which are speeds 2, 3, and 4
    - The experience with setting fan speeds in both Apple Home and Home Assistant changes slightly: Apple Home appears to enforce its last setting when changing via Home Assistant which might not update on the Home Assistant front-end or change on the Panasonic AC
    - Closing the Apple Home app after usage appears to work around this
 - Google Home will not show fan speeds on the thermostat device at all
    - There is a clear reason for this and is unfortunately the trade-off between two 'exceptionally Google' style issues. Home Assistant exposes its climate entities as Google Home thermostat devices rather than a Google Home ac_unit devices because the lack of heat mode in a Google Home ac_unit device is more impactful than the lack of fan speed in a Google Home thermostat device
      - Google Home ac_unit devices do not support heat mode
      - Google Home thermostat devices do not support fan speed
    - See the [Wishlist](#wishlist) for a potential workaround through adding a fan entity to the ESPHome device which can be exposed to Apple Home and Google Home as a fan device

# <a name="general-notes">General Notes</a>
 - This has only been tested using CNT mode. WLAN mode is currently untested and mostly unchanged but should work similarly
 - This is a zero-ecosystem household; there is no love for or preference over Apple Home or Google Home, nor the products of either of these companies
 - Jumpinf00l has zero intention to develop or maintain this code beyond the wishlist
   - If something breaks, it might not be fixed
   - Submit bugs and PRs to the original repo that this was forked from
   - Please be warned and re-read [It's a Fork](#its-a-fork)
 - AI technologies have been used in the customisation of this code
   - *Don't Panic*. There are no AI connectors, internal AI process, or any other such nonsense in this code
   - This code has been passed through OpenAI GPT-4o and Google Gemini 2.5 Flash mostly for sanity-checking code as I push the limits of my knowledge
   - All returned code has been checked and confirmed safe by a real, squishy, living, breathing human

# <a name="example">Example</a>
<details>
<summary>Here's a working cut-down example (add your own esphome, wifi, etc sections)</summary>

```
...

external_components:
  - source: github://jumpinf00l/esphome-panasonic-ac
    components: [panasonic_ac]

uart:
  tx_pin: GPIO4
  rx_pin: GPIO5
  id: ac_uart
  baud_rate: 9600
  parity: EVEN

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

...
```
</details>

# <a name="neat-tweaks">Neat tweaks</a>
Below are some neat tweaks inside the ESPHome YAML which you can use to extend the features beyond this custom component. These are not part of the custom component and are entirely optional, but included here because they may be useful. See the [Neat Tweaks Examples](#neat-tweaks-examples) for the YAML which you can customise as needed

## <a name="net-tweaks-notes">Neat Tweaks Notes</a>
 - These neat tweaks assume that the entity IDs from [Example](#example) are used

## <a name="neat-tweaks-examples">Neat Tweaks Examples</a>
<details>
<summary>Select entities</summary>

 - Select entities for setting Mode, Fan mode, and Preset separate to the Climate entity
   - These are useful for building custom cards or changing the fan mode via the ESPHome device's web UI
   - These are currently achieved through super-dodgy invervals which run every 1 second. Awful for performance, awful for logging, awful for WiFi/API, but they work for now. See the [Wishlist](#wishlist) for a potential fix for this which will move them into the custom component as optional entries under the climate component

```
select:
  - platform: template
    name: "Fan Mode"
    icon: mdi:fan
    id: fan_mode_select
    options:
      - "Auto"
      - "Quiet"
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
          } else if (x == "Quiet") {
            call.set_fan_mode(climate::CLIMATE_FAN_QUIET);
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
    name: "Preset"
    icon: mdi:format-list-bulleted
    id: preset_select
    options:
      - "None"
      - "Boost"
      - "Eco"
    initial_option: "None"
    optimistic: true
    set_action:
      - lambda: |-
          auto call = id(panasonic_ac_id).make_call();
          if (x == "None") {
            call.set_preset(climate::CLIMATE_PRESET_NONE);
          } else if (x == "Boost") {
            call.set_preset(climate::CLIMATE_PRESET_BOOST);
          } else if (x == "Eco") {
            call.set_preset(climate::CLIMATE_PRESET_ECO);
          }
          call.perform();

interval:
  - interval: 1s
    then:
      - lambda: |-
          auto current_fan_mode = id(panasonic_ac_id).fan_mode;
          if (current_fan_mode == climate::CLIMATE_FAN_AUTO) {
              id(fan_mode_select).publish_state("Auto");
          } else if (current_fan_mode == climate::CLIMATE_FAN_QUIET) {
              id(fan_mode_select).publish_state("Quiet");
          } else if (current_fan_mode == climate::CLIMATE_FAN_DIFFUSE) {
              id(fan_mode_select).publish_state("1 - Diffuse");
          } else if (current_fan_mode == climate::CLIMATE_FAN_LOW) {
              id(fan_mode_select).publish_state("2 - Low");
          } else if (current_fan_mode == climate::CLIMATE_FAN_MEDIUM) {
              id(fan_mode_select).publish_state("3 - Medium");
          } else if (current_fan_mode == climate::CLIMATE_FAN_HIGH) {
              id(fan_mode_select).publish_state("4 - High");
          } else if (current_fan_mode == climate::CLIMATE_FAN_FOCUS) {
              id(fan_mode_select).publish_state("5 - Focus");
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
          auto current_preset = id(panasonic_ac_id).preset;
          if (current_preset == climate::CLIMATE_PRESET_NONE) {
            id(preset_select).publish_state("None");
          } else if (current_preset == climate::CLIMATE_PRESET_BOOST) {
            id(preset_select).publish_state("Boost");
          } else if (current_preset == climate::CLIMATE_PRESET_ECO) {
            id(preset_select).publish_state("Eco");
          }
```
</details>

<details>
<summary>Daily energy sensor</summary>

 - Daily Energy sensor as a measurement of power usage over the day. Adds functionality for use with the Home Assistant Energy Dashboard

```
sensor:
  - platform: total_daily_energy
    name: "Daily Energy"
    icon: mdi:lightning-bolt
    power_id: power
    id: energy # The ID of the 'current_power_consumption' entity in the climate component
    unit_of_measurement: 'kWh'
    state_class: total_increasing
    device_class: energy
    accuracy_decimals: 3
    filters:
      - multiply: 0.001 # Multiplication factor from W to kW is 0.001
```
</details>

<details>
<summary>Filter maintenance entities</summary>

 - Entities and button that you can use to trigger an automation to remind you to clean the filter at least a few times in its life
 - Entities:
   - Sensor: Total Runtime
     - Reports runtimesince the ESPHome device started tracking it - this isn't a value provided by the AC unit itself as far as known
     - This sensor will reset to 0 if the entity ID is changed
   - Sensor: Filter remaining hours
     - Set the 'initial_filter_remaining_hours' substitution to something useful to get a time remaining indication of the filter in hours
   - Sensor: Filter remaining percent
     - Set the 'initial_filter_remaining_hours' substitution to something useful to get a percentage remaining indication of the filter in percent
   - Reset button to mark the filter as cleaned and return it to 100%
     - Also press this if changing the 'initial_filter_remaining_hours' substitution

```
substitutions:
  initial_filter_remaining_hours: "500.0" # hours until filter needs cleaning. Remember to press "Filter Remaining Reset" after changing

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

button:
  - platform: template
    name: "Filter Remaining Reset"
    icon: mdi:air-filter
    id: filter_remaining_reset
    on_press:
      then:
        - lambda: |-
            id(glob_filter_remaining_hours) = ${initial_filter_remaining_hours};
            id(glob_filter_remaining_percent) = 100.0;
            id(filter_remaining_hours).publish_state(id(glob_filter_remaining_hours));
            id(filter_remaining_percent).publish_state(id(glob_filter_remaining_percent));

sensor:
  - platform: template
    name: "Runtime Hours"
    icon: mdi:timer-outline
    id: runtime_hours
    unit_of_measurement: "h"
    accuracy_decimals: 1
    update_interval: 60s
    lambda: |-
      return id(glob_runtime_minutes) / 60.0;
  - platform: template
    name: "Filter Remaining Hours"
    icon: mdi:timer-outline
    id: filter_remaining_hours
    unit_of_measurement: "h"
    accuracy_decimals: 1
    update_interval: never
    lambda: |-
      return id(glob_filter_remaining_hours);
  - platform: template
    name: "Filter Remaining Percent"
    icon: mdi:air-filter
    id: filter_remaining_percent
    unit_of_measurement: "%"
    accuracy_decimals: 2
    update_interval: never
    lambda: |-
      return id(glob_filter_remaining_percent);

interval:
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
</details>

# <a name="wishlist">Wishlist</a>
There is no ETA on any item on this list, nor an guarantee that these items will ever be available
 - ~Fix deprecation notices when compiling~ ✅ v0.2.x - Fix Deprecated Schema Warnings
 - ~Set Presets as a Home Assistant style options, similar to work completed on custom fan speed to fan mode change~ ✅ v0.3.x - Presets
   - ~Align presets with the Panasonic IR remote~
     - ~Set Eco as a preset rather than a switch~
     - ~Set Quiet as a fan speed rather than a preset~
 - Poll AC unit for update on each front-end change (fan speed, preset, etc), rather than wait for default 5 sec poll
 - Incorporate Fan mode, Preset, and Mode select entities and Daily energy sensor as sub-entities under the Climate entity
   - This will neaten the code up, neaten the logs up, reduce Wi-Fi/API activity, and improve performance
   - Investigate an option for adding custom names to the fan speed sub-entity in the case that the user wants to customise these
 - Add activity attribute to the climate entity
   - This will indicate the current activity on the AC unit (such as idle once the setpoint temperature has been reached and the compressor slows down)
 - Rename horizontal swing/vane modes to align front-end naming conventions, make horizontal and vertical swing modes optional under the climate entity, and add swing modes back to climate entity
   - This won't support setting fixed swing positions on the climate entity since ESPHome can only toggle horizontal and/or vertical swing on and off
 - Add a fan entity which can be exposed to Apple Home and Google Home to allow controlling of all fan modes
   - Investigate presenting this as a percentage for better Apple Home and Google Home support, or research supported options for each platform
     - 0 - 14.28% = auto <> 28.57% = quiet <> 42.85% = diffuse <> 57.14% = low <> 71.42% = medium <> 85.71% = high <> 100% = focus
 - Remove/hide unsupportable options in fan_only mode from Home Assistant
   - Example: Fan_only doesn't support Eco preset or temperature setpoint
   - This will be tricky with the climate sub-entities
   - This is lowest priority since nothing breaks if setting an unsupported option in fan_only mode, it's reverted on the next poll
 - Replicate all changes to WLAN mode

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
