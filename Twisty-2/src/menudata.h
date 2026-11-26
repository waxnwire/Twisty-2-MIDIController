#pragma once

#include "menusystem.h"
#include "menu/controlparams.h"
#include "menu/loadsave.h"

extern struct menu mainmenu[];
extern const uint8_t NUM_MAIN_MENUS;
extern const char *labels[];
extern const uint16_t LABEL_COUNT;
extern const char *onoff[];
extern const char *ledcolors[];
extern const char *actions[];
extern const char *no_yes[];
extern const char *enctypes[];
extern const char *encmodes[];
extern const char *switchmodes[];
extern const char *switchfunctions[];
extern const char *switchtypes[];
extern const char *outputs[];
extern const char *oscvaluetypes[];
extern const char *oscrangetypes[];
extern struct submenu oscencoderparams[];
extern struct submenu oscswitchparams[];
extern struct submenu encodersettings[];
