#include "kaeltebringer.h"

namespace esphome {
namespace kaeltebringer {

static const char *const TAG = "kaeltebringer";

void KaeltebringerClimate::setup() {
  // This will be called by App.setup()
}

void KaeltebringerClimate::set_beep_enabled(bool enabled) {
  this->beep_enabled_ = enabled;
  this->is_changed = true;
}

void KaeltebringerClimate::set_display_enabled(bool enabled) {
  this->display_enabled_ = enabled;
  this->is_changed = true;
}

void KaeltebringerClimate::set_current_temperature(float current_temperature) {
  if (this->current_temperature == current_temperature) return;
  this->current_temperature = current_temperature;
  this->is_changed = true;
}

void KaeltebringerClimate::set_custom_fan_mode(const std::string &fan_mode) {
  if (this->custom_fan_mode_local_ == fan_mode) return;
  this->custom_fan_mode_local_ = fan_mode;
  this->is_changed = true;
}

void KaeltebringerClimate::set_mode(esphome::climate::ClimateMode mode) {
  if (this->mode == mode) return;
  this->mode = mode;
  this->is_changed = true;
}

void KaeltebringerClimate::set_swing_mode(esphome::climate::ClimateSwingMode swing_mode) {
  if (this->swing_mode == swing_mode) return;
  this->swing_mode = swing_mode;
  this->is_changed = true;
}

void KaeltebringerClimate::set_target_temperature(float target_temperature) {
  if (this->target_temperature == target_temperature) return;
  this->target_temperature = target_temperature;
  this->is_changed = true;
}

void KaeltebringerClimate::build_set_cmd(get_cmd_resp_t *get_cmd_resp) {
  memcpy(m_set_cmd.raw, set_cmd_base, sizeof(m_set_cmd.raw));

  m_set_cmd.data.power = get_cmd_resp->data.power;
  m_set_cmd.data.off_timer_en = 0;
  m_set_cmd.data.on_timer_en = 0;
  m_set_cmd.data.beep = int(this->beep_enabled_);
  m_set_cmd.data.disp = this->display_enabled_ ? 1 : 0;
  m_set_cmd.data.eco = 0;

  switch (get_cmd_resp->data.mode) {
    case 0x01: m_set_cmd.data.mode = 0x03; break;
    case 0x03: m_set_cmd.data.mode = 0x02; break;
    case 0x02: m_set_cmd.data.mode = 0x07; break;
    case 0x04: m_set_cmd.data.mode = 0x01; break;
    case 0x05: m_set_cmd.data.mode = 0x08; break;
  }

  m_set_cmd.data.turbo = get_cmd_resp->data.turbo;
  m_set_cmd.data.mute = get_cmd_resp->data.mute;
  m_set_cmd.data.temp = 15 - get_cmd_resp->data.temp;

  switch (get_cmd_resp->data.fan) {
    case 0x00: m_set_cmd.data.fan = 0x00; break;
    case 0x01: m_set_cmd.data.fan = 0x02; break;
    case 0x04: m_set_cmd.data.fan = 0x06; break;
    case 0x02: m_set_cmd.data.fan = 0x03; break;
    case 0x05: m_set_cmd.data.fan = 0x07; break;
    case 0x03: m_set_cmd.data.fan = 0x05; break;
  }

  m_set_cmd.data.vswing = get_cmd_resp->data.vswing ? 0x07 : 0x00;
  m_set_cmd.data.hswing = get_cmd_resp->data.hswing;
  m_set_cmd.data.half_degree = 0;

  uint8_t xor_byte = 0;
  for (int i = 0; i < sizeof(m_set_cmd.raw) - 1; i++)
    xor_byte ^= m_set_cmd.raw[i];
  m_set_cmd.raw[sizeof(m_set_cmd.raw) - 1] = xor_byte;
}

int KaeltebringerClimate::read_data_line(int readch, uint8_t *buffer, int len) {
  static int pos = 0;
  static bool wait_len = false;
  static int skipch = 0;

  if (readch >= 0) {
    if (readch == 0xBB && skipch == 0 && !wait_len) {
      pos = 0;
      skipch = 3;
      wait_len = true;
      buffer[pos++] = readch;
    } else if (skipch == 0 && wait_len) {
      buffer[pos++] = readch;
      skipch = readch + 1;
      ESP_LOGD(TAG, "len: %d", readch);
      wait_len = false;
    } else if (skipch > 0) {
      buffer[pos++] = readch;
      if (--skipch == 0 && !wait_len) return pos;
    }
  }
  return -1;
}

void KaeltebringerClimate::control(const climate::ClimateCall &call) {
  // Mode change
  if (call.get_mode().has_value()) {
    climate::ClimateMode mode = *call.get_mode();
    get_cmd_resp_t get_resp = {0};
    memcpy(get_resp.raw, m_get_cmd_resp.raw, sizeof(get_resp.raw));
    get_resp.data.power = (mode == climate::CLIMATE_MODE_OFF) ? 0x00 : 0x01;
    if (mode != climate::CLIMATE_MODE_OFF) {
      switch (mode) {
        case climate::CLIMATE_MODE_COOL:       get_resp.data.mode = 0x01; break;
        case climate::CLIMATE_MODE_DRY:        get_resp.data.mode = 0x03; break;
        case climate::CLIMATE_MODE_FAN_ONLY:   get_resp.data.mode = 0x02; break;
        case climate::CLIMATE_MODE_HEAT:       get_resp.data.mode = 0x04; break;
        case climate::CLIMATE_MODE_AUTO:       get_resp.data.mode = 0x05; break;
        default:                              get_resp.data.mode = 0x01; break;
      }
    }
    build_set_cmd(&get_resp);
    ready_to_send_set_cmd_flag = true;
  }
  // Target temperature
  if (call.get_target_temperature().has_value()) {
    float t = *call.get_target_temperature();
    get_cmd_resp_t get_resp = {0};
    memcpy(get_resp.raw, m_get_cmd_resp.raw, sizeof(get_resp.raw));
    get_resp.data.temp = uint8_t(t) - 16;
    build_set_cmd(&get_resp);
    ready_to_send_set_cmd_flag = true;
  }
  // Swing
  if (call.get_swing_mode().has_value()) {
    esphome::climate::ClimateSwingMode swing = *call.get_swing_mode();
    get_cmd_resp_t get_resp = {0};
    memcpy(get_resp.raw, m_get_cmd_resp.raw, sizeof(get_resp.raw));
    switch (swing) {
      case climate::CLIMATE_SWING_OFF:       get_resp.data.hswing = 0; get_resp.data.vswing = 0; break;
      case climate::CLIMATE_SWING_BOTH:      get_resp.data.hswing = 1; get_resp.data.vswing = 1; break;
      case climate::CLIMATE_SWING_VERTICAL:  get_resp.data.hswing = 0; get_resp.data.vswing = 1; break;
      case climate::CLIMATE_SWING_HORIZONTAL:get_resp.data.hswing = 1; get_resp.data.vswing = 0; break;
      default:                              get_resp.data.hswing = 0; get_resp.data.vswing = 0; break;
    }
    build_set_cmd(&get_resp);
    ready_to_send_set_cmd_flag = true;
  }
  // Custom fan mode
  if (const char *cstr = call.get_custom_fan_mode()) {
    std::string fan = cstr;
    this->set_custom_fan_mode(fan);
    get_cmd_resp_t get_resp = {0};
    memcpy(get_resp.raw, m_get_cmd_resp.raw, sizeof(get_resp.raw));
    get_resp.data.turbo = 0;
    get_resp.data.mute = 0;
    if (fan == "Turbo")         { get_resp.data.fan = 0x03; get_resp.data.turbo = 1; }
    else if (fan == "Mute")     { get_resp.data.fan = 0x01; get_resp.data.mute = 1; }
    else if (fan == "Automatic"){ get_resp.data.fan = 0x00; }
    else if (fan == "1")        { get_resp.data.fan = 0x01; }
    else if (fan == "2")        { get_resp.data.fan = 0x04; }
    else if (fan == "3")        { get_resp.data.fan = 0x02; }
    else if (fan == "4")        { get_resp.data.fan = 0x05; }
    else if (fan == "5")        { get_resp.data.fan = 0x03; }
    build_set_cmd(&get_resp);
    ready_to_send_set_cmd_flag = true;
  }
  // Finally send or request
  update();
}

bool KaeltebringerClimate::is_valid_xor(uint8_t *buffer, int len) {
  uint8_t xor_byte = 0;
  for (int i = 0; i < len - 1; i++)
    xor_byte ^= buffer[i];
  if (xor_byte == buffer[len - 1]) return true;
  ESP_LOGW(TAG, "No valid xor crc %02X (calculated %02X)", buffer[len - 1], xor_byte);
  return false;
}

void KaeltebringerClimate::print_hex_str(uint8_t *buffer, int len) {
  char str[250] = {0}, *p = str;
  if (len * 2 > sizeof(str)) ESP_LOGE(TAG, "too long byte data");
  for (int i = 0; i < len; i++)
    p += sprintf(p, "%02X ", buffer[i]);
  ESP_LOGD(TAG, "%s", str);
}

void KaeltebringerClimate::update() {
  uint8_t req_cmd[] = {0xBB, 0x00, 0x01, 0x04, 0x02, 0x01, 0x00, 0xBD};
  if (ready_to_send_set_cmd_flag) {
    ready_to_send_set_cmd_flag = false;
    write_array(m_set_cmd.raw, sizeof(m_set_cmd.raw));
  } else {
    write_array(req_cmd, sizeof(req_cmd));
  }
}

void KaeltebringerClimate::loop() {
  const int max_len = 100;
  static uint8_t buffer[max_len];
  while (available()) {
    int len = read_data_line(read(), buffer, max_len);
    if (len == sizeof(m_get_cmd_resp) && buffer[3] == 0x04) {
      memcpy(m_get_cmd_resp.raw, buffer, len);
      print_hex_str(buffer, len);
      if (!is_valid_xor(buffer, len)) continue;
      float curr_temp = (((buffer[17] << 8) | buffer[18]) / 374.0f - 32.0f) / 1.8f;
      if (m_get_cmd_resp.data.power == 0x00) this->set_mode(climate::CLIMATE_MODE_OFF);
      else {
        switch (m_get_cmd_resp.data.mode) {
          case 0x01: this->set_mode(climate::CLIMATE_MODE_COOL); break;
          case 0x03: this->set_mode(climate::CLIMATE_MODE_DRY); break;
          case 0x02: this->set_mode(climate::CLIMATE_MODE_FAN_ONLY); break;
          case 0x04: this->set_mode(climate::CLIMATE_MODE_HEAT); break;
          case 0x05: this->set_mode(climate::CLIMATE_MODE_AUTO); break;
        }
      }
      // Custom fan
      if (m_get_cmd_resp.data.turbo) this->set_custom_fan_mode("Turbo");
      else if (m_get_cmd_resp.data.mute) this->set_custom_fan_mode("Mute");
      else {
        switch (m_get_cmd_resp.data.fan) {
          case 0x00: this->set_custom_fan_mode("Automatic"); break;
          case 0x01: this->set_custom_fan_mode("1"); break;
          case 0x04: this->set_custom_fan_mode("2"); break;
          case 0x02: this->set_custom_fan_mode("3"); break;
          case 0x05: this->set_custom_fan_mode("4"); break;
          case 0x03: this->set_custom_fan_mode("5"); break;
        }
      }
      // Swing
      if (m_get_cmd_resp.data.hswing && m_get_cmd_resp.data.vswing)
        this->set_swing_mode(climate::CLIMATE_SWING_BOTH);
      else if (!m_get_cmd_resp.data.hswing && !m_get_cmd_resp.data.vswing)
        this->set_swing_mode(climate::CLIMATE_SWING_OFF);
      else if (m_get_cmd_resp.data.vswing)
        this->set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
      else if (m_get_cmd_resp.data.hswing)
        this->set_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);

      this->set_target_temperature(float(m_get_cmd_resp.data.temp + 16));
      this->set_current_temperature(curr_temp);
      if (this->is_changed) this->publish_state();
    }
  }
}

}  // namespace kaeltebringer
}  // namespace esphome
