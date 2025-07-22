#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sdcardP4 {

class SdcardP4Component : public Component, public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_cs_pin(GPIOPin *cs_pin) { this->cs_pin_ = cs_pin; }

  // Méthodes pour lire la carte SD
  bool initialize_card();
  bool read_sector(uint32_t sector, uint8_t *buffer);
  bool write_sector(uint32_t sector, const uint8_t *buffer);
  uint32_t get_card_size();
  bool is_card_present();

 protected:
  GPIOPin *cs_pin_{nullptr};
  bool card_initialized_{false};
  uint32_t card_size_{0};

  // Commandes SD
  uint8_t send_command(uint8_t cmd, uint32_t arg);
  void select_card();
  void deselect_card();
  bool wait_ready();
  void send_initial_clock();
};

}  // namespace sdcardP4
}  // namespace esphome
