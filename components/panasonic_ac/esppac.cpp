#include "esppac.h"

#include "esphome/core/log.h"

namespace esphome {
namespace panasonic_ac {

static const char *const TAG = "panasonic_ac";

climate::ClimateTraits PanasonicAC::traits() {
  auto traits = climate::ClimateTraits();

  traits.set_supports_action(false);
  traits.set_supports_current_temperature(true);
  traits.set_supports_two_point_target_temperature(false);
  traits.set_visual_min_temperature(MIN_TEMPERATURE);
  traits.set_visual_max_temperature(MAX_TEMPERATURE);
  traits.set_visual_temperature_step(TEMPERATURE_STEP);
  traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_FAN_ONLY, climate::CLIMATE_MODE_DRY});
  traits.set_supported_fan_modes({climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_QUIET, climate::CLIMATE_FAN_DIFFUSE, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_FOCUS});
  traits.set_supported_presets({climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_BOOST, climate::CLIMATE_PRESET_ECO});

  // Dynamically set supported swing modes based on enable flags
  std::set<climate::ClimateSwingMode> supported_swing_modes;
  if (this->vertical_swing_enable_ || this->horizontal_swing_enable_) {
      supported_swing_modes.insert(climate::CLIMATE_SWING_OFF);
  }
  if (this->vertical_swing_enable_) {
      supported_swing_modes.insert(climate::CLIMATE_SWING_VERTICAL);
  }
  if (this->horizontal_swing_enable_) {
      supported_swing_modes.insert(climate::CLIMATE_SWING_HORIZONTAL);
  }
  if (this->vertical_swing_enable_ && this->horizontal_swing_enable_) {
      supported_swing_modes.insert(climate::CLIMATE_SWING_BOTH);
  }
  traits.set_supported_swing_modes(supported_swing_modes);
  
  return traits;
}

void PanasonicAC::setup() {
  // Initialize times
  this->init_time_ = millis();
  this->last_packet_sent_ = millis();

  ESP_LOGI(TAG, "Panasonic AC component v%s starting...", VERSION);
}

void PanasonicAC::loop() {
  read_data();  // Read data from UART (if there is any)
}

void PanasonicAC::read_data() {
  while (available())  // Read while data is available
  {
    // if (this->receive_buffer_index >= BUFFER_SIZE) {
    //   ESP_LOGE(TAG, "Receive buffer overflow");
    //   receiveBufferIndex = 0;
    // }

    uint8_t c;
    this->read_byte(&c);  // Store in receive buffer
    this->rx_buffer_.push_back(c);

    this->last_read_ = millis();  // Update lastRead timestamp
  }
}

void PanasonicAC::update_outside_temperature(int8_t temperature) {
  if (temperature > TEMPERATURE_THRESHOLD) {
    ESP_LOGW(TAG, "Received out of range outside temperature: %d", temperature);
    return;
  }

  if (this->outside_temperature_sensor_ != nullptr && this->outside_temperature_sensor_->state != temperature)
    this->outside_temperature_sensor_->publish_state(
        temperature);  // Set current (outside) temperature; no temperature steps
}

void PanasonicAC::update_inside_temperature(int8_t temperature) {
  if (temperature > TEMPERATURE_THRESHOLD) {
    ESP_LOGW(TAG, "Received out of range inside temperature: %d", temperature);
    return;
  }

  if (this->inside_temperature_sensor_ != nullptr && this->inside_temperature_sensor_->state != temperature)
    this->inside_temperature_sensor_->publish_state(
        temperature);  // Set current (inside) temperature; no temperature steps
}

void PanasonicAC::update_current_temperature(int8_t temperature) {
  if (temperature > TEMPERATURE_THRESHOLD) {
    ESP_LOGW(TAG, "Received out of range inside temperature: %d", temperature);
    return;
  }

  this->current_temperature = temperature;
}

void PanasonicAC::update_target_temperature(uint8_t raw_value) {
  float temperature = raw_value * TEMPERATURE_STEP;

  if (temperature > TEMPERATURE_THRESHOLD) {
    ESP_LOGW(TAG, "Received out of range target temperature %.2f", temperature);
    return;
  }

  this->target_temperature = temperature;
}

void PanasonicAC::update_swing_horizontal(const std::string &Swing) {
  this->horizontal_swing_state_ = Swing;

  if (this->horizontal_swing_select_ != nullptr &&
      this->horizontal_swing_select_->state != this->horizontal_swing_state_) {
    this->horizontal_swing_select_->publish_state(
        this->horizontal_swing_state_);  // Set current horizontal swing position
  }
}

void PanasonicAC::update_swing_vertical(const std::string &Swing) {
  this->vertical_swing_state_ = Swing;

  if (this->vertical_swing_select_ != nullptr && this->vertical_swing_select_->state != this->vertical_swing_state_)
    this->vertical_swing_select_->publish_state(this->vertical_swing_state_);  // Set current vertical swing position
}

void PanasonicAC::update_nanoex(bool nanoex) {
  if (this->nanoex_switch_ != nullptr) {
    this->nanoex_state_ = nanoex;
    this->nanoex_switch_->publish_state(this->nanoex_state_);
  }
}

void PanasonicAC::update_eco(bool eco) {
  if (this->eco_switch_ != nullptr) {
    this->eco_state_ = eco;
    this->eco_switch_->publish_state(this->eco_state_);
  }
}

void PanasonicAC::update_econavi(bool econavi) {
  if (this->econavi_switch_ != nullptr) {
    this->econavi_state_ = econavi;
    this->econavi_switch_->publish_state(this->econavi_state_);
  }
}

void PanasonicAC::update_mild_dry(bool mild_dry) {
  if (this->mild_dry_switch_ != nullptr) {
    this->mild_dry_state_ = mild_dry;
    this->mild_dry_switch_->publish_state(this->mild_dry_state_);
  }
}

climate::ClimateAction PanasonicAC::determine_action() {
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    return climate::CLIMATE_ACTION_OFF;
  } else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY) {
    return climate::CLIMATE_ACTION_FAN;
  } else if (this->mode == climate::CLIMATE_MODE_DRY) {
    return climate::CLIMATE_ACTION_DRYING;
  } else if ((this->mode == climate::CLIMATE_MODE_COOL || this->mode == climate::CLIMATE_MODE_HEAT_COOL) &&
             this->current_temperature + TEMPERATURE_TOLERANCE >= this->target_temperature) {
    return climate::CLIMATE_ACTION_COOLING;
  } else if ((this->mode == climate::CLIMATE_MODE_HEAT || this->mode == climate::CLIMATE_MODE_HEAT_COOL) &&
             this->current_temperature - TEMPERATURE_TOLERANCE <= this->target_temperature) {
    return climate::CLIMATE_ACTION_HEATING;
  } else {
    return climate::CLIMATE_ACTION_IDLE;
  }
}

void PanasonicAC::update_current_power_consumption(int16_t power) {
  if (this->current_power_consumption_sensor_ != nullptr && this->current_power_consumption_sensor_->state != power) {
    this->current_power_consumption_sensor_->publish_state(
        power);  // Set current power consumption
  }
}

/*
 * Sensor handling
 */

void PanasonicAC::set_outside_temperature_sensor(sensor::Sensor *outside_temperature_sensor) {
  this->outside_temperature_sensor_ = outside_temperature_sensor;
}

void PanasonicAC::set_inside_temperature_sensor(sensor::Sensor *inside_temperature_sensor) {
  this->inside_temperature_sensor_ = inside_temperature_sensor;
}

void PanasonicAC::set_current_temperature_sensor(sensor::Sensor *current_temperature_sensor)
{
  this->current_temperature_sensor_ = current_temperature_sensor;
  this->current_temperature_sensor_->add_on_state_callback([this](float state)
                                                           {
                                                             this->current_temperature = state;
                                                             this->publish_state();
                                                           });
}

void PanasonicAC::set_vertical_swing_select(select::Select *vertical_swing_select) {
  this->vertical_swing_select_ = vertical_swing_select;
  this->vertical_swing_select_->add_on_state_callback([this](const std::string &value, size_t index) {
    if (value == this->vertical_swing_state_)
      return;
    this->on_vertical_swing_change(value);
  });
}

void PanasonicAC::set_horizontal_swing_select(select::Select *horizontal_swing_select) {
  this->horizontal_swing_select_ = horizontal_swing_select;
  this->horizontal_swing_select_->add_on_state_callback([this](const std::string &value, size_t index) {
    if (value == this->horizontal_swing_state_)
      return;
    this->on_horizontal_swing_change(value);
  });
}

void PanasonicAC::set_nanoex_switch(switch_::Switch *nanoex_switch) {
  this->nanoex_switch_ = nanoex_switch;
  this->nanoex_switch_->add_on_state_callback([this](bool state) {
    if (state == this->nanoex_state_)
      return;
    this->on_nanoex_change(state);
  });
}

void PanasonicAC::set_eco_switch(switch_::Switch *eco_switch) {
  this->eco_switch_ = eco_switch;
  this->eco_switch_->add_on_state_callback([this](bool state) {
    if (state == this->eco_state_)
      return;
    this->on_eco_change(state);
  });
}

void PanasonicAC::set_econavi_switch(switch_::Switch *econavi_switch) {
  this->econavi_switch_ = econavi_switch;
  this->econavi_switch_->add_on_state_callback([this](bool state) {
    if (state == this->econavi_state_)
      return;
    this->on_econavi_change(state);
  });
}

void PanasonicAC::set_mild_dry_switch(switch_::Switch *mild_dry_switch) {
  this->mild_dry_switch_ = mild_dry_switch;
  this->mild_dry_switch_->add_on_state_callback([this](bool state) {
    if (state == this->mild_dry_state_)
      return;
    this->on_mild_dry_change(state);
  });
}

void PanasonicAC::set_current_power_consumption_sensor(sensor::Sensor *current_power_consumption_sensor) {
  this->current_power_consumption_sensor_ = current_power_consumption_sensor;
}

/*
 * Debugging
 */

void PanasonicAC::log_packet(std::vector<uint8_t> data, bool outgoing) {
  if (outgoing) {
    ESP_LOGV(TAG, "TX: %s", format_hex_pretty(data).c_str());
  } else {
    ESP_LOGV(TAG, "RX: %s", format_hex_pretty(data).c_str());
  }
}

}  // namespace panasonic_ac
}  // namespace esphome
