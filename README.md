# BluesMTIoT_V1_DustStation_Matrix_V1

Firmware for the **Custom Notecarrier-MT (`STM32L073CBT6`)** powering the **Construction Site Dust & Air Quality Monitoring Station (Dust Station v1)**.

---

## 1. Project Overview

The **Dust Station v1** is a low-power, cellular/cloud-connected environmental monitoring station designed for construction sites and industrial perimeter monitoring. 

An ultra-low-power **STM32L073CBT6** (ARM Cortex-M0+) microcontroller on a custom-designed **Notecarrier-MT** board acts as the system master:
1. Wakes periodically from `STOP` mode using its 32.768 kHz LSE-backed Real-Time Clock (RTC).
2. Powers on the external peripheral rail (`3p0_GEN`) and communicates over half-duplex **RS-485 (`LPUART1`, Modbus RTU)** with an off-the-shelf **Particles Plus 9000 Series OEM** dust/particle sensor (and a planned custom **Photo Ionization Detector [PID]** sensor and 12V load-switch **Driver Board** on the same RS-485 bus).
3. Configures and triggers a timed air-pump sample, latches the completed record, and reads **PM2.5 ($\mu\text{g/m}^3$)**, **PM10 ($\mu\text{g/m}^3$)**, **Total Particle Counts ($\text{\#/cm}^3$)**, **Average Pump Flow Rate ($\text{mL/min}$)**, **Sample Duration ($\text{s}$)**, **Record Number**, and **Hardware Error Status**.
4. Packages and queues the reading via **I2C (`I2C1`)** into a **Blues Wireless Notecard** (LTE/Wi-Fi/GPS) using a compact binary `note.template` (`sensors.qo`) and triggers a background sync (`hub.sync`) to **Notehub.io** (`com.yahoo.nanogens:dust_station_matrix_v1`), which routes telemetry to **AWS (Lambda $\rightarrow$ DynamoDB)**.
5. Listens for inbound cloud commands (`data.qi`) via the Notecard hardware interrupt pin (`ATTN_P` $\rightarrow$ `PB5`) and local field technician queries/configuration over **IrDA infrared (`USART2`)** or wired **Pogo RS-485 (`USART2`)**, returning to ultra-low-power **`STOP` mode** between samples.

---

## 2. Hardware Architecture (Custom Notecarrier-MT)

* **Microcontroller**: `U2` — STMicroelectronics `STM32L073CBT6` (LQFP48, 128 KB Flash, 20 KB RAM).
* **Clocks**:
  * **HSE (`PH0-OSC_IN`)**: `U30` — 8 MHz external SMD oscillator (`HSE_BYPASS` $\rightarrow$ PLL $\rightarrow$ 16 MHz `SYSCLK`).
  * **LSE (`PC14-OSC32_IN`)**: `U7` — 32.768 kHz external oscillator (`LSE_BYPASS`) for the hardware RTC.
  * Both oscillators are powered by the always-on `3p3_SW` rail so timekeeping continues during `STOP` mode.
* **Power Rails & Control Pins**:
  * **`3p3_SW` (`U16`, `ADP2108AUJZ-3.3`)**: Always-on 3.3V buck regulator supplying the STM32, oscillators, logic gates, and Notecard `VIO_P`.
  * **`3p0_GEN` (`U34`, `SIP32431DR3` Load Switch)**: Controlled by **`PA7` (`GEN_PWR_EN`)**. Supplies switched power to the on-board RS-485 transceiver (`U19`), external SPI2 EEPROM (`U12`), and the external `USART2` IrDA/RS-485 transceiver board (`U9`). Turned **OFF** before entering `STOP` mode and powered **ON** immediately upon wakeup.
  * **`3p6V_VMODEM` (`U6`, `TPS63020` Buck-Boost)**: Supplies the Blues Notecard `VMODEM_P` rail via fuse `U15`. (`R12` 0 $\Omega$ to `PA5` / `POWERUP_NC_EN` is unpopulated so `U6.EN` is pulled high to `3p3_SW` via 200 k$\Omega$ `R30`, keeping the Notecard powered during STM32 `STOP` mode).
  * **Input Protection (`U28`, `DMP2130L-7` P-FET)**: `R37` (10 k$\Omega$) is unpopulated on Rev 1 hardware.
  * **Battery Monitor**: Switched voltage divider (`R38` 100 k$\Omega$ / `R39` 200 k$\Omega$) enabled by **`PA0` (`BATT_MON_EN`)** and read on **`PA1` (`BATT_MON`)**.
* **Communication Interfaces**:
  * **`LPUART1` (`PB11` TX, `PB10` RX, `PB12` `LPUART1_DE`)**: Half-duplex RS-485 via `U19` (`ST4E1240DT`) with 120 $\Omega$ termination (`R22`) to daisy-chained 3-pin Molex Picoblade connectors `U3` and `U21` (115200 baud, 8-N-1).
  * **`USART2` (`PA2` TX, `PA3` RX, `PA4` `USART2_SD_DE`)**: Routed to 6-pin connector `U9` for local laptop diagnostics via either an external **IrDA board** (`#define IR_USART2` in `defines.h`) or wired **Pogo RS-485 board** (`#define POGO_USART2`), running at 115200 baud, 8-N-1.
  * **`I2C1` (`PA9` SCL, `PA10` SDA)**: Communicates with the Blues Wireless Notecard (`U1`, 7-bit I2C address `0x17`) using the official `note-c` library. Hardware reset on **`PB4` (`RESET_NC_RST_BAR`)**.
  * **`SPI2` (`PB13` SCK, `PB14` MISO, `PB15` MOSI, `PB0` `SPI2_CS_BAR`)**: Connected to on-board 32-Mbit SPI Page EEPROM `U12` (`M95P32-EXMNT/E`).
* **Wakeup Interrupts**:
  * **`PB2` (`IR_WAKE_UP`, `EXTI2_3_IRQn`)**: Falling-edge wakeup from the external IR wakeup photodiode (`U4` $\rightarrow$ `R8`, pulled up to `3p3_SW` via 1 M$\Omega$ `R13`; configured with `GPIO_NOPULL`). Wakes the STM32 from `STOP` mode when the host laptop transmits $\ge 12$ leading `0x00` bytes over IrDA.
  * **`PB5` (`ATTN_EXT15`, `EXTI4_15_IRQn`)**: Rising-edge wakeup (`GPIO_MODE_IT_RISING`, external 100 k$\Omega$ `R16`) driven by the Blues Notecard `ATTN_P` pin when inbound `data.qi` commands arrive from Notehub.io.
  * **RTC WakeUp Timer (`RTC_IRQn`)**: Wakes the STM32 at the exact scheduled epoch (`SysState.next_sample_epoch`) to initiate a dust sampling cycle.
* **Status LEDs**:
  * **`PA8` (`LED_B`)**: Blinks 5 times fast on cold boot, and 3 times slowly once Notecard I2C communication (`0x17`) is verified.
  * **`PB8` (`LED_C`)**: Solid ON while a dust sample is actively running.
  * **`PA11` (`LED_A`)**: Awake indicator (inactive on Rev 1 PCB as `VDD_USB` Pin 36 is unconnected).
  * **Multi-Color RGB LED (`U18` / `U17` driven via inverting Schmitt trigger `U5`, `SN74LVC3G14DCUR`)**:
    * **`PA15` (`MULTI_RED`)**, **`PB3` (`MULTI_GREEN`)**, **`PB7` (`MULTI_BLUE`)**.
    * Active-HIGH at the MCU pin (`GPIO_PIN_SET` = LED ON, `GPIO_PIN_RESET` = LED OFF).
    * `MULTI_GREEN` blinks 5 times fast when Notecard `card.version` succeeds; `MULTI_BLUE` flashes once (100 ms) when a dust reading is queued to the Notecard or when `PB5` (`ATTN`) triggers.

---

## 3. Firmware Architecture & Source Files

| File | Description |
| :--- | :--- |
| `Core/Inc/defines.h` | Selects `IR_USART2` vs. `POGO_USART2`, defines binary frame constants (`DLE=0x10`, `STX=0x02`, `SOURCE=0x95`, `DEST=0x00`), message IDs, and timing parameters (`INACTIVITY_SLEEP_MS=1000`, `DEFAULT_SAMPLE_INTERVAL_SEC=300`, `DEFAULT_SAMPLE_DURATION_SEC=7`). |
| `Core/Inc/globals.h` & `Core/Src/globals.c` | Global state structures for `Lpuart1`, `Usart2`, `Time`, `Counter`, and `SysState` (`last_activity_tick`, `sample_interval_sec`, `next_sample_epoch`, `sampling_active`, `attn_pending`, `wakeup_source`). |
| `Core/Inc/particle_sensor.h` & `Core/Src/particle_sensor.c` | Modbus RTU master driver for the **Particles Plus 9000 Series OEM** sensor (Slave ID `247` / `0xF7`). Uses literal register addresses on the wire and Function `0x10` (Write Multiple Registers) for 16-bit and 32-bit writes. Implements `Particle_PowerAndPrepare()`, `Particle_StartSampling()`, `Particle_StopSampling()`, `Particle_Run7SecTest()`, and `Particle_LatchAndReadLatest()`. |
| `Core/Inc/lpuart1.h` & `Core/Src/lpuart1.c` | Multi-protocol RS-485 driver for `LPUART1` supporting dynamic switching between `BUS_PROTOCOL_PROPRIETARY` (`DLE`/`STX`), `BUS_PROTOCOL_MODBUS_DUST`, and `BUS_PROTOCOL_MODBUS_PID`, including `Modbus_CalcCRC16()` and `Modbus_SendAndWait()`. |
| `Core/Inc/usart2.h` & `Core/Src/usart2.c` | Local diagnostic driver for `USART2` (IrDA / Pogo RS-485). Includes a 150-cycle inter-byte TX delay in IrDA mode to prevent optical pulse saturation and ignores ambient `0x00` wakeup bytes until `DLE` (`0x10`). |
| `Core/Inc/process.h` & `Core/Src/process.c` | Packet router handling `VERSION_QUERY` (`0x01` $\rightarrow$ `0x02`), `TIME_QUERY` (`0x0F` $\rightarrow$ `0x10`), `TIME_SET` (`0x11` $\rightarrow$ `0x10`), binary `DUST_QUERY` (`0x12` $\rightarrow$ `0x13`, 25-byte frame), and human-readable `DUST_ASCII_QUERY` (`0x14` $\rightarrow$ `0x15`). |
| `Core/Inc/rtc.h` & `Core/Src/rtc.c` | LSE-backed RTC driver with Backup Register guard (`RTC_BKP_DR0 == 0x32F2`) to preserve time across resets and pure-integer `RTC_GetEpochSeconds()` (seconds since Jan 1, 2000) for drift-free wall-clock scheduling. |
| `Core/Src/main.c` | System initialization, Notecard I2C translation hooks (`noteI2CReset`, `noteI2CTransmit`, `noteI2CReceive`), `Notecard_SendDustReading()`, `processInboundNotes()`, `Enter_Stop_Mode()`, non-blocking sampling state machine, and HAL interrupt callbacks. |
| `Core/Src/n_*.c` & `Core/Inc/note.h` | Official Blues Wireless `note-c` library integrated into the STM32 HAL build. |

---

## 4. Local Diagnostic Commands (`USART2` IrDA / Pogo, 115200 8-N-1)

When waking the STM32 over IrDA from `STOP` mode, prepend at least **12–15 `0x00` bytes** (~1.3 ms of IR light pulses) before the `10 02` (`DLE` `STX`) header:

* **Version Query (`0x01`)**:
  `00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 10 02 00 95 07 01 AF`
* **Time Query (`0x0F`)**:
  `00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 10 02 00 95 07 0F BD`
* **Time Set (`0x11`, e.g., 2026-11-14 02:35:00 AM Saturday)**:
  `00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 10 02 00 95 11 11 00 00 1A 0B 0E 02 23 00 00 06 27`
* **Dust Binary Telemetry Query (`0x12` $\rightarrow$ returns 25-byte `0x13` frame)**:
  `00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 10 02 00 95 07 12 C0`
* **Dust Plain-Text ASCII Query (`0x14` $\rightarrow$ prints formatted summary line)**:
  `00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 10 02 00 95 07 14 C2`

Example ASCII output:
```text
Time=2026-11-14 02:35:43 AM, Rec=41, PM2.5=1.07 ug/m3, PM10=1.26 ug/m3, Counts=6.87/cm3, Flow=1201 mL/min, Dur=7s, Err=0x00
```

---

## 5. Cloud Telemetry Payload (`sensors.qo` on Notehub.io)

Each completed sample is queued to `sensors.qo` using a compact binary Notecard template (`note.template`) and decoded by Notehub.io into JSON:

```json
{
  "counts": 6.741333,
  "dur_s": 7,
  "flow": 1199,
  "pm10": 18.21,
  "pm2_5": 3.29,
  "rec": 45,
  "ts": 811956998
}
```
*(Note: Zero-valued templated fields such as `"err": 0` are omitted over the air by Notecard to conserve cellular bandwidth.)*
