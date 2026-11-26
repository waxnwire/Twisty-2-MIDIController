#pragma once

#include <Arduino.h>

#define WIFI_CREDENTIAL_SLOTS 4
#define WIFI_SSID_LENGTH 32
#define WIFI_PASSWORD_LENGTH 63

struct WifiCredential {
  char ssid[WIFI_SSID_LENGTH + 1];
  char password[WIFI_PASSWORD_LENGTH + 1];
};

struct WifiSettings {
  WifiCredential networks[WIFI_CREDENTIAL_SLOTS];
  uint8_t networkCount;
  uint8_t nextReplacement;
  bool useStaticIp;
  int16_t ip[4];
  int16_t gateway[4];
  int16_t subnet[4];
  uint16_t oscSendPort;
  uint16_t oscListenPort;
  uint16_t randomWalkSpeedMs;
};

extern WifiSettings wifiSettings;

void loadWifiSettings(void);
bool saveWifiSettings(void);
int scanWifiNetworks(void);
int findWifiNetwork(const char *ssid);
bool rememberWifiNetwork(const char *ssid, const char *password);
bool connectWifi(const char *ssid, const char *password);
bool connectSavedWifi(void);
