#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

#define RS485_DE    10

#define RS485Transmit   HIGH
#define RS485Receive    LOW

#define LED_BLUE    6
#define LED_GREEN   5
#define LED_RED     4

#define SPEAKER     3

#include "arbiter.h"
#include "cert.h"
#include "led.h"
#include "sound.h"
#include "songs.h"
#include "space_status.h"

const char *const SSID0 = "mainframe.iot";
const char *const SSID0_PASSWORD = "TODO";

const unsigned long MAX_NOT_CONNECTED_TIME = 20*1000; // milliseconds
const unsigned long STATUS_SEND_PERIOD = 1*1000; // milliseconds

const char *const MQTT_HOST = "spacegate.mainframe.io";
const int MQTT_PORT = 8884;
const unsigned long MQTT_ATTEMPT_WAIT_TIME = 1*1000; // milliseconds

const char *const TIMEZONE = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"; // Europe/Berlin
const char* const NTP_SERVER_1 = "ntp.lan.mainframe.io";
const char* const NTP_SERVER_2 = "de.pool.ntp.org";

const char* const STATUS_TOPIC = "/access-control-system/space-state";
const char* const STATUS_NEXT_TOPIC = "/access-control-system/space-state-next";
const char* const MAIN_DOOR_TOPIC = "/access-control-system/main-door/reed-switch";

const unsigned long SILENCE_PERIOD = 40000; // milliseconds

const unsigned long BUS_PANIC_PERIOD = 3000; // milliseconds

WiFiMulti wifi;
WiFiClientSecure secure;
PubSubClient mqtt_client(MQTT_HOST, MQTT_PORT, secure);

SongPlayer sound(SPEAKER);
Led led(LED_RED, LED_GREEN, LED_BLUE);

Arbiter arbiter;

enum class Event {
  Nothing = 0,
  Ring,
  NoRing,
  Door,
};

SpaceStatus space_status = SpaceStatus::Unknown;
SpaceStatus next_status = SpaceStatus::Unknown;

Event last_event = Event::Nothing;
int64_t last_status_send = -1;
int64_t last_ringing = -1;
int64_t last_rs485_timestamp = -1;

// callback for subscribed topics
static void mqtt_callback(const char* const topic, const byte* const payload, const unsigned int length) {
  const char* const status = (const char*) payload;

  SpaceStatus new_space_status = space_status;
  SpaceStatus new_next_status = next_status;
  if (!strcmp(topic, STATUS_TOPIC)) {
    new_space_status = from_string(status, length);
  } else if (!strcmp(topic, STATUS_NEXT_TOPIC)) {
    new_next_status = from_string(status, length);
  } else if (!strcmp(topic, MAIN_DOOR_TOPIC)) {
    // last_event = Event::Door;
  }

  if (new_space_status != space_status) {
    space_status = new_space_status;
    last_status_send = -1;
  }

  if (new_next_status != next_status) {
    next_status = new_next_status;
    last_status_send = -1;
  }
} // callback


void setup() {
  pinMode(RS485_DE, OUTPUT);
  digitalWrite(RS485_DE, RS485Receive);

  pinMode(SPEAKER, OUTPUT);
  digitalWrite(SPEAKER, LOW);

  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  led.set_color(LedColor::BLUE);

  // debugSerial.begin(115200);
  // debugSerial.println();
  // debugSerial.println("Tür Klingel 2022 von Alexey und Michael(Pluto)");

  Serial.begin(115200);

  mqtt_client.setCallback(mqtt_callback);

  secure.setCACert(CA_CERT);

  // Initialize client
  wifi.addAP(SSID0, SSID0_PASSWORD);
  mqtt_client.setServer(MQTT_HOST, MQTT_PORT);
}

int32_t not_connected_since = -1;
bool wifi_connected = false;
bool ntp_connected = false;
bool mqtt_connected = false;
int32_t mqtt_last_failure = -1;
size_t song = 0;

static void disconnected() {
  unsigned long ts = millis();
  if (not_connected_since < 0) {
    not_connected_since = ts;
  } else {
    if (ts - (unsigned long)not_connected_since > MAX_NOT_CONNECTED_TIME) {
      not_connected_since = ts - MAX_NOT_CONNECTED_TIME;
      if (space_status != SpaceStatus::Unknown || next_status != SpaceStatus::Unknown) {
        space_status = SpaceStatus::Unknown;
        next_status = SpaceStatus::Unknown;
        last_status_send = -1;
      }
    }
  }
}

void loop() {
  if (wifi.run() == WL_CONNECTED) {
    wifi_connected = true;
  } else {
    disconnected();
    wifi_connected = false;
    ntp_connected = false;
    mqtt_connected = false;
    mqtt_last_failure = -1;
  }

  if (wifi_connected) {
    if (!ntp_connected) {
      configTzTime(TIMEZONE, NTP_SERVER_1, NTP_SERVER_2);
      ntp_connected = true;
    }

    if (mqtt_connected) {
      if (!mqtt_client.loop()) {
        disconnected();
        mqtt_connected = false;
        mqtt_last_failure = -1;
      }
    }

    if (!mqtt_connected) {
      if (mqtt_last_failure < 0 || millis() - (unsigned long)mqtt_last_failure > MQTT_ATTEMPT_WAIT_TIME) {
        if (mqtt_client.connect("DoorBell20")) {
          not_connected_since = -1;
          mqtt_connected = true;
          led.set_color(LedColor::CYAN);
          mqtt_client.subscribe(STATUS_TOPIC);
          mqtt_client.subscribe(STATUS_NEXT_TOPIC);
          mqtt_client.subscribe(MAIN_DOOR_TOPIC);
        } else {
          mqtt_last_failure = millis();
        }
      }
    }
  }

  const bool is_playing = sound.loop();

  const auto msg = arbiter.recv();
  unsigned long ts = millis();
  if (arbiter.can_send()) {
    if ((bool)last_event || last_status_send < 0 || ts - (unsigned long)last_status_send > STATUS_SEND_PERIOD) {
      uint8_t c = 'x';
      if ((bool)last_event) {
        switch (last_event) {
          case Event::Ring:
            c = 'W';
            break;
          case Event::NoRing:
            c = 'M';
            break;
          case Event::Door:
            c = 'B';
            break;
        }
      } else {
        switch (space_status) {
          case SpaceStatus::Open:
          case SpaceStatus::OpenPlus:
            switch (next_status) {
              case SpaceStatus::Closed:
              case SpaceStatus::Keyholder:
                c = 'Y';
                break;
              default:
                c = 'G';
                break;
            }
            break;
          case SpaceStatus::Keyholder:
            c = 'Y';
            break;
          case SpaceStatus::Member:
            c = 'C';
            break;
          case SpaceStatus::Closed:
            c = 'R';
            break;
          case SpaceStatus::Unknown:
            c = 'M';
            break;
        }
        if (is_playing)
          c = tolower(c);
      }

      led.set_color(c);

      char buf[16];
      snprintf(buf, sizeof(buf), "Ac C%c L%u", c, 5);
      arbiter.send(buf);

      last_status_send = ts;
      last_event = Event::Nothing;
    }
  }

  if (last_ringing >= 0 && millis() - (unsigned long)last_ringing > SILENCE_PERIOD)
    last_ringing = -1;

  if (msg) {
    last_rs485_timestamp = ts;

    if (msg->temperature) {
      // debugSerial.print(" t");
      // debugSerial.print(*msg->temperature);
    }
    if (msg->voltage) {
      // debugSerial.print(" v");
      // debugSerial.print(*msg->voltage);
    }
    if (msg->button) {
      if (last_event == Event::Nothing) {
        if (is_playing || last_ringing == -1)
          last_event = Event::Ring;
        else
          last_event = Event::NoRing;
      }
      if (! is_playing && last_ringing == -1) {
        last_ringing = millis();
        sound.start_play(SONGS[song]);
        song = (song + 1) % NUM_SONGS;
      }
    }
    // debugSerial.println();
  }

  if (ts - (unsigned long)last_rs485_timestamp > BUS_PANIC_PERIOD) {
    led.set_color(LedColor::MAGENTA);
  }
}
