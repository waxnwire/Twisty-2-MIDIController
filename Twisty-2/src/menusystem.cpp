#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <stdio.h>
#include <string.h>

#include "ClickEncoder.h"
#include "encoderinput.h"
#include "menusystem.h"
#include "menudata.h"

extern Adafruit_SSD1306 display;
extern ClickEncoder rmenuenc;
extern void updatedisplay();

namespace {

constexpr char oscAddressCharacters[] =
    "/abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.";
char stringInputBuffer[64] = {};
char *stringInputTarget = nullptr;
uint8_t stringInputCapacity = 0;
size_t stringInputLength = 0;
int stringInputCharacter = 0;

void drawPathPreview(const char *value, size_t length, int firstLineY) {
  size_t firstVisible = length > 40 ? length - 40 : 0;
  size_t visibleLength = length - firstVisible;
  size_t firstLineLength = visibleLength > 20 ? 20 : visibleLength;

  if (visibleLength > 20) {
    size_t minimumSplit = visibleLength - 20;
    size_t targetSplit = visibleLength / 2;
    size_t bestDistance = visibleLength;
    for (size_t split = minimumSplit; split <= 20; ++split) {
      if (value[firstVisible + split - 1] == '/') {
        size_t distance = split > targetSplit ? split - targetSplit
                                               : targetSplit - split;
        if (distance < bestDistance) {
          firstLineLength = split;
          bestDistance = distance;
        }
      }
    }
  }

  display.setCursor(0, firstLineY);
  for (size_t index = 0; index < firstLineLength; ++index) {
    display.write(value[firstVisible + index]);
  }
  display.setCursor(0, firstLineY + 8);
  for (size_t index = firstLineLength; index < visibleLength; ++index) {
    display.write(value[firstVisible + index]);
  }
}

void drawSelectedString(submenu *field) {
  const char *value = field->stringParameter == nullptr ? "" : field->stringParameter;
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("> ");
  display.print(field->name);
  drawPathPreview(value, strlen(value), 8);
  display.setCursor(0, 24);
  display.print("R edit; L turn del");
  updatedisplay();
}

void drawStringInput(void) {
  submenu *sub = topmenu[topmenuindex].submenus;
  int8_t index = topmenu[topmenuindex].submenuindex;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("* ");
  display.print(sub[index].name);
  drawPathPreview(stringInputBuffer, stringInputLength, 8);
  display.setCursor(0, 24);
  display.print("Next: ");
  size_t characterCount = sizeof(oscAddressCharacters) - 1;
  if (stringInputCharacter >= static_cast<int>(characterCount)) {
    display.print("DONE");
  } else {
    display.print(oscAddressCharacters[stringInputCharacter]);
  }
  display.print(" R+ L-");
  updatedisplay();
}

void finishStringInput(bool accept) {
  if (accept && stringInputTarget != nullptr && stringInputCapacity > 0) {
    snprintf(stringInputTarget, stringInputCapacity, "%s", stringInputBuffer);
  }
  stringInputTarget = nullptr;
  stringInputCapacity = 0;
  menustate = SUBSELECT;
  drawsubmenus();
  drawselector(topmenu[topmenuindex].submenuindex);
}

void beginStringInput(submenu *field) {
  stringInputTarget = field->stringParameter;
  stringInputCapacity = field->stringCapacity;
  stringInputLength = stringInputTarget == nullptr ? 0 : strlen(stringInputTarget);
  if (stringInputCapacity == 0) {
    stringInputTarget = nullptr;
    return;
  }
  if (stringInputLength >= stringInputCapacity) {
    stringInputLength = stringInputCapacity - 1;
  }
  if (stringInputTarget != nullptr) {
    memcpy(stringInputBuffer, stringInputTarget, stringInputLength);
  } else {
    stringInputLength = 0;
  }
  stringInputBuffer[stringInputLength] = '\0';
  stringInputCharacter = 0;
  menustate = TEXT_INPUT;
  drawStringInput();
}

void handleStringInput(int16_t movement) {
  size_t characterCount = sizeof(oscAddressCharacters) - 1;
  bool rightClicked = rmenuenc.getButton() == ClickEncoder::Clicked;
  bool leftClicked = lmenuenc.getButton() == ClickEncoder::Clicked;
  int16_t eraseMovement = lmenuenc.getValue();
  bool erased = false;

  if (leftClicked) {
    finishStringInput(false);
    return;
  }
  while (eraseMovement < 0 && stringInputLength > 0) {
    stringInputBuffer[--stringInputLength] = '\0';
    ++eraseMovement;
    erased = true;
  }
  if (movement != 0) {
    int choices = characterCount + 1;
    stringInputCharacter = (stringInputCharacter + movement) % choices;
    if (stringInputCharacter < 0) {
      stringInputCharacter += choices;
    }
    drawStringInput();
  }
  if (rightClicked) {
    if (stringInputCharacter >= static_cast<int>(characterCount)) {
      finishStringInput(true);
      return;
    }
    if (stringInputTarget != nullptr && stringInputLength < stringInputCapacity - 1) {
      stringInputBuffer[stringInputLength++] = oscAddressCharacters[stringInputCharacter];
      stringInputBuffer[stringInputLength] = '\0';
    }
    drawStringInput();
  } else if (erased) {
    drawStringInput();
  }
}

}

int8_t topmenuindex = 0;
int16_t menustate = SUBSELECT;
struct menu *topmenu = mainmenu;

void drawselector(int8_t index) {
  if (topmenu[topmenuindex].submenus[index].ptype == TYPE_STRING) {
    return;
  }
  int line = index % TOPMENU_LINES;
  display.setCursor(0, TOPMENU_Y + DISPLAY_Y_MENUPAD + line * (DISPLAY_CHAR_HEIGHT + DISPLAY_Y_MENUPAD));
  display.print(">");
  updatedisplay();
}

void draweditselector(int8_t index) {
  int line = index % TOPMENU_LINES;
  display.setCursor(0, TOPMENU_Y + DISPLAY_Y_MENUPAD + line * (DISPLAY_CHAR_HEIGHT + DISPLAY_Y_MENUPAD));
  display.print("*");
  display.setTextColor(WHITE, BLACK);
  updatedisplay();
}

void undrawselector(int8_t index) {
  if (topmenu[topmenuindex].submenus[index].ptype == TYPE_STRING) {
    return;
  }
  int line = index % TOPMENU_LINES;
  display.setCursor(0, TOPMENU_Y + DISPLAY_Y_MENUPAD + line * (DISPLAY_CHAR_HEIGHT + DISPLAY_Y_MENUPAD));
  display.print(" ");
  updatedisplay();
}

void drawtopmenu(int8_t index) {
  display.fillScreen(BLACK);
  display.setCursor(TOPMENU_X, TOPMENU_Y);
  int i = (index / TOPMENU_LINES) * TOPMENU_LINES;
  int last = i + NUM_MAIN_MENUS % TOPMENU_LINES;
  if ((i + TOPMENU_LINES) <= NUM_MAIN_MENUS) {
    last = i + TOPMENU_LINES;
  }
  int y = DISPLAY_Y_OFFSET;

  for (; i < last; ++i) {
    display.setCursor(TOPMENU_X, y);
    display.print(topmenu[i].name);
    y += DISPLAY_CHAR_HEIGHT + DISPLAY_Y_MENUPAD;
  }
  updatedisplay();
}

void drawsubmenu(int8_t index) {
  submenu *sub = topmenu[topmenuindex].submenus;
  int y = SUBMENU_Y + DISPLAY_Y_MENUPAD + (index % SUBMENU_LINES) * (DISPLAY_CHAR_HEIGHT + DISPLAY_Y_MENUPAD);
  display.setCursor(SUBMENU_X, y);
  display.print(sub[index].name);

  display.setCursor(SUBMENU_VALUE_X, y);
  display.print("      ");
  display.setCursor(SUBMENU_VALUE_X, y);
  if (sub[index].ptype == TYPE_STRING) {
    display.print("...");
  } else if (sub[index].step != 0 && sub[index].parameter != nullptr) {
    int16_t val = *sub[index].parameter;
    char temp[7];
    switch (sub[index].ptype) {
      case TYPE_INTEGER:
        sprintf(temp, "%6d", val);
        display.print(temp);
        display.print(" ");
        break;
      case TYPE_FLOAT:
        sprintf(temp, "%1.2f", (float)val / 1000);
        display.print(temp);
        display.print(" ");
        break;
      case TYPE_TEXT:
        if (val > sub[index].max) val = sub[index].max;
        if (val < 0) val = 0;
        display.print(sub[index].ptext[val]);
        display.print(" ");
        break;
      case TYPE_NONE:
      default:
        display.print("     ");
        break;
    }
  }
  updatedisplay();
}

void drawsubmenus() {
  int8_t index = topmenu[topmenuindex].submenuindex;
  int8_t len = topmenu[topmenuindex].numsubmenus;
  submenu *sub = topmenu[topmenuindex].submenus;
  if (sub[index].ptype == TYPE_STRING) {
    drawSelectedString(&sub[index]);
    return;
  }
  display.fillScreen(BLACK);
  display.setCursor(0, DISPLAY_Y_OFFSET);
  int i = (index / SUBMENU_LINES) * SUBMENU_LINES;
  int last = i + len % SUBMENU_LINES;
  if ((i + SUBMENU_LINES) <= len) {
    last = i + SUBMENU_LINES;
  }

  for (; i < last; ++i) {
    drawsubmenu(i);
  }
  updatedisplay();
}

void domenus(void) {
  int16_t enc = rmenuenc.getValue();
  int8_t index;

  if (menustate == TEXT_INPUT) {
    handleStringInput(enc);
    return;
  }

  switch (menustate) {
    case SUBSELECT:
      if (enc != 0) {
        int8_t previousIndex = topmenu[topmenuindex].submenuindex;
        int submenupage = topmenu[topmenuindex].submenuindex / SUBMENU_LINES;
        undrawselector(previousIndex);
        topmenu[topmenuindex].submenuindex += enc;
        if (topmenu[topmenuindex].submenuindex < 0) {
          topmenu[topmenuindex].submenuindex = 0;
        }
        if (topmenu[topmenuindex].submenuindex >= topmenu[topmenuindex].numsubmenus - 1) {
          topmenu[topmenuindex].submenuindex = topmenu[topmenuindex].numsubmenus - 1;
        }
        int8_t currentIndex = topmenu[topmenuindex].submenuindex;
        if ((currentIndex / SUBMENU_LINES) != submenupage ||
            topmenu[topmenuindex].submenus[previousIndex].ptype == TYPE_STRING ||
            topmenu[topmenuindex].submenus[currentIndex].ptype == TYPE_STRING) {
          drawsubmenus();
        }
        drawselector(currentIndex);
      }
      if (!digitalRead(RMENU_ENCSW_IN)) {
        index = topmenu[topmenuindex].submenuindex;
        submenu *sub = topmenu[topmenuindex].submenus;
        if (sub[index].ptype == TYPE_STRING) {
          beginStringInput(&sub[index]);
        } else {
          undrawselector(index);
          draweditselector(index);
          menustate = PARAM_INPUT;
        }
        while (!digitalRead(RMENU_ENCSW_IN)) delay(10);
      }
      break;
    case PARAM_INPUT: {
      submenu *sub = topmenu[topmenuindex].submenus;
      index = topmenu[topmenuindex].submenuindex;
      if (enc != 0) {
        int16_t value = *sub[index].parameter + enc * sub[index].step;
        if (value < sub[index].min) value = sub[index].min;
        if (value > sub[index].max) value = sub[index].max;
        *sub[index].parameter = value;
        if (sub[index].handler != nullptr) {
          sub[index].handler();
        }
        drawsubmenu(index);
      }
      if (!digitalRead(RMENU_ENCSW_IN)) {
        undrawselector(topmenu[topmenuindex].submenuindex);
        drawselector(topmenu[topmenuindex].submenuindex);
        menustate = SUBSELECT;
        if (sub[index].exithandler != nullptr) {
          sub[index].exithandler();
        }
        while (!digitalRead(RMENU_ENCSW_IN)) delay(10);
      }
      break;
    }
  }
}
