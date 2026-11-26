#pragma once

#include "ClickEncoder.h"
#include "menu/config.h"
#include "menu/controller.h"

#define A_MUX_0 9
#define A_MUX_1 8
#define A_MUX_2 7
#define A_MUX_3 6

#define ENCA_IN 11
#define ENCB_IN 17
#define ENCSW_IN 10

#define LMENU_ENCA_IN 18
#define LMENU_ENCB_IN 19
#define LMENU_ENCSW_IN 16

#define RMENU_ENCA_IN 21
#define RMENU_ENCB_IN 22

extern ClickEncoder enc[NUMENCODERS];
extern ClickEncoder lmenuenc;
extern ClickEncoder rmenuenc;

void start_encoder_timer(void);
