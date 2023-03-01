#pragma once

#include <Arduino.h>
#include "CRC32.h"

struct Message {
  uint8_t address;
  std::optional<uint16_t> voltage;
  std::optional<int16_t> temperature;
  std::optional<uint8_t> button;
};

class Parser {
  public:
    Parser()
      : state(State::Initial)
      , value(0)
      , count(0)
      , crc(0x04c11db7, 0x00000000, 0xffffffff, false, false)
      , sign(false)
    {}

    void reset() {
      state = State::Initial;
      reset_num();
      crc.restart();
      address = std::nullopt;
      voltage = std::nullopt;
      temperature = std::nullopt;
      button = std::nullopt;
    }

    std::optional<Message> parse(const char ch) {
      if (state != State::Checksum && state != State::Invalid) {
        crc.add(ch);
      }

      switch (state) {
        case State::Initial:
          if (ch == ':')
            state = State::Idle;
          break;
        case State::Idle:
          switch (ch) {
            case 'A':
              state = State::Address;
              break;
            case 'V':
              state = State::Voltage;
              reset_num();
              break;
            case 'T':
              state = State::Temperature;
              reset_num();
              break;
            case 'B':
              state = State::Button;
              reset_num();
              break;
            case ' ':
              break;
            case '|':
              state = State::Checksum;
              reset_num();
              break;
            default:
              if (ch >= 'A' || ch <= 'Z')
                state = State::Skipping;
              else
                state = State::Invalid;
          }
          break;
        case State::Skipping:
          switch (ch) {
            case ' ':
              state = State::Idle;
              break;
            case '|':
              state = State::Checksum;
              reset_num();
              break;
            default:
              break;
          }
          break;
        case State::Invalid:
          break;
        case State::Address:
          if (ch >= '0' && ch <= '9') {
            address = ch - '0';
            state = State::Idle;
          } else if (ch >= 'a' && ch <= 'f') {
            address = ch - 'a' + 10;
            state = State::Idle;
          } else {
            state = State::Invalid;
          }
          break;
        case State::Voltage:
          if (parse_num(ch)) {
            voltage = value;
            reset_num();
          }
          break;
        case State::Temperature:
          if (parse_num(ch, true)) {
            temperature = sign ? -value : value;
            reset_num();
          }
          break;
        case State::Button:
          if (ch >= '0' && ch <= '5') {
            button = ch - '0';
            state = State::Idle;
          } else {
            state = State::Invalid;
          }
          break;
        case State::Checksum:
          if (ch >= '0' && ch <= '9') {
            ++count;
            value = value * 16 + (ch - '0');
          } else if (ch >= 'a' && ch <= 'f') {
            ++count;
            value = value * 16 + (ch - 'a' + 10);
          } else if (ch == ' ' || ch == '\r') {
            // do nothing
          } else if (ch == '\n' && count == 8) {
            if (crc.getCRC() == value)
                state = State::Initial;
            else
                state = State::Initial;
          } else {
            state = State::Invalid;
          }
          break;
      }

      if (state == State::Initial && address) {
        return Message {
          address.value(),
          voltage,
          temperature,
          button,
        };
      } else {
        return std::nullopt;
      }
    }

  private:
    void reset_num() {
      value = 0;
      sign = false;
      count = 0;
    }

    bool parse_num(const char ch, bool sgn = false) {
      switch (ch) {
        case ' ':
        case '|':
          if (count > 0) {
            state = (ch == '|') ? State::Checksum : State::Idle;
            return true;
          } else {
            state = State::Invalid;
            return false;
          }
        default:
          if (sgn && ch == '-' && count == 0 && sign == false) {
            sign = true;
          } else if (ch >= '0' && ch <= '9' && count < 4) {
            ++count;
            value = value * 10 + (ch - '0');
          } else {
            state = State::Invalid;
          }
          return false;
      }
    }

  private:
    enum class State {
      Initial,
      Idle,
      Skipping,
      Invalid,
      Address,
      Voltage,
      Temperature,
      Button,
      Checksum
    };

    State state;
    uint32_t value;
    uint32_t count;
    bool sign;
    CRC32 crc;
    std::optional<uint8_t> address;
    std::optional<uint16_t> voltage;
    std::optional<int16_t> temperature;
    std::optional<uint8_t> button;
};
