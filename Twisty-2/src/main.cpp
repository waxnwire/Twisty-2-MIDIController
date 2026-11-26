
// Copyright 2023 Rich Heslip
//
// Author: Rich Heslip 
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
/*
 * Twisty 2 MIDI controller using RP2040 or RP2350
 * uses 16 multiplexed encoders for encoder and button input with RGB LEDs
 * two encoders for menu inputs
 * I2C OLED display
 * R Heslip Dec 2025 
 * based on my Twisty MIDI controller code and other stuff
 * 
 */


#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_TinyUSB.h>  
#include <MIDI.h>
#include "encoderinput.h"
//#include "StepSeq.h"
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include "LittleFS.h"
#include <Control_Surface.h>
#include "webBrowser.h"
#include "osc.h"
#include "encodercontrol.h"
#include "encodermenu.h"
#include "menusystem.h"
#include "menudata.h"
#include "globalsettings.h"
#include "wifisettings.h"

#ifndef BLUETOOTH
#define BLUETOOTH  // define for bluetooth MIDI - you MUST set up IPV4 + Bluetooth stack in the arduino Tools menu
#endif
#ifdef BLUETOOTH
#include <MIDI_Interfaces/BluetoothMIDI_Interface.hpp>
#endif

#define TRUE 1
#define FALSE 0

#define PIN_WIRE_SDA 2
#define PIN_WIRE_SCL 3
#define LEDPIN 4

#define NUMPIXELS 16 // 
#define LED_RED 0x1f0000  // only using 5 bits to keep LEDs from getting too bright
#define LED_GREEN 0x001f00
#define LED_BLUE 0x00001f
#define LED_ORANGE (LED_RED|LED_GREEN)
#define LED_VIOLET (LED_RED|LED_BLUE)
#define LED_AQUA (LED_GREEN|LED_BLUE)
#define LED_BLACK 0
#define LED_WHITE (LED_RED|LED_GREEN|LED_BLUE)

int32_t colorpalette[8]={LED_RED,LED_ORANGE,LED_GREEN,LED_AQUA,LED_BLUE,LED_VIOLET,LED_WHITE,LED_BLACK}; // limited color palette makes things easier

int8_t brightness_table[32] ={  // more or less exponential table to compensate for perceived brightness
  1,1,2,2,
  3,3,4,4,
  6,6,8,8,
  12,12,16,16,
  25,25,34,34,
  44,44,58,58,
  72,72,96,96,
  110,110,127,127
};

Adafruit_NeoPixel LEDS(NUMPIXELS, LEDPIN, NEO_GRB + NEO_KHZ800);

#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 32  // OLED display height, in pixels
#define SCREEN_BUFFER_SIZE (SCREEN_WIDTH * ((SCREEN_HEIGHT + 7) / 8))
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);

#define DISPLAY_BLANK_MS 120*1000  // display blanking time
#define LEDFLASH_EDIT  250   // LED flash while editing
int32_t displaytimer ; // display blanking timer
int32_t LEDtimer; // LED flash timer
bool LEDstate;   // for LED flash
#define OLED_DISPLAY   // for graphics conditionals

// use Control Surface MIDI
USBMIDI_Interface usbMIDI;

HardwareSerialMIDI_Interface serialMIDI {Serial1, MIDI_BAUD};

#ifdef BLUETOOTH
// Instantiate a MIDI over BLE interface
BluetoothMIDI_Interface bleMIDI;
#endif

#define DEFAULT_VELOCITY 127
uint16_t page=0; // CC page 0-3
uint8_t lastnotesent=0; // keeps track of last note sent when switch sends note messages
bool displaySwitchLEDs;    // toggle to show switch or encoder states

// Forward declarations
void updatedisplay();
void save_restore();

// values use in save/retore menu
int16_t saverestore_slot=1;
int16_t saverestore_action=0; // initial setting is restore
int16_t saverestore_confirm=0; // take/don't take action

enum control {ENCODER,BUTTON};
int16_t lastcontrol=0; // keeps track of last used control

enum ui_states {UI_SEND_MIDI,UI_EDIT,UI_LOADSAVE,UI_PREMENU,UI_ENCODER_MENU};
int16_t UI_state=UI_SEND_MIDI;
bool returnToPremenu=false;
bool returnToEncoderMenu=false;
struct menu controllerEditMenu[] = {{"", nullptr, 0, 0}};
bool pendingSwitchClick[NUMENCODERS] = {};
bool randomWidthEditActive[NUMENCODERS] = {};
bool momentarySwitchActive[NUMENCODERS] = {};
float randomWalkValue[CONTROLLER_PAGES][NUMENCODERS] = {};
bool randomWalkInitialized[CONTROLLER_PAGES][NUMENCODERS] = {};
bool switchActionActive[CONTROLLER_PAGES][NUMENCODERS] = {};
bool switchRampRunning[CONTROLLER_PAGES][NUMENCODERS] = {};
float switchReturnValue[CONTROLLER_PAGES][NUMENCODERS] = {};
float switchRampTarget[CONTROLLER_PAGES][NUMENCODERS] = {};
uint32_t lastRandomWalkStep = 0;
uint32_t lastSwitchRampStep = 0;

#include "fileio.h"

 // midi related stuff

void sendnoteOn(uint8_t channel,uint8_t pitch, uint8_t velocity) {
  MIDIAddress midiaddress ={pitch,Channel_1 + (channel-1)}; // control surface library requires this form of MIDI addressing -I'm not a fan of the design but its the only Arduino BLE MIDI library I could find
  usbMIDI.sendNoteOn(midiaddress, velocity);
  serialMIDI.sendNoteOn(midiaddress, velocity);
#ifdef BLUETOOTH
  bleMIDI.sendNoteOn(midiaddress, velocity);
#endif
}

void sendnoteOff(uint8_t channel, uint8_t pitch,uint8_t velocity) {
  MIDIAddress midiaddress= {pitch,Channel_1 + (channel-1)};
  usbMIDI.sendNoteOff(midiaddress, velocity);
  serialMIDI.sendNoteOff(midiaddress, velocity);
#ifdef BLUETOOTH
  bleMIDI.sendNoteOff(midiaddress, velocity);
#endif
}

// message 0x0B control change.
// 2nd parameter is the control number number (0-119).
// 3rd parameter is the control value (0-127).

void sendcontrolChange(uint8_t channel, uint8_t control, uint8_t value) {
  MIDIAddress midiaddress= {control,Channel_1 + (channel-1)};  
  usbMIDI.sendControlChange(midiaddress, value); 
  serialMIDI.sendControlChange(midiaddress, value); 
#ifdef BLUETOOTH
  bleMIDI.sendControlChange(midiaddress, value); 
#endif
}

// message program change.
// 2nd parameter is the PC value (0-127).

void sendprogramChange(uint8_t channel, uint8_t value) {
  MIDIAddress midiaddress= {value,Channel_1 + (channel-1)};  // confusing way of sending MIDI messages
  usbMIDI.sendProgramChange(midiaddress); 
  serialMIDI.sendProgramChange(midiaddress); 
#ifdef BLUETOOTH
  bleMIDI.sendProgramChange(midiaddress); 
#endif
}

// turn off the display to avoid burn in but keep the display buffer intact
// the next call to display.display() will automagically restore what was there plus whatver has been drawn since

void blankdisplay(void) {
  uint8_t tempbuf[SCREEN_BUFFER_SIZE];
  uint8_t * buf, * tbuf;
  buf=display.getBuffer();
  tbuf= tempbuf;
  memcpy(tbuf,buf,SCREEN_BUFFER_SIZE); // copy frame buffer to temp storage
  display.clearDisplay();
  display.display();  // turn off the OLEDs
  memcpy(buf,tbuf,SCREEN_BUFFER_SIZE); // restore the old frame buffer
}

// update the display and reset the display blanking timer
void updatedisplay(){
  display.display();
  displaytimer=millis();
}

// show CC value of encoder on display
void showencodercc(int16_t page, int16_t encoder){
  display.setTextSize(2);
  display.setCursor(0,16);
  display.print("              ");
  display.setCursor(0,16);
  if (controls[page].encoder[encoder].output == OSC_OUTPUT) {
    display.printf("OSC %.2f", controls[page].encoder[encoder].oscValueMilli / 1000.0f);
    display.setTextSize(1);
    updatedisplay();
    return;
  }
  // the default label 0 "CC" is a special case where we show the CC number in large font, otherwise we show a custom label
  if (controls[page].encoder[encoder].labelindex ==1){
    display.printf("%s %d %d","CC",controls[page].encoder[encoder].ccnumber,controls[page].encoder[encoder].value);
  } else{
    display.printf("%s %d",labels[controls[page].encoder[encoder].labelindex],controls[page].encoder[encoder].value);
  }
  display.setTextSize(1);
  updatedisplay();  
}

// show CC value of switch on display
void showswitchcc(int16_t page, int16_t button){
  display.setTextSize(2);
  display.setCursor(0,16);
  display.print("              ");
  display.setCursor(0,16);
  // the default label 0 "CC" is a special case where we show the CC number in large font, otherwise we show a custom label
  // ADD IN HERE ABOUT LABEL == 0 FOR OTHER 
  if (controls[page].encswitch[button].labelindex ==1){
    // CC message type - show CC number and value
    display.printf("%s %d %d","CC",controls[page].encswitch[button].ccnumber,controls[page].encswitch[button].value);
  }else{
    display.printf("%s %d",labels[controls[page].encswitch[button].labelindex],controls[page].encswitch[button].value);
  } 
  display.setTextSize(1);
  updatedisplay();  
}

// show page number on display
void showpage(int16_t page){
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("                     "); // erase the whole line
  display.setCursor(0,0);
  display.printf("Pg %d",page);
  display.setTextSize(1);
//  updatedisplay();  
}

// show CC number on display
void showcc(int16_t cc){
  display.setTextSize(1);
  display.setCursor(32,0);
  display.printf("CC %d",cc);
//  updatedisplay();  
}
// show MIDI channel on display
void showchannel(int16_t channel){
  display.setTextSize(1);
  display.setCursor(80,0);
  display.printf("Ch %d",channel);
  updatedisplay(); 
}

// show encoder values
void showencoder(int16_t page,int16_t controlindex) {
    showpage(page+1);  // for display use 1 based indices
    if (controls[page].encoder[controlindex].output == OSC_OUTPUT) {
      display.setCursor(32,0);
      display.print("OSC");
      display.setCursor(80,0);
      display.print("      ");
    } else {
      showcc(controls[page].encoder[controlindex].ccnumber);
      showchannel(controls[page].encoder[controlindex].channel);
    }
    showencodercc(page,controlindex);
}

void showEncoderWidth(int16_t page, int16_t encoder, bool editing) {
  display.setTextSize(1);
  display.setCursor(0, 8);
  display.print("                    ");
  if (editing) {
    display.setCursor(0, 8);
    display.printf("Width %.2f", controls[page].encoder[encoder].randomWalkWidth);
  }
  updatedisplay();
}

// show encoder switch mode and value
// this got a bit messy after I added multiple switch types
void showswitch(int16_t page,int16_t controlindex) {
  showpage(page+1);  // for display use 1 based indices
  display.setCursor(32,0);
  if (controls[page].encoder[controlindex].output == OSC_OUTPUT) {
    display.print("OSC");
  } else switch (controls[page].encswitch[controlindex].type) {
    case CCMESSAGE:
      display.printf("CC %d",controls[page].encswitch[controlindex].ccnumber);
      break;
    case PCMESSAGE:
      display.printf("Program");
      break;   
    case NOTEMESSAGE:
      display.printf("Note");
      break;   
    case SETENC:
      display.printf("SetEnc");
      break;
    default:
      break;      
  }
  if (controls[page].encoder[controlindex].output == OSC_OUTPUT) {
    display.setCursor(80,0);
    display.print("      ");
  } else if (controls[page].encswitch[controlindex].type == SETENC) {
    showchannel(controls[page].encoder[controlindex].channel);
  } else {
    showchannel(controls[page].encswitch[controlindex].channel);
  }
  display.setTextSize(2);
  display.setCursor(0,16);
  display.print("              ");
  display.setCursor(0,16);
  if (controls[page].encoder[controlindex].output == OSC_OUTPUT) {
    display.printf("OSC %d", controls[page].encswitch[controlindex].value ==
                                 controls[page].encswitch[controlindex].maxvalue);
  } else switch (controls[page].encswitch[controlindex].type) {
    case CCMESSAGE:
  // the default label 0 "CC" is a special case where we show the CC number in large font, otherwise we show a custom label
    if (controls[page].encswitch[controlindex].labelindex ==0) display.printf("%s %d %d","CC",controls[page].encswitch[controlindex].ccnumber,controls[page].encswitch[controlindex].value);
    else display.printf("%s %d",labels[controls[page].encswitch[controlindex].labelindex],controls[page].encswitch[controlindex].value);
    break;
    case PCMESSAGE:
      if (controls[page].encswitch[controlindex].labelindex ==0) display.printf("%s %d","Prog",controls[page].encswitch[controlindex].value);
      else display.printf("%s %d",labels[controls[page].encswitch[controlindex].labelindex],controls[page].encswitch[controlindex].value);
      break;
    case NOTEMESSAGE:
      if (controls[page].encswitch[controlindex].labelindex ==0) display.printf("%s %d","Note",controls[page].encswitch[controlindex].value);
      else display.printf("%s %d",labels[controls[page].encswitch[controlindex].labelindex],controls[page].encswitch[controlindex].value);
      break;  
    case SETENC:  // show the encoder value since it was just set
      showencodercc(page,controlindex);
      break;  
    default:
      break;
  }       
  display.setTextSize(1);
  updatedisplay();  
}

// show the LED associated with a encoder
void showencoderLED(int16_t page,int16_t encoder) {
  int32_t color=colorpalette[controls[page].encoder[encoder].colorindex];
  color=color/31; // scale color down to 1 bit **** this isn't going to work with custom colors
  int value = controls[page].encoder[encoder].value;
  if (controls[page].encoder[encoder].output == OSC_OUTPUT) {
    int oscValue = controls[page].encoder[encoder].oscValueMilli;
    if (controls[page].encoder[encoder].oscRange == OSC_RANGE_0_TO_1) {
      value = oscValue * 127 / 1000;
    } else if (controls[page].encoder[encoder].oscRange == OSC_RANGE_NEG1_TO_1) {
      value = (oscValue + 1000) * 127 / 2000;
    } else {
      value = 64;
    }
  }
  value = constrain(value, 0, 127);
  color=color*brightness_table[(value>>2 & 0x1f)];
  LEDS.setPixelColor(encoder,color);
}

// show the LED associated with a switch
void showswitchLED(int16_t page,int16_t button) {
  if (controls[page].encswitch[button].value == controls[page].encswitch[button].maxvalue) LEDS.setPixelColor(button,LED_WHITE);
  else LEDS.setPixelColor(button,LED_BLACK);
}

void sendEncoderOutput(int16_t page, int16_t index, float value) {
  controllerencoder &encoder = controls[page].encoder[index];
  if (encoder.output == OSC_OUTPUT) {
    if (encoder.oscAddress[0] != '\0') {
      sendOSCControlValue(encoder.oscAddress, value, static_cast<oscvaluetype>(encoder.oscValueType));
    }
    return;
  }
  int midiValue = constrain(static_cast<int>(value + (value >= 0.0f ? 0.5f : -0.5f)), 0, 127);
  sendcontrolChange(encoder.channel, encoder.ccnumber, midiValue);
}

void sendSwitchOutput(int16_t page, int16_t index, bool active) {
  controllerswitch &controlSwitch = controls[page].encswitch[index];
  if (controls[page].encoder[index].output == OSC_OUTPUT) {
    if (controlSwitch.oscAddress[0] != '\0') {
      sendOSCControlValue(controlSwitch.oscAddress, active ? 1.0f : 0.0f,
                          static_cast<oscvaluetype>(controlSwitch.oscValueType));
    }
    return;
  }

  switch (controlSwitch.type) {
    case CCMESSAGE:
      sendcontrolChange(controlSwitch.channel, controlSwitch.ccnumber, controlSwitch.value);
      break;
    case PCMESSAGE:
      sendprogramChange(controlSwitch.channel, controlSwitch.value);
      break;
    case NOTEMESSAGE:
      if (active) {
        sendnoteOn(controlSwitch.channel, controlSwitch.value, DEFAULT_VELOCITY);
        lastnotesent = controlSwitch.value;
      } else {
        sendnoteOff(controlSwitch.channel, lastnotesent, 0);
      }
      break;
    default:
      break;
  }
}

float encoderCenter(int16_t page, int16_t index);
void showSwitchState(int16_t page, int16_t index);

float switchTargetValue(int16_t page, int16_t index) {
  const controllerencoder &encoder = controls[page].encoder[index];
  const controllerswitch &controlSwitch = controls[page].encswitch[index];
  float midiValue = constrain(controlSwitch.maxvalue, 0, 127);
  if (encoder.output == MIDI_OUTPUT) {
    return midiValue;
  }
  float normalized = midiValue / 127.0f;
  return encoder.oscRange == OSC_RANGE_0_TO_1 ? normalized
                                               : normalized * 2.0f - 1.0f;
}

void setEncoderOutputValue(int16_t page, int16_t index, float value) {
  controllerencoder &encoder = controls[page].encoder[index];
  if (encoder.output == MIDI_OUTPUT) {
    int minimum = encoder.minvalue < encoder.maxvalue ? encoder.minvalue : encoder.maxvalue;
    int maximum = encoder.minvalue > encoder.maxvalue ? encoder.minvalue : encoder.maxvalue;
    encoder.value = constrain(static_cast<int>(value + 0.5f), minimum, maximum);
    sendEncoderOutput(page, index, encoder.value);
  } else {
    int minimum = encoder.oscRange == OSC_RANGE_0_TO_1 ? 0 : -1000;
    int maximum = 1000;
    encoder.oscValueMilli =
        constrain(static_cast<int>(value * 1000.0f), minimum, maximum);
    sendEncoderOutput(page, index, encoder.oscValueMilli / 1000.0f);
  }
  randomWalkInitialized[page][index] = false;
  showencoderLED(page, index);
  if (page == ::page && UI_state == UI_SEND_MIDI) {
    showencoder(page, index);
  }
}

void setSwitchAction(int16_t page, int16_t index, bool active) {
  controllerswitch &controlSwitch = controls[page].encswitch[index];
  bool wasActive = switchActionActive[page][index];
  controlSwitch.value = active ? controlSwitch.maxvalue : controlSwitch.minvalue;

  if (active && !wasActive) {
    switchReturnValue[page][index] = encoderCenter(page, index);
  }
  switchActionActive[page][index] = active;

  if (controlSwitch.function == SWITCH_JUMP) {
    setEncoderOutputValue(page, index,
                          active ? switchTargetValue(page, index)
                                 : switchReturnValue[page][index]);
  } else if (controlSwitch.function == SWITCH_RAMP) {
    switchRampTarget[page][index] =
        active ? switchTargetValue(page, index) : switchReturnValue[page][index];
    switchRampRunning[page][index] = true;
  } else {
    if (!active && wasActive && randomWalkInitialized[page][index]) {
      setEncoderOutputValue(page, index, randomWalkValue[page][index]);
    }
    randomWalkInitialized[page][index] = false;
  }

  sendSwitchOutput(page, index, active);
  showSwitchState(page, index);
}

void showSwitchState(int16_t page, int16_t index) {
  lastcontrol = index;
  showswitchLED(page, index);
  showswitch(page, index);
  updatedisplay();
}

void clickSwitch(int16_t page, int16_t index) {
  controllerswitch &controlSwitch = controls[page].encswitch[index];
  if (controlSwitch.mode == MOMENTARY) {
    setSwitchAction(page, index, true);
    setSwitchAction(page, index, false);
  } else {
    setSwitchAction(page, index, !switchActionActive[page][index]);
  }
}

void beginMomentarySwitch(int16_t page, int16_t index) {
  setSwitchAction(page, index, true);
  momentarySwitchActive[index] = true;
}

void endMomentarySwitch(int16_t page, int16_t index) {
  setSwitchAction(page, index, false);
  momentarySwitchActive[index] = false;
}

float encoderCenter(int16_t page, int16_t index) {
  const controllerencoder &encoder = controls[page].encoder[index];
  return encoder.output == OSC_OUTPUT
             ? encoder.oscValueMilli / 1000.0f
             : static_cast<float>(encoder.value);
}

void synchronizeSwitchActions(void) {
  for (int16_t targetPage = 0; targetPage < CONTROLLER_PAGES; ++targetPage) {
    for (int16_t index = 0; index < NUMENCODERS; ++index) {
      controllerswitch &controlSwitch = controls[targetPage].encswitch[index];
      controlSwitch.output = controls[targetPage].encoder[index].output;
      bool active = controlSwitch.mode == TOGGLE &&
                    controlSwitch.value == controlSwitch.maxvalue;
      switchActionActive[targetPage][index] = active;
      switchReturnValue[targetPage][index] = encoderCenter(targetPage, index);
      switchRampTarget[targetPage][index] =
          active ? switchTargetValue(targetPage, index)
                 : switchReturnValue[targetPage][index];
      switchRampRunning[targetPage][index] = false;
      randomWalkInitialized[targetPage][index] = false;
    }
  }
}

void restoreCurrentEditBuffer(void) {
  int16_t previousOutput = controls[page].encoder[lastcontrol].output;
  bool outputChanged = previousOutput != editbuffer.encoder.output;
  if (outputChanged && switchActionActive[page][lastcontrol]) {
    setSwitchAction(page, lastcontrol, false);
  }
  restore_from_editbuffer(page, lastcontrol);
  if (outputChanged) {
    controllerswitch &controlSwitch = controls[page].encswitch[lastcontrol];
    controlSwitch.value = controlSwitch.minvalue;
    switchActionActive[page][lastcontrol] = false;
    switchRampRunning[page][lastcontrol] = false;
    randomWalkInitialized[page][lastcontrol] = false;
    switchReturnValue[page][lastcontrol] = encoderCenter(page, lastcontrol);
  }
}

void processRandomWalk(void) {
  uint32_t now = millis();
  uint16_t speed = wifiSettings.randomWalkSpeedMs;
  if (now - lastRandomWalkStep < speed) {
    return;
  }
  lastRandomWalkStep = now;

  for (int16_t index = 0; index < NUMENCODERS; ++index) {
    controllerencoder &encoder = controls[page].encoder[index];
    if (controls[page].encswitch[index].function != SWITCH_WOBBLE ||
        !switchActionActive[page][index] || encoder.randomWalkWidth <= 0.0f) {
      randomWalkInitialized[page][index] = false;
      continue;
    }

    float center = encoderCenter(page, index);
    float minimum;
    float maximum;
    float step;
    if (encoder.output == OSC_OUTPUT) {
      step = 0.01f;
      if (encoder.oscRange == OSC_RANGE_0_TO_1) {
        minimum = 0.0f;
        maximum = 1.0f;
      } else {
        minimum = -1.0f;
        maximum = 1.0f;
      }
      if (encoder.oscRange == OSC_RANGE_DELTA) {
        center = 0.0f;
      }
    } else {
      step = 1.0f;
      minimum = encoder.minvalue < encoder.maxvalue ? encoder.minvalue : encoder.maxvalue;
      maximum = encoder.minvalue > encoder.maxvalue ? encoder.minvalue : encoder.maxvalue;
    }

    float lower = minimum > center - encoder.randomWalkWidth
                      ? minimum
                      : center - encoder.randomWalkWidth;
    float upper = maximum < center + encoder.randomWalkWidth
                      ? maximum
                      : center + encoder.randomWalkWidth;
    float &value = randomWalkValue[page][index];
    if (!randomWalkInitialized[page][index] || value < lower || value > upper) {
      value = center;
      randomWalkInitialized[page][index] = true;
    }
    if (lower < upper) {
      float next = value + (random(0, 2) == 0 ? -step : step);
      if (next < lower) {
        next = upper < value + step ? upper : value + step;
      } else if (next > upper) {
        next = lower > value - step ? lower : value - step;
      }
      value = next;
    }
    sendEncoderOutput(page, index, value);
  }
}

void processSwitchRamps(void) {
  uint32_t now = millis();
  uint16_t speed = wifiSettings.randomWalkSpeedMs;
  if (now - lastSwitchRampStep < speed) {
    return;
  }
  lastSwitchRampStep = now;

  for (int16_t index = 0; index < NUMENCODERS; ++index) {
    if (!switchRampRunning[page][index]) {
      continue;
    }
    float current = encoderCenter(page, index);
    float step = controls[page].encoder[index].output == OSC_OUTPUT ? 0.01f : 1.0f;
    float difference = switchRampTarget[page][index] - current;
    if (difference > step) {
      current += step;
    } else if (difference < -step) {
      current -= step;
    } else {
      current = switchRampTarget[page][index];
      switchRampRunning[page][index] = false;
    }
    setEncoderOutputValue(page, index, current);
  }
}

// show all the encoder LEDs on a page
void showencoderLEDs(int16_t page) {
  for (int16_t i=0;i< NUMPIXELS;++i) showencoderLED(page,i);
}

float getWebEncoderCurrentValue(int16_t targetPage, int16_t index) {
  if (randomWalkInitialized[targetPage][index]) {
    return randomWalkValue[targetPage][index];
  }
  const controllerencoder &encoder = controls[targetPage].encoder[index];
  if (encoder.output == OSC_OUTPUT && encoder.oscRange == OSC_RANGE_DELTA) {
    return 0.0f;
  }
  return encoderCenter(targetPage, index);
}

void notifyWebControlUpdated(int16_t targetPage, int16_t index, bool encoder,
                             bool sendOutput) {
  if (encoder) {
    randomWalkInitialized[targetPage][index] = false;
    if (sendOutput) {
      sendEncoderOutput(targetPage, index, encoderCenter(targetPage, index));
    }
  } else if (sendOutput) {
    controllerswitch &controlSwitch = controls[targetPage].encswitch[index];
    sendSwitchOutput(targetPage, index,
                     controlSwitch.value == controlSwitch.maxvalue);
  }
  if (targetPage != page || UI_state != UI_SEND_MIDI) {
    return;
  }
  if (encoder) {
    showencoderLED(targetPage, index);
    if (!displaySwitchLEDs && lastcontrol == index) {
      showencoder(targetPage, index);
    }
  } else {
    showswitchLED(targetPage, index);
    if (displaySwitchLEDs && lastcontrol == index) {
      showswitch(targetPage, index);
    }
  }
  LEDS.show();
}

void prepareWebConfigurationLoad(void) {
  for (int16_t i = 0; i < NUMENCODERS; ++i) {
    if (momentarySwitchActive[i]) {
      endMomentarySwitch(page, i);
    }
    pendingSwitchClick[i] = false;
    randomWidthEditActive[i] = false;
    momentarySwitchActive[i] = false;
  }
}

void notifyWebConfigurationLoaded(void) {
  synchronizeSwitchActions();
  for (int16_t p = 0; p < CONTROLLER_PAGES; ++p) {
    for (int16_t i = 0; i < NUMENCODERS; ++i) {
      randomWalkInitialized[p][i] = false;
      randomWalkValue[p][i] = encoderCenter(p, i);
    }
  }
  if (UI_state == UI_SEND_MIDI) {
    showencoderLEDs(page);
    if (displaySwitchLEDs) {
      showswitch(page, lastcontrol);
    } else {
      showencoder(page, lastcontrol);
    }
    LEDS.show();
    updatedisplay();
  }
}

// flush all encoder messages 
// pending messages cause race conditions in the menus sometimes 
void flush_encoders(void) {
  ClickEncoder::Button button;
  ClickEncoder::ButtonEvent event;  
  for (int i=0;i<NUMENCODERS;++i) {  
    enc[i].getButton();
    enc[i].getButtonEvent();
    enc[i].getValue();
  }
  lmenuenc.getButton();
  lmenuenc.getButtonEvent();
  lmenuenc.getValue();
  rmenuenc.getButton();
  rmenuenc.getButtonEvent();
  rmenuenc.getValue();
}

void fatalerror(const char * errorstring){
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.printf("FATAL ERROR\n\n%s", errorstring);
  updatedisplay();

  while (1) {
    for (int16_t i=0;i<NUMENCODERS;++i) LEDS.setPixelColor(i,LED_RED); // flashing red
    LEDS.show();
    delay(75);
    for (int16_t i=0;i<NUMENCODERS;++i) LEDS.setPixelColor(i,LED_BLACK); // 
    LEDS.show();
    delay(75);
  } 
}

// ***** Menu handler functions *****

// menu function to handle save/restore menus - called when user clicks "Confirm?" menu value
void save_restore(void) {
  display.clearDisplay();
  display.setCursor(0,12);
  if ((saverestore_action == 1) && (saverestore_confirm ==1)) {
    if (saveconfig(saverestore_slot)) display.printf("Saved to Slot %d", saverestore_slot);
    else display.printf("File Write Error");
  } 
  if ((saverestore_action == 0) && (saverestore_confirm ==1)) {
    if (loadconfig(saverestore_slot)) {
      synchronizeSwitchActions();
      display.printf("Restored from Slot %d", saverestore_slot);
    } else display.printf("File Read Error");
  }
  if ((saverestore_action == 2) && (saverestore_confirm ==1)) {
    LittleFS.format();
    display.printf("FFS ReFormatted");       
  }
  if (saverestore_confirm == 0) display.printf("Aborted Save/Restore"); 
  display.display();
  saverestore_action=saverestore_confirm=0; // reset the menu
  page=lastcontrol=0;
  showencoderLEDs(page); // update the LEDs
  LEDS.show();
  delay(3000);     // delay here to show above fail/success message
  display.clearDisplay();
  showencoder(page,lastcontrol);   // restore the display
  updatedisplay();
  flush_encoders();   // toss any encoder messages
  UI_state=UI_SEND_MIDI;
}

// Custom MIDI callback that prints incoming messages.
struct MyMIDI_Callbacks : FineGrainedMIDI_Callbacks<MyMIDI_Callbacks> {
  void onControlChange(Channel channel, uint8_t controller, uint8_t value, Cable cable) {
    //set the LEDs to the value of the CC message for feedback
    controls[page].encoder[lastcontrol].value=value; // update the encoder value for feedback
    showencoderLED(page,lastcontrol); // update the LED for feedback
    showencoder(page,lastcontrol); // update the display for feedback
    updatedisplay();
    Serial << "Control Change: " << channel << ", controller " << controller
           << ", value " << value << ", " << cable << endl;
  }
} midiCallbacks;

void setup() {
  Serial.begin(115200);
 // while (!Serial);  // wait for USB connection

// init IO ports
  pinMode(A_MUX_0, OUTPUT);    // encoder mux addresses
  pinMode(A_MUX_1, OUTPUT);  
  pinMode(A_MUX_2, OUTPUT);  
  pinMode(A_MUX_3, OUTPUT);
  pinMode(LMENU_ENCA_IN, INPUT_PULLUP);  // menu encoder and switches
  pinMode(LMENU_ENCB_IN, INPUT_PULLUP);    
  pinMode(LMENU_ENCSW_IN, INPUT_PULLUP); 
  pinMode(RMENU_ENCA_IN, INPUT_PULLUP);  // menu encoder and switches
  pinMode(RMENU_ENCB_IN, INPUT_PULLUP);    
  pinMode(RMENU_ENCSW_IN, INPUT_PULLUP); 

// set up I2C pins - meant to use I2C0 controller but messed up the schematic pins so have to use I2C1
  Wire1.setSDA(PIN_WIRE_SDA);
  Wire1.setSCL(PIN_WIRE_SCL);
  Wire1.begin();

// set up timer interrupt 
  start_encoder_timer();
 
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    fatalerror(""); // Don't proceed, loop forever
  }
  display.setRotation(2);
  display.clearDisplay();
  display.setTextSize(2);

	display.setTextColor(WHITE,BLACK); // foreground, background  
  display.setCursor(20,0);
  display.printf("Twisty 2\n"); // power on message
  display.display();
  delay(3000);

  initcontrols(); // set up default encoder and switch values 

  LEDS.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  showencoderLEDs(0); // show page 0 encoder LED colors
  LEDS.show();

  if (!LittleFS.begin()) fatalerror("Can't mount FS"); // start up filesystem

#ifdef BLUETOOTH
  bleMIDI.setName("Twisty 2");
#endif

  MIDI_Interface::beginAll();
  usbMIDI.setCallbacks(midiCallbacks);
//  Control_Surface.begin(); // Initialize the Control Surface MIDI interfaces

  display.clearDisplay();
  display.setTextSize(1);  
  showencoder(0,0);   // put first encoder values on the display
  updatedisplay();

  flush_encoders();  // clear any initial junk from encoders

  displaytimer=millis(); // reset display blanking timer

  loadWifiSettings();
  if (connectSavedWifi()) {
    startWifiFileManager();
    setupOSC();
  }
}

void loop() {
  ClickEncoder::Button button;
  ClickEncoder::ButtonEvent event;
  int16_t t,n;

  MIDI_Interface::updateAll(); // Update the Control Surface MIDI interfaces

  if ((millis()-displaytimer) > DISPLAY_BLANK_MS) blankdisplay(); // protect the OLED from burnin

  switch (UI_state) {
    case UI_SEND_MIDI: {  // process encoders
      ClickEncoder::Button controlButtons[NUMENCODERS];
      ClickEncoder::ButtonEvent controlButtonEvents[NUMENCODERS];
      for (int i = 0; i < NUMENCODERS; ++i) {
        controlButtonEvents[i] = enc[i].getButtonEvent();
        controlButtons[i] = enc[i].getButton();
        if (controlButtonEvents[i] == ClickEncoder::ActiveEdge) {
          pendingSwitchClick[i] = true;
        }
      }

      for (int i=0;i<NUMENCODERS;++i) {
        if ((t=enc[i].getValue()) !=0) { // if encoder has moved process
          controllerencoder &encoder = controls[page].encoder[i];
          if (controlButtons[i] == ClickEncoder::Held) {
            float widthStep = encoder.output == OSC_OUTPUT ? 0.01f : 1.0f;
            float maximumWidth = encoder.output == OSC_OUTPUT
                                     ? (encoder.oscRange == OSC_RANGE_0_TO_1 ? 1.0f : 2.0f)
                                     : 127.0f;
            encoder.randomWalkWidth =
                constrain(encoder.randomWalkWidth + t * widthStep, 0.0f, maximumWidth);
            randomWidthEditActive[i] = true;
            randomWalkInitialized[page][i] = false;
            if (momentarySwitchActive[i]) {
              endMomentarySwitch(page, i);
            }
            showEncoderWidth(page, i, true);
            continue;
          }
          if (encoder.output == OSC_OUTPUT) {
            if (encoder.oscRange == OSC_RANGE_DELTA) {
              sendOSCControlValue(encoder.oscAddress, static_cast<float>(t),
                                  static_cast<oscvaluetype>(encoder.oscValueType));
            } else {
              int stepMilli = encoder.oscRange == OSC_RANGE_0_TO_1 ? 8 : 16;
              int value = encoder.oscValueMilli + t * stepMilli;
              encoder.oscValueMilli = encoder.oscRange == OSC_RANGE_0_TO_1
                                          ? constrain(value, 0, 1000)
                                          : constrain(value, -1000, 1000);
              sendEncoderOutput(page, i, encoder.oscValueMilli / 1000.0f);
            }
            randomWalkInitialized[page][i] = false;
            lastcontrol=i;
            showencoderLED(page,lastcontrol);
            showencoder(page,lastcontrol);
            showEncoderWidth(page, i, false);
            continue;
          }
          // t is a signed integer, positive for clockwise, negative for counterclockwise
          if(controls[page].encoder[i].type == CCTYPE) {  // only process CC messages for now
            if (controls[page].encoder[i].minvalue < controls[page].encoder[i].maxvalue ) { // normal direction
              controls[page].encoder[i].value=controls[page].encoder[i].value + t;
              if (controls[page].encoder[i].value > controls[page].encoder[i].maxvalue){
                controls[page].encoder[i].value = controls[page].encoder[i].maxvalue;
              }
              if (controls[page].encoder[i].value < controls[page].encoder[i].minvalue){
                controls[page].encoder[i].value = controls[page].encoder[i].minvalue;
              }
            } else {
              controls[page].encoder[i].value=controls[page].encoder[i].value - t; // encoder direction is reversed
              if (controls[page].encoder[i].value > controls[page].encoder[i].minvalue){
                controls[page].encoder[i].value = controls[page].encoder[i].minvalue;
              }
              if (controls[page].encoder[i].value < controls[page].encoder[i].maxvalue){
                controls[page].encoder[i].value = controls[page].encoder[i].maxvalue;
              }
            }
          }else if(controls[page].encoder[i].type == DELTATYPE) {
            // Delta messages mean 0-63 for counterclockwise and 65-127 for clockwise, 64 is no change
            if (t>0){
              controls[page].encoder[i].value=127;
              //Serial.println("Delta CW");
            } else{
              controls[page].encoder[i].value=0;
              //Serial.println("Delta CCW");
            }
          }
          sendcontrolChange(controls[page].encoder[i].channel, controls[page].encoder[i].ccnumber,controls[page].encoder[i].value);
          lastcontrol=i;  // save index of the last used encoder
          if(controls[page].encoder[i].type == CCTYPE) {
            // only update the LED and display for CC messages, not delta messages
            // delta messages are updated via the MIDI callback function
            showencoderLED(page,lastcontrol);
            showencoder(page,lastcontrol);
            showEncoderWidth(page, i, false);
            updatedisplay();
          }
          randomWalkInitialized[page][i] = false;
        }
      }
        // process switches
      for (int i=0;i<NUMENCODERS;++i) {
        event=controlButtonEvents[i];
        button=controlButtons[i];

        controllerswitch &controlSwitch = controls[page].encswitch[i];
        if (button == ClickEncoder::Held && controlSwitch.mode == MOMENTARY &&
            pendingSwitchClick[i] && !randomWidthEditActive[i] &&
            !momentarySwitchActive[i]) {
          beginMomentarySwitch(page, i);
        }

        if (event == ClickEncoder::InActiveEdge) {
          if (randomWidthEditActive[i]) {
            randomWidthEditActive[i] = false;
            pendingSwitchClick[i] = false;
            if (momentarySwitchActive[i]) {
              endMomentarySwitch(page, i);
            }
            showEncoderWidth(page, i, false);
          } else if (momentarySwitchActive[i]) {
            endMomentarySwitch(page, i);
            pendingSwitchClick[i] = false;
          } else if (pendingSwitchClick[i]) {
            clickSwitch(page, i);
            pendingSwitchClick[i] = false;
          }
        }
      }

      processRandomWalk();
      processSwitchRamps();

      if ((t=lmenuenc.getValue()) !=0) { // left encoder changes controls page
        page=constrain(page+t,0,CONTROLLER_PAGES-1);
        showencoder(page,0);
        showencoderLEDs(page);
      }

      button=lmenuenc.getButton();
      if (button == ClickEncoder::DoubleClicked) { // left encoder double click shows switch states
        if (!displaySwitchLEDs) for (int sw=0; sw<NUMENCODERS;sw++) {
          showswitchLED(page,sw);
          displaySwitchLEDs=1; // toggle display mode
        }
        else {
          showencoderLEDs(page);
          displaySwitchLEDs=0; // toggle display mode
        }
      }

      if (button == ClickEncoder::Clicked) { // click to enter edit menu
        beginEncoderMenu(controls[page].encoder[lastcontrol].output);
        flush_encoders();
        UI_state=UI_ENCODER_MENU;
      }

      if (rmenuenc.getButton() == ClickEncoder::Clicked) {
        beginGlobalSettings();
        flush_encoders();
        UI_state=UI_PREMENU;
      }
      break;
    }

    case UI_ENCODER_MENU: {
      EncoderMenuAction action = handleEncoderMenu();
      if (action == ENCODER_MENU_RETURN_TO_CONTROLLER) {
        display.clearDisplay();
        showencoder(page,lastcontrol);
        showencoderLED(page,lastcontrol);
        updatedisplay();
        flush_encoders();
        UI_state=UI_SEND_MIDI;
      } else if (action != ENCODER_MENU_NONE) {
        copy_to_editbuffer(page,lastcontrol);
        topmenu = controllerEditMenu;
        topmenuindex = 0;
        controllerEditMenu[0].submenuindex = 0;

        if (action == ENCODER_MENU_EDIT_SETTINGS) {
          controllerEditMenu[0].submenus = encodersettings;
          controllerEditMenu[0].numsubmenus = 3;
        } else if (action == ENCODER_MENU_EDIT_MIDI_ENCODER ||
            action == ENCODER_MENU_EDIT_OSC_ENCODER) {
          controllerEditMenu[0].submenus =
              action == ENCODER_MENU_EDIT_MIDI_ENCODER ? controlparams : oscencoderparams;
          controllerEditMenu[0].numsubmenus =
              action == ENCODER_MENU_EDIT_MIDI_ENCODER ? 8 : 5;
          editbuffer.encoder.output =
              action == ENCODER_MENU_EDIT_MIDI_ENCODER ? MIDI_OUTPUT : OSC_OUTPUT;
          editbuffer.encswitch.output = editbuffer.encoder.output;
        } else {
          controllerEditMenu[0].submenus =
              action == ENCODER_MENU_EDIT_MIDI_SWITCH ? &controlparams[8] : oscswitchparams;
          controllerEditMenu[0].numsubmenus =
              action == ENCODER_MENU_EDIT_MIDI_SWITCH ? 7 : 4;
          editbuffer.encswitch.output =
              action == ENCODER_MENU_EDIT_MIDI_SWITCH ? MIDI_OUTPUT : OSC_OUTPUT;
          editbuffer.encoder.output = editbuffer.encswitch.output;
        }

        menustate = SUBSELECT;
        drawsubmenus();
        drawselector(controllerEditMenu[0].submenuindex);
        updatedisplay();
        flush_encoders();
        returnToEncoderMenu=true;
        UI_state=UI_EDIT;
      }
      break;
    }

    case UI_PREMENU: {
      GlobalSettingsAction action = handleGlobalSettings();
      if (action == GLOBAL_SETTINGS_OPEN_SAVELOAD) {
        topmenu=mainmenu;
        topmenuindex=1;
        menustate=SUBSELECT;
        copy_to_editbuffer(page,lastcontrol);
        topmenu[topmenuindex].submenuindex=0;
        drawsubmenus();
        drawselector(topmenu[topmenuindex].submenuindex);
        updatedisplay();
        flush_encoders();
        returnToPremenu=true;
        UI_state=UI_LOADSAVE;
      } else if (action == GLOBAL_SETTINGS_WIFI_CONNECTED) {
        startWifiFileManager();
        setupOSC();
      } else if (action == GLOBAL_SETTINGS_OSC_SETTINGS_UPDATED) {
        setupOSC();
      } else if (action == GLOBAL_SETTINGS_RETURN_TO_CONTROLLER) {
        display.clearDisplay();
        showencoder(page,lastcontrol);
        showencoderLED(page,lastcontrol);
        updatedisplay();
        flush_encoders();
        UI_state=UI_SEND_MIDI;
      }
      break;
    }

    case UI_EDIT:  // do edit menu 
      if (menustate == TEXT_INPUT) {
        domenus();
        break;
      }
      for (n=0; n< NUMENCODERS;++n) {   // see if a button is pressed
        if ((button=enc[n].getButton()) == ClickEncoder::Clicked) break;
      }
      if ((n< NUMENCODERS) || ((t=lmenuenc.getValue()) !=0)) { // button press or scroll thru controls with left encoder
        restoreCurrentEditBuffer();
        showencoderLED(page,lastcontrol); // restore control color  
        if (n<NUMENCODERS) lastcontrol=n;    
        else lastcontrol=constrain(lastcontrol+t,0,NUMENCODERS-1);
        copy_to_editbuffer(page,lastcontrol); //copy encoder parameters for editing
        drawsubmenus();
        drawselector(topmenu[topmenuindex].submenuindex);
        updatedisplay();
      }
      if (lmenuenc.getButton() == ClickEncoder::Clicked) { // click to exit menu
        display.clearDisplay();
        restoreCurrentEditBuffer();
        showencoder(page,lastcontrol); // redraw the encoder display
        showencoderLED(page,lastcontrol); // update the LED too
        updatedisplay();
        if (returnToPremenu) {
          returnToPremenu=false;
          UI_state=UI_PREMENU;
          beginGlobalSettings();
        } else if (returnToEncoderMenu) {
          returnToEncoderMenu=false;
          UI_state=UI_ENCODER_MENU;
          beginEncoderMenu(controls[page].encoder[lastcontrol].output);
        } else {
          UI_state=UI_SEND_MIDI;
        }
        while(!digitalRead(LMENU_ENCSW_IN)) delay(10); // loop here till button released 
        flush_encoders();   // toss any encoder messages
      }
      else {
        domenus();
        if ((millis()-LEDtimer) > LEDFLASH_EDIT) {
          LEDtimer=millis();
          if (LEDstate) {
            LEDstate=0;
            LEDS.setPixelColor(lastcontrol,colorpalette[editbuffer.encoder.colorindex]); // flash LED in the current editing color at full brightness
          }
          else {
            LEDstate=1;
            LEDS.setPixelColor(lastcontrol,LED_BLACK);
          }
        }
      }
      break;
    case UI_LOADSAVE:  // save/load menu state
      if (lmenuenc.getButton() == ClickEncoder::Clicked) { // click to exit menus
        display.clearDisplay();
        showencoder(page,lastcontrol); // redraw the encoder display
        showencoderLED(page,lastcontrol); // update the LED too
        updatedisplay();
        if (returnToPremenu) {
          returnToPremenu=false;
          UI_state=UI_PREMENU;
          beginGlobalSettings();
        } else {
          UI_state=UI_SEND_MIDI;
        }
        while(!digitalRead(LMENU_ENCSW_IN)) delay(10); // loop here till button released 
        flush_encoders();   // toss any encoder messages
      }
      else domenus();
      break;
    default:
      break;
  }  // end switch

  LEDS.show();
  handleClinet();  // handle any incoming client requests for the web server
  loopOSC(); // handle any incoming OSC messages
}
