#include "main.h"
#include "stdio.h"
#include "rtc.h"
#include <time.h>

void RTC_Init(void)
{
  // Initialize RTC only if backup register DR0 does not hold the magic key
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) != 0x32F2)
  {
    // Default fallback time: September 23, 2025, 3:26:00 PM
    Time.set_year = 25;
    Time.set_month = 9;
    Time.set_day = 23;
    Time.set_hour = 3;       // 1-12 format (3 PM + set_ampm=1 -> 15:00)
    Time.set_minute = 26;
    Time.set_second = 0;
    Time.set_ampm = 1;       // PM
    Time.set_weekday = RTC_WEEKDAY_TUESDAY;

    RTC_SetTime();
  }
}

void RTC_SetTime(void)
{
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  // Convert incoming 12-hour + AM/PM to 24-hour format
  uint8_t hour24 = Time.set_hour;  // Time.set_hour is 1-12

  if (Time.set_ampm == 1) { // PM
      if (hour24 != 12) {
          hour24 += 12;
      }
  } else { // AM
      if (hour24 == 12) {
          hour24 = 0;
      }
  }

  // Convert to BCD format
  sTime.Hours = ((hour24 / 10) << 4) | (hour24 % 10);
  sTime.Minutes = ((Time.set_minute / 10) << 4) | (Time.set_minute % 10);
  sTime.Seconds = ((Time.set_second / 10) << 4) | (Time.set_second % 10);
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;

  sDate.WeekDay = (Time.set_weekday >= 1 && Time.set_weekday <= 7) ? Time.set_weekday : RTC_WEEKDAY_MONDAY;
  sDate.Month = ((Time.set_month / 10) << 4) | (Time.set_month % 10);
  sDate.Date = ((Time.set_day / 10) << 4) | (Time.set_day % 10);
  sDate.Year = ((Time.set_year / 10) << 4) | (Time.set_year % 10);

  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }

  // Mark RTC as initialized in Backup Register 0
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0x32F2);
}

void RTC_ReadTime(void)
{
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  // Wait for RTC shadow registers to synchronize (required after STOP mode)
  HAL_RTC_WaitForSynchro(&hrtc);

  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  uint8_t hour24 = sTime.Hours;

  Time.read_ampm = (hour24 >= 12) ? 1 : 0;

  uint8_t hour12 = hour24 % 12;
  if (hour12 == 0) hour12 = 12;

  Time.read_hour = hour12;
  Time.read_minute = sTime.Minutes;
  Time.read_second = sTime.Seconds;
  Time.read_centisecond = (uint8_t)((255 - sTime.SubSeconds) * 25 / 64);

  Time.read_year = sDate.Year;
  Time.read_month = sDate.Month;
  Time.read_day = sDate.Date;
  Time.read_full_year = 2000 + Time.read_year;
  Time.read_weekday = sDate.WeekDay;
}

uint8_t RTC_IsTimeSet(void)
{
  return (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == 0x32F2);
}

const char* GetWeekdayString(uint8_t weekday)
{
  switch(weekday)
  {
    case 1: return "Monday";
    case 2: return "Tuesday";
    case 3: return "Wednesday";
    case 4: return "Thursday";
    case 5: return "Friday";
    case 6: return "Saturday";
    case 7: return "Sunday";
    default: return "Unknown";
  }
}

uint32_t RTC_GetEpochSeconds(void)
{
  RTC_ReadTime();

  uint8_t hour24 = Time.read_hour;
  if (Time.read_ampm == 1) {
      if (hour24 != 12) hour24 += 12;
  } else {
      if (hour24 == 12) hour24 = 0;
  }

  // Pure integer seconds since Jan 1, 2000 (immune to newlib mktime issues)
  uint32_t y = Time.read_year; // 0..99 (2000..2099)
  uint32_t m = (Time.read_month >= 1 && Time.read_month <= 12) ? Time.read_month : 1;
  uint32_t d = (Time.read_day >= 1 && Time.read_day <= 31) ? Time.read_day : 1;

  static const uint16_t days_before_month[12] = {
      0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
  };

  uint32_t leap_days = (y + 3) / 4; // Leap years since 2000 (2000 was a leap year)
  if ((y % 4 == 0) && (m > 2)) {
      leap_days += 1;
  }

  uint32_t total_days = (y * 365UL) + leap_days + days_before_month[m - 1] + (d - 1);
  return (total_days * 86400UL) + ((uint32_t)hour24 * 3600UL) + ((uint32_t)Time.read_minute * 60UL) + Time.read_second;
}

