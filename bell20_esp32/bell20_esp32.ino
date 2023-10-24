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

const char *const SSID0 = "mainframe.iot";
const char *const SSID0_PASSWORD = "TODO";
const unsigned long MAX_NOT_CONNECTED_TIME = 20*1000; // milliseconds
const unsigned long STATUS_SEND_PERIOD = 1*1000; // milliseconds
const char *const MQTT_HOST = "spacegate.mainframe.io";
const int MQTT_PORT = 8884;

const char* const STATUS_TOPIC = "/access-control-system/space-state";
const char* const MAIN_DOOR_TOPIC = "/access-control-system/main-door/reed-switch";

const unsigned long SILENCE_PERIOD = 40000; // milliseconds

WiFiMulti wifi;
WiFiClientSecure secure;
PubSubClient mqtt_client(MQTT_HOST, MQTT_PORT, secure);

SongPlayer sound(SPEAKER);
Led led(LED_RED, LED_GREEN, LED_BLUE);

Arbiter arbiter;

enum class Status {
  Unknown = 0,
  Open,
  Closed,
  Closing,
};

enum class Event {
  Nothing = 0,
  Ring,
  NoRing,
  Door,
};

Status space_status = Status::Unknown;
Event last_event = Event::Nothing;
int64_t last_status_send = -1;
int64_t last_ringing = -1;

// callback for subscribed topics
static void mqtt_callback(const char* const topic, const byte* const payload, const unsigned int length) {
  const char* const status = (const char*) payload;

  // debugSerial.print(topic);
  // debugSerial.print(" => ");

  // Ignore anything but the space state
  if (!strcmp(topic, STATUS_TOPIC)) {
    // debugSerial.println("Status topic matches!");
    Status new_status = Status::Unknown;
    if (!strncmp(status, "open", length) || !strncmp(status, "open+", length)) {
      new_status = Status::Open;
      led.set_color(LedColor::GREEN);
    } else if (!strncmp(status, "none", length)) {
      new_status = Status::Closed;
      led.set_color(LedColor::RED);
    } else if (!strncmp(status, "closing", length)) {
      new_status = Status::Closing;
      led.set_color(LedColor::YELLOW);
    } else {
      new_status = Status::Unknown;
    }

    if (new_status != space_status) {
      // debugSerial.printf("Status changed to %u\n", new_status);
      space_status = new_status;
      last_status_send = -1;
    }
  } else if (!strcmp(topic, MAIN_DOOR_TOPIC)) {
    // last_event = Event::Door;
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
size_t song = 0;

void loop() {
  if (wifi.run() == WL_CONNECTED) {
    not_connected_since = -1;
  } else {
    unsigned long ts = millis();
    if (not_connected_since < 0) {
      // debugSerial.println("WiFi connection is interrupted");
      not_connected_since = ts;
    } else {
      if (ts - (unsigned long)not_connected_since > MAX_NOT_CONNECTED_TIME) {
        not_connected_since = ts - MAX_NOT_CONNECTED_TIME;
        // debugSerial.println("WiFi is not connected for long time, status unknown");
        space_status = Status::Unknown;
        last_status_send = -1;
      }
    }
  }

  if (not_connected_since < 0 && !mqtt_client.loop()) {
    // debugSerial.println("Connecting to MQTT server...");
    if (mqtt_client.connect("DoorBell20")) {
      led.set_color(LedColor::CYAN);
      mqtt_client.subscribe(STATUS_TOPIC);
      mqtt_client.subscribe(MAIN_DOOR_TOPIC);
    }
  }

  const bool is_playing = sound.loop();

  const auto msg = arbiter.recv();
  if (arbiter.can_send()) {
    unsigned long ts = millis();
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
      } else if (is_playing) {
        switch (space_status) {
          case Status::Open:
            c = 'g';
            break;
          case Status::Closing:
            c = 'y';
            break;
          case Status::Closed:
            c = 'r';
            break;
          case Status::Unknown:
            c = 'm';
            break;
        }
      } else {
        switch (space_status) {
          case Status::Open:
            c = 'G';
            break;
          case Status::Closing:
            c = 'Y';
            break;
          case Status::Closed:
            c = 'R';
            break;
          case Status::Unknown:
            c = 'M';
            break;
        }
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
    // debugSerial.print("a");
    // debugSerial.print(msg->address);
    if (msg->temperature) {
      // debugSerial.print(" t");
      // debugSerial.print(*msg->temperature);
    }
    if (msg->voltage) {
      // debugSerial.print(" v");
      // debugSerial.print(*msg->voltage);
    }
    if (msg->button) {
      // debugSerial.print(" B");
      // debugSerial.print(*msg->button);
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
}
