#include "sdcardP4.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sdcardP4 {

static const char *const TAG = "sdcardP4";

void SdcardP4Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sdcardP4...");
  
  if (this->cs_pin_ == nullptr) {
    ESP_LOGE(TAG, "CS pin not configured!");
    this->mark_failed();
    return;
  }
  
  this->cs_pin_->setup();
  this->cs_pin_->digital_write(true);  // Deselect
  
  // Test simple
  this->spi_setup();
  delay(100);
  
  if (this->initialize_card()) {
    this->card_ready_ = true;
    ESP_LOGI(TAG, "SD Card ready");
  } else {
    ESP_LOGW(TAG, "SD Card initialization failed");
  }
}

void SdcardP4Component::dump_config() {
  ESP_LOGCONFIG(TAG, "sdcardP4:");
  LOG_PIN("  CS Pin: ", this->cs_pin_);
  ESP_LOGCONFIG(TAG, "  Status: %s", this->card_ready_ ? "Ready" : "Not Ready");
}

bool SdcardP4Component::initialize_card() {
  if (this->cs_pin_ == nullptr) return false;
  
  // Test basique SPI
  this->select_card();
  uint8_t response = this->send_command(0, 0);  // CMD0
  this->deselect_card();
  
  return (response != 0xFF);  // Si pas de réponse = pas de carte
}

void SdcardP4Component::select_card() {
  if (this->cs_pin_) this->cs_pin_->digital_write(false);
}

void SdcardP4Component::deselect_card() {
  if (this->cs_pin_) this->cs_pin_->digital_write(true);
}

uint8_t SdcardP4Component::send_command(uint8_t cmd, uint32_t arg) {
  this->write_byte(0x40 | cmd);
  this->write_byte((arg >> 24) & 0xFF);
  this->write_byte((arg >> 16) & 0xFF);
  this->write_byte((arg >> 8) & 0xFF);
  this->write_byte(arg & 0xFF);
  this->write_byte(0x95);  // CRC for CMD0
  
  for (int i = 0; i < 8; i++) {
    uint8_t response = this->read_byte();
    if (!(response & 0x80)) return response;
  }
  return 0xFF;
}

}  // namespace sdcardP4
}  // namespace esphome

