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
    ESP_LOGV(TAG, "Copying data to cmd: Main");
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
    ESP_LOGV(TAG, "Requested temperature change");
    this->cmd[1] = *call.get_target_temperature() / TEMPERATURE_STEP;
  }
  
  if (call.get_fan_mode().has_value()) {
    ESP_LOGV(TAG, "Requested fan mode change");

    if (*call.get_fan_mode() == climate::CLIMATE_FAN_QUIET) {
      this->cmd[3] = 0xA0; // Set fan to Auto for Quiet mode
      this->cmd[5] = (this->cmd[5] & 0xF0) + 0x04; // Set Quiet bit in byte 5
    } else {
      // Clear the Quiet bit (0x04) in byte 5 when a non-Quiet fan mode is selected.
      // Preserve other bits (like 0x02 for Boost) if they are set in byte 5.
      this->cmd[5] = this->cmd[5] & (~0x04); 
      
      switch (*call.get_fan_mode()) {
        case climate::CLIMATE_FAN_AUTO:
          this->cmd[3] = 0xA0;
          break;
        case climate::CLIMATE_FAN_DIFFUSE:
          this->cmd[3] = 0x30;
          break;
        case climate::CLIMATE_FAN_LOW:
          this->cmd[3] = 0x40;
          break;
        case climate::CLIMATE_FAN_MEDIUM:
          this->cmd[3] = 0x50;
          break;
        case climate::CLIMATE_FAN_HIGH:
          this->cmd[3] = 0x60;
          break;
        case climate::CLIMATE_FAN_FOCUS:
          this->cmd[3] = 0x70;
          break;
        default:
          ESP_LOGW(TAG, "Unsupported fan mode requested");
          break;
      }
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

  if (call.get_preset().has_value()) {
    ESP_LOGV(TAG, "Requested preset change");
    this->cmd[3] = 0xA0; // Set fan mode to Auto (0xA0)
    switch (*call.get_preset()) {
      case climate::CLIMATE_PRESET_NONE:
        this->cmd[5] = (this->cmd[5] & 0xF0); // Clear right nibble of byte 5 (including Boost and Quiet)
        this->cmd[8] = 0x00; // Turn eco OFF
        this->preset = climate::CLIMATE_PRESET_NONE; // Optimistic update
        break;
      case climate::CLIMATE_PRESET_BOOST:
        this->cmd[5] = (this->cmd[5] & 0xF0) + 0x02; // Set right nibble of byte 5 to Boost
        this->cmd[8] = 0x00; // Turn eco OFF
        this->preset = climate::CLIMATE_PRESET_BOOST; // Optimistic update
        break;
      case climate::CLIMATE_PRESET_ECO:
        this->cmd[5] = (this->cmd[5] & 0xF0); // Clear right nibble of byte 5 (including Boost and Quiet)
        this->cmd[8] = 0x40; // Turn eco ON
        this->preset = climate::CLIMATE_PRESET_ECO; // Optimistic update
        break;
      default:
        ESP_LOGW(TAG, "Unsupported preset requested");
        break;
    }
    this->publish_state(); // Publish the climate component's state to reflect the optimistic preset

    // If Eco or None (from Eco) preset is involved, activate suppression
    if (*call.get_preset() == climate::CLIMATE_PRESET_ECO || *call.get_preset() == climate::CLIMATE_PRESET_NONE) {
      this->suppress_poll_update_for_eco_preset_ = true;
      this->suppress_poll_timeout_ = millis() + SUPPRESSION_DURATION_MS;
    }
  }
}

/*
 * Set the data array to the fields
 */
void PanasonicACCNT::set_data(bool set) {
  this->mode = determine_mode(this->data[0]);
  this->fan_mode = determine_fan_mode(this->data[3]); // determine_fan_mode now considers data[5]
  this->preset = determine_preset(this->data[5]); // determine_preset no longer considers 0x04 for Sleep

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
    ESP_LOGI(TAG, "Horizontal swing enable is: %s", this->horizontal_swing_enable_ ? "true" : "false");
    ESP_LOGI(TAG, "Vertical swing enable is: %s", this->vertical_swing_enable_ ? "true" : "false");
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
    // Always extract the polled data into a temporary vector first
    std::vector<uint8_t> temp_polled_data = std::vector<uint8_t>(this->rx_buffer_.begin() + 2, this->rx_buffer_.begin() + 12);

    // Store original data to restore after checks (if needed)
    std::vector<uint8_t> original_data = this->data; 
    this->data = temp_polled_data; // Temporarily make polled data active for determine_preset/eco

    bool should_publish_poll_state = true; // Flag to decide if we publish the polled state

    if (this->suppress_poll_update_for_eco_preset_) {
        // Check for timeout
        if (millis() > this->suppress_poll_timeout_) {
            ESP_LOGW(TAG, "Optimistic Eco/Preset suppression timed out. Publishing polled state.");
            this->suppress_poll_update_for_eco_preset_ = false; // Reset flag
        } else {
            // Determine the polled Eco and Preset states using the temporary data
            bool current_polled_eco_state = determine_eco(this->data[8]); // Uses temp_polled_data[8]
            climate::ClimatePreset current_polled_preset = determine_preset(this->data[5]); // Uses temp_polled_data[5] and temp_polled_data[8]

            if ((this->eco_state_ == current_polled_eco_state) && (this->preset == current_polled_preset)) {
                // Polled state matches optimistic state, so clear the flag and proceed
                this->suppress_poll_update_for_eco_preset_ = false;
                ESP_LOGD(TAG, "Poll confirmed optimistic Eco/Preset state, proceeding with update.");
            } else {
                // Polled state does not match optimistic state, suppress this update
                should_publish_poll_state = false;
                ESP_LOGD(TAG, "Suppressing poll update for Eco/Preset - polled state does not match optimistic.");
            }
        }
    }
    
    this->data = original_data; // Restore original data (important!)

    if (should_publish_poll_state) {
        this->data = temp_polled_data; // Assign the polled data to the actual data member for the real update
        this->set_data(true);
        this->publish_state();
        if (this->state_ != ACState::Ready)
            this->state_ = ACState::Ready;
    }
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

climate::ClimateFanMode PanasonicACCNT::determine_fan_mode(uint8_t fan_mode_byte) {
  // Check if the "Quiet" bit (0x04) is set in data[5].
  // This bit determines if Quiet mode is active, regardless of the fan_mode_byte.
  if ((this->data[5] & 0x04) == 0x04) {
    return climate::CLIMATE_FAN_QUIET;
  }

  // If Quiet bit is not set, then determine based on the actual fan mode byte.
  switch (fan_mode_byte) {
    case 0xA0:
      return climate::CLIMATE_FAN_AUTO;
    case 0x30:
      return climate::CLIMATE_FAN_DIFFUSE;
    case 0x40:
      return climate::CLIMATE_FAN_LOW;
    case 0x50:
      return climate::CLIMATE_FAN_MEDIUM;
    case 0x60:
      return climate::CLIMATE_FAN_HIGH;
    case 0x70:
      return climate::CLIMATE_FAN_FOCUS;
    default:
      ESP_LOGW(TAG, "Received unknown fan mode");
      return climate::CLIMATE_FAN_AUTO;
  }
}

std::string PanasonicACCNT::determine_vertical_swing(uint8_t swing) {
  uint8_t nib = (swing >> 4) & 0x0F;  // Left nib for vertical swing

  switch (nib) {
    case 0x0F:
      return "Auto";
    case 0x0E:
      return "Swing";
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
    case 0x00: // Unsupported
      ESP_LOGW(TAG, "Received unsupported vertical swing mode");
      return "Auto";
    default: // Unknown
      ESP_LOGW(TAG, "Received unknown vertical swing mode");
      return "Auto";
  }
}

std::string PanasonicACCNT::determine_horizontal_swing(uint8_t swing) {
  uint8_t nib = (swing >> 0) & 0x0F;  // Right nib for horizontal swing

  switch (nib) {
    case 0x0D:
      return "Auto";
    case 0x07:
      return "Swing";
    case 0x08:
      return "Wide";
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
    default: // Unknown
      ESP_LOGW(TAG, "Received unknown horizontal swing mode");
      return "unknown";
  }
}

climate::ClimatePreset PanasonicACCNT::determine_preset(uint8_t preset_byte) {
  // First, check if eco mode is active via data[8]
  if ((this->data[8] & 0x40) == 0x40) {
    return climate::CLIMATE_PRESET_ECO;
  }

  // Then check for Boost preset (0x02 in preset_byte).
  // Quiet (0x04) is now handled by determine_fan_mode, so it should not be here.
  if ((preset_byte & 0x02) == 0x02) { 
      return climate::CLIMATE_PRESET_BOOST;
  }
  
  // If neither Eco nor Boost is active, return NONE.
  return climate::CLIMATE_PRESET_NONE;
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
    ESP_LOGV(TAG, "Copying data to cmd: Vertical swing");
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
    ESP_LOGV(TAG, "Copying data to cmd: Horizontal swing");
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
    ESP_LOGV(TAG, "Copying data to cmd: NanoeX switch");
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
    ESP_LOGV(TAG, "Copying data to cmd: Eco switch");
    this->cmd = this->data;
  }

  this->eco_state_ = state; // Optimistically set the state of the Eco switch
  // Also update climate preset optimistically if Eco switch is linked to it
  if (state) {
      this->preset = climate::CLIMATE_PRESET_ECO;
  } else {
      this->preset = climate::CLIMATE_PRESET_NONE;
  }
  
  this->publish_state(); // Publish the climate component's optimistic state

  if (state) {
    ESP_LOGV(TAG, "Turning eco mode on");
    this->cmd[8] = 0x40;  // Set the byte corresponding to eco mode
  } else {
    ESP_LOGV(TAG, "Turning eco mode off");
    this->cmd[8] = 0x00;  // Clear the byte corresponding to eco mode
  }

  // Activate suppression for next poll
  this->suppress_poll_update_for_eco_preset_ = true;
  this->suppress_poll_timeout_ = millis() + SUPPRESSION_DURATION_MS;

  // Send the modified command immediately
  send_command(this->cmd, CommandType::Normal, CTRL_HEADER);
}

void PanasonicACCNT::on_econavi_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd: Econavi switch");
    this->cmd = this->data;
  }

  this->econavi_state_ = state;

  if (state) {
    ESP_LOGV(TAG, "Turning econavi mode on");
    this->cmd[5] = (this->cmd[5] & 0xF0) + 0x10; // Only set the bit, don't clear the entire byte
  } else {
    ESP_LOGV(TAG, "Turning econavi mode off");
    this->cmd[5] = (this->cmd[5] & 0xF0); // Clear the bit, don't clear the entire byte
  }

}

void PanasonicACCNT::on_mild_dry_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd: Mild dry switch");
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
