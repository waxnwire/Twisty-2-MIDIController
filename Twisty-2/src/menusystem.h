#pragma once

#include <Arduino.h>

#include "menu/config.h"

enum menumodes { TOPSELECT, SUBSELECT, PARAM_INPUT, TEXT_INPUT };
enum paramtype { TYPE_NONE, TYPE_INTEGER, TYPE_FLOAT, TYPE_TEXT, TYPE_STRING };

struct submenu {
  const char *name;
  int16_t min;
  int16_t max;
  int16_t step;
  enum paramtype ptype;
  const char **ptext;
  int16_t *parameter;
  void (*handler)(void);
  void (*exithandler)(void);
  char *stringParameter = nullptr;
  uint8_t stringCapacity = 0;
};

struct menu {
  const char *name;
  struct submenu *submenus;
  int8_t submenuindex;
  int8_t numsubmenus;
};

extern int8_t topmenuindex;
extern int16_t menustate;
extern struct menu *topmenu;

void drawselector(int8_t index);
void draweditselector(int8_t index);
void undrawselector(int8_t index);
void drawtopmenu(int8_t index);
void drawsubmenu(int8_t index);
void drawsubmenus(void);
void domenus(void);
