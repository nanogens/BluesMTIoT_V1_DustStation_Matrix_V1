/*
 * globals.h
 *
 *  Created on: Sep 3, 2025
 *      Author: MT
 */

#ifndef INC_GLOBALS_H_
#define INC_GLOBALS_H_

#include <stdint.h>
#include "defines.h"

struct Lpuart1 {
  uint8_t sent;
  uint8_t crcsend;
  uint8_t bufferrx[1];
  uint8_t payload[MAX_LPUART1_ARRAY];
  uint8_t status;
  uint8_t got;
  uint8_t messagelength;
  uint8_t messageidglobal;
  uint8_t crcmsg;
  uint8_t crcset;
  uint8_t prevmessageidglobal;
};
extern struct Lpuart1 Lpuart1;

struct Lpuart1shadow1 {
  uint8_t messageid;
  uint8_t payload[MAX_LPUART1_ARRAY];
};
extern struct Lpuart1shadow1 Lpuart1shadow;

struct Usart21 {
  uint8_t sent;
  uint8_t crcsend;
  uint8_t bufferrx[1];
  uint8_t payload[MAX_USART2_ARRAY];
  uint8_t status;
  uint8_t got;
  uint8_t messagelength;
  uint8_t messageidglobal;
  uint8_t crcmsg;
  uint8_t crcset;
  uint8_t prevmessageidglobal;
};
extern struct Usart21 Usart2;

struct Usart2shadow1 {
  uint8_t messageid;
  uint8_t payload[MAX_USART2_ARRAY];
};
extern struct Usart2shadow1 Usart2shadow;

struct Counter1 {
  uint32_t yi;
  uint32_t y0;
  uint32_t y1;
  uint8_t streaming;
  uint32_t totalvalidpacketcount;
  uint32_t reedswitchcount;
  int repeat;
};
extern struct Counter1 Counter;

struct Time1 {
  uint8_t reserved0;
  uint8_t boxselection;
  uint8_t set_year;
  uint8_t set_month;
  uint8_t set_day;
  uint8_t set_hour;
  uint8_t set_minute;
  uint8_t set_second;
  uint8_t set_ampm;
  uint8_t set_weekday;
  uint8_t read_year;
  uint8_t read_month;
  uint8_t read_day;
  uint8_t read_hour;
  uint8_t read_minute;
  uint8_t read_second;
  uint8_t read_centisecond;
  uint8_t read_ampm;
  uint16_t read_full_year;
  uint8_t read_weekday;
};
extern struct Time1 Time;

struct SystemState1
{
  volatile uint32_t last_activity_tick;
  uint32_t sample_interval_sec;
  uint32_t next_sample_epoch;
  volatile uint8_t sampling_active;
  volatile uint8_t attn_pending;
  volatile uint8_t rtc_wakeup_pending;
  volatile uint8_t wakeup_source;
};
extern struct SystemState1 SysState;

#endif

