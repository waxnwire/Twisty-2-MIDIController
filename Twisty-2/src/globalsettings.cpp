#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

#include "encoderinput.h"
#include "globalsettings.h"
#include "menusystem.h"
#include "wifisettings.h"

extern Adafruit_SSD1306 display;
extern void updatedisplay(void);

namespace {

enum Screen {
  PREMENU,
  WIFI_MENU,
  WIFI_SCAN_LIST,
  WIFI_SAVED_LIST,
  PASSWORD_ENTRY,
  IP_MENU,
  OSC_MENU,
  MESSAGE
};

constexpr int menuLines = 3;
constexpr int premenuCount = 4;
constexpr int wifiMenuCount = 2;
constexpr int ipMenuCount = 14;
constexpr int saveIpIndex = 13;
constexpr char passwordCharacters[] =
    " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()-_=+[]{};:',.<>/?\"`\\|~";

Screen screen = PREMENU;
Screen messageReturnScreen = PREMENU;
int selected = 0;
int scanCount = 0;
char selectedSsid[WIFI_SSID_LENGTH + 1] = {};
char password[WIFI_PASSWORD_LENGTH + 1] = {};
uint8_t passwordLength = 0;
int passwordCharacter = 0;
bool editingIpValue = false;
bool originalStaticIpValue = false;
int16_t originalOctetValue = 0;
bool editingOscPort = false;
uint16_t originalOscPort = 0;
char messageLine1[22] = {};
char messageLine2[22] = {};
struct IpSettingsSnapshot {
  bool useStaticIp;
  int16_t ip[4];
  int16_t gateway[4];
  int16_t subnet[4];
} originalIpSettings;

void snapshotIpSettings(void) {
  originalIpSettings.useStaticIp = wifiSettings.useStaticIp;
  memcpy(originalIpSettings.ip, wifiSettings.ip, sizeof(originalIpSettings.ip));
  memcpy(originalIpSettings.gateway, wifiSettings.gateway, sizeof(originalIpSettings.gateway));
  memcpy(originalIpSettings.subnet, wifiSettings.subnet, sizeof(originalIpSettings.subnet));
}

void restoreIpSettings(void) {
  wifiSettings.useStaticIp = originalIpSettings.useStaticIp;
  memcpy(wifiSettings.ip, originalIpSettings.ip, sizeof(originalIpSettings.ip));
  memcpy(wifiSettings.gateway, originalIpSettings.gateway, sizeof(originalIpSettings.gateway));
  memcpy(wifiSettings.subnet, originalIpSettings.subnet, sizeof(originalIpSettings.subnet));
}

void drawMessage(const char *line1, const char *line2, Screen returnScreen) {
  screen = MESSAGE;
  messageReturnScreen = returnScreen;
  snprintf(messageLine1, sizeof(messageLine1), "%s", line1);
  snprintf(messageLine2, sizeof(messageLine2), "%s", line2);
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(messageLine1);
  display.setCursor(0, 8);
  display.print(messageLine2);
  display.setCursor(0, 24);
  //display.print("R click next L back");
  updatedisplay();
}

void drawMenu(const char *title, const char *const *items, int itemCount) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(title);

  int first = (selected / menuLines) * menuLines;
  for (int row = 0; row < menuLines && first + row < itemCount; ++row) {
    int item = first + row;
    display.setCursor(0, 8 + row * 8);
    display.print(item == selected ? ">" : " ");
    display.print(items[item]);
  }
  updatedisplay();
}

void drawPremenu(void) {
  static const char *const items[] = {"Save / Load", "WiFi", "IP settings", "OSC settings"};
  drawMenu("Global settings", items, premenuCount);
}

void drawWifiMenu(void) {
  static const char *const items[] = {"Scan networks", "Saved networks"};
  drawMenu("WiFi settings", items, wifiMenuCount);
}

int currentListCount(void) {
  return screen == WIFI_SAVED_LIST ? wifiSettings.networkCount : scanCount;
}

String currentListName(int index) {
  if (screen == WIFI_SAVED_LIST) {
    return wifiSettings.networks[index].ssid;
  }
  return WiFi.SSID(index);
}

void drawNetworkList(void) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(screen == WIFI_SAVED_LIST ? "Saved networks" : "WiFi networks");

  int count = currentListCount();
  int first = (selected / menuLines) * menuLines;
  for (int row = 0; row < menuLines && first + row < count; ++row) {
    int item = first + row;
    String name = currentListName(item);
    if (name.length() > 20) {
      name = name.substring(0, 20);
    }
    display.setCursor(0, 8 + row * 8);
    display.print(item == selected ? ">" : " ");
    display.print(name);
  }
  updatedisplay();
}

int16_t *octetValue(int index) {
  if (index >= 1 && index <= 4) {
    return &wifiSettings.ip[index - 1];
  }
  if (index >= 5 && index <= 8) {
    return &wifiSettings.gateway[index - 5];
  }
  if (index >= 9 && index <= 12) {
    return &wifiSettings.subnet[index - 9];
  }
  return nullptr;
}

void drawIpMenu(void) {
  static const char *const labels[] = {
      "Static IP", "IP 1", "IP 2", "IP 3", "IP 4",
      "Gateway 1", "Gateway 2", "Gateway 3", "Gateway 4",
      "Subnet 1", "Subnet 2", "Subnet 3", "Subnet 4",
      "Save & reconnect"};

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("IP ");
  display.print(WiFi.localIP());

  int first = (selected / menuLines) * menuLines;
  for (int row = 0; row < menuLines && first + row < ipMenuCount; ++row) {
    int item = first + row;
    display.setCursor(0, 8 + row * 8);
    display.print(item == selected ? (editingIpValue ? "*" : ">") : " ");
    display.print(labels[item]);
    display.setCursor(SUBMENU_VALUE_X, 8 + row * 8);
    if (item == 0) {
      display.print(wifiSettings.useStaticIp ? "On" : "Off");
    } else if (item < saveIpIndex) {
      display.printf("%3d", *octetValue(item));
    }
  }
  updatedisplay();
}

uint16_t *selectedOscPort(void) {
  return selected == 0 ? &wifiSettings.oscSendPort : &wifiSettings.oscListenPort;
}

void drawOscMenu(void) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("OSC settings");
  display.setCursor(0, 8);
  display.print(editingOscPort && selected == 0 ? "*" : selected == 0 ? ">" : " ");
  display.print("Send port");
  display.setCursor(SUBMENU_VALUE_X, 8);
  display.print(wifiSettings.oscSendPort);
  display.setCursor(0, 16);
  display.print(editingOscPort && selected == 1 ? "*" : selected == 1 ? ">" : " ");
  display.print("Listen port");
  display.setCursor(SUBMENU_VALUE_X, 16);
  display.print(wifiSettings.oscListenPort);
  display.setCursor(0, 24);
  //display.print(editingOscPort ? "R turn/click L cancel" : "R click edit L back");
  updatedisplay();
}

void drawPasswordEditor(void) {
  size_t characterCount = sizeof(passwordCharacters) - 1;
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("WiFi: ");
  display.print(selectedSsid);
  display.setCursor(0, 8);
  display.print("*Pass: ");
  int visibleStars = passwordLength;
  if (visibleStars > 10) {
    visibleStars = 10;
  }
  for (int index = 0; index < visibleStars; ++index) {
    display.print("*");
  }
  if (passwordLength > visibleStars) {
    display.print("+");
  }
  display.setCursor(0, 16);
  display.print("Next: ");
  if (passwordCharacter >= static_cast<int>(characterCount)) {
    display.print("DONE");
  } else if (passwordCharacters[passwordCharacter] == ' ') {
    display.print("SPACE");
  } else {
    display.print(passwordCharacters[passwordCharacter]);
  }
  display.setCursor(0, 24);
  display.print(passwordLength == 0 ? "Rturn pick, click add" : "Lturn deletes last");
  updatedisplay();
}

void beginPasswordEntry(const char *ssid) {
  snprintf(selectedSsid, sizeof(selectedSsid), "%s", ssid);
  memset(password, 0, sizeof(password));
  passwordLength = 0;
  passwordCharacter = 0;
  screen = PASSWORD_ENTRY;
  drawPasswordEditor();
}

GlobalSettingsAction connectCurrentNetwork(void) {
  if (!connectWifi(selectedSsid, password)) {
    drawMessage("Connection failed", "Retry password", PASSWORD_ENTRY);
    return GLOBAL_SETTINGS_NONE;
  }

  bool saved = rememberWifiNetwork(selectedSsid, password);
  memset(password, 0, sizeof(password));
  passwordLength = 0;
  drawMessage(saved ? "WiFi connected" : "Connected; save failed",
              saved ? "Network saved" : "Check LittleFS",
              WIFI_MENU);
  return GLOBAL_SETTINGS_WIFI_CONNECTED;
}

GlobalSettingsAction connectSelectedNetwork(void) {
  if (screen == WIFI_SAVED_LIST) {
    const WifiCredential &credential = wifiSettings.networks[selected];
    if (connectWifi(credential.ssid, credential.password)) {
      drawMessage("WiFi connected", "Using saved network", WIFI_MENU);
      return GLOBAL_SETTINGS_WIFI_CONNECTED;
    }
    beginPasswordEntry(credential.ssid);
    drawMessage("Saved password failed", "Enter it again", PASSWORD_ENTRY);
    return GLOBAL_SETTINGS_NONE;
  }

  String ssid = WiFi.SSID(selected);
  snprintf(selectedSsid, sizeof(selectedSsid), "%s", ssid.c_str());
  int savedIndex = findWifiNetwork(selectedSsid);
  if (savedIndex >= 0 &&
      connectWifi(wifiSettings.networks[savedIndex].ssid,
                  wifiSettings.networks[savedIndex].password)) {
    drawMessage("WiFi connected", "Using saved network", WIFI_MENU);
    return GLOBAL_SETTINGS_WIFI_CONNECTED;
  }
  beginPasswordEntry(selectedSsid);
  return GLOBAL_SETTINGS_NONE;
}

GlobalSettingsAction applyIpSettings(void) {
  if (!saveWifiSettings()) {
    drawMessage("Could not save IP", "Check LittleFS", IP_MENU);
    return GLOBAL_SETTINGS_NONE;
  }
  snapshotIpSettings();
  if (!connectSavedWifi()) {
    drawMessage("IP saved", "Reconnect failed", IP_MENU);
    return GLOBAL_SETTINGS_NONE;
  }
  drawMessage("IP settings applied", "WiFi connected", IP_MENU);
  return GLOBAL_SETTINGS_WIFI_CONNECTED;
}

void returnToWifiMenu(void) {
  screen = WIFI_MENU;
  selected = 0;
  drawWifiMenu();
}

}

void beginGlobalSettings(void) {
  screen = PREMENU;
  selected = 0;
  editingIpValue = false;
  editingOscPort = false;
  drawPremenu();
}

GlobalSettingsAction handleGlobalSettings(void) {
  int16_t movement = rmenuenc.getValue();
  bool rightClicked = rmenuenc.getButton() == ClickEncoder::Clicked;
  bool leftClicked = lmenuenc.getButton() == ClickEncoder::Clicked;

  if (screen == MESSAGE) {
    if (leftClicked) {
      screen = messageReturnScreen;
      if (screen == WIFI_MENU) {
        beginGlobalSettings();
      } else if (screen == PASSWORD_ENTRY) {
        returnToWifiMenu();
      } else if (screen == WIFI_SCAN_LIST || screen == WIFI_SAVED_LIST) {
        returnToWifiMenu();
      } else if (screen == IP_MENU) {
        restoreIpSettings();
        beginGlobalSettings();
      } else if (screen == OSC_MENU) {
        beginGlobalSettings();
      }
    } else if (rightClicked) {
      screen = messageReturnScreen;
      if (screen == PREMENU) {
        drawPremenu();
      } else if (screen == WIFI_MENU) {
        drawWifiMenu();
      } else if (screen == WIFI_SCAN_LIST || screen == WIFI_SAVED_LIST) {
        drawNetworkList();
      } else if (screen == IP_MENU) {
        drawIpMenu();
      } else if (screen == OSC_MENU) {
        drawOscMenu();
      } else if (screen == PASSWORD_ENTRY) {
        drawPasswordEditor();
      }
    }
    return GLOBAL_SETTINGS_NONE;
  }

  if (screen == PREMENU) {
    if (leftClicked) {
      return GLOBAL_SETTINGS_RETURN_TO_CONTROLLER;
    }
    if (movement != 0) {
      selected = constrain(selected + movement, 0, premenuCount - 1);
      drawPremenu();
    }
    if (!rightClicked) {
      return GLOBAL_SETTINGS_NONE;
    }
    if (selected == 0) {
      return GLOBAL_SETTINGS_OPEN_SAVELOAD;
    }
    if (selected == 1) {
      screen = WIFI_MENU;
      selected = 0;
      drawWifiMenu();
    } else if (selected == 2) {
      snapshotIpSettings();
      screen = IP_MENU;
      selected = 0;
      editingIpValue = false;
      drawIpMenu();
    } else {
      screen = OSC_MENU;
      selected = 0;
      editingOscPort = false;
      drawOscMenu();
    }
    return GLOBAL_SETTINGS_NONE;
  }

  if (screen == WIFI_MENU) {
    if (leftClicked) {
      beginGlobalSettings();
      return GLOBAL_SETTINGS_NONE;
    }
    if (movement != 0) {
      selected = constrain(selected + movement, 0, wifiMenuCount - 1);
      drawWifiMenu();
    }
    if (!rightClicked) {
      return GLOBAL_SETTINGS_NONE;
    }
    if (selected == 0) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Scanning WiFi...");
      updatedisplay();
      scanCount = scanWifiNetworks();
      selected = 0;
      if (scanCount <= 0) {
        drawMessage(scanCount == 0 ? "No networks found" : "WiFi scan failed",
                    "Try again from menu",
                    WIFI_MENU);
      } else {
        screen = WIFI_SCAN_LIST;
        drawNetworkList();
      }
    } else {
      if (wifiSettings.networkCount == 0) {
        drawMessage("No saved networks", "Scan to add WiFi", WIFI_MENU);
      } else {
        screen = WIFI_SAVED_LIST;
        selected = 0;
        drawNetworkList();
      }
    }
    return GLOBAL_SETTINGS_NONE;
  }

  if (screen == WIFI_SCAN_LIST || screen == WIFI_SAVED_LIST) {
    int count = currentListCount();
    if (leftClicked) {
      returnToWifiMenu();
      return GLOBAL_SETTINGS_NONE;
    }
    if (movement != 0 && count > 0) {
      selected = constrain(selected + movement, 0, count - 1);
      drawNetworkList();
    }
    if (rightClicked && count > 0) {
      return connectSelectedNetwork();
    }
    return GLOBAL_SETTINGS_NONE;
  }

  if (screen == PASSWORD_ENTRY) {
    size_t characterCount = sizeof(passwordCharacters) - 1;
    if (leftClicked) {
      returnToWifiMenu();
      return GLOBAL_SETTINGS_NONE;
    }
    int16_t eraseCount = lmenuenc.getValue();
    while (eraseCount < 0 && passwordLength > 0) {
      password[--passwordLength] = '\0';
      ++eraseCount;
    }
    if (movement != 0) {
      int characterChoices = characterCount + 1;
      passwordCharacter = (passwordCharacter + movement) % characterChoices;
      if (passwordCharacter < 0) {
        passwordCharacter += characterChoices;
      }
      drawPasswordEditor();
    }
    if (rightClicked) {
      if (passwordCharacter >= static_cast<int>(characterCount)) {
        return connectCurrentNetwork();
      }
      if (passwordLength < WIFI_PASSWORD_LENGTH) {
        password[passwordLength++] = passwordCharacters[passwordCharacter];
        password[passwordLength] = '\0';
      }
      drawPasswordEditor();
    }
    return GLOBAL_SETTINGS_NONE;
  }

  if (screen == IP_MENU) {
    if (editingIpValue) {
      if (leftClicked) {
        if (selected == 0) {
          wifiSettings.useStaticIp = originalStaticIpValue;
        } else {
          *octetValue(selected) = originalOctetValue;
        }
        editingIpValue = false;
        drawIpMenu();
        return GLOBAL_SETTINGS_NONE;
      }
      if (movement != 0) {
        if (selected == 0) {
          int value = (wifiSettings.useStaticIp ? 1 : 0) + movement;
          wifiSettings.useStaticIp = constrain(value, 0, 1) != 0;
        } else {
          int16_t *value = octetValue(selected);
          *value = constrain(*value + movement, 0, 255);
        }
        drawIpMenu();
      }
      if (rightClicked) {
        editingIpValue = false;
        drawIpMenu();
      }
      return GLOBAL_SETTINGS_NONE;
    }

    if (leftClicked) {
      restoreIpSettings();
      beginGlobalSettings();
      return GLOBAL_SETTINGS_NONE;
    }
    if (movement != 0) {
      selected = constrain(selected + movement, 0, ipMenuCount - 1);
      drawIpMenu();
    }
    if (!rightClicked) {
      return GLOBAL_SETTINGS_NONE;
    }
    if (selected < saveIpIndex) {
      editingIpValue = true;
      if (selected == 0) {
        originalStaticIpValue = wifiSettings.useStaticIp;
      } else {
        originalOctetValue = *octetValue(selected);
      }
      drawIpMenu();
    } else if (selected == saveIpIndex) {
      return applyIpSettings();
    }
    return GLOBAL_SETTINGS_NONE;
  }

  if (screen == OSC_MENU) {
    if (editingOscPort) {
      if (leftClicked) {
        *selectedOscPort() = originalOscPort;
        editingOscPort = false;
        drawOscMenu();
        return GLOBAL_SETTINGS_NONE;
      }
      if (movement != 0) {
        int port = static_cast<int>(*selectedOscPort()) + movement;
        *selectedOscPort() = constrain(port, 1, 65535);
        drawOscMenu();
      }
      if (rightClicked) {
        editingOscPort = false;
        if (!saveWifiSettings()) {
          *selectedOscPort() = originalOscPort;
          Serial.println("OSC settings: unable to save port");
          drawMessage("Could not save ports", "Check LittleFS", OSC_MENU);
          return GLOBAL_SETTINGS_NONE;
        }
        drawOscMenu();
        return GLOBAL_SETTINGS_OSC_SETTINGS_UPDATED;
      }
      return GLOBAL_SETTINGS_NONE;
    }

    if (leftClicked) {
      beginGlobalSettings();
      return GLOBAL_SETTINGS_NONE;
    }
    if (movement != 0) {
      selected = constrain(selected + movement, 0, 1);
      drawOscMenu();
    }
    if (rightClicked) {
      originalOscPort = *selectedOscPort();
      editingOscPort = true;
      drawOscMenu();
    }
    return GLOBAL_SETTINGS_NONE;
  }

  return GLOBAL_SETTINGS_NONE;
}
