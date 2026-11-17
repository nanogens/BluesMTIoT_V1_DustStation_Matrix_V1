#ifndef INC_PARTICLE_SENSOR_H_
#define INC_PARTICLE_SENSOR_H_

#include <stdint.h>
#include <stdbool.h>

// Default Slave ID for Particles Plus 9000 Series
#define PARTICLE_SLAVE_ID          247  // 0xF7

// Key Holding Registers (Literal Modbus Addresses from Register Map)
#define PP_REG_SERIAL_NUM          40044   // U32 (2 regs, R)
#define PP_REG_SAMPLING_STATUS     40500   // U16: 2=Stopped, 3=Delay, 6=Sampling
#define PP_REG_START_STOP          40501   // U16: 1=Start, 0=Stop
#define PP_REG_LAST_RECORD_NUM     40804   // I32: Latest completed record number
#define PP_REG_LATCH_RECORD_NUM    41000   // I32: Write -1 to latch latest completed record
#define PP_REG_SAMPLE_SECONDS      41031   // U16: Actual sample duration (seconds)
#define PP_REG_AVG_FLOW_RATE       41032   // U16: Average flow rate (mL/min)
#define PP_REG_ERROR_STATUS        41033   // U16: Error bitmask (0 = OK)
#define PP_REG_PM2_5               41088   // Float: PM2.5 mass concentration (ug/m3)
#define PP_REG_PM10                41090   // Float: PM10 mass concentration (ug/m3)
#define PP_REG_TOTAL_COUNTS        41092   // Float: Total particle counts (#/cm3)

#define PP_REG_START_DELAY         40503   // U16: Start delay in tenths of a second (e.g., 0)
#define PP_REG_SAMPLE_TIME         40504   // U16: Sample time in tenths of a second (e.g., 70 = 7.0s)
#define PP_REG_HOLD_TIME           40505   // U16: Hold time in tenths of a second (e.g., 0)
#define PP_REG_CYCLES              40506   // U16: Number of cycles (1 = single sample per start)

// Update ParticleReading_t struct:
typedef struct {
    bool valid;
    uint32_t record_number;
    float pm2_5;
    float pm10;
    float total_counts;
    uint16_t sample_seconds;
    uint16_t flow_rate_ml_min;
    uint16_t error_status;
} ParticleReading_t;

extern ParticleReading_t LatestDustReading;

// Core Typed Read / Write Helpers
bool Particle_WriteU16(uint16_t reg_addr, uint16_t value);
bool Particle_WriteI32(uint16_t reg_addr, int32_t value);
bool Particle_ReadU16(uint16_t reg_addr, uint16_t *value_out);
bool Particle_ReadU32(uint16_t reg_addr, uint32_t *value_out);
bool Particle_WriteU32(uint16_t reg_addr, uint32_t value);
bool Particle_ReadFloat(uint16_t reg_addr, float *value_out);

// High-Level Instrument Operations
bool Particle_StartSampling(void);
bool Particle_StopSampling(void);
bool Particle_Run7SecTest(void);
bool Particle_LatchAndReadLatest(ParticleReading_t *out_data);

bool Particle_ConfigureTiming(uint16_t sample_seconds);

bool Particle_PowerAndPrepare(uint16_t sample_seconds);

#endif /* INC_PARTICLE_SENSOR_H_ */
