#include "esp_one_wire.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace dallas {

static const char *const TAG = "dallas.one_wire";

ESPOneWire::ESPOneWire(InternalGPIOPin *pin) {
  pin_ = pin->to_isr(); 
  pin_.digital_write(false); 
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
}

bool HOT IRAM_ATTR ESPOneWire::reset() {

  uint32_t start0_before_high;
  uint32_t start1;
  uint32_t start2;
  uint32_t duration_for_pin_mode_tri_state = 0;
  uint32_t duration_untill_high = 0;
  uint32_t duration_untill_high2 = 0;
  uint32_t duration_untill_device_pulls_low = 0;
  uint32_t duration_untill_device_releases_bus = 0;
  bool sensor_present = false;
  bool bus_state = false;

  {
    // This part of the code is a bit of a gambling but the intention is to make sure the bus is high
    // before starting to detect if the client pulls the line low. Since it's time critical this is
    // within a interrupt protected area
      InterruptLock lock;

    // See reset here:
    // https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf

    // Send 480µs LOW TX reset pulse
    pin_.pin_mode(gpio::FLAG_OUTPUT);
    delayMicroseconds(480);

    // Release the bus
    start0_before_high = micros();
    pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

    // Start timer to make sure the 480us client period is fulfilled
    start1 = micros();

    duration_for_pin_mode_tri_state = micros()-start0_before_high;

    
    // Wait for bus to be high
    do { 
      duration_untill_high = micros()-start1;
      bus_state = pin_.digital_read(); 
    } while ( bus_state == false && duration_untill_high <= 15 );
    duration_untill_high2 = micros()-start1;

    // Start time to make sure the client pulls the bus low within 240us
    start2 = micros();

    // If the bus wasn't detected as high after 15us something is wrong
    if (duration_untill_high > 15) {
      ESP_LOGE(TAG, "In the reset phase, the bus wasn't release to tri-state within the allowed 15us");
    }
    // Ok, bus has reached high state
    else {
      // Wait for bus to be low - Client presence detection
      do { 
        duration_untill_device_pulls_low = micros()-start2;
        bus_state = pin_.digital_read(); 
      } while ( bus_state == true && duration_untill_device_pulls_low <= 240 );

      // If precense puls detected, wait for client to release to tri-state
      if (duration_untill_device_pulls_low > 240) {
        ESP_LOGW(TAG, "In the reset phase, the sensor didn't pull the line low within the allowed 240us");
      }
      else {
        // Define the sensor as present
        sensor_present = true;
        
        // Then wait for sensor to release bus into Tri-state mode
        do { 
          duration_untill_device_releases_bus = micros()-start1;
          bus_state = pin_.digital_read(); 
        } while ( bus_state == false && duration_untill_device_releases_bus <= 480 );

        // If sensor didn't release the bus to tri-state within allowed time, write a warning
        if (duration_untill_device_releases_bus > 480) {
          ESP_LOGW(TAG, "In the reset phase, the sensor pulled the bus low in %lu us but didn't release the bus into tri-state within 480us", duration_untill_device_pulls_low);
        }
      }
    }
  }

  while(micros()-start1 <= 480){}
          ESP_LOGVV(TAG, "Test: start0_before_high, %lu, start1, %lu, start2, %lu, duration_for_pin_mode_tri_state, %lu, duration_untill_high, %lu, duration_untill_high2, %lu, duration_untill_device_pulls_low, %lu, duration_untill_device_releases_bus, %lu, micros, %lu", start0_before_high, start1, start2, duration_for_pin_mode_tri_state, duration_untill_high, duration_untill_high2, duration_untill_device_pulls_low, duration_untill_device_releases_bus, micros());

  return sensor_present;
}

void HOT IRAM_ATTR ESPOneWire::write_bit(bool bit) {
  InterruptLock lock;

  // Drive bus low
  // First set bus low. This shall be at least 1 us, but since the ESP doesn't execute the pin commands very fast
  // we take another approach and verifies that the bus is low

  uint32_t start0_before = micros();
  pin_.pin_mode(gpio::FLAG_OUTPUT);

  uint32_t start1 = micros();
  uint32_t time_when_confirmed_low = 0;
  do {
    time_when_confirmed_low = micros();
  } while (pin_.digital_read() &&  (time_when_confirmed_low - start1) <= 15 );
  if ( time_when_confirmed_low - start1 > 15 ) {
    ESP_LOGE(TAG, "write bit, bus not low within 15us. It tooked %lu us", time_when_confirmed_low - start1);
  }

  // Then wait at least 1us for a 1 and 60us for a 0 (2us and 62us as the resolution is 1us on the timer)
  while ( (micros() - time_when_confirmed_low) <=  (bit ? 2 : 62)) {}
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  
    
  // Then wait until the bus is confirmed high
  uint32_t time_when_confirmed_high = 0;
  do {
    time_when_confirmed_high = micros();
  } while (!pin_.digital_read() &&  (time_when_confirmed_high - time_when_confirmed_low) <= 120 );
  if ( (time_when_confirmed_high - time_when_confirmed_low) > 120 ) {
    ESP_LOGE(TAG, "write bit, bus not high within 120us. It tooked %lu us", time_when_confirmed_high - time_when_confirmed_low);
  }

  // Finnish the time stop that must take at least 60 us
  while ( micros() - time_when_confirmed_low <= 62 ) {}
}

bool HOT IRAM_ATTR ESPOneWire::read_bit() {
  InterruptLock lock;

  // Drive bus low
  // First set bus low. This shall be at least 1 us, but since the ESP doesn't execute the pin commands very fast
  // we take another approach and verifies that the bus is low

  uint32_t start0_before = micros();
  pin_.pin_mode(gpio::FLAG_OUTPUT);

  uint32_t start1 = micros();
  uint32_t time_when_confirmed_low = 0;
  do {
    time_when_confirmed_low = micros();
  } while (pin_.digital_read() &&  (time_when_confirmed_low - start1) <= 15 );
  if ( time_when_confirmed_low - start1 > 15 ) {
    ESP_LOGE(TAG, "read bit, bus not low within 15us. It tooked %lu us", time_when_confirmed_low - start1);
  }

  // Then set the bus in tri-state. But only do this when the bus has been low for at least 1us (2us as the resolution is 1us on the timer)
  while ( (micros() - time_when_confirmed_low) <= 2) {}
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  uint32_t time_when_confirmed_high = 0;
  bool pin_state=false;
  do {
    time_when_confirmed_high = micros();
    pin_state = pin_.digital_read();
  } while (!pin_state &&  (time_when_confirmed_high - start1) <= 60 );

  // Finnish the time stop that must take at least 60 us
  while ( micros() - time_when_confirmed_low <= 60 ) {}

  // If bus got high within 15 us then it's a 1, if not it's a 0
  bool r =  time_when_confirmed_high <= time_when_confirmed_low+15;

  return r;
}

void IRAM_ATTR ESPOneWire::write8(uint8_t val) {
  ESP_LOGVV(TAG, "write8: 0x%02x", val); //TBD_PADE Remove this log
  for (uint8_t i = 0; i < 8; i++) {
    this->write_bit(bool((1u << i) & val));
  }
}

void IRAM_ATTR ESPOneWire::write64(uint64_t val) {
  ESP_LOGVV(TAG, "write64"); //TBD_PADE Remove this log
  uint8_t* bytes = reinterpret_cast<uint8_t*>(&val);
  for (uint8_t i = 0; i < 8; i++) {
    this->write8(bytes[i]);
  }
}


uint8_t IRAM_ATTR ESPOneWire::read8() {
  uint8_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint8_t(this->read_bit()) << i);
  }
  ESP_LOGVV(TAG, "read8: 0x%02x", ret); //TBD_PADE Remove this log
  return ret;
}

uint64_t IRAM_ATTR ESPOneWire::read64() {
  ESP_LOGVV(TAG, "read64"); //TBD_PADE Remove this log
  uint64_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint64_t(this->read8()) << i * 8);
  }
  return ret;
}

void IRAM_ATTR ESPOneWire::select(uint64_t address) {
  ESP_LOGVV(TAG, "select: 0x%02x", address); //TBD_PADE Remove this log
  this->write8(ONE_WIRE_ROM_SELECT);
  this->write64(address);
}

void IRAM_ATTR ESPOneWire::reset_search() {
  ESP_LOGVV(TAG, "reset_search"); //TBD_PADE Remove this log
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->rom_number_ = 0;
}
uint64_t IRAM_ATTR ESPOneWire::search() {
  ESP_LOGVV(TAG, "search"); //TBD_PADE Remove this log
  if (this->last_device_flag_) {
    return 0u;
  }

  if (!this->reset()) {
    // Reset failed or no devices present
    this->reset_search();
    return 0u;
  }

  uint8_t id_bit_number = 1;
  uint8_t last_zero = 0;
  uint8_t rom_byte_number = 0;
  bool search_result = false;
  uint8_t rom_byte_mask = 1;

  // Initiate search
  this->write8(ONE_WIRE_ROM_SEARCH);
  do {
    // read bit
    bool id_bit = this->read_bit();
    // read its complement
    bool cmp_id_bit = this->read_bit();

    if (id_bit && cmp_id_bit) {
      // No devices participating in search
      break;
    }

    bool branch;

    if (id_bit != cmp_id_bit) {
      // only chose one branch, the other one doesn't have any devices.
      branch = id_bit;
    } else {
      // there are devices with both 0s and 1s at this bit
      if (id_bit_number < this->last_discrepancy_) {
        branch = (this->rom_number8_()[rom_byte_number] & rom_byte_mask) > 0;
      } else {
        branch = id_bit_number == this->last_discrepancy_;
      }

      if (!branch) {
        last_zero = id_bit_number;
      }
    }

    if (branch) {
      // set bit
      this->rom_number8_()[rom_byte_number] |= rom_byte_mask;
    } else {
      // clear bit
      this->rom_number8_()[rom_byte_number] &= ~rom_byte_mask;
    }

    // choose/announce branch
    this->write_bit(branch);
    id_bit_number++;
    rom_byte_mask <<= 1;
    if (rom_byte_mask == 0u) {
      // go to next byte
      rom_byte_number++;
      rom_byte_mask = 1;
    }
  } while (rom_byte_number < 8);  // loop through all bytes

  if (id_bit_number >= 65) {
    this->last_discrepancy_ = last_zero;
    if (this->last_discrepancy_ == 0) {
      // we're at root and have no choices left, so this was the last one.
      this->last_device_flag_ = true;
    }
    search_result = true;
  }

  search_result = search_result && (this->rom_number8_()[0] != 0);
  if (!search_result) {
    this->reset_search();
    return 0u;
  }

  return this->rom_number_;
}
std::vector<uint64_t> ESPOneWire::search_vec() {
  ESP_LOGVV(TAG, "search_vec"); //TBD_PADE Remove this log
  std::vector<uint64_t> res;

  this->reset_search();
  uint64_t address;
  while ((address = this->search()) != 0u)
    res.push_back(address);

  return res;
}

void IRAM_ATTR ESPOneWire::skip() {
  ESP_LOGVV(TAG, "skip"); //TBD_PADE Remove this log
  this->write8(ONE_WIRE_ROM_SKIP);  // skip ROM
}

uint8_t IRAM_ATTR *ESPOneWire::rom_number8_() { return reinterpret_cast<uint8_t *>(&this->rom_number_); }

}  // namespace dallas
}  // namespace esphome
