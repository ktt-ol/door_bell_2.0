#!/bin/sh
# NB: Edit .arduino*/packages/esp32/hardware/esp32/*/platform.txt and replace --std=gnu++11 with --std=gnu++17

arduino-cli lib install crc PubSubClient
arduino-cli -b esp32:esp32:esp32c3 compile
