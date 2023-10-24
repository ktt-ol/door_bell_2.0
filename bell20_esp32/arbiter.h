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
      addr = MY_ADDRESS;
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
    }

    std::optional<Message> recv() {
      uint32_t now = micros();
      if (Serial.available()) {
        timer = now;
        const auto msg = parser.parse(Serial.read());
        if (msg) {
          parser.reset();
          addr = msg->address;
          return msg;
        }
      } else {
        if (now - timer >= CYCLE) {
          timer = now;
          parser.reset();
          ++addr;
          if (addr >= NUM_ADDRESSES)
            addr = 0;
        }
      }
      return std::nullopt;
    }

  private:
    Parser parser;
    int8_t addr;
    uint32_t timer;
};
