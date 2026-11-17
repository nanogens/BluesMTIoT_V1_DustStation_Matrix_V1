#ifndef INC_USART2_H_
#define INC_USART2_H_

#include <stdbool.h>
#include <stdint.h>
#include "stm32l0xx_hal.h"
#include "defines.h"

//==============================================================================
// Port Selection Type
//==============================================================================
typedef enum {
    COMM_PORT_LPUART1 = 0,
    COMM_PORT_USART2  = 1
} CommPort_t;

//==============================================================================
// Function Prototypes
//==============================================================================
void Start_Uart2_Rx(void);
void Parse_Usart2_Byte(uint8_t byte);

uint8_t Send_Data(CommPort_t port, uint8_t *data, uint16_t length);
void SendByte(CommPort_t port, uint8_t data);
void SendHeader(CommPort_t port, uint8_t msg_length, uint8_t msg_id);
void ContinueSend(void);

void FalseHeader(uint8_t byte);
bool Search_MsgID(uint8_t settingorquery, uint8_t messageidglobal);

void Reset_Usart2_RxState(void);

//==============================================================================
// Handles
//==============================================================================
#ifdef IR_USART2_SEL
  extern IRDA_HandleTypeDef hirda2;
  extern IRDA_HandleTypeDef* pActiveIrda;
  #define pActiveUart NULL
#else
  extern UART_HandleTypeDef husart2;
  extern UART_HandleTypeDef* pActiveUart;
  #define pActiveIrda NULL
#endif

//==============================================================================
// Variables
//==============================================================================
extern uint8_t rx_buffer[MAX_RX_BUFFER_ARRAY];
extern uint8_t tx_buffer[MAX_TX_BUFFER_ARRAY];
extern volatile uint16_t rx_index;
extern volatile uint16_t tx_index;
extern volatile uint16_t tx_length;
extern volatile uint8_t tx_busy;

//==============================================================================
// Macros
//==============================================================================
#if IR_USART2_SEL // IR version
  #define MY_UART_HANDLE   hirda2
  #define MY_UART_INSTANCE USART2

  #define DE_RE_ENABLE()   HAL_GPIO_WritePin(USART2_SD_DE_GPIO_Port, USART2_SD_DE_Pin, GPIO_PIN_RESET)
  #define DE_RE_DISABLE()  HAL_GPIO_WritePin(USART2_SD_DE_GPIO_Port, USART2_SD_DE_Pin, GPIO_PIN_RESET)
#else // POGO version
  #define MY_UART_HANDLE   husart2
  #define MY_UART_INSTANCE USART2

  #define DE_RE_ENABLE()   HAL_GPIO_WritePin(USART2_SD_DE_GPIO_Port, USART2_SD_DE_Pin, GPIO_PIN_SET)
  #define DE_RE_DISABLE()  HAL_GPIO_WritePin(USART2_SD_DE_GPIO_Port, USART2_SD_DE_Pin, GPIO_PIN_RESET)
#endif

#endif /* INC_USART2_H_ */
