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
  // See reset here:
  // https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf

  bool sensor_present = false;

  // Send 480µs LOW TX reset pulse
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  delayMicroseconds(480);
  
  // This part of the code is a bit of a gambling but the intention is to make sure the bus is high
  // before starting to detect if the client pulls the line low. Since it's time critical this is
  // within a interrupt protected area
  {
    InterruptLock lock;
    // Release the bus
    pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    // Start timer to make sure the 480us client period is fulfilled
    uint32_t duration1 = 0;
    uint32_t start1 = micros();
    
    // Wait for bus to be high
    bool bus_state = false;
    for (int i=0; bus_state == false && duration1 <= 15; i++) { 
      bus_state=pin_.digital_read(); 
      duration1 = micros()-start1;
    }
    // If the bus wasn't detected as high after 15us something is wrong
    if (duration1 > 15) {
      ESP_LOGE(TAG, "In the reset phase, the bus wasn't release to tri-state within the allowed 15us");
    }
  }
  // Ok, bus has reached high state
  else {
    // Start time to make sure the client pulles the bus low within 240us
    uint32_t duration2 = 0;
    uint32_t start2 = micros();

    // Wait for bus to be low - Client presence detection
    for (int i=0; bus_state == true && duration2 <= 240; i++) { 
      bus_state = pin_.digital_read(); 
      duration2 = micros()-start2;
    }

    // If precense puls detected, wait for client to release to tri-state
    if (duration2 > 240) {
      ESP_LOGW(TAG, "In the reset phase, the sensor didn't pull the line low within the allowed 240us");
    }
    else {
      // Define the sensor as present
      sensor_present = true;

      // Then wait for sensor to release bus into Tri-state mode
      for (int i=0; bus_state == false && duration1 <= 480; i++) { 
        bus_state = pin_.digital_read(); 
        duration1 = micros()-start1;
      }

      // If sensor didn't release the bus to tri-state within allowed time, write a warning
      if (duration1 > 480) {
        ESP_LOGW(TAG, "In the reset phase, the sensor pulled the bus low in %ius but didn't release the bus into tri-state within 480us", duration1);
      }
    }

    while(micros()-start1 <= 480){}

  ESP_LOGVV(TAG, "Reset, duration1, %i, duration2, %i, sensor_present, %i", duration1, duration2, sensor_present); //TBD_PADE Remove this log

  }


  return sensor_present;
}

void HOT IRAM_ATTR ESPOneWire::write_bit(bool bit) {
  // drive bus low
  pin_.pin_mode(gpio::FLAG_OUTPUT);

  // from datasheet:
  // write 0 low time: t_low0: min=60µs, max=120µs
  // write 1 low time: t_low1: min=1µs, max=15µs
  // time slot: t_slot: min=60µs, max=120µs
  // recovery time: t_rec: min=1µs
  // ds18b20 appears to read the bus after roughly 14µs
  uint32_t delay0 = bit ? 6 : 60;
  uint32_t delay1 = bit ? 54 : 5;

  // delay A/C
  delayMicroseconds(delay0);
  // release bus
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP); 
  // delay B/D
  delayMicroseconds(delay1);
}

bool HOT IRAM_ATTR ESPOneWire::read_bit() {
  // drive bus low
  pin_.pin_mode(gpio::FLAG_OUTPUT);

  // note: for reading we'll need very accurate timing, as the
  // timing for the digital_read() is tight; according to the datasheet,
  // we should read at the end of 16µs starting from the bus low
  // typically, the ds18b20 pulls the line high after 11µs for a logical 1
  // and 29µs for a logical 0

  uint32_t start = micros();
  // datasheet says >1µs
  delayMicroseconds(3);

  // release bus, delay E
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  // Unfortunately some frameworks have different characteristics than others
  // esp32 arduino appears to pull the bus low only after the digital_write(false),
  // whereas on esp-idf it already happens during the pin_mode(OUTPUT)
  // manually correct for this with these constants.

#ifdef USE_ESP32
  uint32_t timing_constant = 12;
#else
  uint32_t timing_constant = 14;
#endif

  // measure from start value directly, to get best accurate timing no matter
  // how long pin_mode/delayMicroseconds took
  while (micros() - start < timing_constant)
    ;

  // sample bus to read bit from peer
  bool r = pin_.digital_read();

  // read slot is at least 60µs; get as close to 60µs to spend less time with interrupts locked
  uint32_t now = micros();
  if (now - start < 60)
    delayMicroseconds(60 - (now - start));

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
    ret |= (uint64_t(this->read_bit()) << i);
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

  {
    InterruptLock lock; //TBD_PADE This shall be done on lower level
    if (!this->reset()) {
      // Reset failed or no devices present
      this->reset_search();
      return 0u;
    }
  }

  uint8_t id_bit_number = 1;
  uint8_t last_zero = 0;
  uint8_t rom_byte_number = 0;
  bool search_result = false;
  uint8_t rom_byte_mask = 1;

  {
    InterruptLock lock; //TBD_PADE This shall be done on lower level
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
  }

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
