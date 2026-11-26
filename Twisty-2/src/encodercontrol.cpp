#include "encodercontrol.h"

#include <stdio.h>

struct controllerpage controls[CONTROLLER_PAGES];
struct controller editbuffer;

void initcontrols(void) {
  int16_t ccnum = BASE_CC;
  for (int16_t p = 0; p < CONTROLLER_PAGES; ++p) {
    for (int16_t i = 0; i < NUMENCODERS; ++i) {
      controls[p].encoder[i].type = CCTYPE;
      controls[p].encoder[i].mode = 0;
      controls[p].encoder[i].channel = DEFAULT_ENCODER_CHANNEL;
      controls[p].encoder[i].ccnumber = ccnum;
      controls[p].encoder[i].minvalue = 0;
      controls[p].encoder[i].maxvalue = 127;
      controls[p].encoder[i].value = 64;
      controls[p].encoder[i].colorindex = p;
      controls[p].encoder[i].labelindex = 1;
      controls[p].encoder[i].output = MIDI_OUTPUT;
      controls[p].encoder[i].oscValueType = OSC_FLOAT;
      controls[p].encoder[i].oscRange = OSC_RANGE_0_TO_1;
      controls[p].encoder[i].oscValueMilli = 500;
      controls[p].encoder[i].randomWalkWidth = 0.0f;
      snprintf(controls[p].encoder[i].oscAddress,
               sizeof(controls[p].encoder[i].oscAddress),
               "/twisty/page/%d/encoder/%d", p + 1, i + 1);
      controls[p].encswitch[i].mode = TOGGLE;
      controls[p].encswitch[i].type = CCTYPE;
      controls[p].encswitch[i].channel = DEFAULT_SWITCH_CHANNEL;
      controls[p].encswitch[i].ccnumber = ccnum;
      controls[p].encswitch[i].minvalue = 0;
      controls[p].encswitch[i].maxvalue = 127;
      controls[p].encswitch[i].value = 0;
      controls[p].encswitch[i].colorindex = p;
      controls[p].encswitch[i].labelindex = 1;
      controls[p].encswitch[i].output = MIDI_OUTPUT;
      controls[p].encswitch[i].function = SWITCH_JUMP;
      controls[p].encswitch[i].oscValueType = OSC_INTEGER;
      snprintf(controls[p].encswitch[i].oscAddress,
               sizeof(controls[p].encswitch[i].oscAddress),
               "/twisty/page/%d/switch/%d", p + 1, i + 1);
      ++ccnum;
    }
  }
}

void copy_to_editbuffer(int16_t page, int16_t index) {
  editbuffer.encoder.type = controls[page].encoder[index].type;
  editbuffer.encoder.mode = controls[page].encoder[index].mode;
  editbuffer.encoder.channel = controls[page].encoder[index].channel;
  editbuffer.encoder.ccnumber = controls[page].encoder[index].ccnumber;
  editbuffer.encoder.value = controls[page].encoder[index].value;
  editbuffer.encoder.minvalue = controls[page].encoder[index].minvalue;
  editbuffer.encoder.maxvalue = controls[page].encoder[index].maxvalue;
  editbuffer.encoder.colorindex = controls[page].encoder[index].colorindex;
  editbuffer.encoder.labelindex = controls[page].encoder[index].labelindex;
  editbuffer.encoder.output = controls[page].encoder[index].output;
  editbuffer.encoder.oscValueType = controls[page].encoder[index].oscValueType;
  editbuffer.encoder.oscRange = controls[page].encoder[index].oscRange;
  editbuffer.encoder.oscValueMilli = controls[page].encoder[index].oscValueMilli;
  editbuffer.encoder.randomWalkWidth = controls[page].encoder[index].randomWalkWidth;
  snprintf(editbuffer.encoder.oscAddress, sizeof(editbuffer.encoder.oscAddress), "%s",
           controls[page].encoder[index].oscAddress);
  editbuffer.encswitch.mode = controls[page].encswitch[index].mode;
  editbuffer.encswitch.type = controls[page].encswitch[index].type;
  editbuffer.encswitch.channel = controls[page].encswitch[index].channel;
  editbuffer.encswitch.ccnumber = controls[page].encswitch[index].ccnumber;
  editbuffer.encswitch.minvalue = controls[page].encswitch[index].minvalue;
  editbuffer.encswitch.maxvalue = controls[page].encswitch[index].maxvalue;
  editbuffer.encswitch.colorindex = controls[page].encswitch[index].colorindex;
  editbuffer.encswitch.labelindex = controls[page].encswitch[index].labelindex;
  editbuffer.encswitch.output = controls[page].encoder[index].output;
  editbuffer.encswitch.function = controls[page].encswitch[index].function;
  editbuffer.encswitch.oscValueType = controls[page].encswitch[index].oscValueType;
  snprintf(editbuffer.encswitch.oscAddress, sizeof(editbuffer.encswitch.oscAddress), "%s",
           controls[page].encswitch[index].oscAddress);
}

void restore_from_editbuffer(int16_t page, int16_t index) {
  controls[page].encoder[index].type = editbuffer.encoder.type;
  controls[page].encoder[index].mode = editbuffer.encoder.mode;
  controls[page].encoder[index].channel = editbuffer.encoder.channel;
  controls[page].encoder[index].ccnumber = editbuffer.encoder.ccnumber;
  controls[page].encoder[index].minvalue = editbuffer.encoder.minvalue;
  controls[page].encoder[index].maxvalue = editbuffer.encoder.maxvalue;
  controls[page].encoder[index].colorindex = editbuffer.encoder.colorindex;
  controls[page].encoder[index].labelindex = editbuffer.encoder.labelindex;
  controls[page].encoder[index].output = editbuffer.encoder.output;
  controls[page].encoder[index].oscValueType = editbuffer.encoder.oscValueType;
  controls[page].encoder[index].oscRange = editbuffer.encoder.oscRange;
  controls[page].encoder[index].oscValueMilli = editbuffer.encoder.oscValueMilli;
  controls[page].encoder[index].randomWalkWidth = editbuffer.encoder.randomWalkWidth;
  if (controls[page].encoder[index].output == OSC_OUTPUT) {
    float maximumWidth =
        controls[page].encoder[index].oscRange == OSC_RANGE_0_TO_1 ? 1.0f : 2.0f;
    if (controls[page].encoder[index].randomWalkWidth > maximumWidth) {
      controls[page].encoder[index].randomWalkWidth = maximumWidth;
    }
  }
  snprintf(controls[page].encoder[index].oscAddress,
           sizeof(controls[page].encoder[index].oscAddress), "%s",
           editbuffer.encoder.oscAddress);
  controls[page].encswitch[index].mode = editbuffer.encswitch.mode;
  controls[page].encswitch[index].type = editbuffer.encswitch.type;
  controls[page].encswitch[index].channel = editbuffer.encswitch.channel;
  controls[page].encswitch[index].ccnumber = editbuffer.encswitch.ccnumber;
  controls[page].encswitch[index].minvalue = editbuffer.encswitch.minvalue;
  controls[page].encswitch[index].maxvalue = editbuffer.encswitch.maxvalue;
  controls[page].encswitch[index].colorindex = editbuffer.encswitch.colorindex;
  controls[page].encswitch[index].labelindex = editbuffer.encswitch.labelindex;
  controls[page].encswitch[index].output = editbuffer.encoder.output;
  editbuffer.encswitch.output = editbuffer.encoder.output;
  controls[page].encswitch[index].function = editbuffer.encswitch.function;
  controls[page].encswitch[index].oscValueType = editbuffer.encswitch.oscValueType;
  snprintf(controls[page].encswitch[index].oscAddress,
           sizeof(controls[page].encswitch[index].oscAddress), "%s",
           editbuffer.encswitch.oscAddress);
}
