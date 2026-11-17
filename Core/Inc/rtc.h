#ifndef INC_RTC_H_
#define INC_RTC_H_

#include <stdint.h>
#include "stm32l0xx_hal.h"

extern RTC_HandleTypeDef hrtc;

void RTC_Init(void);
void RTC_SetTime(void);
void RTC_ReadTime(void);
uint8_t RTC_IsTimeSet(void);
uint32_t RTC_GetEpochSeconds(void);
const char* GetWeekdayString(uint8_t weekday);

#endif /* INC_RTC_H_ */
