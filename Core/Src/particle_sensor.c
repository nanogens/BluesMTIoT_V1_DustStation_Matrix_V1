#include "particle_sensor.h"
#include "lpuart1.h"
#include "main.h"
#include <string.h>

#define PARTICLE_TIMEOUT_MS 250

ParticleReading_t LatestDustReading = {0};

// 1. Send literal register addresses on the wire (no -40001 offset)
static inline uint16_t RegToOffset(uint16_t reg_addr)
{
    return reg_addr;
}

// 2. Use Function 0x10 (Write Multiple Registers) for 16-bit writes
bool Particle_WriteU16(uint16_t reg_addr, uint16_t value)
{
    uint16_t offset = RegToOffset(reg_addr);
    uint8_t tx[11];
    uint8_t rx[16];
    uint16_t rx_len = 0;

    tx[0] = PARTICLE_SLAVE_ID;          // 0xF7
    tx[1] = 0x10;                       // Function 0x10: Write Multiple Registers
    tx[2] = (uint8_t)(offset >> 8);     // 0x9E (40501 high byte)
    tx[3] = (uint8_t)(offset & 0xFF);   // 0x35 (40501 low byte)
    tx[4] = 0x00;                       // Quantity High
    tx[5] = 0x01;                       // Quantity Low (1 register)
    tx[6] = 0x02;                       // Byte Count (2 bytes)
    tx[7] = (uint8_t)(value >> 8);      // Value High
    tx[8] = (uint8_t)(value & 0xFF);    // Value Low

    uint16_t crc = Modbus_CalcCRC16(tx, 9);
    tx[9]  = (uint8_t)(crc & 0xFF);     // 0x32 (for Start)
    tx[10] = (uint8_t)(crc >> 8);       // 0x58 (for Start)

    if (!Modbus_SendAndWait(BUS_PROTOCOL_MODBUS_DUST, tx, 11, rx, &rx_len, PARTICLE_TIMEOUT_MS))
        return false;

    return (rx_len == 8 && rx[0] == PARTICLE_SLAVE_ID && rx[1] == 0x10);
}

// Function 0x10: Write Multiple Registers (2 registers for 32-bit I32)
// Particles Plus Byte Order: Big-Endian within each register, Little-Endian across registers
bool Particle_WriteI32(uint16_t reg_addr, int32_t value)
{
    uint16_t offset = RegToOffset(reg_addr);
    uint32_t raw = (uint32_t)value;
    uint16_t low_word  = (uint16_t)(raw & 0xFFFF);
    uint16_t high_word = (uint16_t)((raw >> 16) & 0xFFFF);

    uint8_t tx[13];
    uint8_t rx[16];
    uint16_t rx_len = 0;

    tx[0]  = PARTICLE_SLAVE_ID;
    tx[1]  = 0x10; // Write Multiple Registers
    tx[2]  = (uint8_t)(offset >> 8);
    tx[3]  = (uint8_t)(offset & 0xFF);
    tx[4]  = 0x00; // Quantity High
    tx[5]  = 0x02; // Quantity Low (2 registers = 4 bytes)
    tx[6]  = 0x04; // Byte count
    tx[7]  = (uint8_t)(high_word >> 8);
    tx[8] = (uint8_t)(high_word & 0xFF);
    tx[9]  = (uint8_t)(low_word >> 8);
    tx[10]  = (uint8_t)(low_word & 0xFF);

    uint16_t crc = Modbus_CalcCRC16(tx, 11);
    tx[11] = (uint8_t)(crc & 0xFF);
    tx[12] = (uint8_t)(crc >> 8);

    if (!Modbus_SendAndWait(BUS_PROTOCOL_MODBUS_DUST, tx, 13, rx, &rx_len, PARTICLE_TIMEOUT_MS))
        return false;

    return (rx_len == 8 && rx[0] == PARTICLE_SLAVE_ID && rx[1] == 0x10);
}

// Function 0x03: Read Single 16-bit Register
bool Particle_ReadU16(uint16_t reg_addr, uint16_t *value_out)
{
    uint16_t offset = RegToOffset(reg_addr);
    uint8_t tx[8];
    uint8_t rx[16];
    uint16_t rx_len = 0;

    tx[0] = PARTICLE_SLAVE_ID;
    tx[1] = 0x03; // Read Holding Registers
    tx[2] = (uint8_t)(offset >> 8);
    tx[3] = (uint8_t)(offset & 0xFF);
    tx[4] = 0x00;
    tx[5] = 0x01; // 1 Register

    uint16_t crc = Modbus_CalcCRC16(tx, 6);
    tx[6] = (uint8_t)(crc & 0xFF);
    tx[7] = (uint8_t)(crc >> 8);

    if (!Modbus_SendAndWait(BUS_PROTOCOL_MODBUS_DUST, tx, 8, rx, &rx_len, PARTICLE_TIMEOUT_MS))
        return false;

    if (rx_len == 7 && rx[0] == PARTICLE_SLAVE_ID && rx[1] == 0x03 && rx[2] == 2)
    {
        *value_out = ((uint16_t)rx[3] << 8) | (uint16_t)rx[4];
        return true;
    }
    return false;
}

// Function 0x03: Read 32-bit Unsigned Integer (2 Registers)
bool Particle_ReadU32(uint16_t reg_addr, uint32_t *value_out)
{
    uint16_t offset = RegToOffset(reg_addr);
    uint8_t tx[8];
    uint8_t rx[16];
    uint16_t rx_len = 0;

    tx[0] = PARTICLE_SLAVE_ID;
    tx[1] = 0x03;
    tx[2] = (uint8_t)(offset >> 8);
    tx[3] = (uint8_t)(offset & 0xFF);
    tx[4] = 0x00;
    tx[5] = 0x02; // 2 Registers

    uint16_t crc = Modbus_CalcCRC16(tx, 6);
    tx[6] = (uint8_t)(crc & 0xFF);
    tx[7] = (uint8_t)(crc >> 8);

    if (!Modbus_SendAndWait(BUS_PROTOCOL_MODBUS_DUST, tx, 8, rx, &rx_len, PARTICLE_TIMEOUT_MS))
        return false;

    if (rx_len == 9 && rx[0] == PARTICLE_SLAVE_ID && rx[1] == 0x03 && rx[2] == 4)
    {
        uint16_t high_word = ((uint16_t)rx[3] << 8) | (uint16_t)rx[4];
        uint16_t low_word  = ((uint16_t)rx[5] << 8) | (uint16_t)rx[6];
        *value_out = ((uint32_t)high_word << 16) | (uint32_t)low_word;
        return true;
    }
    return false;
}

bool Particle_WriteU32(uint16_t reg_addr, uint32_t value)
{
    return Particle_WriteI32(reg_addr, (int32_t)value);
}

// Function 0x03: Read 32-bit IEEE-754 Float (2 Registers)
bool Particle_ReadFloat(uint16_t reg_addr, float *value_out)
{
    uint32_t raw_bits = 0;
    if (!Particle_ReadU32(reg_addr, &raw_bits))
        return false;

    memcpy(value_out, &raw_bits, sizeof(float));
    return true;
}

// =========================================================================
// HIGH-LEVEL INSTRUMENT OPERATIONS
// =========================================================================

bool Particle_StartSampling(void)
{
    return Particle_WriteU16(PP_REG_START_STOP, 1);
}

bool Particle_StopSampling(void)
{
    return Particle_WriteU16(PP_REG_START_STOP, 0);
}

bool Particle_Run7SecTest(void)
{
    // 1. Start Sampling (MbWrite 40501 U16 1)
    if (!Particle_StartSampling())
        return false;

    // 2. Sample for 7 seconds
    HAL_Delay(7000);

    // 3. Stop Sampling (MbWrite 40501 U16 0)
    if (!Particle_StopSampling())
        return false;

    return true;
}

bool Particle_LatchAndReadLatest(ParticleReading_t *out_data)
{
    if (out_data == NULL) return false;
    out_data->valid = false;

    // 1. Latch the latest completed record into the 41000+ shadow block
    Particle_WriteI32(PP_REG_LATCH_RECORD_NUM, -1);
    HAL_Delay(50);

    // 2. Read the latched Record Number from 41000 (fallback to 40804)
    if (!Particle_ReadU32(PP_REG_LATCH_RECORD_NUM, &out_data->record_number))
    {
        Particle_ReadU32(PP_REG_LAST_RECORD_NUM, &out_data->record_number);
    }

    // 3. Read PM2.5 (41088), PM10 (41090), and Total Particle Counts (41092)
    bool got_pm25 = Particle_ReadFloat(PP_REG_PM2_5, &out_data->pm2_5);
    bool got_pm10 = Particle_ReadFloat(PP_REG_PM10, &out_data->pm10);
    Particle_ReadFloat(PP_REG_TOTAL_COUNTS, &out_data->total_counts);

    // 4. Read Sample Seconds (41031), Flow Rate (41032), and Error Status (41033)
    Particle_ReadU16(PP_REG_SAMPLE_SECONDS, &out_data->sample_seconds);
    Particle_ReadU16(PP_REG_AVG_FLOW_RATE, &out_data->flow_rate_ml_min);
    Particle_ReadU16(PP_REG_ERROR_STATUS, &out_data->error_status);

    out_data->valid = (got_pm25 || got_pm10);
    return out_data->valid;
}

bool Particle_ConfigureTiming(uint16_t sample_seconds)
{
    // Add 50ms spacing between Modbus writes so the sensor processes every command
    if (!Particle_WriteU16(PP_REG_SAMPLE_TIME, sample_seconds)) return false;
    HAL_Delay(50);

    if (!Particle_WriteU16(PP_REG_HOLD_TIME, 0)) return false;
    HAL_Delay(50);

    if (!Particle_WriteU16(PP_REG_CYCLES, 1)) return false;
    HAL_Delay(50);

    // Verify 40504 took effect; if sensor uses 0.1s units, write sample_seconds * 10
    uint16_t readback = 0;
    if (Particle_ReadU16(PP_REG_SAMPLE_TIME, &readback) && readback != sample_seconds)
    {
        Particle_WriteU16(PP_REG_SAMPLE_TIME, sample_seconds * 10);
        HAL_Delay(50);
    }
    return true;
}

bool Particle_PowerAndPrepare(uint16_t sample_seconds)
{
    // FUTURE DRIVER BOARD HOOK:
    // 1. Send RS-485 command to Driver Board to turn ON 12V load switch for Particles Plus
    // 2. HAL_Delay(1500); // Allow Particles Plus firmware to boot

    // 3. Ensure sensor is in a clean STOPPED state before configuring registers
    Particle_StopSampling();
    HAL_Delay(100);

    // 4. Re-apply sample timing settings (required every time sensor power is cycled)
    return Particle_ConfigureTiming(sample_seconds);
}

