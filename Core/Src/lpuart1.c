#include "main.h"
#include "string.h"
#include "lpuart1.h"

static BusProtocol_t active_bus_protocol = BUS_PROTOCOL_PROPRIETARY;

// Proprietary protocol RX state
static uint8_t lpuart1_rx_state = 0;
static uint8_t lpuart1_payload_idx = 0;

// Modbus RTU RX state
#define MODBUS_MAX_FRAME 256
static uint8_t modbus_rx_buf[MODBUS_MAX_FRAME];
static volatile uint16_t modbus_rx_idx = 0;
static volatile uint16_t modbus_expected_len = 0;
static volatile uint8_t modbus_frame_ready = 0;

// Shared TX buffer
static uint8_t lpuart1_tx_buffer[MAX_TX_BUFFER_ARRAY];
static volatile uint16_t lpuart1_tx_index = 0;
static volatile uint16_t lpuart1_tx_length = 0;
static volatile uint8_t lpuart1_tx_busy = 0;

void Start_Lpuart1_Rx(void)
{
    __HAL_UART_CLEAR_OREFLAG(&hlpuart1);
    __HAL_UART_CLEAR_FEFLAG(&hlpuart1);
    __HAL_UART_CLEAR_NEFLAG(&hlpuart1);
    __HAL_UART_FLUSH_DRREGISTER(&hlpuart1);
    HAL_UART_Receive_IT(&hlpuart1, Lpuart1.bufferrx, 1);
}

void Reset_Lpuart1_RxState(void)
{
    lpuart1_rx_state = 0;
    modbus_rx_idx = 0;
    modbus_expected_len = 0;
    modbus_frame_ready = 0;
}

void Lpuart1_SetProtocol(BusProtocol_t protocol)
{
    active_bus_protocol = protocol;
    Reset_Lpuart1_RxState();
}

uint8_t Lpuart1_Send_Data(uint8_t *data, uint16_t length)
{
    if (lpuart1_tx_busy || length == 0 || length > sizeof(lpuart1_tx_buffer)) return 0;

    memcpy(lpuart1_tx_buffer, data, length);
    lpuart1_tx_length = length;
    lpuart1_tx_index = 0;
    lpuart1_tx_busy = 1;

    // Enable RS-485 Driver (PB12 HIGH)
    HAL_GPIO_WritePin(LPUART1_DE_GPIO_Port, LPUART1_DE_Pin, GPIO_PIN_SET);
    HAL_UART_Transmit_IT(&hlpuart1, &lpuart1_tx_buffer[0], 1);

    return 1;
}

void Lpuart1_ContinueSend(void)
{
    lpuart1_tx_index++;
    if (lpuart1_tx_index < lpuart1_tx_length)
    {
        HAL_UART_Transmit_IT(&hlpuart1, &lpuart1_tx_buffer[lpuart1_tx_index], 1);
    }
    else
    {
        // Return RS-485 transceiver to receive mode (PB12 LOW)
        HAL_GPIO_WritePin(LPUART1_DE_GPIO_Port, LPUART1_DE_Pin, GPIO_PIN_RESET);
        lpuart1_tx_busy = 0;
    }
}

// =========================================================================
// PROPRIETARY BINARY PROTOCOL HELPERS
// =========================================================================

void Lpuart1_SendByte(uint8_t data)
{
    Lpuart1_Send_Data(&data, 1);
    while (lpuart1_tx_busy);
    Lpuart1.crcsend += data;
}

void Lpuart1_SendHeader(uint8_t msg_length, uint8_t msg_id)
{
    Lpuart1.crcsend = 0;
    Lpuart1_SendByte(DLE);
    Lpuart1_SendByte(STX);
    Lpuart1_SendByte(SOURCE);
    Lpuart1_SendByte(DEST);
    Lpuart1_SendByte(msg_length);
    Lpuart1_SendByte(msg_id);
}

// =========================================================================
// MODBUS RTU HELPERS
// =========================================================================

uint16_t Modbus_CalcCRC16(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t pos = 0; pos < len; pos++)
    {
        crc ^= (uint16_t)buf[pos];
        for (int i = 8; i != 0; i--)
        {
            if ((crc & 0x0001) != 0)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc; // Transmitted Low-Byte first, High-Byte second
}

static void Parse_Modbus_RTU_Byte(uint8_t byte)
{
    if (modbus_frame_ready) return;

    if (modbus_rx_idx < MODBUS_MAX_FRAME)
    {
        modbus_rx_buf[modbus_rx_idx++] = byte;
    }
    else
    {
        modbus_rx_idx = 0;
        modbus_expected_len = 0;
        return;
    }

    // Determine expected frame length once Function Code (byte 1) and Byte Count (byte 2) arrive
    if (modbus_rx_idx == 2)
    {
        uint8_t func = modbus_rx_buf[1];
        if (func & 0x80)
        {
            // Modbus Exception Response: [ID][Func|0x80][ExCode][CRCL][CRCH] = 5 bytes
            modbus_expected_len = 5;
        }
        else if (func == 0x06 || func == 0x10)
        {
            // Write Single (0x06) or Write Multiple (0x10) Echo Response = 8 bytes
            modbus_expected_len = 8;
        }
    }
    else if (modbus_rx_idx == 3 && modbus_rx_buf[1] == 0x03)
    {
        // Read Holding Registers (0x03): [ID][0x03][ByteCount][Data...][CRCL][CRCH]
        modbus_expected_len = 5 + modbus_rx_buf[2];
    }

    if (modbus_expected_len > 0 && modbus_rx_idx >= modbus_expected_len)
    {
        uint16_t calc_crc = Modbus_CalcCRC16(modbus_rx_buf, modbus_expected_len - 2);
        uint16_t rx_crc   = (uint16_t)modbus_rx_buf[modbus_expected_len - 2] |
                            ((uint16_t)modbus_rx_buf[modbus_expected_len - 1] << 8);

        if (calc_crc == rx_crc)
        {
            modbus_frame_ready = 1;
        }
        modbus_rx_idx = 0;
        modbus_expected_len = 0;
    }
}

bool Modbus_SendAndWait(BusProtocol_t target_protocol, const uint8_t *tx_frame, uint16_t tx_len,
                        uint8_t *rx_out, uint16_t *rx_len_out, uint32_t timeout_ms)
{
    Lpuart1_SetProtocol(target_protocol);

    if (!Lpuart1_Send_Data((uint8_t *)tx_frame, tx_len))
    {
        Lpuart1_SetProtocol(BUS_PROTOCOL_PROPRIETARY);
        return false;
    }

    // Wait for TX shift-out to complete
    while (lpuart1_tx_busy);

    // Wait for Modbus RTU response frame until timeout
    uint32_t start = HAL_GetTick();
    while (!modbus_frame_ready)
    {
        if ((HAL_GetTick() - start) >= timeout_ms)
        {
            Lpuart1_SetProtocol(BUS_PROTOCOL_PROPRIETARY);
            return false;
        }
    }

    if (rx_out != NULL && rx_len_out != NULL)
    {
        *rx_len_out = (uint16_t)(modbus_rx_buf[1] == 0x03 ? (5 + modbus_rx_buf[2]) :
                                ((modbus_rx_buf[1] & 0x80) ? 5 : 8));
        memcpy(rx_out, modbus_rx_buf, *rx_len_out);
    }

    // Return bus to default proprietary listener mode
    Lpuart1_SetProtocol(BUS_PROTOCOL_PROPRIETARY);
    return true;
}

// =========================================================================
// MULTI-PROTOCOL RECEIVE ROUTER (Called from LPUART1 RX Interrupt)
// =========================================================================

void Parse_Lpuart1_Byte(uint8_t byte)
{
    if (active_bus_protocol == BUS_PROTOCOL_PROPRIETARY)
    {
        // 1. Existing DLE/STX Binary Protocol
        if (Lpuart1.status == FILLED) return;

        switch (lpuart1_rx_state)
        {
            case 0: // DLE
                if (byte == DLE) {
                    Lpuart1.crcmsg = byte;
                    lpuart1_rx_state = 1;
                }
                break;

            case 1: // STX
                if (byte == STX) {
                    Lpuart1.crcmsg += byte;
                    lpuart1_rx_state = 2;
                } else {
                    lpuart1_rx_state = (byte == DLE) ? 1 : 0;
                }
                break;

            case 2: // SOURCE (Host = 0x00)
                if (byte == DEST) {
                    Lpuart1.crcmsg += byte;
                    lpuart1_rx_state = 3;
                } else {
                    lpuart1_rx_state = 0;
                }
                break;

            case 3: // DEST (STM32 = 0x95)
                if (byte == SOURCE) {
                    Lpuart1.crcmsg += byte;
                    lpuart1_rx_state = 4;
                } else {
                    lpuart1_rx_state = 0;
                }
                break;

            case 4: // LENGTH
                Lpuart1.messagelength = byte;
                Lpuart1.crcmsg += byte;
                lpuart1_payload_idx = 0;
                lpuart1_rx_state = 5;
                break;

            case 5: // MSG ID
                Lpuart1.messageidglobal = byte;
                Lpuart1.crcmsg += byte;
                lpuart1_rx_state = (Lpuart1.messagelength <= 7) ? 7 : 6;
                break;

            case 6: // PAYLOAD
                if (lpuart1_payload_idx < MAX_LPUART1_ARRAY) {
                    Lpuart1.payload[lpuart1_payload_idx++] = byte;
                    Lpuart1.crcmsg += byte;
                    if (lpuart1_payload_idx >= (Lpuart1.messagelength - 7)) {
                        lpuart1_rx_state = 7;
                    }
                } else {
                    lpuart1_rx_state = 0;
                }
                break;

            case 7: // CRC
                if (byte == Lpuart1.crcmsg) {
                    Lpuart1shadow.messageid = Lpuart1.messageidglobal;
                    if (lpuart1_payload_idx > 0) {
                        memcpy(Lpuart1shadow.payload, Lpuart1.payload, lpuart1_payload_idx);
                    }
                    Lpuart1.status = FILLED;
                }
                lpuart1_rx_state = 0;
                break;

            default:
                lpuart1_rx_state = 0;
                break;
        }
    }
    else if (active_bus_protocol == BUS_PROTOCOL_MODBUS_DUST)
    {
        // 2. Particles Plus OEM Modbus RTU Protocol
        Parse_Modbus_RTU_Byte(byte);
    }
    else if (active_bus_protocol == BUS_PROTOCOL_MODBUS_PID)
    {
        // 3. PID Sensor Modbus RTU Protocol (Uses same RTU frame parser)
        Parse_Modbus_RTU_Byte(byte);
    }
    else
    {
        // 4. Placeholder for future bus protocols
    }
}
