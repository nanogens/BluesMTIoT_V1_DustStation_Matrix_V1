#include "main.h"
#include "string.h"
#include "usart2.h"

// IMPORTANT NOTE:
// Use this QUERY to test response on USART2:
// Query: 10 02 00 95 07 01 AF
// Response: 10 02 95 00 0D 02 00 00 00 01 00 01 B8

// =========================================================================
// INITIALIZATION
// =========================================================================

void Start_Uart2_Rx(void)
{


#ifdef IR_USART2_SEL
    // Flush any power-up glitch bytes and clear error flags on USART2 IrDA
    __HAL_IRDA_CLEAR_OREFLAG(&hirda2);
    __HAL_IRDA_CLEAR_FEFLAG(&hirda2);
    __HAL_IRDA_CLEAR_NEFLAG(&hirda2);
    __HAL_IRDA_FLUSH_DRREGISTER(&hirda2);
    HAL_IRDA_Receive_IT(&hirda2, Usart2.bufferrx, 1);
#else
    __HAL_UART_CLEAR_OREFLAG(&husart2);
    __HAL_UART_CLEAR_FEFLAG(&husart2);
    __HAL_UART_CLEAR_NEFLAG(&husart2);
    __HAL_UART_FLUSH_DRREGISTER(&husart2);
    HAL_UART_Receive_IT(&husart2, Usart2.bufferrx, 1);
#endif
}


// =========================================================================
// TRANSMIT FUNCTIONS
// =========================================================================

uint8_t Send_Data(CommPort_t port, uint8_t *data, uint16_t length)
{
    if (tx_busy || length > sizeof(tx_buffer)) return 0;

    memcpy(tx_buffer, data, length);
    tx_length = length;
    tx_index = 0;
    tx_busy = 1;

    DE_RE_ENABLE();
#ifdef IR_USART2_SEL
    USART2->CR1 &= ~USART_CR1_RE;

    for (uint16_t f = 0; f < length; f++)
    {
        HAL_IRDA_Transmit_IT(pActiveIrda, &tx_buffer[f], 1);
        while (!(USART2->ISR & USART_ISR_TC));
        for (volatile int i = 0; i < 150; i++);
    }

    USART2->CR1 |= USART_CR1_RE;
    DE_RE_DISABLE();
    tx_busy = 0;
#else
    HAL_UART_Transmit_IT(pActiveUart, &tx_buffer[0], 1);
#endif

    return 1;
}

void ContinueSend(void)
{
    tx_index++;
    if (tx_index < tx_length)
    {
#ifdef IR_USART2_SEL
        HAL_IRDA_Transmit_IT(pActiveIrda, &tx_buffer[tx_index], 1);
#else
        HAL_UART_Transmit_IT(pActiveUart, &tx_buffer[tx_index], 1);
#endif
    }
    else
    {
#ifdef IR_USART2_SEL
            // Re-enable IrDA receiver
        USART2->CR1 |= USART_CR1_RE;
#endif
        DE_RE_DISABLE();

        tx_busy = 0;
    }
}

void SendByte(CommPort_t port, uint8_t data)
{
    Send_Data(port, &data, 1);
    while (tx_busy); // Wait for non-blocking ISR transfer to complete
    Usart2.crcsend += data;
}

void SendHeader(CommPort_t port, uint8_t msg_length, uint8_t msg_id)
{
    Usart2.crcsend = 0;
    SendByte(port, DLE);
    SendByte(port, STX);
    SendByte(port, SOURCE);
    SendByte(port, DEST);
    SendByte(port, msg_length);
    SendByte(port, msg_id);
}

void Parse_Usart2_Byte(uint8_t byte)
{
  if (Usart2.status != FILLED)
  {
    if (Usart2.got == 0)
	{
      FalseHeader(byte);
    }
	else if (Usart2.got == 1)
	{
      if (byte == STX)
	  {
        Usart2.got = 2;
      }
	  else
	  {
        FalseHeader(byte);
      }
    }
	else if (Usart2.got == 2)
	{
      if (byte == DEST) // Source as defined by Viewer
      {
        Usart2.got = 3;
      }
	  else
	  {
        FalseHeader(byte);
      }
    }
	else if (Usart2.got == 3)
	{
      if (byte == SOURCE) // Source as defined by Viewer
      {
        Usart2.got = 4;
      }
	  else
	  {
        FalseHeader(byte);
      }
    }
	else if (Usart2.got == 4) // message length
    {
      // Make a function to check message length
      Usart2.messagelength = byte;
      Usart2.got = 5;
    }
	else if (Usart2.got == 5)
	{
      // Make a function to check message id
      Usart2.messageidglobal = byte;
      Usart2.got = 6;
    }

    // Query
    else if ((Usart2.got == 6) && (Search_MsgID(QUERY, Usart2.messageidglobal) == true))
	{
      Usart2.crcmsg = byte;

      if (Usart2.crcmsg == (DLE + STX + DEST + SOURCE + Usart2.messagelength + Usart2.messageidglobal) % 256)
	  {
        Usart2.status = FILLED; // immediately block it from re-entering this receive interrupt until present request is processed in main loop
        Usart2shadow.messageid = Usart2.messageidglobal;

        if (Usart2.messageidglobal == BOOTLOADER_FWUPDATE_SET_MSGID)
		{
          HAL_GPIO_TogglePin(LED_A_GPIO_Port, LED_A_Pin);
        }
      }
      Usart2.got = 0;
      Usart2.crcmsg = 0;
    }

    // Setting - next 2 blocks
    else if (
      (Search_MsgID(SETTING, Usart2.messageidglobal) == true) &&
      ((Usart2.got >= 6) && (Usart2.got < (Usart2.messagelength - 1)))
    )
	{
      if (((Usart2.got - 6) < MAX_USART2_ARRAY) && ((Usart2.got - 6) >= 0)) // protection
      {
        Usart2.payload[Usart2.got - 6] = byte;
        Usart2.got++;
      }
	  else
	  {
        Usart2.got = 0;
      }
    }
	else if (
      (Usart2.got <= (Usart2.messagelength - 1)) &&
      (Search_MsgID(SETTING, Usart2.messageidglobal) == true)
    )
	{
      Usart2.crcmsg = byte; // the crc at the end of Set Working Parameters
      // it is DEST + SOURCE and not SOURCE + DEST because dest & source values are in relation to what we send not receive.
      Usart2.crcset = DLE + STX + DEST + SOURCE + Usart2.messagelength + Usart2.messageidglobal;

      if (Usart2.messagelength >= 7)
	  {
        for (uint16_t x = 0; x < (Usart2.messagelength - 7); x++) // add up the incoming bytes in the incomingbuffer0 to check it against the crc
        {
          if (x < MAX_USART2_ARRAY) // protection
          {
            Usart2.crcset += Usart2.payload[x];
          }
        }
      }

      if (Usart2.crcset == Usart2.crcmsg) //Uart1.crcmsg)  // if it equals the crc of the message packet
      {
        Usart2.status = FILLED; // immediately block it from reentering uart0 rx until request is processed in main loop
        // makes us reply host without making any setting if its outside range
        Usart2.got = 0;

        for (uint16_t y = 0; y < MAX_USART2_ARRAY; y++)
		{
          Usart2shadow.payload[y] = Usart2.payload[y];
        }
        Usart2shadow.messageid = Usart2.messageidglobal;
      }
	  else
	  {
        Usart2.got = 0;
      }
    }
	else
	{
      Usart2.got = 0;
      Usart2.crcmsg = 0;
    }
  }
  else
  {
    Usart2.got = 0;
    Usart2.crcmsg = 0;
  }
}

void Reset_Usart2_RxState(void)
{
  Usart2.got = 0;
		Usart2.crcmsg = 0;
}

// =========================================================================
// LEGACY / HELPER STUBS
// =========================================================================

void FalseHeader(uint8_t byte)
{
  if(byte == DLE)
  {
	   Usart2.got = 1;
  }
  else
  {
	   Usart2.got = 0;
    Usart2.crcmsg = 0;
  }
}

bool Search_MsgID(uint8_t settingorquery, uint8_t messageidglobal)
{
  if (settingorquery == QUERY)
  {
    if (
        (messageidglobal == VERSION_QUERY_MSGID) ||
        (messageidglobal == TIME_QUERY_MSGID) ||
        (messageidglobal == DUST_QUERY_MSGID) ||
        (messageidglobal == DUST_ASCII_QUERY_MSGID)
    )
    {
      return true;
    }
    return false;
  }
  else // SETTING messages
  {
    if (
        (messageidglobal == TIME_SET_MSGID)
    )
    {
      return true;
    }
    return false;
  }
}

