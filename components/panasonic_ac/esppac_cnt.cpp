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
    this->log_packet(this->rx_buffer_); // Use base class log_packet

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
    this->cmd[1] = (uint8_t)(*call.get_target_temperature() / TEMPERATURE_STEP);
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
        this->cmd[5] = (this->cmd[5] & 0xF0) + 0x04; // Set the quiet bit in cmd[5] (preset byte)
        break;
      case climate::CLIMATE_FAN_DIFFUSE:
        ESP_LOGI(TAG, "Fan mode: 1 - Diffuse");
        this->cmd[3] = 0x3C;
        break;
      case climate::CLIMATE_FAN_LOW:
        ESP_LOGI(TAG, "Fan mode: 2 - Low");
        this->cmd[3] = 0x40;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        ESP_LOGI(TAG, "Fan mode: 3 - Medium");
        this->cmd[3] = 0x60;
        break;
      case climate::CLIMATE_FAN_HIGH:
        ESP_LOGI(TAG, "Fan mode: 4 - High");
        this->cmd[3] = 0x80;
        break;
      case climate::CLIMATE_FAN_FOCUS:
        ESP_LOGI(TAG, "Fan mode: 5 - Focus");
        this->cmd[3] = 0xC0;
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
        this->cmd[4] = 0x36;  // Reset both to center
        break;
      case climate::CLIMATE_SWING_VERTICAL:
        this->cmd[4] = (this->cmd[4] & 0x0F) | 0xA0;  // Keep horizontal swing but set vertical swing to auto
        break;
      case climate::CLIMATE_SWING_HORIZONTAL:
        this->cmd[4] = (this->cmd[4] & 0xF0) | 0x0D;  // Keep vertical swing but set horizontal swing to auto
        break;
      default:
        ESP_LOGV(TAG, "Unsupported swing mode requested");
        break;
    }
  }

  if (call.get_preset().has_value()) {
    ESP_LOGV(TAG, "Requested preset change");

    if (*call.get_preset() == climate::CLIMATE_PRESET_NONE) {
      ESP_LOGV(TAG, "Setting preset to: None");
      this->cmd[5] = (this->cmd[5] & 0xF0);  // Clear right nib for CLIMATE_PRESET_NONE
      // If the preset being cleared was CLIMATE_PRESET_ECO, turn off the eco setting
      if (this->preset == climate::CLIMATE_PRESET_ECO) {
        this->cmd[8] = 0x00; // Turn eco OFF
      }
    } else if (*call.get_preset() == climate::CLIMATE_PRESET_ECO) {
      ESP_LOGV(TAG, "Setting preset to: Eco");
      this->cmd[5] = (this->cmd[5] & 0xF0) + 0x01;
      this->cmd[8] = 0x40; // Turn eco ON
    } else if (*call.get_preset() == climate::CLIMATE_PRESET_BOOST) {
      ESP_LOGV(TAG, "Setting preset to: Powerful");
      this->cmd[5] = (this->cmd[5] & 0xF0) + 0x02;
    } else if (*call.get_preset() == climate::CLIMATE_PRESET_SLEEP) {
      ESP_LOGV(TAG, "Setting preset to: Quiet");
      this->cmd[5] = (this->cmd[5] & 0xF0) + 0x04;
    } else {
      ESP_LOGV(TAG, "Unsupported preset requested");
    }
  }

  send_command(this->cmd, CommandType::Normal, CTRL_HEADER);
}

void PanasonicACCNT::handle_cmd() {
  if (this->cmd.empty())
    return;

  if (millis() - this->last_packet_sent_ > CMD_INTERVAL) {
    ESP_LOGV(TAG, "Sending command");
    send_command(this->cmd, CommandType::Normal, CTRL_HEADER);
    this->cmd.clear();  // Clear command once sent
  }
}

void PanasonicACCNT::handle_poll() {
  if (millis() - this->last_packet_received_ > POLL_INTERVAL && !this->waiting_for_response_) {
    ESP_LOGV(TAG, "Polling AC");
    send_command(CMD_POLL, CommandType::Normal, POLL_HEADER);
  }
}

void PanasonicACCNT::send_command(std::vector<uint8_t> command, CommandType type, uint8_t header) {
  uint8_t counter = 0;
  if (type == CommandType::Normal) {
    this->waiting_for_response_ = true;
    counter = this->data[6] + 1;  // Increment counter
  }

  // Build the packet
  std::vector<uint8_t> packet;
  packet.push_back(header);             // Header
  packet.push_back(command.size());     // Length
  packet.insert(packet.end(), command.begin(), command.end());  // Command
  packet.push_back(counter);            // Counter
  packet.push_back(0x00);               // Checksum (initially 0)

  // Calculate checksum
  uint8_t checksum = 0;
  for (uint8_t byte : packet) {
    checksum += byte;
  }
  packet[packet.size() - 1] = checksum;

  send_packet(packet, type);
}

void PanasonicACCNT::send_packet(const std::vector<uint8_t> &packet, CommandType type) {
  this->last_packet_sent_ = millis();

  if (type == CommandType::Normal) {
    this->log_packet(packet, true);
  }

  this->write_array(packet); // Use write_array
}

bool PanasonicACCNT::verify_packet() {
  if (this->rx_buffer_.empty()) {
    ESP_LOGW(TAG, "Received empty packet");
    return false;
  }

  // Verify header
  if (this->rx_buffer_[0] != CTRL_HEADER && this->rx_buffer_[0] != POLL_HEADER) {
    ESP_LOGW(TAG, "Invalid header: 0x%02X", this->rx_buffer_[0]);
    return false;
  }

  // Verify length
  if (this->rx_buffer_[1] != this->rx_buffer_.size() - 4) { // Length byte doesn't include header, length byte, counter, checksum
    ESP_LOGW(TAG, "Invalid length: 0x%02X, actual: 0x%02X", this->rx_buffer_[1], this->rx_buffer_.size() - 4);
    return false;
  }

  // Verify checksum
  uint8_t checksum = 0;
  for (uint8_t byte : this->rx_buffer_) {
    checksum += byte;
  }
  if (checksum != 0) {
    ESP_LOGW(TAG, "Invalid checksum: 0x%02X", checksum);
    return false;
  }

  return true;
}

void PanasonicACCNT::handle_packet() {
  // Update internal data
  this->data = this->rx_buffer_;

  // Update AC mode
  this->mode = this->determine_mode(this->rx_buffer_[0]);

  // Update target temperature
  this->update_target_temperature(this->rx_buffer_[1]); // Corrected to update_target_temperature

  // Update fan mode
  this->fan_mode = this->determine_fan_mode(this->rx_buffer_[3], this->rx_buffer_[5]); // Pass both bytes

  // Update swing modes
  std::string verticalSwing = this->determine_vertical_swing(this->rx_buffer_[4]);
  std::string horizontalSwing = this->determine_horizontal_swing(this->rx_buffer_[4]);

  this->update_swing_vertical(verticalSwing);
  this->update_swing_horizontal(horizontalSwing);

  // Update presets and switches based on bits in byte 5 and 8
  // Byte 5 contains bits for Powerful, Quiet/Sleep, Econavi
  // Byte 8 contains bit for Eco
  bool nanoex = this->determine_preset_nanoex(this->rx_buffer_[5]);
  bool eco = this->determine_preset_eco(this->rx_buffer_[8]);
  bool econavi = this->determine_preset_econavi(this->rx_buffer_[5]);
  bool mild_dry = this->determine_preset_mild_dry(this->rx_buffer_[5]);

  this->update_nanoex(nanoex);
  this->update_eco(eco);
  this->update_econavi(econavi);
  this->update_mild_dry(mild_dry);

  // Determine preset from current state
  if (eco) {
      this->preset = climate::CLIMATE_PRESET_ECO;
  } else if (econavi) {
      this->preset = climate::CLIMATE_PRESET_ECO; // Or another specific preset for Econavi if applicable
  } else if (this->fan_mode == climate::CLIMATE_FAN_QUIET) {
      this->preset = climate::CLIMATE_PRESET_SLEEP;
  } else if ((this->rx_buffer_[5] & 0x02) == 0x02) { // Powerful mode
      this->preset = climate::CLIMATE_PRESET_BOOST;
  } else {
      this->preset = climate::CLIMATE_PRESET_NONE;
  }

  // Current Temperature sensor (if AC reports it, otherwise use provided sensor)
  if (this->rx_buffer_.size() > 22) { // Check if packet has enough bytes for current temp
    int8_t current_temp = this->rx_buffer_[22] / TEMPERATURE_STEP; // Assuming current temperature is at byte 22
    if (current_temp >= MIN_TEMPERATURE && current_temp <= MAX_TEMPERATURE) {
      this->update_current_temperature(current_temp);
    } else {
      ESP_LOGW(TAG, "Invalid current temperature received: %d. Using configured sensor.", current_temp);
    }
  }

  // Outside temperature sensor
  if (this->rx_buffer_.size() > 24) { // Check if packet has enough bytes for outside temp
    int8_t outside_temp = this->rx_buffer_[24] / TEMPERATURE_STEP; // Assuming outside temperature is at byte 24
    if (outside_temp >= -127 && outside_temp <= 127) { // Assuming reasonable range for outside temp
      this->update_outside_temperature(outside_temp);
    } else {
      ESP_LOGW(TAG, "Invalid outside temperature received: %d", outside_temp);
    }
  }

  // Inside temperature sensor
  if (this->rx_buffer_.size() > 26) { // Check if packet has enough bytes for inside temp
    int8_t inside_temp = this->rx_buffer_[26] / TEMPERATURE_STEP; // Assuming inside temperature is at byte 26
    if (inside_temp >= -127 && inside_temp <= 127) { // Assuming reasonable range for inside temp
      this->update_inside_temperature(inside_temp);
    } else {
      ESP_LOGW(TAG, "Invalid inside temperature received: %d", inside_temp);
    }
  }
  
  // Power Consumption sensor
  if (this->rx_buffer_.size() > 30) { // Check if packet has enough bytes for power consumption
    int16_t power_consumption = this->determine_power_consumption((int8_t)this->rx_buffer_[28], (int8_t)this->rx_buffer_[29], (int8_t)this->rx_buffer_[30]);
    this->update_current_power_consumption(power_consumption);
  }


  this->publish_state();
}

climate::ClimateMode PanasonicACCNT::determine_mode(uint8_t mode) {
  uint8_t nib1 = (mode >> 4) & 0x0F; // Left nib for mode
  uint8_t nib2 = mode & 0x0F;        // Right nib for AC on/off

  if (nib2 == 0x00) { // Off
    return climate::CLIMATE_MODE_OFF;
  }

  switch (nib1) {
    case 0x00: // Auto
      return climate::CLIMATE_MODE_HEAT_COOL;
    case 0x01: // Dry
      return climate::CLIMATE_MODE_DRY;
    case 0x02: // Cool
      return climate::CLIMATE_MODE_COOL;
    case 0x03: // Heat
      return climate::CLIMATE_MODE_HEAT;
    case 0x04: // Fan Only
      return climate::CLIMATE_MODE_FAN_ONLY;
    default:
      return climate::CLIMATE_MODE_OFF; // Should not happen
  }
}

climate::ClimateFanMode PanasonicACCNT::determine_fan_mode(uint8_t fan_mode_byte_3, uint8_t fan_mode_byte_5) {
  uint8_t fan_speed_setting = fan_mode_byte_3;
  uint8_t preset_setting = fan_mode_byte_5 & 0x0F; // Get the right nibble for preset

  if (preset_setting == 0x04) { // Quiet/Sleep mode
      return climate::CLIMATE_FAN_QUIET;
  }

  switch (fan_speed_setting) {
    case 0xA0:
      return climate::CLIMATE_FAN_AUTO;
    case 0x3C:
      return climate::CLIMATE_FAN_DIFFUSE;
    case 0x40:
      return climate::CLIMATE_FAN_LOW;
    case 0x60:
      return climate::CLIMATE_FAN_MEDIUM;
    case 0x80:
      return climate::CLIMATE_FAN_HIGH;
    case 0xC0:
      return climate::CLIMATE_FAN_FOCUS;
    default:
      return climate::CLIMATE_FAN_AUTO; // Default to auto
  }
}

std::string PanasonicACCNT::determine_vertical_swing(uint8_t swing) {
  uint8_t vertical_swing_nib = (swing >> 4) & 0x0F; // Left nib for vertical swing

  if (vertical_swing_nib == 0xA) { // Auto
    return "auto";
  }
  // No explicit positions for vertical swing in this protocol, usually auto or fixed
  // If specific positions are needed, they would be defined here.
  return "fixed"; // Assume fixed if not auto
}

std::string PanasonicACCNT::determine_horizontal_swing(uint8_t swing) {
  uint8_t horizontal_swing_nib = swing & 0x0F; // Right nib for horizontal swing

  if (horizontal_swing_nib == 0xD) { // Auto
    return "auto";
  }
  // No explicit positions for horizontal swing in this protocol, usually auto or fixed
  // If specific positions are needed, they would be defined here.
  return "fixed"; // Assume fixed if not auto
}

climate::ClimatePreset PanasonicACCNT::determine_preset(uint8_t preset) {
  uint8_t preset_nib = preset & 0x0F; // Right nib for preset

  switch (preset_nib) {
    case 0x00:
      return climate::CLIMATE_PRESET_NONE;
    case 0x01: // Eco
      return climate::CLIMATE_PRESET_ECO;
    case 0x02: // Powerful / Boost
      return climate::CLIMATE_PRESET_BOOST;
    case 0x04: // Quiet / Sleep
      return climate::CLIMATE_PRESET_SLEEP;
    default:
      return climate::CLIMATE_PRESET_NONE; // Default to none
  }
}

bool PanasonicACCNT::determine_preset_nanoex(uint8_t preset) {
  // NanoeX is not explicitly controlled by a preset in this protocol version from what I can see.
  // This might be controlled by a separate bit or not available via this module.
  // Returning false for now, unless specific bit is identified.
  return false;
}

bool PanasonicACCNT::determine_preset_eco(uint8_t byte_8) {
    return (byte_8 & 0x40) == 0x40; // Bit 6 in byte 8 for eco mode
}

bool PanasonicACCNT::determine_preset_econavi(uint8_t preset) {
    return (preset & 0x08) == 0x08; // Bit 3 in byte 5 for econavi
}

bool PanasonicACCNT::determine_preset_mild_dry(uint8_t preset) {
    // Assuming Mild Dry is a bit in the preset byte (byte 5) based on common Panasonic AC features
    // This is a placeholder, actual bit needs to be confirmed if different.
    return false; // Placeholder, need actual bit definition
}

int16_t PanasonicACCNT::determine_power_consumption(int8_t byte_28, int8_t byte_29, int8_t byte_30) {
    // This is a placeholder implementation based on typical power consumption byte structures.
    // The exact calculation may vary depending on the AC model and protocol.
    // Assuming byte_28 is high byte, byte_29 is mid byte, byte_30 is low byte for 24-bit value or similar.
    // A more accurate interpretation would require specific protocol documentation.
    int32_t raw_value = (static_cast<uint8_t>(byte_28) << 16) | (static_cast<uint8_t>(byte_29) << 8) | static_cast<uint8_t>(byte_30);
    // Assuming the value might be in watts, and a simple scaling factor if needed.
    // For example, if it's in 0.1W units:
    // return static_cast<int16_t>(raw_value / 10);
    return static_cast<int16_t>(raw_value); // Returning raw value for now, adjust as needed.
}


void PanasonicACCNT::on_horizontal_swing_change(const std::string &swing) {
  if (this->state_ != ACState::Ready)
    return;

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->data;
  }

  this->horizontal_swing_state_ = swing;

  if (swing == "auto") {
    ESP_LOGV(TAG, "Setting horizontal swing to auto");
    this->cmd[4] = (this->cmd[4] & 0xF0) | 0x0D;
  } else {
    // This implementation does not support specific fixed horizontal swing positions.
    // If needed, more logic here to map string positions to byte values.
    ESP_LOGW(TAG, "Unsupported horizontal swing position: %s", swing.c_str());
  }

  send_command(this->cmd, CommandType::Normal, CTRL_HEADER);
}

void PanasonicACCNT::on_vertical_swing_change(const std::string &swing) {
  if (this->state_ != ACState::Ready)
    return;

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->data;
  }

  this->vertical_swing_state_ = swing;

  if (swing == "auto") {
    ESP_LOGV(TAG, "Setting vertical swing to auto");
    this->cmd[4] = (this->cmd[4] & 0x0F) | 0xA0;
  } else {
    // This implementation does not support specific fixed vertical swing positions.
    // If needed, more logic here to map string positions to byte values.
    ESP_LOGW(TAG, "Unsupported vertical swing position: %s", swing.c_str());
  }

  send_command(this->cmd, CommandType::Normal, CTRL_HEADER);
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
    ESP_LOGV(TAG, "Turning nanoeX on");
    // Placeholder: Need to identify the byte and bit for nanoeX control in CN-CNT protocol
    // For now, assuming byte 5 bit for nanoeX (example, might be different)
    this->cmd[5] |= 0x10; // Example bit
  } else {
    ESP_LOGV(TAG, "Turning nanoeX off");
    this->cmd[5] &= ~0x10; // Example bit
  }
  send_command(this->cmd, CommandType::Normal, CTRL_HEADER);
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

  send_command(this->cmd, CommandType::Normal, CTRL_HEADER);
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
    ESP_LOGV(TAG, "Turning mild dry mode on");
    // Placeholder: Need to identify the byte and bit for mild dry control in CN-CNT protocol
    this->cmd[5] |= 0x20; // Example bit, might be different
  } else {
    ESP_LOGV(TAG, "Turning mild dry mode off");
    this->cmd[5] &= ~0x20; // Example bit
  }
  send_command(this->cmd, CommandType::Normal, CTRL_HEADER);
}


}  // namespace CNT
}  // namespace panasonic_ac
}  // namespace esphome
