#include <Arduino.h>
#include <Adafruit_SSD1306.h>

#include "encoderinput.h"
#include "encodermenu.h"
#include "menu/controller.h"
#include "wifisettings.h"

extern Adafruit_SSD1306 display;
extern void updatedisplay(void);

namespace {

enum EncoderMenuScreen {
  ENCODER_MENU_SCREEN,
  RANDOM_WALK_MENU_SCREEN,
  RANDOM_WALK_SPEED_EDIT
};

constexpr int menuLines = 3;
constexpr int encoderMenuCount = 4;
constexpr uint16_t minimumRandomWalkSpeedMs = 10;
constexpr uint16_t maximumRandomWalkSpeedMs = 5000;
constexpr uint16_t randomWalkSpeedStepMs = 10;

EncoderMenuScreen screen = ENCODER_MENU_SCREEN;
int selected = 0;
int selectedOutput = MIDI_OUTPUT;
uint16_t originalRandomWalkSpeedMs = 0;

void drawMenu(const char *title, const char *const *items, int count) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(title);

  int first = (selected / menuLines) * menuLines;
  for (int row = 0; row < menuLines && first + row < count; ++row) {
    int item = first + row;
    display.setCursor(0, 8 + row * 8);
    display.print(item == selected ? ">" : " ");
    display.print(items[item]);
  }
  updatedisplay();
}

void drawEncoderMenu(void) {
  const char *items[] = {
      "Settings",
      selectedOutput == MIDI_OUTPUT ? "MIDI Encoder" : "OSC Encoder",
      selectedOutput == MIDI_OUTPUT ? "MIDI Switch" : "OSC Switch",
      "Wobble Speed"};
  drawMenu("ENCODER MENU", items, encoderMenuCount);
}

void drawRandomWalkMenu(void) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Wobble / Ramp");
  display.setCursor(0, 8);
  display.print(screen == RANDOM_WALK_SPEED_EDIT ? "*" : ">");
  display.print("Speed");
  display.setCursor(90, 8);
  display.printf("%ums", wifiSettings.randomWalkSpeedMs);
  display.setCursor(0, 24);
  //display.print(screen == RANDOM_WALK_SPEED_EDIT
  //                  ? "R turn/click; L cancel"
  //                  : "R click edit; L back");
  updatedisplay();
}

}

void beginEncoderMenu(int output) {
  screen = ENCODER_MENU_SCREEN;
  selected = 0;
  selectedOutput = output;
  drawEncoderMenu();
}

EncoderMenuAction handleEncoderMenu(void) {
  int16_t movement = rmenuenc.getValue();
  bool rightClicked = rmenuenc.getButton() == ClickEncoder::Clicked;
  bool leftClicked = lmenuenc.getButton() == ClickEncoder::Clicked;

  if (screen == ENCODER_MENU_SCREEN) {
    if (leftClicked) {
      return ENCODER_MENU_RETURN_TO_CONTROLLER;
    }
    if (movement != 0) {
      selected = constrain(selected + movement, 0, encoderMenuCount - 1);
      drawEncoderMenu();
    }
    if (!rightClicked) {
      return ENCODER_MENU_NONE;
    }
    switch (selected) {
      case 0:
        return ENCODER_MENU_EDIT_SETTINGS;
      case 1:
        return selectedOutput == MIDI_OUTPUT ? ENCODER_MENU_EDIT_MIDI_ENCODER
                                             : ENCODER_MENU_EDIT_OSC_ENCODER;
      case 2:
        return selectedOutput == MIDI_OUTPUT ? ENCODER_MENU_EDIT_MIDI_SWITCH
                                             : ENCODER_MENU_EDIT_OSC_SWITCH;
      default:
        screen = RANDOM_WALK_MENU_SCREEN;
        selected = 0;
        drawRandomWalkMenu();
        return ENCODER_MENU_NONE;
    }
  }

  if (screen == RANDOM_WALK_MENU_SCREEN) {
    if (leftClicked) {
      screen = ENCODER_MENU_SCREEN;
      selected = 3;
      drawEncoderMenu();
      return ENCODER_MENU_NONE;
    }
    if (movement != 0) {
      selected = 0;
    }
    if (rightClicked) {
      originalRandomWalkSpeedMs = wifiSettings.randomWalkSpeedMs;
      screen = RANDOM_WALK_SPEED_EDIT;
    }
    drawRandomWalkMenu();
    return ENCODER_MENU_NONE;
  }

  if (leftClicked) {
    wifiSettings.randomWalkSpeedMs = originalRandomWalkSpeedMs;
    screen = RANDOM_WALK_MENU_SCREEN;
    drawRandomWalkMenu();
    return ENCODER_MENU_NONE;
  }
  if (movement != 0) {
    int speed = static_cast<int>(wifiSettings.randomWalkSpeedMs) +
                movement * randomWalkSpeedStepMs;
    wifiSettings.randomWalkSpeedMs =
        constrain(speed, minimumRandomWalkSpeedMs, maximumRandomWalkSpeedMs);
    drawRandomWalkMenu();
  }
  if (rightClicked) {
    screen = RANDOM_WALK_MENU_SCREEN;
    if (!saveWifiSettings()) {
      Serial.println("Random Walk: unable to save speed setting");
    }
    drawRandomWalkMenu();
  }
  return ENCODER_MENU_NONE;
}
