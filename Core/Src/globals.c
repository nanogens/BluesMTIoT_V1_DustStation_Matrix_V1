#include "globals.h"

struct Lpuart1 Lpuart1 = {0};
struct Lpuart1shadow1 Lpuart1shadow = {0};
struct Counter1 Counter;
struct Usart21 Usart2 = {0};
struct Usart2shadow1 Usart2shadow = {0};
struct Time1 Time = {0};
struct SystemState1 SysState =
{
  .last_activity_tick = 0,
  .sample_interval_sec = DEFAULT_SAMPLE_INTERVAL_SEC,
  .next_sample_epoch = 0,
  .sampling_active = 0,
  .attn_pending = 0,
  .rtc_wakeup_pending = 0,
  .wakeup_source = WAKEUP_NONE
};
