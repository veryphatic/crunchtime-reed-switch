/*
 * Copyright (c) Clinton Freeman 2017
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <ESP8266Wifi.h>
#include <WiFiUdp.h>
#include <Arduino.h>
#include <OSCMessage.h>
#include <SPI.h>
#include <MFRC522.h>
#include "../env_config.h"


WiFiUDP Udp;

struct WifiEnv {
  const char* ssid;
  const char* password;
  IPAddress ip;

  WifiEnv(const char* ssid, const char* password, IPAddress ip)
        : ssid(ssid), password(password), ip(ip) {}
};

// home (ssid, password, ipaddress) -- set in the env_config.h files
// constexpr WifiEnv home(HOME_SSID, HOME_PASSWORD, HOME_IP);

// counterpilot (ssid, password, ipaddress) -- set in the env_config.h files
WifiEnv counterpilot(COUNTER_SSID, COUNTER_PASSWORD, COUNTER_IP);

// set the current wifi environment -- change this to switch between environments
const WifiEnv& currentEnv = counterpilot;

// set ssid, password, ip from wifi env
const char* ssid = currentEnv.ssid;
const char* password = currentEnv.password;
IPAddress labIp = currentEnv.ip;

// change these for each sensor
const unsigned int sensorId = 27;         // EditThis: The ID of the sensor (useful for multple sensors).
const char *hostname = "ctsensor-27";     // EditThis: The hostname for the sensor (for easy network lookup)

const unsigned int talPort = 8765;       // EditThis: The port listenting for tally OSC messages, default to 8765.

constexpr uint8_t RST_PIN = D3;          // The pins that the RFID sensor is connected to.
constexpr uint8_t SS_PIN = D8;  // 15;
const unsigned int ACK_TIMEOUT = 250;

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

const unsigned int localPort = 53001; // The local listening port for UDP packets.
const unsigned int uuidLength = 17;   // The length of RFID uuids.

// presence averaging
const int UUID_HISTORY_LEN = 5;
char uuidHistory[UUID_HISTORY_LEN][uuidLength];
int uuidIndex = 0;

typedef struct State_struct (*StateFn)(struct State_struct current_state);

typedef struct State_struct {
  char uuid[uuidLength];
  char sentUuid[uuidLength];

  StateFn update; // The current function to use to update state.
} State;

State Idle(State current_state);
State Detected(State current_state);
State state;

void sendOSCData(IPAddress dstIp, const unsigned int port, const char *tag);
bool getOSCData();

void connectWiFi() {
  // Prevent need for powercyle after upload.
  WiFi.disconnect();

  // Set a unique hostname
  WiFi.setHostname(hostname);

  // Use DHCP to connect and obtain IP Address.
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait until we have connected to the WiFi AP.
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Connected! IP Address:");
  Serial.println(WiFi.localIP());
  Serial.printf("Sensor ID %d\n", sensorId);
}

// setup executes once after booting. It configures the underlying hardware for
// use in the main loop.
void setup() {
  Serial.begin(9600);
  Serial.printf("wifi: %s\n", ssid);
  randomSeed(analogRead(0));

  // Put in a startup delay of ten to twenty seconds before connecting to the WiFi.
  delay(random(0, 15000));
  connectWiFi();

  // Init UDP to broadcast OSC messages.
  Udp.begin(localPort);

  // Init MFRC522 RFID Sensor.
  SPI.begin();
  mfrc522.PCD_Init();

  // Improve sensitivity but needs to be tested on the table for interferrence
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
  mfrc522.PCD_DumpVersionToSerial();

  memset(state.uuid, '\0', uuidLength);
  sprintf(&state.uuid[0], "nothing\0");

  memset(state.sentUuid, '\0', uuidLength);
  sprintf(&state.sentUuid[0], "nothing\0");
  state.update = Idle;
}

void sendPacket(IPAddress dstIp, const unsigned int dstPort, const char *tag) {
  OSCMessage msg(tag);
  int err = Udp.beginPacket(dstIp, dstPort);
  Serial.print("Begin: ");
  Serial.println(err);
  msg.send(Udp);
  err = Udp.endPacket();
  Serial.print("End: ");
  Serial.println(err);
  msg.empty();
}

// sendOSCData broadcasts RFID information over OSC to the dst address.
void sendOSCData(IPAddress dstIp, const unsigned int dstPort, const char *tag) {
  bool ack = false;

  while (!ack) {
    sendPacket(dstIp, dstPort, tag);
    delay(ACK_TIMEOUT);
    ack = getOSCData();
  }
}

bool getOSCData() {
  Serial.println("Getting OSC DATA");

  OSCMessage msg;
  int size = Udp.parsePacket();
  Serial.println(size);

  if (size > 0) {
    while (size--) {
      msg.fill(Udp.read());
    }

    if (!msg.hasError()) {
      Serial.println("got message");
      return true;
    } else {
      OSCErrorCode error = msg.getError();
      Serial.print("error: ");
      Serial.println(error);
    }
  }

  return false;
}

// getUuid is a helper routine that fetches the ID of the RFID chip in uuid.
void getUuid(char uuid[uuidLength]) {
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    for (unsigned int i = 0; i < ((uuidLength - 1) / 2); i++) {
      sprintf(&uuid[(i * 2)], "%02X", mfrc522.uid.uidByte[i]);
    }
  } else {
    sprintf(&uuid[0], "nothing\0");
  }

  mfrc522.PICC_IsNewCardPresent();
  mfrc522.PICC_ReadCardSerial();
}

State Idle(State current_state) {
  // Scan for current RFID tags.
  getUuid(current_state.uuid);

  // If we haven't detected any RFID cards, we don't need to do anything.
  int cmp = strncmp(current_state.uuid, "nothing", 7);
  if (cmp == 0) {
    return current_state;
  }

  // Notify everyone of the detected UUID.
  char tag[64];
  sprintf(tag, "/cue/%don/start", sensorId);
  sendOSCData(labIp, talPort, tag);

  Serial.print("Detected: ");
  Serial.println(current_state.uuid);
  strncpy(current_state.sentUuid, current_state.uuid, uuidLength);

  current_state.update = Detected;
  return current_state;
}

State Detected(State current_state) {
  // Scan for RFID and see if the same UUID is present.
  getUuid(current_state.uuid);

  // If the RFID card, is still there, dont' do anything.
  int cmp = strncmp(current_state.uuid, "nothing", 7);
  if (cmp != 0) {
    return current_state;
  }

  // Notify everyone of the departed UUID.
  char tag[64];
  sprintf(tag, "/cue/%doff/start", sensorId);
  sendOSCData(labIp, talPort, tag);

  Serial.print("Departed: ");
  Serial.println(current_state.sentUuid);
  strncpy(current_state.sentUuid, current_state.uuid, uuidLength);

  current_state.update = Idle;
  return current_state;
}

// loop repeats over and over on the microcontroller.
void loop() {
  delay(20);

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  state = state.update(state);
}
