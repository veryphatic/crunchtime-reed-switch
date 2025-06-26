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
#include <Arduino.h>
#include <OSCMessage.h>
#include "../env_config.h"
#include "./wifi.h"


// ------------------------------------------------------------------------------------------------------------------ Edit these settings

// Sets the wifi environment - use env_config.h to store values
const WifiEnv wifi(HOME_SSID, HOME_PASSWORD, CRUNCH_TALLY);

// change these for each sensor
const unsigned int sensorId = 27;         // Give the sensor a unique ID
const char *hostname = "ctsensor-27";     // Append the sensor ID to the hostname

// OSC settings
const unsigned int talPort = 8765;       // Set the port listenting for tally OSC messages, default to 8765.


// ------------------------------------------------------------------------------------------------------------------ Don't edit anything below here

// set the current wifi environment -- change this to switch between environments
const WifiEnv& currentEnv = wifi;

// set ssid, password, ip from wifi env
const char* ssid = currentEnv.ssid;
const char* password = currentEnv.password;
IPAddress labIp = currentEnv.ip;


constexpr uint8_t REED_PIN = D1;         // Reed switch connected to GPIO D1
const unsigned int ACK_TIMEOUT = 250;
const unsigned int DEBOUNCE_DELAY = 50;  // Debounce delay in milliseconds
const unsigned int localPort = 53001;    // The local listening port for UDP packets.

// Reed switch state tracking
bool lastReedState = LOW;               // Assume switch starts open (HIGH = open, LOW = closed)
bool currentReedState = LOW;
unsigned long lastDebounceTime = 0;

typedef struct State_struct (*StateFn)(struct State_struct current_state);

typedef struct State_struct {
  bool switchClosed;
  bool lastSentState;

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

  Serial.println("\n");
  Serial.println("WiFi Connected! IP Address:");
  Serial.println(WiFi.localIP());
  Serial.printf("Sensor ID %d\n", sensorId);
}

// setup executes once after booting. It configures the underlying hardware for
// use in the main loop.
void setup() {
  Serial.begin(9600);
  Serial.printf("\nwifi: %s\n", ssid);
  randomSeed(analogRead(0));

  // Configure reed switch pin with internal pull-up resistor
  pinMode(REED_PIN, INPUT_PULLUP);

  // Put in a startup delay of ten to twenty seconds before connecting to the WiFi.
  delay(random(0, 15000));
  connectWiFi();

  // Init UDP to broadcast OSC messages.
  Udp.begin(localPort);

  // Initialize state
  state.switchClosed = true;
  state.lastSentState = true;
  state.update = Idle;

  Serial.println("Reed switch sensor initialized");
}

void sendPacket(IPAddress dstIp, const unsigned int dstPort, const char *tag) {
  OSCMessage msg(tag);
  int err = Udp.beginPacket(dstIp, dstPort);
  if (err == 0) {
    Serial.println("Could not initalie buffer");
  } 

  msg.send(Udp);

  err = Udp.endPacket();
  if (err == 0) {
    Serial.println("Could not send buffered data");
  }

  msg.empty();
}

// sendOSCData broadcasts reed switch information over OSC to the dst address.
void sendOSCData(IPAddress dstIp, const unsigned int dstPort, const char *tag) {
  bool ack = false;

  while (!ack) {
    sendPacket(dstIp, dstPort, tag);
    delay(ACK_TIMEOUT);
    ack = getOSCData();
  }
}

bool getOSCData() {
  OSCMessage msg;
  int size = Udp.parsePacket();

  if (size > 0) {
    while (size--) {
      msg.fill(Udp.read());
    }

    if (!msg.hasError()) {
      Serial.println("OSC: ack");
      return true;
    } else {
      OSCErrorCode error = msg.getError();
      Serial.print("OSC error: ");
      Serial.println(error);
    }
  }

  return false;
}

// readReedSwitch reads the reed switch state with debouncing
bool readReedSwitch() {
  bool reading = digitalRead(REED_PIN);

  // If the switch changed, due to noise or pressing:
  if (reading != lastReedState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != currentReedState) {
      currentReedState = reading;
    }
  }

  // save the reading. Next time through the loop, it'll be the lastReedState:
  lastReedState = reading;

  // Return true if switch is closed (LOW = closed, HIGH = open)
  return (currentReedState == LOW);
}

State Idle(State current_state) {
  // Read current reed switch state
  current_state.switchClosed = readReedSwitch();
  
  // If switch is closed, we don't need to do anything.
  if (current_state.switchClosed) {
    return current_state;
  }

  Serial.println("Magnet detected: next state Detected. Sent OSC");

  // Notify everyone that switch iks open (object detected).
  char tag[64];
  sprintf(tag, "/cue/%don/start", sensorId);
  sendOSCData(labIp, talPort, tag);

  current_state.lastSentState = true;

  current_state.update = Detected;
  return current_state;
}

State Detected(State current_state) {
  // Read current reed switch state
  current_state.switchClosed = readReedSwitch();

  // If the switch is open, don't do anything.
  if (!current_state.switchClosed) {
    return current_state;
  }

  Serial.println("Magnet removed: next state Idle. Sent OSC");

  // Notify everyone that switch closed (object removed).
  char tag[64];
  sprintf(tag, "/cue/%doff/start", sensorId);
  sendOSCData(labIp, talPort, tag);

  current_state.lastSentState = false;

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