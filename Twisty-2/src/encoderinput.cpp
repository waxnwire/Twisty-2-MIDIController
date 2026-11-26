#include <Arduino.h>
#include <hardware/irq.h>
#include <hardware/sync.h>
#include <hardware/timer.h>

#include "encoderinput.h"

#define ENCDIVIDE 4
#define TIMER_MICROS 1000
#define ALARM_NUM 0
#define ALARM_IRQ timer_hardware_alarm_get_irq_num(timer_hw, ALARM_NUM)

ClickEncoder enc[NUMENCODERS] = {
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE),
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE),
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE),
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE),
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE),
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE),
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE),
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE),
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE),
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE),
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE),
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE),
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE),
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE),
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE),
  ClickEncoder(ENCA_IN, ENCB_IN, ENCSW_IN, ENCDIVIDE)
};

ClickEncoder lmenuenc(LMENU_ENCA_IN, LMENU_ENCB_IN, LMENU_ENCSW_IN, ENCDIVIDE);
ClickEncoder rmenuenc(RMENU_ENCA_IN, RMENU_ENCB_IN, RMENU_ENCSW_IN, ENCDIVIDE);

static void alarm_irq(void);

static void alarm_in_us_arm(uint32_t delay_us) {
  uint64_t target = timer_hw->timerawl + delay_us;
  timer_hw->alarm[ALARM_NUM] = (uint32_t)target;
}

void start_encoder_timer(void) {
  hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
  irq_set_exclusive_handler(ALARM_IRQ, alarm_irq);
  irq_set_enabled(ALARM_IRQ, true);
  alarm_in_us_arm(TIMER_MICROS);
}

static void alarm_irq(void) {
  for (int addr = 0; addr < NUMENCODERS; ++addr) {
    digitalWrite(A_MUX_0, addr & 1);
    digitalWrite(A_MUX_1, addr & 2);
    digitalWrite(A_MUX_2, addr & 4);
    digitalWrite(A_MUX_3, addr & 8);
    delayMicroseconds(4);
    enc[addr].service();
  }
  lmenuenc.service();
  rmenuenc.service();
  hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
  alarm_in_us_arm(TIMER_MICROS);
}
