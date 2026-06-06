# Submersible AZmini Blues MT IoT

A custom Notecarrier board for the Blues IoT Notecard (LTE / WiFi / GPS), designed to be approximately the size of a Notecard itself (~38 x 66 mm).

## Hardware Features

- **STM32L073CBTx** microcontroller (48-pin LQFP)
- **M.2 connector** for Blues Notecard
- **8 MHz external oscillator** (HSE) for STM32 system clock
- **32.768 kHz external oscillator** (LSE) for RTC
- **I2C1** (PA9/PA10) — Communication with Notecard
- **I2C2** (PB13/PB14) — QWIIC connector for future I2C expansion
- **USART2** (PA2/PA3) — RS-485 transceiver with DE control (PA4)
- **LPUART1** (PB10/PB11) — IrDA port (future expansion)
- **3.0V regulator** — Powers STM32 and Notecard (VIO)
- **3.6V/2A regulator** (TPS63020DSJR) — VMODEM power for RF transmission, enabled via PB6
- **USB** connection direct to Notecard for Edge browser configuration
- **JTAG/SWD** (PA13/PA14) for programming and debugging
- **LED indicators** — LED_A (PB15), LED_B (PA8)
- **Notecard ATTN** interrupt (PA1, falling edge via SN74LVC1G125 buffer)
- **Notecard reset** (PB4, active low, pulled high)

## MCU Pin Mapping

| STM32 Pin | Function | Connected To |
|-----------|----------|-------------|
| PA1 | EXTI1 (falling IRQ) | ATTN_P from Notecard (via U4) |
| PA2 | USART2_TX | RS-485 transceiver |
| PA3 | USART2_RX | RS-485 transceiver |
| PA4 | GPIO Out | USART2_DE (RS-485 enable) |
| PA8 | GPIO Out | LED_B |
| PA9 | I2C1_SCL | Notecard I2C |
| PA10 | I2C1_SDA | Notecard I2C |
| PA13 | SWDIO | JTAG/SWD |
| PA14 | SWCLK | JTAG/SWD |
| PB4 | GPIO Out | RESET_NC_RST_BAR |
| PB6 | GPIO Out | POWERUP_NC_EN (3.6V reg) |
| PB9 | GPIO In | SDOWN_BUFF |
| PB10 | LPUART1_RX | IrDA port |
| PB11 | LPUART1_TX | IrDA port |
| PB12 | GPIO Out | LPUART1_SD (IrDA shutdown) |
| PB13 | I2C2_SCL | QWIIC connector |
| PB14 | I2C2_SDA | QWIIC connector |
| PB15 | GPIO Out | LED_A |

## Clock Configuration

- HSE: 8 MHz (external oscillator, bypass mode)
- SYSCLK: 16 MHz (HSE → PLL x4 ÷2)
- LSE: 32.768 kHz (external oscillator, bypass mode) — RTC clock
- APB1/APB2: 16 MHz

## Toolchain

- **IDE**: STM32CubeIDE
- **HAL/LL Driver**: STM32Cube FW_L0 V1.12.4
- **MCU**: STM32L073CBTx

## Getting Started

1. Clone the repository
2. Open with STM32CubeIDE
3. The `.cproject`, `.project`, and `.mxproject` files are included for direct import
4. Build and flash via JTAG/SWD
