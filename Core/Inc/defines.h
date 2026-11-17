/*
 * defines.h
 *
 *  Created on: Sep 16, 2026
 *      Author: MT
 */

#ifndef INC_DEFINES_H_
#define INC_DEFINES_H_

// Choose ONLY ONE of the following:
//#define POGO_USART2   1   // Use USART2 (Wired connection also known as Pogo pins)
#define IR_USART2     1   // Use USART2 (IrDA)

// Configure the actual defines used in code
#ifdef IR_USART2
    #define IR_USART2_SEL 1
#elif defined(POGO_USART2)
    #define POGO_USART2_SEL 1
#else
    #error "Please select either POGO_USART2 or IR_USART2 in defines.h"
#endif

// LPUART
#define DLE 0x10
#define STX 0x02
#define SOURCE 0x95 // must be opposite on receiving end
#define DEST 0x00

// Messages
#define FILLED 1
#define CLEAR 0

#define QUERY 0
#define SETTING 1

// Array sizes
// TX and RX BUFFER ARRAY.
// To be used for either LPUART1 and USART2
#define MAX_RX_BUFFER_ARRAY 256
#define MAX_TX_BUFFER_ARRAY 256

// LPUART1
#define MAX_LPUART1_ARRAY 256
// USART2
#define MAX_USART2_ARRAY 256

// Messages ========================================================
// Version
#define VERSION_QUERY_MSGLGT 0x07
#define VERSION_QUERY_MSGID 0x01

#define VERSION_RESP_MSGLGT 0x0D
#define VERSION_RESP_MSGID 0x02

// Time
#define TIME_QUERY_MSGLGT 0x07
#define TIME_QUERY_MSGID 0x0F

#define TIME_RESP_MSGLGT 0x11
#define TIME_RESP_MSGID 0x10

#define TIME_SET_MSGLGT 0x11
#define TIME_SET_MSGID 0x11

// Power Management & Scheduling ===================================
#define INACTIVITY_SLEEP_MS                  1000   // Enter STOP mode after 1 seconds of inactivity (easy to change)
#define FAST_SAMPLE_THRESHOLD_SEC   1        // If sample interval <= 1s, stay awake with GEN_PWR_EN ON
#define DEFAULT_SAMPLE_INTERVAL_SEC 300    // was 300.  Default 5 minutes (300s); valid range 300s - 3600s

// Wakeup Source Flags
#define WAKEUP_NONE                    0
#define WAKEUP_USART2_IR            1       // PB2 (IR_WAKE_UP) falling edge
#define WAKEUP_NOTECARD_ATTN 2       // PB5 (ATTN_EXT15) falling edge
#define WAKEUP_RTC_TIMER            3       // Scheduled RTC WakeUp timer

// Sampling Timing
#define DEFAULT_SAMPLE_DURATION_SEC 7       // Active pump sample time in seconds (e.g., 7s for bench test, 30s-60s for site)

// Dust Sensor Telemetry Query
#define DUST_QUERY_MSGLGT 0x07
#define DUST_QUERY_MSGID  0x12

#define DUST_RESP_MSGLGT  0x19  // 7 header/CRC + 18 payload bytes = 25 (0x19)
#define DUST_RESP_MSGID   0x13

// Human-Readable ASCII Telemetry Query
#define DUST_ASCII_QUERY_MSGLGT 0x07
#define DUST_ASCII_QUERY_MSGID  0x14

#define DUST_ASCII_RESP_MSGID   0x15
#define DUST_ASCII_RESP_MSGLGT  0x87
// BOOT LOADER ==================================================
// Note: Messages below are used exclusively by the Bootloader, so do NOT use them here in the Application firmware.

#define BOOTLOADER_VERSION_QUERY_MSGID 0x60
#define BOOTLOADER_VERSION_QUERY_MSGLGT 0x07

#define BOOTLOADER_VERSION_RESP_MSGID 0x61
#define BOOTLOADER_VERSION_RESP_MSGLGT 0x0B

#define BOOTLOADER_FWUPDATE_SET_MSGID 0x62
#define BOOTLOADER_FWUPDATE_SET_MSGLGT 0x08

#define BOOTLOADER_FWUPDATE_RESP_MSGID 0x63
#define BOOTLOADER_FWUPDATE_RESP_MSGLGT 0x08

#define BOOTLOADER_TRANSMITDATA_SET_MSGID   0x64
#define BOOTLOADER_TRANSMITDATA_SET_MSGLGT  0x8E

#define BOOTLOADER_TRANSMITDATA_RESP_MSGID   0x65
#define BOOTLOADER_TRANSMITDATA_RESP_MSGLGT  0x0E

#endif /* INC_DEFINES_H_ */
