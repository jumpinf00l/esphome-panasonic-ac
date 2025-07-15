// esppac_cnt.cpp
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
        // If Quiet mode was active, clear the quiet bit
        this->cmd[5] = (this->cmd[5] & 0xF0);
        break;
      case climate::CLIMATE_FAN_QUIET:
        ESP_LOGI(TAG, "Fan mode: Quiet");
        this->cmd[3] = 0x34; // Set a default fan mode if quiet is selected to avoid issues with current fan mode
        this->cmd[5] = (this->cmd[5] & 0xF0) + 0x04; // Set the quiet bit in cmd[5] (preset byte)
        break;
      case climate::CLIMATE_FAN_DIFFUSE:
        ESP_LOGI(TAG, "Fan mode: 1 - Diffuse");
        this->cmd[3] = 0x30;
        // If Quiet mode was active, clear the quiet bit
        this->cmd[5] = (this->cmd[5] & 0xF0);
        break;
      case climate::CLIMATE_FAN_LOW:
        ESP_LOGI(TAG, "Fan mode: 2 - Low");
        this->cmd[3] = 0x40;
        // If Quiet mode was active, clear the quiet bit
        this->cmd[5] = (this->cmd[5] & 0xF0);
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        ESP_LOGI(TAG, "Fan mode: 3 - Medium");
        this->cmd[3] = 0x50;
        // If Quiet mode was active, clear the quiet bit
        this->cmd[5] = (this->cmd[5] & 0xF0);
        break;
      case climate::CLIMATE_FAN_HIGH:
        ESP_LOGI(TAG, "Fan mode: 4 - High");
        this->cmd[3] = 0x60;
        // If Quiet mode was active, clear the quiet bit
        this->cmd[5] = (this->cmd[5] & 0xF0);
        break;
      case climate::CLIMATE_FAN_FOCUS:
        ESP_LOGI(TAG, "Fan mode: 5 - Focus");
        this->cmd[3] = 0x70;
        // If Quiet mode was active, clear the quiet bit
        this->cmd[5] = (this->cmd[5] & 0xF0);
        break;
      default:
        ESP_LOGV(TAG, "Unsupported fan mode requested");
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
        this->cmd[4] = 0x36; // Reset both to center
        break;
      case climate::CLIMATE_SWING_VERTICAL:
        this->cmd[4] = 0xC0;
        break;
      case climate::CLIMATE_SWING_HORIZONTAL:
        this->cmd[4] = 0x81;
        break;
      default:
        ESP_LOGV(TAG, "Unsupported swing mode requested");
        break;
    }
  }

  if (call.get_preset().has_value()) {
    ESP_LOGV(TAG, "Requested preset change");

    // Clear any existing preset bits in byte 5 if changing preset
    this->cmd[5] = (this->cmd[5] & 0xF0);

    switch (*call.get_preset()) {
      case climate::CLIMATE_PRESET_NONE:
        ESP_LOGI(TAG, "Preset: None");
        this->cmd[8] = 0x00; // Turn eco OFF
        break;
      case climate::CLIMATE_PRESET_BOOST:
        ESP_LOGI(TAG, "Preset: Powerful");
        this->cmd[5] = (this->cmd[5] & 0xF0) + 0x02;
        break;
      case climate::CLIMATE_PRESET_ECO:
        ESP_LOGI(TAG, "Preset: Eco");
        this->cmd[8] = 0x40; // Turn eco ON
        break;
      default:
        ESP_LOGV(TAG, "Unsupported preset requested");
        break;
    }
  }

  send_command(this->cmd, CommandType::Normal, CTRL_HEADER);
}

void PanasonicACCNT::handle_packet() {
  if (this->rx_buffer_[0] == POLL_HEADER) // Poll response
  {
    ESP_LOGV(TAG, "Received Poll Response");
    this->data = this->rx_buffer_;
    this->state_ = ACState::Ready;

    this->mode = this->determine_mode(this->rx_buffer_[0]);
    this->target_temperature = this->determine_target_temperature(this->rx_buffer_[1]);
    this->fan_mode = this->determine_fan_mode(this->rx_buffer_[3], this->rx_buffer_[5]); // Pass both bytes
    this->swing_mode = this->determine_swing_mode(this->rx_buffer_[4]);
    this->preset = this->determine_preset(this->rx_buffer_[5]);
    this->nanoex_state_ = this->determine_preset_nanoex(this->rx_buffer_[5]);
    this->eco_state_ = this->determine_eco(this->rx_buffer_[8]);
    this->econavi_state_ = this->determine_econavi(this->rx_buffer_[5]);
    this->mild_dry_state_ = this->determine_mild_dry(this->rx_buffer_[5]);
    
    // Update sensors
    if (this->rx_buffer_.size() > 19) { // Check if the buffer is large enough for temperature data
      int8_t current_temp = (int8_t)this->rx_buffer_[19];
      this->update_current_temperature(current_temp);
      ESP_LOGI(TAG, "Current temperature: %d", current_temp); // Log current temperature
    }

    if (this->rx_buffer_.size() > 21) {
      this->update_outside_temperature((int8_t)this->rx_buffer_[21]);
    }
    if (this->rx_buffer_.size() > 23) {
      this->update_inside_temperature((int8_t)this->rx_buffer_[23]);
    }
    if (this->rx_buffer_.size() > 30) {
      uint16_t power_consumption =
          determine_power_consumption((int8_t)this->rx_buffer_[28], (int8_t)this->rx_buffer_[29], (int8_t)this->rx_buffer_[30]);
      this->update_current_power_consumption(power_consumption);
    }
    
    this->publish_state();
  } else if (this->rx_buffer_[0] == CTRL_HEADER) { // Control response
    ESP_LOGV(TAG, "Received Control Response");
    // Control responses are not used to update state, as they are often
    // incomplete, and a poll response is sent immediately after a control
    // command, so we just wait for that.
  } else {
    ESP_LOGD(TAG, "Received unknown packet");
  }
}

climate::ClimateMode PanasonicACCNT::determine_mode(uint8_t mode) {
  uint8_t nib1 = (mode >> 4) & 0x0F; // Left nib for mode
  uint8_t nib2 = (mode >> 0) & 0x0F; // Right nib for AC power state

  if (nib2 == 0x00) { // If AC is off
    return climate::CLIMATE_MODE_OFF;
  }

  switch (nib1) {
    case 0x00:
      return climate::CLIMATE_MODE_HEAT_COOL;
    case 0x01:
      return climate::CLIMATE_MODE_AUTO; // Not sure about this
    case 0x02:
      return climate::CLIMATE_MODE_DRY;
    case 0x03:
      return climate::CLIMATE_MODE_COOL;
    case 0x04:
      return climate::CLIMATE_MODE_HEAT;
    case 0x06:
      return climate::CLIMATE_MODE_FAN_ONLY;
    default:
      return climate::CLIMATE_MODE_OFF;
  }
}

// Modified determine_fan_mode to check both bytes
climate::ClimateFanMode PanasonicACCNT::determine_fan_mode(uint8_t fan_mode_byte_3, uint8_t fan_mode_byte_5) {
  // Check for Quiet mode first
  if (((fan_mode_byte_5 >> 0) & 0x0F) == 0x04) {
    ESP_LOGI(TAG, "Setting fan mode to: Quiet (from byte 5)");
    return climate::CLIMATE_FAN_QUIET;
  }

  // Otherwise, use the existing logic for other fan modes
  uint8_t fan_mode_val = (fan_mode_byte_3 >> 4) & 0x0F; // Left nib for fan mode

  switch (fan_mode_val) {
    case 0x0A:
      ESP_LOGI(TAG, "Setting fan mode to: Auto");
      return climate::CLIMATE_FAN_AUTO;
    case 0x03:
      ESP_LOGI(TAG, "Setting fan mode to: Diffuse");
      return climate::CLIMATE_FAN_DIFFUSE;
    case 0x04:
      ESP_LOGI(TAG, "Setting fan mode to: Low");
      return climate::CLIMATE_FAN_LOW;
    case 0x05:
      ESP_LOGI(TAG, "Setting fan mode to: Medium");
      return climate::CLIMATE_FAN_MEDIUM;
    case 0x06:
      ESP_LOGI(TAG, "Setting fan mode to: High");
      return climate::CLIMATE_FAN_HIGH;
    case 0x07:
      ESP_LOGI(TAG, "Setting fan mode to: Focus");
      return climate::CLIMATE_FAN_FOCUS;
    default:
      ESP_LOGW(TAG, "Unknown fan mode 0x%02X", fan_mode_byte_3);
      return climate::CLIMATE_FAN_AUTO;
  }
}


std::string PanasonicACCNT::determine_horizontal_swing(uint8_t swing) {
  if (swing == 0x81) {
    return "auto";
  } else if (swing == 0x36) {
    return "off"; // assuming 0x36 means off/center for horizontal
  } else {
    ESP_LOGW(TAG, "Unknown horizontal swing mode 0x%02X", swing);
    return "off"; // default to off
  }
}

std::string PanasonicACCNT::determine_vertical_swing(uint8_t swing) {
  if (swing == 0xC0) {
    return "auto";
  } else if (swing == 0x36) {
    return "off"; // assuming 0x36 means off/center for vertical
  } else {
    ESP_LOGW(TAG, "Unknown vertical swing mode 0x%02X", swing);
    return "off"; // default to off
  }
}

climate::ClimateSwingMode PanasonicACCNT::determine_swing_mode(uint8_t swing) {
  if (swing == 0xFD) {
    return climate::CLIMATE_SWING_BOTH;
  } else if (swing == 0xC0) {
    return climate::CLIMATE_SWING_VERTICAL;
  } else if (swing == 0x81) {
    return climate::CLIMATE_SWING_HORIZONTAL;
  } else {
    return climate::CLIMATE_SWING_OFF; // Default to off
  }
}

climate::ClimatePreset PanasonicACCNT::determine_preset(uint8_t preset) {
  uint8_t nibble = (preset >> 0) & 0x0F; // Right nibble

  switch (nibble) {
    case 0x00:
      return climate::CLIMATE_PRESET_NONE;
    case 0x02: // Powerful / Boost
      return climate::CLIMATE_PRESET_BOOST;
    // 0x04 was previously CLIMATE_PRESET_ECO (quiet). Now it's CLIMATE_FAN_QUIET and should not be a preset.
    // So, if 0x04 is set, it means the Quiet fan mode is active, not an ECO preset.
    // We handle the actual ECO preset from byte 8.
    default:
      return climate::CLIMATE_PRESET_NONE;
  }
}

bool PanasonicACCNT::determine_preset_nanoex(uint8_t preset) {
  uint8_t nibble = (preset >> 0) & 0x0F; // Right nibble
  return (nibble == 0x01); // Assuming 0x01 is for NanoeX
}

bool PanasonicACCNT::determine_eco(uint8_t value) {
  return ((value >> 6) & 0x01) == 0x01; // Bit 6 of byte 8 is for Eco mode
}

bool PanasonicACCNT::determine_econavi(uint8_t value) {
  uint8_t nibble = (value >> 4) & 0x0F; // Left nibble of byte 5
  return (nibble == 0x01); // Assuming 0x01 in left nibble of byte 5 is for Econavi
}

bool PanasonicACCNT::determine_mild_dry(uint8_t value) {
  uint8_t nibble = (value >> 0) & 0x0F; // Right nibble of byte 5
  return (nibble == 0x08); // Assuming 0x08 in right nibble of byte 5 is for Mild Dry
}

uint16_t PanasonicACCNT::determine_power_consumption(uint8_t byte_28, uint8_t multiplier, uint8_t offset) {
    // This is a simplified example based on common patterns.
    // You might need to adjust this based on the actual protocol documentation.
    uint16_t raw_value = (byte_28 << 8) | multiplier; // Example combination
    return (raw_value / 10) + offset; // Example calculation
}

void PanasonicACCNT::log_packet(const std::vector<uint8_t> &packet) {
  ESP_LOGV(TAG, "Packet: %s", format_hex_pretty(packet).c_str());
}

bool PanasonicACCNT::verify_packet() {
  uint8_t packet_size = this->rx_buffer_[1]; // Size is at index 1

  if (this->rx_buffer_.size() < packet_size) {
    ESP_LOGW(TAG, "Packet too short, got %d, expected %d", this->rx_buffer_.size(), packet_size);
    return false;
  }

  // Verify header (already done in loop, but good to double check)
  if (this->rx_buffer_[0] != POLL_HEADER && this->rx_buffer_[0] != CTRL_HEADER) {
    ESP_LOGW(TAG, "Invalid header: 0x%02X", this->rx_buffer_[0]);
    return false;
  }

  // Verify checksum
  uint8_t checksum = 0;
  for (size_t i = 0; i < packet_size - 1; i++) {
    checksum += this->rx_buffer_[i];
  }

  if (checksum != this->rx_buffer_[packet_size - 1]) {
    ESP_LOGW(TAG, "Invalid checksum: 0x%02X, expected 0x%02X", this->rx_buffer_[packet_size - 1], checksum);
    return false;
  }

  return true;
}

void PanasonicACCNT::handle_cmd() {
  if (this->cmd.empty())
    return;

  if (millis() - this->last_packet_sent_ > CMD_INTERVAL) {
    ESP_LOGV(TAG, "Sending Command");
    send_command(this->cmd, CommandType::Normal, CTRL_HEADER);
    this->cmd.clear(); // Clear command after sending
  }
}

void PanasonicACCNT::handle_poll() {
  if (millis() - this->last_packet_sent_ > POLL_INTERVAL) {
    ESP_LOGV(TAG, "Polling AC");
    send_command(CMD_POLL, CommandType::Normal, POLL_HEADER);
  }
}

void PanasonicACCNT::send_command(std::vector<uint8_t> command, CommandType type, uint8_t header) {
  std::vector<uint8_t> packet;
  packet.push_back(header); // Header
  packet.push_back(command.size() + 2); // Length (command + header + checksum)
  packet.insert(packet.end(), command.begin(), command.end()); // Command

  uint8_t checksum = 0;
  for (uint8_t byte : packet) {
    checksum += byte;
  }
  packet.push_back(checksum); // Checksum

  write_bytes(packet);
  this->last_packet_sent_ = millis();
}

void PanasonicACCNT::send_packet(const std::vector<uint8_t> &command, CommandType type) {
  // This function seems redundant with send_command, consider removing or refactoring if not truly needed.
  // For now, I'll assume send_command is the primary method for sending.
  // This function is currently not used.
}

}  // namespace CNT
}  // namespace panasonic_ac
}  // namespace esphome
