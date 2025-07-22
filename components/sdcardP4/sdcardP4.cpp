#include "sdcardP4.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sdcardP4 {

static const char *const TAG = "sdcardP4";

// Commandes SD
#define CMD0    0   // GO_IDLE_STATE
#define CMD1    1   // SEND_OP_COND
#define CMD8    8   // SEND_IF_COND
#define CMD9    9   // SEND_CSD
#define CMD10   10  // SEND_CID
#define CMD12   12  // STOP_TRANSMISSION
#define CMD16   16  // SET_BLOCKLEN
#define CMD17   17  // READ_SINGLE_BLOCK
#define CMD18   18  // READ_MULTIPLE_BLOCK
#define CMD23   23  // SET_BLOCK_COUNT
#define CMD24   24  // WRITE_BLOCK
#define CMD25   25  // WRITE_MULTIPLE_BLOCK
#define CMD41   41  // APP_SEND_OP_COND
#define CMD55   55  // APP_CMD
#define CMD58   58  // READ_OCR

void SdcardP4Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sdcardP4...");
  
  if (this->cs_pin_ != nullptr) {
    this->cs_pin_->setup();
    this->cs_pin_->digital_write(true);  // Désélectionner la carte
  }
  
  // Initialiser SPI
  this->spi_setup();
  
  // Attendre que la carte soit prête
  delay(10);
  
  // Initialiser la carte SD
  if (this->initialize_card()) {
    ESP_LOGI(TAG, "SD Card initialized successfully");
    this->card_initialized_ = true;
  } else {
    ESP_LOGE(TAG, "Failed to initialize SD Card");
    this->card_initialized_ = false;
  }
}

void SdcardP4Component::loop() {
  // Vérifier périodiquement si la carte est présente
  if (!this->is_card_present() && this->card_initialized_) {
    ESP_LOGW(TAG, "SD Card removed");
    this->card_initialized_ = false;
  }
}

void SdcardP4Component::dump_config() {
  ESP_LOGCONFIG(TAG, "sdcardP4:");
  LOG_PIN("  CS Pin: ", this->cs_pin_);
  ESP_LOGCONFIG(TAG, "  Card initialized: %s", YESNO(this->card_initialized_));
  if (this->card_initialized_) {
    ESP_LOGCONFIG(TAG, "  Card size: %u MB", this->card_size_ / (1024 * 1024));
  }
}

float SdcardP4Component::get_setup_priority() const {
  return setup_priority::HARDWARE;
}

bool SdcardP4Component::initialize_card() {
  if (this->cs_pin_ == nullptr) {
    ESP_LOGE(TAG, "CS pin not configured");
    return false;
  }

  // Envoyer les pulses d'initialisation
  this->send_initial_clock();
  
  // CMD0 - Reset la carte
  this->select_card();
  uint8_t response = this->send_command(CMD0, 0);
  this->deselect_card();
  
  if (response != 0x01) {
    ESP_LOGE(TAG, "CMD0 failed, response: 0x%02X", response);
    return false;
  }
  
  // CMD8 - Vérifier si c'est une carte SDHC/SDXC
  this->select_card();
  response = this->send_command(CMD8, 0x01AA);
  this->deselect_card();
  
  bool is_sdhc = (response == 0x01);
  
  // Initialisation avec ACMD41
  uint32_t timeout = millis() + 1000;
  do {
    this->select_card();
    this->send_command(CMD55, 0);  // APP_CMD
    response = this->send_command(CMD41, is_sdhc ? 0x40000000 : 0);
    this->deselect_card();
    
    if (millis() > timeout) {
      ESP_LOGE(TAG, "Card initialization timeout");
      return false;
    }
    delay(10);
  } while (response != 0x00);
  
  // Définir la taille de bloc à 512 bytes
  this->select_card();
  response = this->send_command(CMD16, 512);
  this->deselect_card();
  
  if (response != 0x00) {
    ESP_LOGE(TAG, "CMD16 failed, response: 0x%02X", response);
    return false;
  }
  
  ESP_LOGI(TAG, "SD Card initialization successful");
  return true;
}

uint8_t SdcardP4Component::send_command(uint8_t cmd, uint32_t arg) {
  uint8_t crc = 0xFF;
  if (cmd == CMD0) crc = 0x95;
  if (cmd == CMD8) crc = 0x87;
  
  // Envoyer la commande
  this->write_byte(0x40 | cmd);
  this->write_byte((arg >> 24) & 0xFF);
  this->write_byte((arg >> 16) & 0xFF);
  this->write_byte((arg >> 8) & 0xFF);
  this->write_byte(arg & 0xFF);
  this->write_byte(crc);
  
  // Attendre la réponse (max 8 tentatives)
  uint8_t response;
  for (int i = 0; i < 8; i++) {
    response = this->read_byte();
    if (!(response & 0x80)) {
      return response;
    }
  }
  return 0xFF;  // Timeout
}

void SdcardP4Component::select_card() {
  if (this->cs_pin_ != nullptr) {
    this->cs_pin_->digital_write(false);
  }
}

void SdcardP4Component::deselect_card() {
  if (this->cs_pin_ != nullptr) {
    this->cs_pin_->digital_write(true);
  }
  this->read_byte();  // Clock supplémentaire
}

void SdcardP4Component::send_initial_clock() {
  this->deselect_card();
  for (int i = 0; i < 10; i++) {
    this->read_byte();  // Envoie des pulses de clock
  }
}

bool SdcardP4Component::wait_ready() {
  uint32_t timeout = millis() + 500;
  while (millis() < timeout) {
    if (this->read_byte() == 0xFF) {
      return true;
    }
  }
  return false;
}

bool SdcardP4Component::is_card_present() {
  this->select_card();
  uint8_t response = this->send_command(CMD58, 0);  // READ_OCR
  this->deselect_card();
  return response == 0x00;
}

bool SdcardP4Component::read_sector(uint32_t sector, uint8_t *buffer) {
  if (!this->card_initialized_) {
    return false;
  }
  
  this->select_card();
  uint8_t response = this->send_command(CMD17, sector * 512);
  
  if (response != 0x00) {
    this->deselect_card();
    return false;
  }
  
  // Attendre le token de début de données
  uint32_t timeout = millis() + 100;
  while (millis() < timeout) {
    uint8_t token = this->read_byte();
    if (token == 0xFE) {
      // Lire les données
      for (int i = 0; i < 512; i++) {
        buffer[i] = this->read_byte();
      }
      // Lire le CRC (2 bytes)
      this->read_byte();
      this->read_byte();
      this->deselect_card();
      return true;
    }
  }
  
  this->deselect_card();
  return false;
}

bool SdcardP4Component::write_sector(uint32_t sector, const uint8_t *buffer) {
  if (!this->card_initialized_) {
    return false;
  }
  
  this->select_card();
  uint8_t response = this->send_command(CMD24, sector * 512);
  
  if (response != 0x00) {
    this->deselect_card();
    return false;
  }
  
  // Envoyer le token de début de données
  this->write_byte(0xFE);
  
  // Envoyer les données
  for (int i = 0; i < 512; i++) {
    this->write_byte(buffer[i]);
  }
  
  // Envoyer un CRC factice
  this->write_byte(0xFF);
  this->write_byte(0xFF);
  
  // Vérifier la réponse
  response = this->read_byte() & 0x1F;
  this->deselect_card();
  
  return (response == 0x05);  // Data accepted
}

uint32_t SdcardP4Component::get_card_size() {
  return this->card_size_;
}

}  // namespace sdcardP4
}  // namespace esphome
