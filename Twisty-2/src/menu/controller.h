#pragma once

#include <stdint.h>

#define NUMENCODERS 16
#define CONTROLLER_PAGES 4
#define BASE_CC 16
#define DEFAULT_ENCODER_CHANNEL 1
#define DEFAULT_SWITCH_CHANNEL 2

enum encodertypes { CCTYPE, DELTATYPE };
enum switchmode { MOMENTARY, TOGGLE };
enum switchtype { CCMESSAGE, PCMESSAGE, NOTEMESSAGE, SETENC };
enum switchfunction { SWITCH_JUMP, SWITCH_RAMP, SWITCH_WOBBLE };
enum controloutput { MIDI_OUTPUT, OSC_OUTPUT };
enum oscvaluetype { OSC_FLOAT, OSC_INTEGER, OSC_STRING };
enum oscrangemode { OSC_RANGE_0_TO_1, OSC_RANGE_NEG1_TO_1, OSC_RANGE_DELTA };

struct controllerencoder {
  int16_t type;
  int16_t mode;
  int16_t channel;
  int16_t ccnumber;
  int16_t minvalue;
  int16_t maxvalue;
  int16_t value;
  int16_t colorindex;
  int16_t labelindex;
  int16_t output;
  int16_t oscValueType;
  int16_t oscRange;
  int16_t oscValueMilli;
  float randomWalkWidth;
  char oscAddress[64] = "";
  char label[12] = "";
};

struct controllerswitch {
  int16_t mode;
  int16_t type;
  int16_t channel;
  int16_t ccnumber;
  int16_t minvalue;
  int16_t maxvalue;
  int16_t value;
  int16_t colorindex;
  int16_t labelindex;
  int16_t output;
  int16_t function;
  int16_t oscValueType;
  char oscAddress[64] = "";
  char label[12] = "";
};

struct controllerpage {
  struct controllerencoder encoder[NUMENCODERS];
  struct controllerswitch encswitch[NUMENCODERS];
};

struct controller {
  struct controllerencoder encoder;
  struct controllerswitch encswitch;
};
