#pragma once

#include <ESP8266Wifi.h>
#include <WiFiUdp.h>

WiFiUDP Udp;

struct WifiEnv {
  const char* ssid;
  const char* password;
  IPAddress ip;

  WifiEnv(const char* ssid, const char* password, IPAddress ip)
        : ssid(ssid), password(password), ip(ip) {}
};