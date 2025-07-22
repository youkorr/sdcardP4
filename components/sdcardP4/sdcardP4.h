#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sdcardP4 {

class SdcardP4Component : public Component, public spi::SPIDevice<> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_cs_pin(GPIOPin *cs_pin) { this->cs_pin_ = cs_pin; }
  
  // API publique
  bool initialize_card();
  bool is_ready() const { return this->card_ready_; }

 protected:
  GPIOPin *cs_pin_{nullptr};
  bool card_ready_{false};
  
  void select_card();
  void deselect_card();
  uint8_t send_command(uint8_t cmd, uint32_t arg);
};

}  // namespace sdcardP4
}  // namespace esphome


