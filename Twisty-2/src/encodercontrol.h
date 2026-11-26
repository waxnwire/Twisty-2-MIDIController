#pragma once

#include "menu/controller.h"

extern struct controllerpage controls[CONTROLLER_PAGES];
extern struct controller editbuffer;

void initcontrols(void);
void copy_to_editbuffer(int16_t page, int16_t index);
void restore_from_editbuffer(int16_t page, int16_t index);
