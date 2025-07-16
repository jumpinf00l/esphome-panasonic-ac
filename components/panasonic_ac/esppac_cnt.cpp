#include "esppac_cnt.h"
#include "esppac_commands_cnt.h"

namespace esphome {
namespace panasonic_ac {
namespace CNT {

static const char *const TAG = "panasonic_ac.cz_tacg1";

void PanasonicACCNT::setup() {
  PanasonicAC::setup();

  ESP_LOGD(TAG, "Using CZ-TACG1 protocol via CN-CNT");
}

void PanasonicACCNT::loop() {
  PanasonicAC::read_data();

  if (millis() - this->last_read_ > READ_TIMEOUT &&
      !this->rx_buffer_.empty())  // Check if our read timed out and we received something
  {
    log_packet(this->rx_buffer_);

    if (!verify_packet())  // Verify length, header, counter and checksum
      return;

    this->waiting_for_response_ = false;
    this->last_packet_received_ = millis();  // Set the time at which we received our last packet

    handle_packet();

    this->rx_buffer_.clear();  // Reset buffer
  }
  handle_cmd();
  handle_poll();  // Handle sending poll packets
}

/*
 * ESPHome control request
 */

void PanasonicACCNT::control(const climate::ClimateCall &call) {
  if (this->state_ != ACState::Ready)
    return;

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->data;
  }

  if (call.get_mode().has_value()) {
    ESP_LOGV(TAG, "Requested mode change");

    switch (*call.get_mode()) {
      case climate::CLIMATE_MODE_COOL:
        this->cmd[0] = 0x34;
        break;
      case climate::CLIMATE_MODE_HEAT:
        this->cmd[0] = 0x44;
        break;
      case climate::CLIMATE_MODE_DRY:
        this->cmd[0] = 0x24;
        break;
      case climate::CLIMATE_MODE_HEAT_COOL:
        this->cmd[0] = 0x04;
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        this->cmd[0] = 0x64;
        break;
      case climate::CLIMATE_MODE_OFF:
        this->cmd[0] = this->cmd[0] & 0xF0;  // Strip right nib to turn AC off
        break;
      default:
        ESP_LOGV(TAG, "Unsupported mode requested");
        break;
    }
  }

  if (call.get_target_temperature().has_value()) {
    this->cmd[1] = *call.get_target_temperature() / TEMPERATURE_STEP;
  }
  
  if (call.get_fan_mode().has_value()) {
    ESP_LOGI(TAG, "Requested fan mode change");

    if(this->preset != climate::CLIMATE_PRESET_NONE)
    {
      ESP_LOGV(TAG, "Setting preset to: None");
      this->cmd[5] = (this->cmd[5] & 0xF0);  // Clear right nib for CLIMATE_PRESET_NONE
      // If the preset being cleared was CLIMATE_PRESET_ECO, turn off the eco setting
      if (this->preset == climate::CLIMATE_PRESET_ECO) {
        this->cmd[8] = 0x00; // Turn eco OFF
      }
    }
    
    switch (*call.get_fan_mode()) {
      case climate::CLIMATE_FAN_AUTO:
        ESP_LOGI(TAG, "Fan mode: Auto");
        this->cmd[3] = 0xA0;
        break;
      case climate::CLIMATE_FAN_QUIET:
        ESP_LOGI(TAG, "Fan mode: Quiet");
        this->cmd[3] = 0x28; // Set fan mode byte to 0x28 for Quiet
        break;
      case climate::CLIMATE_FAN_DIFFUSE:
        ESP_LOGI(TAG, "Fan mode: 1 - Diffuse");
        this->cmd[3] = 0x30;
        break;
      case climate::CLIMATE_FAN_LOW:
        ESP_LOGI(TAG, "Fan mode: 2 - Low");
        this->cmd[3] = 0x40;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        ESP_LOGI(TAG, "Fan mode: 3 - Medium");
        this->cmd[3] = 0x50;
        break;
      case climate::CLIMATE_FAN_HIGH:
        ESP_LOGI(TAG, "Fan mode: 4 - High");
        this->cmd[3] = 0x60;
        break;
      case climate::CLIMATE_FAN_FOCUS:
        ESP_LOGI(TAG, "Fan mode: 5 - Focus");
        this->cmd[3] = 0x70;
        break;
      default:
        ESP_LOGW(TAG, "Unsupported fan mode requested");
        break;
    }
  }
  
  if (call.get_swing_mode().has_value()) {
    ESP_LOGV(TAG, "Requested swing mode change");

    switch (*call.get_swing_mode()) {
      case climate::CLIMATE_SWING_BOTH:
        this->cmd[4] = 0xFD;
        break;
      case climate::CLIMATE_SWING_OFF:
        this->cmd[4] = 0x36;  // Reset both to center
        break;
      case climate::CLIMATE_SWING_VERTICAL:
        this->cmd[4] = 0xF6;  // Swing vertical, horizontal center
        break;
      case climate::CLIMATE_SWING_HORIZONTAL:
        this->cmd[4] = 0x3D;  // Swing horizontal, vertical center
        break;
      default:
        ESP_LOGV(TAG, "Unsupported swing mode requested");
        break;
    }
  }

/*  if (call.get_custom_preset().has_value()) {
    ESP_LOGV(TAG, "Requested preset change");

    std::string preset = *call.get_custom_preset();

    if (preset.compare("Normal") == 0)
      this->cmd[5] = (this->cmd[5] & 0xF0);  // Clear right nib for normal mode
    else if (preset.compare("Powerful") == 0)
      this->cmd[5] = (this->cmd[5] & 0xF0) + 0x02;  // Clear right nib and set powerful mode
    else if (preset.compare("Quiet") == 0)
      this->cmd[5] = (this->cmd[5] & 0xF0) + 0x04;  // Clear right nib and set quiet mode
    else
      ESP_LOGV(TAG, "Unsupported preset requested");
  } */

  if (call.get_preset().has_value()) {
    ESP_LOGV(TAG, "Requested preset change");

    switch (*call.get_preset()) {
      case climate::CLIMATE_PRESET_NONE:
        this->cmd[5] = (this->cmd[5] & 0xF0);             // Clear right nib for CLIMATE_PRESET_NONE
        this->cmd[8] = 0x00;                              // Turn eco OFF
        break;
      case climate::CLIMATE_PRESET_BOOST:
        this->cmd[5] = (this->cmd[5] & 0xF0) + 0x02;      // Clear right nib and set CLIMATE_PRESET_BOOST
        this->cmd[8] = 0x00;                              // Turn eco OFF
        break;
      case climate::CLIMATE_PRESET_ECO:                              
        this->cmd[5] = (this->cmd[5] & 0xF0);             // Clear other preset bits in cmd[5] (like quiet if it was set)
        this->cmd[8] = 0x40;                              // Turn eco ON
        break;
      default:
        ESP_LOGW(TAG, "Unsupported preset requested");
        break;
    }
  }
  
}

/*
 * Set the data array to the fields
 */
void PanasonicACCNT::set_data(bool set) {
  this->mode = determine_mode(this->data[0]);
  this->fan_mode = determine_fan_mode(this->data[3]);
  this->preset = determine_preset(this->data[5]);

  std::string verticalSwing = determine_vertical_swing(this->data[4]);
  std::string horizontalSwing = determine_horizontal_swing(this->data[4]);

  bool nanoex = determine_preset_nanoex(this->data[5]);
  bool eco = determine_eco(this->data[8]);
  bool econavi = determine_econavi(this->data[5]);
  bool mildDry = determine_mild_dry(this->data[2]);
  
  this->update_target_temperature((int8_t) this->data[1]);

  if (set) {
    // Also set current and outside temperature
    // 128 means not supported
    if (this->current_temperature_sensor_ == nullptr) {
      if(this->rx_buffer_[18] != 0x80)
        this->update_current_temperature((int8_t)this->rx_buffer_[18]);
      else if(this->rx_buffer_[21] != 0x80)
        this->update_current_temperature((int8_t)this->rx_buffer_[21]);
      else
        ESP_LOGV(TAG, "Current temperature is not supported");
    }

    if (this->outside_temperature_sensor_ != nullptr)
    {
      if(this->rx_buffer_[19] != 0x80)
        this->update_outside_temperature((int8_t)this->rx_buffer_[19]);
      else if(this->rx_buffer_[22] != 0x80)
        this->update_outside_temperature((int8_t)this->rx_buffer_[22]);
      else
        ESP_LOGV(TAG, "Outside temperature is not supported");
    }

    if (this->inside_temperature_sensor_ != nullptr)
    {
      if(this->rx_buffer_[18] != 0x80)
        this->update_inside_temperature((int8_t)this->rx_buffer_[18]);
      else if(this->rx_buffer_[21] != 0x80)
        this->update_inside_temperature((int8_t)this->rx_buffer_[21]);
      else
        ESP_LOGV(TAG, "Inside temperature is not supported");
    }

    if(this->current_power_consumption_sensor_ != nullptr) {
      uint16_t power_consumption = determine_power_consumption((int8_t)this->rx_buffer_[28], (int8_t)this->rx_buffer_[29], (int8_t)this->rx_buffer_[30]);
      this->update_current_power_consumption(power_consumption);
    }
  }

  if (verticalSwing == "auto" && horizontalSwing == "auto")
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  else if (verticalSwing == "auto")
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  else if (horizontalSwing == "auto")
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  else
    this->swing_mode = climate::CLIMATE_SWING_OFF;

  this->update_swing_vertical(verticalSwing);
  this->update_swing_horizontal(horizontalSwing);

  this->preset = preset;

  this->update_nanoex(nanoex);
  this->update_eco(eco);
  this->update_econavi(econavi);
  this->update_mild_dry(mildDry);
}

/*
 * Send a command, attaching header, packet length and checksum
 */
void PanasonicACCNT::send_command(std::vector<uint8_t> command, CommandType type, uint8_t header = CNT::CTRL_HEADER) {
  uint8_t length = command.size();
  command.insert(command.begin(), header);
  command.insert(command.begin() + 1, length);

  uint8_t checksum = 0;

  for (uint8_t i : command)
    checksum -= i;  // Add to checksum

  command.push_back(checksum);

  send_packet(command, type);  // Actually send the constructed packet
}

/*
 * Send a raw packet, as is
 */
void PanasonicACCNT::send_packet(const std::vector<uint8_t> &packet, CommandType type) {
  this->last_packet_sent_ = millis();  // Save the time when we sent the last packet

  if (type != CommandType::Response)     // Don't wait for a response for responses
    this->waiting_for_response_ = true;  // Mark that we are waiting for a response

  write_array(packet);       // Write to UART
  log_packet(packet, true);  // Write to log
}

/*
 * Loop handling
 */

void PanasonicACCNT::handle_poll() {
  if (millis() - this->last_packet_sent_ > POLL_INTERVAL) {
    ESP_LOGV(TAG, "Polling AC");
    send_command(CMD_POLL, CommandType::Normal, POLL_HEADER);
  }
}

void PanasonicACCNT::handle_cmd() {
  if (!this->cmd.empty() && millis() - this->last_packet_sent_ > CMD_INTERVAL) {
    ESP_LOGV(TAG, "Sending Command");
    send_command(this->cmd, CommandType::Normal, CTRL_HEADER);
    this->cmd.clear();
  }
}

/*
 * Packet handling
 */

bool PanasonicACCNT::verify_packet() {
  if (this->rx_buffer_.size() < 12) {
    ESP_LOGW(TAG, "Dropping invalid packet (length)");

    this->rx_buffer_.clear();  // Reset buffer
    return false;
  }

  // Check if header matches
  if (this->rx_buffer_[0] != CTRL_HEADER && this->rx_buffer_[0] != POLL_HEADER) {
    ESP_LOGW(TAG, "Dropping invalid packet (header)");

    this->rx_buffer_.clear();  // Reset buffer
    return false;
  }

  // Packet length minus header, packet length and checksum
  if (this->rx_buffer_[1] != this->rx_buffer_.size() - 3) {
    ESP_LOGD(TAG, "Dropping invalid packet (length mismatch)");

    this->rx_buffer_.clear();  // Reset buffer
    return false;
  }

  uint8_t checksum = 0;

  for (uint8_t b : this->rx_buffer_) {
    checksum += b;
  }

  if (checksum != 0) {
    ESP_LOGD(TAG, "Dropping invalid packet (checksum)");

    this->rx_buffer_.clear();  // Reset buffer
    return false;
  }

  return true;
}

void PanasonicACCNT::handle_packet() {
  if (this->rx_buffer_[0] == POLL_HEADER) {
    this->data = std::vector<uint8_t>(this->rx_buffer_.begin() + 2, this->rx_buffer_.begin() + 12);

    this->set_data(true);
    this->publish_state();

    if (this->state_ != ACState::Ready)
      this->state_ = ACState::Ready;  // Mark as ready after first poll
  } else {
    ESP_LOGD(TAG, "Received unknown packet");
  }
}

climate::ClimateMode PanasonicACCNT::determine_mode(uint8_t mode) {
  uint8_t nib1 = (mode >> 4) & 0x0F;  // Left nib for mode
  uint8_t nib2 = (mode >> 0) & 0x0F;  // Right nib for power state

  if (nib2 == 0x00)
    return climate::CLIMATE_MODE_OFF;

  switch (nib1) {
    case 0x00:  // Auto
      return climate::CLIMATE_MODE_HEAT_COOL;
    case 0x03:  // Cool
      return climate::CLIMATE_MODE_COOL;
    case 0x04:  // Heat
      return climate::CLIMATE_MODE_HEAT;
    case 0x02:  // Dry
      return climate::CLIMATE_MODE_DRY;
    case 0x06:  // Fan only
      return climate::CLIMATE_MODE_FAN_ONLY;
    default:
      ESP_LOGW(TAG, "Received unknown climate mode");
      return climate::CLIMATE_MODE_OFF;
  }
}

climate::ClimateFanMode PanasonicACCNT::determine_fan_mode(uint8_t fan_mode) {
  switch (fan_mode) {
    case 0xA0:
      ESP_LOGI(TAG, "Setting fan mode to: Auto");
      return climate::CLIMATE_FAN_AUTO;
    case 0x28:
      ESP_LOGI(TAG, "Setting fan mode to: Quiet");
      return climate::CLIMATE_FAN_QUIET;
    case 0x30:
      ESP_LOGI(TAG, "Setting fan mode to: 1 - Diffuse");
      return climate::CLIMATE_FAN_DIFFUSE;
    case 0x40:
      ESP_LOGI(TAG, "Setting fan mode to: 2 - Low");
      return climate::CLIMATE_FAN_LOW;
    case 0x50:
      ESP_LOGI(TAG, "Setting fan mode to: 3 - Medium");
      return climate::CLIMATE_FAN_MEDIUM;
    case 0x60:
      ESP_LOGI(TAG, "Setting fan mode to: 4 - High");
      return climate::CLIMATE_FAN_HIGH;
    case 0x70:
      ESP_LOGI(TAG, "Setting fan mode to: 5 - Focus");
      return climate::CLIMATE_FAN_FOCUS;
    default:
      ESP_LOGW(TAG, "Received unknown fan mode");
      return climate::CLIMATE_FAN_AUTO;
  }
}

std::string PanasonicACCNT::determine_vertical_swing(uint8_t swing) {
  uint8_t nib = (swing >> 4) & 0x0F;  // Left nib for vertical swing

  switch (nib) {
    case 0x0E:
      return "Swing";
    case 0x0F:
      return "Auto";
    case 0x01:
      return "Top";
    case 0x02:
      return "Top Middle";
    case 0x03:
      return "Middle";
    case 0x04:
      return "Bottom Middle";
    case 0x05:
      return "Bottom";
    case 0x00:
      return "unsupported";
    default:
      ESP_LOGW(TAG, "Received unknown vertical swing mode: 0x%02X", nib);
      return "Unknown";
  }
}

std::string PanasonicACCNT::determine_horizontal_swing(uint8_t swing) {
  uint8_t nib = (swing >> 0) & 0x0F;  // Right nib for horizontal swing

  switch (nib) {
    case 0x0D:
      return "Auto";
    case 0x09:
      return "Left";
    case 0x0A:
      return "Left Center";
    case 0x06:
      return "Center";
    case 0x0B:
      return "Right Center";
    case 0x0C:
      return "Right";
    case 0x00:
      return "unsupported";
    default:
      ESP_LOGW(TAG, "Received unknown horizontal swing mode");
      return "Unknown";
  }
}

climate::ClimatePreset PanasonicACCNT::determine_preset(uint8_t preset_byte) {
  // First, check if eco mode is active via data[8]
  if ((this->data[8] & 0x40) == 0x40) {
    return climate::CLIMATE_PRESET_ECO;
  }

  // Then check for other presets based on preset_byte (data[5])
  switch (preset_byte & 0x0F) { // Check only the right nibble
    case 0x00: // No preset
      return climate::CLIMATE_PRESET_NONE;
    case 0x02: // Powerful / Boost
      return climate::CLIMATE_PRESET_BOOST;
    // 0x04 was previously CLIMATE_PRESET_ECO (quiet). Now it's CLIMATE_FAN_QUIET and should not be a preset.
    // So, if 0x04 is found here, and eco is not active, it implies no preset.
    case 0x04: // This was old "Quiet" preset, now should not be a preset, return NONE
      return climate::CLIMATE_PRESET_NONE;
    default:
      ESP_LOGW(TAG, "Received unknown climate preset: 0x%02X", preset_byte);
      return climate::CLIMATE_PRESET_NONE;
  }
}

bool PanasonicACCNT::determine_preset_nanoex(uint8_t preset) {
  uint8_t nib = (preset >> 4) & 0x04;  // Left nib for nanoex

  if (nib == 0x04)
    return true;
  else if (nib == 0x00)
    return false;
  else {
    ESP_LOGW(TAG, "Received unknown nanoex value");
    return false;
  }
}

bool PanasonicACCNT::determine_eco(uint8_t value) {
  if (value == 0x40)
    return true;
  else if (value == 0x00)
    return false;
  else {
    ESP_LOGW(TAG, "Received unknown eco value");
    return false;
  }
}

bool PanasonicACCNT::determine_econavi(uint8_t value) {
  uint8_t nib = value & 0x10;
  
  if (nib == 0x10)
    return true;
  else if (nib == 0x00)
    return false;
  else {
    ESP_LOGW(TAG, "Received unknown econavi value");
    return false;
  }
}

bool PanasonicACCNT::determine_mild_dry(uint8_t value) {
  if (value == 0x7F)
    return true;
  else if (value == 0x80)
    return false;
  else {
    ESP_LOGW(TAG, "Received unknown mild dry value");
    return false;
  }
}

uint16_t PanasonicACCNT::determine_power_consumption(uint8_t byte_28, uint8_t byte_29, uint8_t offset) {
  return (uint16_t)(byte_28 + (byte_29 * 256)) - offset;
}

/*
 * Sensor handling
 */

void PanasonicACCNT::on_vertical_swing_change(const std::string &swing) {
  if (this->state_ != ACState::Ready)
    return;

  ESP_LOGD(TAG, "Setting vertical swing position");

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->data;
  }

  if (swing == "Bottom")
    this->cmd[4] = (this->cmd[4] & 0x0F) + 0x50;
  else if (swing == "Bottom Middle")
    this->cmd[4] = (this->cmd[4] & 0x0F) + 0x40;
  else if (swing == "Middle")
    this->cmd[4] = (this->cmd[4] & 0x0F) + 0x30;
  else if (swing == "Top Middle")
    this->cmd[4] = (this->cmd[4] & 0x0F) + 0x20;
  else if (swing == "Top")
    this->cmd[4] = (this->cmd[4] & 0x0F) + 0x10;
  else if (swing == "Swing")
    this->cmd[4] = (this->cmd[4] & 0x0F) + 0xE0;
  else if (swing == "Auto")
    this->cmd[4] = (this->cmd[4] & 0x0F) + 0xF0;
  else {
    ESP_LOGW(TAG, "Unsupported vertical swing position received");
    return;
  }

}

void PanasonicACCNT::on_horizontal_swing_change(const std::string &swing) {
  if (this->state_ != ACState::Ready)
    return;

  ESP_LOGD(TAG, "Setting horizontal swing position");

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->data;
  }

  if (swing == "Left")
    this->cmd[4] = (this->cmd[4] & 0xF0) + 0x09;
  else if (swing == "Left Center")
    this->cmd[4] = (this->cmd[4] & 0xF0) + 0x0A;
  else if (swing == "Center")
    this->cmd[4] = (this->cmd[4] & 0xF0) + 0x06;
  else if (swing == "Right Center")
    this->cmd[4] = (this->cmd[4] & 0xF0) + 0x0B;
  else if (swing == "Right")
    this->cmd[4] = (this->cmd[4] & 0xF0) + 0x0C;
  else if (swing == "Auto")
    this->cmd[4] = (this->cmd[4] & 0xF0) + 0x0D;
  else {
    ESP_LOGW(TAG, "Unsupported horizontal swing position received");
    return;
  }

}

void PanasonicACCNT::on_nanoex_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->data;
  }

  this->nanoex_state_ = state;

  if (state) {
    ESP_LOGV(TAG, "Turning nanoex on");
    this->cmd[5] = (this->cmd[5] & 0x0F) + 0x40;
  } else {
    ESP_LOGV(TAG, "Turning nanoex off");
    this->cmd[5] = (this->cmd[5] & 0x0F);
  }
}

void PanasonicACCNT::on_eco_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->data;
  }

  this->eco_state_ = state;

  if (state) {
    ESP_LOGV(TAG, "Turning eco mode on");
    this->cmd[8] = 0x40;  // Set the byte corresponding to eco mode
  } else {
    ESP_LOGV(TAG, "Turning eco mode off");
    this->cmd[8] = 0x00;  // Clear the byte corresponding to eco mode
  }

  // Optionally, send the modified command immediately
  send_command(this->cmd, CommandType::Normal, CTRL_HEADER);
}

void PanasonicACCNT::on_econavi_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->data;
  }

  this->econavi_state_ = state;

  if (state) {
    ESP_LOGV(TAG, "Turning econavi mode on");
    this->cmd[5] = 0x10;
  } else {
    ESP_LOGV(TAG, "Turning econavi mode off");
    this->cmd[5] = 0x00;
  }

}

void PanasonicACCNT::on_mild_dry_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->data;
  }

  this->mild_dry_state_ = state;

  if (state) {
    ESP_LOGV(TAG, "Turning mild dry on");
    this->cmd[2] = 0x7F;
  } else {
    ESP_LOGV(TAG, "Turning mild dry off");
    this->cmd[2] = 0x80;
  }

}

}  // namespace CNT
}  // namespace panasonic_ac
}  // namespace esphome
