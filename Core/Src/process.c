/*
 * process.c
 *
 *  Created on: Jun 23, 2026
 *      Author: MT
 */
#include "stdio.h"
#include "usart2.h"
#include "lpuart1.h"
#include "main.h"
#include "string.h"
#include "process.h"
#include "rtc.h"
#include "particle_sensor.h"
#include "misc.h"


// Function definition
void ProcessMsg(void)
{
    // 1. Check for incoming packets on LPUART1 (RS-485)
    if (Lpuart1.status == FILLED)
    {
        switch (Lpuart1shadow.messageid)
        {
            case VERSION_QUERY_MSGID:
                Version_Resp(COMM_PORT_LPUART1);
                Lpuart1shadow.messageid = 0;
                break;

            default:
                Lpuart1shadow.messageid = 0;
                break;
        }
        Lpuart1.status = CLEAR;
        Counter.totalvalidpacketcount++;
    }

    // 2. Check for incoming packets on USART2 (IrDA / Pogo)
    if (Usart2.status == FILLED)
    {
        switch (Usart2shadow.messageid)
        {
            case VERSION_QUERY_MSGID:
                Version_Resp(COMM_PORT_USART2);
                Usart2shadow.messageid = 0;
                break;

            case TIME_QUERY_MSGID:
                RTC_ReadTime();
                Time_Resp(COMM_PORT_USART2);
                Usart2shadow.messageid = 0;
                break;

            case TIME_SET_MSGID:
                Time_StoreData(COMM_PORT_USART2);
                RTC_ReadTime(); // read back the time data we just stored for the response
                Time_Resp(COMM_PORT_USART2);
                Usart2shadow.messageid = 0;
                break;

            case DUST_QUERY_MSGID:
                Dust_Resp(COMM_PORT_USART2);
                Usart2shadow.messageid = 0;
                break;

            case DUST_ASCII_QUERY_MSGID:
                Dust_AsciiResp(COMM_PORT_USART2);
                Usart2shadow.messageid = 0;
                break;

            default:
                Usart2shadow.messageid = 0;
                break;
        }
        Usart2.status = CLEAR;
        Counter.totalvalidpacketcount++;
    }
}

void Version_Resp(CommPort_t port)
{
    if (port == COMM_PORT_LPUART1)
    {
        Lpuart1_SendHeader(VERSION_RESP_MSGLGT, VERSION_RESP_MSGID);
        Lpuart1_SendByte(0x00); // reserved
        Lpuart1_SendByte(0x00); // reserved - boxselection
        Lpuart1_SendByte(0x00); // firmware version
        Lpuart1_SendByte(0x01);
        Lpuart1_SendByte(0x00); // software version
        Lpuart1_SendByte(0x01);
        Lpuart1_SendByte(Lpuart1.crcsend);
    }
    else
    {
        SendHeader(port, VERSION_RESP_MSGLGT, VERSION_RESP_MSGID);
        SendByte(port, 0x00); // reserved
        SendByte(port, 0x00); // reserved - boxselection
        SendByte(port, 0x00); // firmware version
        SendByte(port, 0x01);
        SendByte(port, 0x00); // software version
        SendByte(port, 0x01);
        SendByte(port, Usart2.crcsend);
    }
}

void Time_StoreData(CommPort_t port)
{
    if (port == COMM_PORT_USART2)
    {
        // Saving the stuff FW received from Viewer (as a SET)
        Time.boxselection = Usart2shadow.payload[0];
        Time.reserved0    = Usart2shadow.payload[1];
        Time.set_year     = Usart2shadow.payload[2]; // 0-99
        Time.set_month    = Usart2shadow.payload[3]; // 1-12
        Time.set_day      = Usart2shadow.payload[4]; // 1-31
        Time.set_hour     = Usart2shadow.payload[5]; // 1-12 (12-hour format)
        Time.set_minute   = Usart2shadow.payload[6]; // 0-59
        Time.set_second   = Usart2shadow.payload[7]; // 0-59
        Time.set_ampm     = Usart2shadow.payload[8]; // 0=AM, 1=PM
        Time.set_weekday  = Usart2shadow.payload[9]; // 1=Monday, 7=Sunday

        // Set the new time based on what we received above
        RTC_SetTime();
        SysState.next_sample_epoch = RTC_GetEpochSeconds() + SysState.sample_interval_sec;
    }
}

void Time_Resp(CommPort_t port)
{
    if (port == COMM_PORT_USART2)
    {
        SendHeader(port, TIME_RESP_MSGLGT, TIME_RESP_MSGID);
        SendByte(port, Time.boxselection);     // Box selection
        SendByte(port, Time.reserved0);        // Reserved0
        SendByte(port, Time.read_year);        // 0-99 (00-99)
        SendByte(port, Time.read_month);       // 1-12
        SendByte(port, Time.read_day);         // 1-31
        SendByte(port, Time.read_hour);        // 1-12
        SendByte(port, Time.read_minute);      // 0-59
        SendByte(port, Time.read_second);      // 0-59
        SendByte(port, Time.read_ampm);        // 0=AM, 1=PM
        SendByte(port, Time.read_weekday);     // 1=Monday, 7=Sunday
        SendByte(port, Usart2.crcsend);
    }
}

void Dust_Resp(CommPort_t port)
{
    if (port == COMM_PORT_USART2)
    {
        uint32_t pm25_bits = 0;
        uint32_t pm10_bits = 0;
        memcpy(&pm25_bits, &LatestDustReading.pm2_5, sizeof(uint32_t));
        memcpy(&pm10_bits, &LatestDustReading.pm10, sizeof(uint32_t));

        SendHeader(port, DUST_RESP_MSGLGT, DUST_RESP_MSGID);

        // Byte 0: Valid flag (1 = valid reading latched, 0 = none yet)
        SendByte(port, LatestDustReading.valid ? 0x01 : 0x00);

        // Bytes 1-4: Record Number (Big-Endian U32)
        SendByte(port, (uint8_t)(LatestDustReading.record_number >> 24));
        SendByte(port, (uint8_t)(LatestDustReading.record_number >> 16));
        SendByte(port, (uint8_t)(LatestDustReading.record_number >> 8));
        SendByte(port, (uint8_t)(LatestDustReading.record_number & 0xFF));

        // Bytes 5-8: PM2.5 IEEE-754 Float (Big-Endian)
        SendByte(port, (uint8_t)(pm25_bits >> 24));
        SendByte(port, (uint8_t)(pm25_bits >> 16));
        SendByte(port, (uint8_t)(pm25_bits >> 8));
        SendByte(port, (uint8_t)(pm25_bits & 0xFF));

        // Bytes 9-12: PM10 IEEE-754 Float (Big-Endian)
        SendByte(port, (uint8_t)(pm10_bits >> 24));
        SendByte(port, (uint8_t)(pm10_bits >> 16));
        SendByte(port, (uint8_t)(pm10_bits >> 8));
        SendByte(port, (uint8_t)(pm10_bits & 0xFF));

        // Bytes 13-14: Sample Duration in seconds (Big-Endian U16)
        SendByte(port, (uint8_t)(LatestDustReading.sample_seconds >> 8));
        SendByte(port, (uint8_t)(LatestDustReading.sample_seconds & 0xFF));

        // Bytes 15-16: Average Flow Rate in mL/min (Big-Endian U16)
        SendByte(port, (uint8_t)(LatestDustReading.flow_rate_ml_min >> 8));
        SendByte(port, (uint8_t)(LatestDustReading.flow_rate_ml_min & 0xFF));

        // Byte 17: Error Status low byte (0x00 = No Error)
        SendByte(port, (uint8_t)(LatestDustReading.error_status & 0xFF));

        SendByte(port, Usart2.crcsend);
    }
}

void Dust_AsciiResp(CommPort_t port)
{
    if (port == COMM_PORT_USART2)
    {
        RTC_ReadTime();

        uint32_t pm25_x100   = (uint32_t)(LatestDustReading.pm2_5 * 100.0f + 0.5f);
        uint32_t pm10_x100   = (uint32_t)(LatestDustReading.pm10 * 100.0f + 0.5f);
        uint32_t counts_x100 = (uint32_t)(LatestDustReading.total_counts * 100.0f + 0.5f);

        char ascii_buf[160] = {0};
        int len = snprintf(
            ascii_buf, sizeof(ascii_buf),
            "\r\nTime=20%02u-%02u-%02u %02u:%02u:%02u %s, Rec=%lu, PM2.5=%lu.%02lu ug/m3, PM10=%lu.%02lu ug/m3, Counts=%lu.%02lu/cm3, Flow=%u mL/min, Dur=%us, Err=0x%02X\r\n",
            Time.read_year, Time.read_month, Time.read_day,
            Time.read_hour, Time.read_minute, Time.read_second,
            (Time.read_ampm == 1) ? "PM" : "AM",
            (unsigned long)LatestDustReading.record_number,
            (unsigned long)(pm25_x100 / 100), (unsigned long)(pm25_x100 % 100),
            (unsigned long)(pm10_x100 / 100), (unsigned long)(pm10_x100 % 100),
            (unsigned long)(counts_x100 / 100), (unsigned long)(counts_x100 % 100),
            LatestDustReading.flow_rate_ml_min,
            LatestDustReading.sample_seconds,
            LatestDustReading.error_status
        );

        if (len > 0)
        {
            Send_Data(port, (uint8_t *)ascii_buf, (uint16_t)len);
            while (tx_busy);
        }
    }
}



