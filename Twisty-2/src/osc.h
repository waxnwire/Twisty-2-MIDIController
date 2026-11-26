#pragma once

#include <Arduino.h>

#include "menu/controller.h"

void setupOSC();
void loopOSC();

void sendOSCFloat(const char* address, float value);
void sendOSCInt(const char* address, int32_t value);
void sendOSCString(const char* address, const char* value);
void sendOSCControlValue(const char* address, float value, oscvaluetype valueType);

extern float oscReceivedFloat;
extern int32_t oscReceivedInt;
extern String oscReceivedString;