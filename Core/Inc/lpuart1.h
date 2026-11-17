#ifndef INC_LPUART1_H_
#define INC_LPUART1_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32l0xx_hal.h"
#include "defines.h"

typedef enum {
    BUS_PROTOCOL_PROPRIETARY = 0, // Existing DLE/STX binary protocol
    BUS_PROTOCOL_MODBUS_DUST = 1, // Particles Plus OEM (Modbus RTU, Slave ID 247)
    BUS_PROTOCOL_MODBUS_PID  = 2  // PID Sensor (Modbus RTU, reserved for later)
} BusProtocol_t;

extern UART_HandleTypeDef hlpuart1;

void Start_Lpuart1_Rx(void);
void Parse_Lpuart1_Byte(uint8_t byte);
void Reset_Lpuart1_RxState(void);
void Lpuart1_SetProtocol(BusProtocol_t protocol);

// Proprietary Binary Protocol Helpers
uint8_t Lpuart1_Send_Data(uint8_t *data, uint16_t length);
void Lpuart1_ContinueSend(void);
void Lpuart1_SendByte(uint8_t data);
void Lpuart1_SendHeader(uint8_t msg_length, uint8_t msg_id);

// Modbus RTU Helpers (Shared by Dust & PID sensors)
uint16_t Modbus_CalcCRC16(const uint8_t *buf, uint16_t len);
bool Modbus_SendAndWait(BusProtocol_t target_protocol, const uint8_t *tx_frame, uint16_t tx_len,
                        uint8_t *rx_out, uint16_t *rx_len_out, uint32_t timeout_ms);

#endif /* INC_LPUART1_H_ */
