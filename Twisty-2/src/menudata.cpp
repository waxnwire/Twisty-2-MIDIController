#include "menudata.h"
#include "menu/controller.h"

extern struct controller editbuffer;
extern int16_t saverestore_slot;
extern int16_t saverestore_action;
extern int16_t saverestore_confirm;
void save_restore(void);

const char *onoff[] = {"   Off", "    On"};
const char *ledcolors[] = {"   Red", "Orange", " Green", "  Aqua", "  Blue", "Violet", " White"};
const char *actions[] = {"  Load", "  Save", "Format"};
const char *no_yes[] = {"    No", "   Yes"};
const char *enctypes[] = {"    CC", " Delta"};
const char *encmodes[] = {"Absolu", " Delta"};
const char *switchmodes[] = {"Moment", "Toggle"};
const char *switchfunctions[] = {"Jump", "Ramp", "Wobble"};
const char *switchtypes[] = {"    CC", "    PC", "  Note"};
const char *outputs[] = {"  MIDI", "   OSC"};
const char *oscvaluetypes[] = {" Float", "   Int", "String"};
const char *oscrangetypes[] = {" 0 to 1", "-1 to 1", " Delta"};

const char *labels[] = {
  "AAAAAA", "    CC", "Alloff", "   Arp", "Attack", "Balanc", "  Band", "Bandwi", "  Bank", "  Bass",
  "  Bend", "Bounce", "   BPM", "Breath", "  Chan", "Chorus", " Clear",
  " Color", "  Comp", "crossT", " Crush", "   Cut", "Cutoff", "Dampng", " Decay",
  " Delay", "Densty", " Depth", "Detune", "Diffus", "Distrt", "Double", " Drive",
  "   Dry", "  Duck", "Effect", "   End", "    EQ", "Expres", "  Fade", "Feedbk",
  "  Feel", "Filter", "Flange", "Flutte", "    FM", "Freeze", "  Freq", "  Gain",
  "  Gate", " Glide", "Glitch", " Grain", "  High", "  Hiss", "  Hold", "I.SndA",
  "I.SndB", "Jitter", "  Knee", "Legato", " Level", "   Lfo", " Limit", " Local",
  "  Loop", "   Low", "Master", "   Mid", "   Mix", "   Mod", "  Mono", " Morph",
  "  Mute", "Octave", "Offset", "  Omni", "   Osc", " Osc 1", " Osc 2", " Osc 3",
  " Osc 4", "OvrDub", "   Pan", " Pedal", " Phase", "Phaser", " Pitch", "  Play",
  "  Poly", "Portam", "Positn", "  Post", "   Pre", "PreAmp", "PreDel", "     Q",
  "R.SndA", "R.SndB", "  Rand", "  Rate", " Ratio", "Record", "  Redo", "Releas",
  "Repeat", " Reset", " Reson", "Resume", "Return", "Reverb", "Revers", "Rewind",
  "Rice_1", "Rice_2", "Rice_3", "Rice_4", "Sample", "Satura", " Scene", " Scrub",
  "  Send", " Shape", "Shimmr", "  Size", " Slice", " Smear", "  Solo", " Sound",
  " Speed", " Spray", "Spread", " Start", "  Stop", "Stretc", "Sustai", "  Sync",
  "  Tape", " Tempo", "Textur", "Timbre", "  Time", "  Tone", " Track", "Transp",
  "Treble", "Tremol", "Triggr", "  Trim", "  Tune", "  Type", " Vocal", " Voice",
  "Volume", "  Warp", "  Wave", "   Wet", " Width", "Window", "   Wow", "  XPos",
  "  YPos"
};

#define NUM_LABELS (sizeof(labels) / sizeof(labels[0]))
const uint16_t LABEL_COUNT = NUM_LABELS;

struct submenu controlparams[] = {
  {"Enc MIDI Chan.", 1, 16, 1, TYPE_INTEGER, nullptr, &editbuffer.encoder.channel, nullptr, nullptr},
  {"Enc Type", 0, 1, 1, TYPE_TEXT, enctypes, &editbuffer.encoder.type, nullptr, nullptr},
  {"Enc Mode", 0, 1, 1, TYPE_TEXT, encmodes, &editbuffer.encoder.mode, nullptr, nullptr},
  {"Enc CC No.", 0, 127, 1, TYPE_INTEGER, nullptr, &editbuffer.encoder.ccnumber, nullptr, nullptr},
  {"Enc Label", 0, NUM_LABELS - 1, 1, TYPE_TEXT, labels, &editbuffer.encoder.labelindex, nullptr, nullptr},
  {"Enc Color", 0, 5, 1, TYPE_TEXT, ledcolors, &editbuffer.encoder.colorindex, nullptr, nullptr},
  {"Enc Min", 0, 127, 1, TYPE_INTEGER, nullptr, &editbuffer.encoder.minvalue, nullptr, nullptr},
  {"Enc Max", 0, 127, 1, TYPE_INTEGER, nullptr, &editbuffer.encoder.maxvalue, nullptr, nullptr},
  {"Switch MIDI Chan.", 1, 16, 1, TYPE_INTEGER, nullptr, &editbuffer.encswitch.channel, nullptr, nullptr},
  {"Switch Mode", 0, 1, 1, TYPE_TEXT, switchmodes, &editbuffer.encswitch.mode, nullptr, nullptr},
  {"Switch Type", 0, 2, 1, TYPE_TEXT, switchtypes, &editbuffer.encswitch.type, nullptr, nullptr},
  {"Switch CC No.", 0, 127, 1, TYPE_INTEGER, nullptr, &editbuffer.encswitch.ccnumber, nullptr, nullptr},
  {"Switch Label", 0, NUM_LABELS - 1, 1, TYPE_TEXT, labels, &editbuffer.encswitch.labelindex, nullptr, nullptr},
  {"Switch Min", 0, 127, 1, TYPE_INTEGER, nullptr, &editbuffer.encswitch.minvalue, nullptr, nullptr},
  {"Switch Max", 0, 127, 1, TYPE_INTEGER, nullptr, &editbuffer.encswitch.maxvalue, nullptr, nullptr}
};

struct submenu encodersettings[] = {
  {"Output", MIDI_OUTPUT, OSC_OUTPUT, 1, TYPE_TEXT, outputs,
   &editbuffer.encoder.output, nullptr, nullptr},
  {"Switch", SWITCH_JUMP, SWITCH_WOBBLE, 1, TYPE_TEXT, switchfunctions,
   &editbuffer.encswitch.function, nullptr, nullptr},
  {"Switch Mode", MOMENTARY, TOGGLE, 1, TYPE_TEXT, switchmodes,
   &editbuffer.encswitch.mode, nullptr, nullptr}
};

struct submenu oscencoderparams[] = {
  {"OSC Path", 0, 0, 0, TYPE_STRING, nullptr, nullptr, nullptr, nullptr,
   editbuffer.encoder.oscAddress, sizeof(editbuffer.encoder.oscAddress)},
  {"OSC Value", 0, 2, 1, TYPE_TEXT, oscvaluetypes, &editbuffer.encoder.oscValueType, nullptr, nullptr},
  {"OSC Range", 0, 2, 1, TYPE_TEXT, oscrangetypes, &editbuffer.encoder.oscRange, nullptr, nullptr},
  {"Enc Label", 0, NUM_LABELS - 1, 1, TYPE_TEXT, labels, &editbuffer.encoder.labelindex, nullptr, nullptr},
  {"Enc Color", 0, 5, 1, TYPE_TEXT, ledcolors, &editbuffer.encoder.colorindex, nullptr, nullptr}
};

struct submenu oscswitchparams[] = {
  {"OSC Path", 0, 0, 0, TYPE_STRING, nullptr, nullptr, nullptr, nullptr,
   editbuffer.encswitch.oscAddress, sizeof(editbuffer.encswitch.oscAddress)},
  {"OSC Value", 0, 2, 1, TYPE_TEXT, oscvaluetypes, &editbuffer.encswitch.oscValueType, nullptr, nullptr},
  {"Switch Mode", 0, 1, 1, TYPE_TEXT, switchmodes, &editbuffer.encswitch.mode, nullptr, nullptr},
  {"Switch Label", 0, NUM_LABELS - 1, 1, TYPE_TEXT, labels, &editbuffer.encswitch.labelindex, nullptr, nullptr}
};

struct submenu loadsave[] = {
  {"Slot", 1, 16, 1, TYPE_INTEGER, nullptr, &saverestore_slot, nullptr, nullptr},
  {"Action", 0, 2, 1, TYPE_TEXT, actions, &saverestore_action, nullptr, nullptr},
  {"Confirm?", 0, 1, 1, TYPE_TEXT, no_yes, &saverestore_confirm, nullptr, save_restore}
};

struct menu mainmenu[] = {
  {"", controlparams, 0, sizeof(controlparams) / sizeof(controlparams[0])},
  {"", loadsave, 0, sizeof(loadsave) / sizeof(loadsave[0])}
};

const uint8_t NUM_MAIN_MENUS = sizeof(mainmenu) / sizeof(mainmenu[0]);
