#pragma once

#include "parser.h"

class Arbiter {
    static const uint8_t MY_ADDRESS = 0x0c;
    static const uint8_t NUM_ADDRESSES = 16;
    static const uint32_t CYCLE = 32 * 1000000 * 10 / 115200;
  public:
    Arbiter()
      : addr(-128)
    { }

    bool can_send() const {
      return (addr == MY_ADDRESS);
    }

    void send(const char* bytes) {
      digitalWrite(RS485_DE, RS485Transmit);
      CRC32 crc(0x04c11db7, 0x00000000, 0xffffffff, false, false);
      Serial.print(':');
      crc.add(':');
      Serial.print(bytes);
      for (const char* b = bytes; *b; ++b)
        crc.add(*b);
      Serial.print('|');
      crc.add('|');
      Serial.printf("%08x\n", crc.getCRC());
      Serial.flush();
      digitalWrite(RS485_DE, RS485Receive);

      idle = false;
      addr = MY_ADDRESS;
      timer = micros();
    }

    std::optional<Message> recv() {
      uint32_t now = micros();
      if (Serial.available()) {
        timer = now;
        idle = false;
        const auto msg = parser.parse(Serial.read());
        if (msg) {
          parser.reset();
          addr = msg->address;
          return msg;
        }
      } else {
        const uint32_t dt = idle ? 0 : SYMBOL;
        if (now - timer >= CYCLE + dt) {
          parser.reset();
          addr = (addr + (now - timer - dt) / CYCLE) % NUM_ADDRESSES;
          timer = now;
          idle = true;
        }
      }
      return std::nullopt;
    }

  private:
    Parser parser;
    int8_t addr;
    uint32_t timer;
    bool idle;
};
