#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

#include "wifisettings.h"

namespace {

constexpr char settingsPath[] = "/wifi-settings.json";
constexpr uint32_t connectionTimeoutMs = 10000;

bool copyString(char *destination, size_t capacity, const char *source) {
  if (source == nullptr) {
    source = "";
  }
  size_t length = strlen(source);
  if (length >= capacity) {
    return false;
  }
  memcpy(destination, source, length + 1);
  return true;
}

void setDefaultIpSettings() {
  const int16_t defaultIp[4] = {192, 168, 1, 50};
  const int16_t defaultGateway[4] = {192, 168, 1, 1};
  const int16_t defaultSubnet[4] = {255, 255, 255, 0};
  memcpy(wifiSettings.ip, defaultIp, sizeof(defaultIp));
  memcpy(wifiSettings.gateway, defaultGateway, sizeof(defaultGateway));
  memcpy(wifiSettings.subnet, defaultSubnet, sizeof(defaultSubnet));
}

void loadOctets(JsonDocument &document, const char *prefix, int16_t values[4], const int16_t defaults[4]) {
  for (uint8_t octet = 0; octet < 4; ++octet) {
    char key[12];
    snprintf(key, sizeof(key), "%s%u", prefix, octet);
    int value = document[key] | defaults[octet];
    values[octet] = (value >= 0 && value <= 255) ? value : defaults[octet];
  }
}

void saveOctets(JsonDocument &document, const char *prefix, const int16_t values[4]) {
  for (uint8_t octet = 0; octet < 4; ++octet) {
    char key[12];
    snprintf(key, sizeof(key), "%s%u", prefix, octet);
    document[key] = values[octet];
  }
}

IPAddress makeAddress(const int16_t octets[4]) {
  return IPAddress(octets[0], octets[1], octets[2], octets[3]);
}

}

WifiSettings wifiSettings = {};

void loadWifiSettings(void) {
  memset(&wifiSettings, 0, sizeof(wifiSettings));
  setDefaultIpSettings();
  wifiSettings.oscSendPort = 10111;
  wifiSettings.oscListenPort = 8000;
  wifiSettings.randomWalkSpeedMs = 500;

  if (!LittleFS.exists(settingsPath)) {
    return;
  }

  File file = LittleFS.open(settingsPath, "r");
  if (!file) {
    Serial.println("WiFi settings: unable to open file");
    return;
  }

  JsonDocument document;
  DeserializationError error = deserializeJson(document, file);
  file.close();
  if (error) {
    Serial.print("WiFi settings: invalid JSON: ");
    Serial.println(error.f_str());
    return;
  }

  wifiSettings.useStaticIp = document["staticIp"] | false;
  wifiSettings.nextReplacement = (document["nextReplacement"] | 0) % WIFI_CREDENTIAL_SLOTS;
  int sendPort = document["oscSendPort"] | 10111;
  int listenPort = document["oscListenPort"] | 8000;
  int randomWalkSpeed = document["randomWalkSpeedMs"] | 500;
  if (sendPort > 0 && sendPort <= 65535) {
    wifiSettings.oscSendPort = sendPort;
  }
  if (listenPort > 0 && listenPort <= 65535) {
    wifiSettings.oscListenPort = listenPort;
  }
  if (randomWalkSpeed >= 10 && randomWalkSpeed <= 5000) {
    wifiSettings.randomWalkSpeedMs = randomWalkSpeed;
  }

  const int16_t defaultIp[4] = {192, 168, 1, 50};
  const int16_t defaultGateway[4] = {192, 168, 1, 1};
  const int16_t defaultSubnet[4] = {255, 255, 255, 0};
  loadOctets(document, "ip", wifiSettings.ip, defaultIp);
  loadOctets(document, "gateway", wifiSettings.gateway, defaultGateway);
  loadOctets(document, "subnet", wifiSettings.subnet, defaultSubnet);

  for (uint8_t index = 0; index < WIFI_CREDENTIAL_SLOTS; ++index) {
    char ssidKey[12];
    char passwordKey[16];
    snprintf(ssidKey, sizeof(ssidKey), "ssid%u", index);
    snprintf(passwordKey, sizeof(passwordKey), "password%u", index);
    const char *ssid = document[ssidKey] | "";
    const char *password = document[passwordKey] | "";

    if (ssid[0] == '\0') {
      continue;
    }
    if (!copyString(wifiSettings.networks[wifiSettings.networkCount].ssid,
                    sizeof(wifiSettings.networks[0].ssid), ssid) ||
        !copyString(wifiSettings.networks[wifiSettings.networkCount].password,
                    sizeof(wifiSettings.networks[0].password), password)) {
      Serial.println("WiFi settings: skipped an invalid credential entry");
      continue;
    }
    ++wifiSettings.networkCount;
  }
}

bool saveWifiSettings(void) {
  JsonDocument document;
  document["version"] = 1;
  document["staticIp"] = wifiSettings.useStaticIp;
  document["oscSendPort"] = wifiSettings.oscSendPort;
  document["oscListenPort"] = wifiSettings.oscListenPort;
  document["randomWalkSpeedMs"] = wifiSettings.randomWalkSpeedMs;
  document["nextReplacement"] = wifiSettings.nextReplacement;
  saveOctets(document, "ip", wifiSettings.ip);
  saveOctets(document, "gateway", wifiSettings.gateway);
  saveOctets(document, "subnet", wifiSettings.subnet);

  for (uint8_t index = 0; index < WIFI_CREDENTIAL_SLOTS; ++index) {
    char ssidKey[12];
    char passwordKey[16];
    snprintf(ssidKey, sizeof(ssidKey), "ssid%u", index);
    snprintf(passwordKey, sizeof(passwordKey), "password%u", index);
    document[ssidKey] = wifiSettings.networks[index].ssid;
    document[passwordKey] = wifiSettings.networks[index].password;
  }

  File file = LittleFS.open(settingsPath, "w");
  if (!file) {
    Serial.println("WiFi settings: unable to write file");
    return false;
  }

  size_t bytesWritten = serializeJson(document, file);
  file.close();
  if (bytesWritten == 0) {
    Serial.println("WiFi settings: write failed");
    return false;
  }
  return true;
}

int scanWifiNetworks(void) {
  WiFi.mode(WIFI_STA);
  int count = WiFi.scanNetworks();
  if (count < 0) {
    Serial.print("WiFi scan failed: ");
    Serial.println(count);
  }
  return count;
}

int findWifiNetwork(const char *ssid) {
  if (ssid == nullptr) {
    return -1;
  }
  for (uint8_t index = 0; index < wifiSettings.networkCount; ++index) {
    if (strcmp(wifiSettings.networks[index].ssid, ssid) == 0) {
      return index;
    }
  }
  return -1;
}

bool rememberWifiNetwork(const char *ssid, const char *password) {
  if (ssid == nullptr || password == nullptr ||
      strlen(ssid) == 0 || strlen(ssid) > WIFI_SSID_LENGTH ||
      strlen(password) > WIFI_PASSWORD_LENGTH) {
    Serial.println("WiFi settings: invalid credential length");
    return false;
  }

  int index = findWifiNetwork(ssid);
  if (index < 0) {
    if (wifiSettings.networkCount < WIFI_CREDENTIAL_SLOTS) {
      index = wifiSettings.networkCount++;
    } else {
      index = wifiSettings.nextReplacement;
      wifiSettings.nextReplacement = (wifiSettings.nextReplacement + 1) % WIFI_CREDENTIAL_SLOTS;
    }
  }

  if (!copyString(wifiSettings.networks[index].ssid,
                  sizeof(wifiSettings.networks[index].ssid), ssid) ||
      !copyString(wifiSettings.networks[index].password,
                  sizeof(wifiSettings.networks[index].password), password)) {
    Serial.println("WiFi settings: failed to copy credentials");
    return false;
  }
  return saveWifiSettings();
}

bool connectWifi(const char *ssid, const char *password) {
  if (ssid == nullptr || password == nullptr || ssid[0] == '\0') {
    Serial.println("WiFi: invalid network credentials");
    return false;
  }

  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  if (wifiSettings.useStaticIp) {
    IPAddress gateway = makeAddress(wifiSettings.gateway);
    WiFi.config(makeAddress(wifiSettings.ip), gateway, gateway, makeAddress(wifiSettings.subnet));
  }

  WiFi.begin(ssid, password);
  uint32_t startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < connectionTimeoutMs) {
    delay(100);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool connectSavedWifi(void) {
  for (uint8_t index = 0; index < wifiSettings.networkCount; ++index) {
    if (connectWifi(wifiSettings.networks[index].ssid, wifiSettings.networks[index].password)) {
      return true;
    }
  }
  return false;
}
