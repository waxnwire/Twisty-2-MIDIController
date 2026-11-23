
#include "osc.h"
#include <WiFi.h>
#include <ArduinoOSCWiFi.h>

// ========================================
// OSC CONFIGURATION
// ========================================

// Change this to your norns IP address
const char* NORNS_IP = "192.168.18.84";

const uint16_t NORNS_PORT = 10111;
const uint16_t TWISTY_OSC_PORT = 8000;

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

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("OSC: WiFi not connected");
        return;
    }

    Serial.println("Starting OSC...");

    // Receive float
    OscWiFi.subscribe(
        TWISTY_OSC_PORT,
        "/twisty/test/float",
        [](float value) {

            oscReceivedFloat = value;

            Serial.print("OSC Float: ");
            Serial.println(value, 6);
        }
    );

    // Receive integer
    OscWiFi.subscribe(
        TWISTY_OSC_PORT,
        "/twisty/test/int",
        [](int32_t value) {

            oscReceivedInt = value;

            Serial.print("OSC Integer: ");
            Serial.println(value);
        }
    );

    // Receive string
    OscWiFi.subscribe(
        TWISTY_OSC_PORT,
        "/twisty/test/string",
        [](String value) {

            oscReceivedString = value;

            Serial.print("OSC String: ");
            Serial.println(value);
        }
    );

    Serial.print("OSC listening on port ");
    Serial.println(TWISTY_OSC_PORT);
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
    Serial.println("sending a float");
    if (WiFi.status() != WL_CONNECTED) return;

    OscWiFi.send(
        String(NORNS_IP),
        NORNS_PORT,
        String(address),
        value
    );
}

void sendOSCInt(const char* address, int32_t value) {

    if (WiFi.status() != WL_CONNECTED) return;

    OscWiFi.send(
        String(NORNS_IP),
        NORNS_PORT,
        String(address),
        value
    );
}

void sendOSCString(const char* address, const char* value) {

    if (WiFi.status() != WL_CONNECTED) return;

    OscWiFi.send(
        String(NORNS_IP),
        NORNS_PORT,
        String(address),
        String(value)
    );
}
