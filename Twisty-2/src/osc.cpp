
#include "osc.h"
#include <WiFi.h>
#include <ArduinoOSCWiFi.h>
#include <math.h>

#include "wifisettings.h"

// ========================================
// OSC CONFIGURATION
// ========================================

// Change this to your norns IP address
const char* NORNS_IP = "192.168.18.84";

// ========================================
// RECEIVED VALUES
// ========================================

float oscReceivedFloat = 0.0;
int32_t oscReceivedInt = 0;
String oscReceivedString = "";

// ========================================
// INITIALISE OSC
// ========================================

void setupOSC() {

    static uint16_t subscribedPort = 0;
    if (subscribedPort == wifiSettings.oscListenPort) {
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("OSC: WiFi not connected");
        return;
    }

    Serial.println("Starting OSC...");
    if (subscribedPort != 0) {
        OscWiFi.unsubscribe(subscribedPort);
    }

    // Receive float
    OscWiFi.subscribe(
        wifiSettings.oscListenPort,
        "/twisty/test/float",
        [](float value) {

            oscReceivedFloat = value;

            Serial.print("OSC Float: ");
            Serial.println(value, 6);
        }
    );

    // Receive integer
    OscWiFi.subscribe(
        wifiSettings.oscListenPort,
        "/twisty/test/int",
        [](int32_t value) {

            oscReceivedInt = value;

            Serial.print("OSC Integer: ");
            Serial.println(value);
        }
    );

    // Receive string
    OscWiFi.subscribe(
        wifiSettings.oscListenPort,
        "/twisty/test/string",
        [](String value) {

            oscReceivedString = value;

            Serial.print("OSC String: ");
            Serial.println(value);
        }
    );

    Serial.print("OSC listening on port ");
    Serial.println(wifiSettings.oscListenPort);
    subscribedPort = wifiSettings.oscListenPort;
}

// ========================================
// OSC UPDATE
// ========================================

void loopOSC() {

    if (WiFi.status() == WL_CONNECTED) {
        OscWiFi.update();
    }
}

// ========================================
// SEND OSC
// ========================================

void sendOSCFloat(const char* address, float value) {
    if (WiFi.status() != WL_CONNECTED) return;

    OscWiFi.send(
        String(NORNS_IP),
        wifiSettings.oscSendPort,
        String(address),
        value
    );
}

void sendOSCInt(const char* address, int32_t value) {

    if (WiFi.status() != WL_CONNECTED) return;

    OscWiFi.send(
        String(NORNS_IP),
        wifiSettings.oscSendPort,
        String(address),
        value
    );
}

void sendOSCString(const char* address, const char* value) {

    if (WiFi.status() != WL_CONNECTED) return;

    OscWiFi.send(
        String(NORNS_IP),
        wifiSettings.oscSendPort,
        String(address),
        String(value)
    );
}

void sendOSCControlValue(const char* address, float value, oscvaluetype valueType) {
    switch (valueType) {
        case OSC_INTEGER: {
            int32_t integerValue = static_cast<int32_t>(value >= 0.0f ? value + 0.5f : value - 0.5f);
            sendOSCInt(address, integerValue);
            break;
        }
        case OSC_STRING: {
            String stringValue(value, 3);
            sendOSCString(address, stringValue.c_str());
            break;
        }
        case OSC_FLOAT:
        default:
            sendOSCFloat(address, value);
            break;
    }
}
