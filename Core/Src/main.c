/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <usart2.h>
#include <lpuart1.h>
#include "particle_sensor.h"
#include "note.h"

#include "process.h"
#include "rtc.h"
#include "inits.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// Notecard I2C address (7-bit)
#define NOTE_I2C_ADDR        0x17
// Maximum I2C buffer size for Notecard
#define NOTE_I2C_MAX_SIZE    256
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef hlpuart1;
IRDA_HandleTypeDef hirda2;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi2;

/* USER CODE BEGIN PV */
#ifdef IR_USART2_SEL
  IRDA_HandleTypeDef* pActiveIrda = &hirda2;
#else
  UART_HandleTypeDef* pActiveUart = &husart2;
#endif

// USART2 variables
uint8_t rx_buffer[MAX_RX_BUFFER_ARRAY];
uint8_t tx_buffer[MAX_TX_BUFFER_ARRAY];
volatile uint16_t rx_index;
volatile uint16_t tx_index;
volatile uint16_t tx_length;
volatile uint8_t tx_busy;

// LPUART1 variables (temporary only)
uint8_t rx_byte;
uint8_t tx_message[] = "LPUART1 Ready\r\n";

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_IRDA_Init(void);
static void MX_I2C1_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_RTC_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */
// =========================================================================
// STOP MODE Functions
// =========================================================================
void Set_IR_WakeUp_As_Input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = IR_WAKE_UP_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(IR_WAKE_UP_GPIO_Port, &GPIO_InitStruct);
}

void Set_IR_WakeUp_As_Interrupt(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = IR_WAKE_UP_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(IR_WAKE_UP_GPIO_Port, &GPIO_InitStruct);
}

void Enter_Stop_Mode(void)
{
    uint32_t now_epoch = RTC_GetEpochSeconds();

    // If the scheduled sample time has already arrived or passed while awake, return immediately
    if (SysState.next_sample_epoch != 0 && now_epoch >= SysState.next_sample_epoch)
    {
        return;
    }

    uint32_t sleep_sec = (SysState.next_sample_epoch > now_epoch)
                         ? (SysState.next_sample_epoch - now_epoch)
                         : SysState.sample_interval_sec;
    if (sleep_sec == 0) sleep_sec = 1;

    // 1. Configure RTC WakeUp Timer for exact remaining seconds (0-indexed: sleep_sec - 1)
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
    SysState.rtc_wakeup_pending = 0;
    //SysState.attn_pending = 0;
    SysState.wakeup_source = WAKEUP_NONE;
    HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, (sleep_sec > 1) ? (sleep_sec - 1) : 1, RTC_WAKEUPCLOCK_CK_SPRE_16BITS);

    // 2. Power down external peripherals (RS-485, IrDA TX, EEPROM)
    HAL_GPIO_WritePin(GEN_PWR_EN_GPIO_Port, GEN_PWR_EN_Pin, GPIO_PIN_RESET);
    HAL_Delay(10); // Let 3p0_GEN rail discharge cleanly

    // Abort sleep immediately if an IR wakeup pulse arrived during the 10ms rail discharge!
    if (HAL_GPIO_ReadPin(IR_WAKE_UP_GPIO_Port, IR_WAKE_UP_Pin) == GPIO_PIN_RESET)
    {
        HAL_GPIO_WritePin(GEN_PWR_EN_GPIO_Port, GEN_PWR_EN_Pin, GPIO_PIN_SET);
        SysState.last_activity_tick = HAL_GetTick();
        return;
    }

    // 3. Turn off status LEDs and suspend SysTick
    HAL_GPIO_WritePin(LED_A_GPIO_Port, LED_A_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_RESET);
    HAL_SuspendTick();

    // 4. Disable UART/IrDA NVIC interrupts while unpowered so line drops don't trigger false IRQs
    HAL_NVIC_DisableIRQ(USART2_IRQn);
    HAL_NVIC_DisableIRQ(LPUART1_IRQn);

    // 5. Arm PB2 (IR_WAKE_UP) and PB5 (ATTN) EXTI interrupts cleanly
    Set_IR_WakeUp_As_Interrupt();
    __HAL_GPIO_EXTI_CLEAR_IT(IR_WAKE_UP_Pin);
    __HAL_GPIO_EXTI_CLEAR_IT(ATTN_EXT15_Pin);
    __HAL_RTC_WAKEUPTIMER_EXTI_CLEAR_FLAG();
    HAL_NVIC_ClearPendingIRQ(EXTI2_3_IRQn);
    HAL_NVIC_ClearPendingIRQ(EXTI4_15_IRQn);
    HAL_NVIC_ClearPendingIRQ(RTC_IRQn);
    HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);
    HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

    // 6. Enter STOP Mode (PWR_MAINREGULATOR_ON for fast wakeup)
    HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);

    // ================== MCU WAKES UP HERE ==================

    // 7. Power up GEN_PWR_EN FIRST so U34 & IrDA transceiver warm up in parallel
    HAL_GPIO_WritePin(GEN_PWR_EN_GPIO_Port, GEN_PWR_EN_Pin, GPIO_PIN_SET);

    // 8. Resume SysTick and restore System Clock immediately
    HAL_ResumeTick();
    SystemClock_Config();
    HAL_GPIO_WritePin(LED_A_GPIO_Port, LED_A_Pin, GPIO_PIN_SET);

    // 9. Disarm PB2 EXTI, re-enable UART NVIC IRQs, and start RX immediately
    HAL_NVIC_DisableIRQ(EXTI2_3_IRQn);
    Set_IR_WakeUp_As_Input();
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_NVIC_EnableIRQ(LPUART1_IRQn);
    Start_Uart2_Rx();
    Start_Lpuart1_Rx();

    // 10. Reset inactivity timer so MCU stays awake for at least INACTIVITY_SLEEP_MS
    SysState.last_activity_tick = HAL_GetTick();
}

// =========================================================================
// OFFICIAL TRANSLATION HOOKS: Maps STM32 HAL directly to note-c API
// =========================================================================

// Signature matches: typedef bool (*i2cResetFn) (uint16_t deviceAddress);
bool noteI2CReset(uint16_t deviceAddress) {
    return (HAL_I2C_IsDeviceReady(&hi2c1, (deviceAddress << 1), 3, 100) == HAL_OK);
}

const char *noteI2CTransmit(uint16_t deviceAddress, uint8_t *buffer, uint16_t size) {
    uint8_t *writebuf = malloc(size + 1);
    if (writebuf == NULL) {
        return "I2C: malloc failed";
    }
    writebuf[0] = (uint8_t)size;
    memcpy(&writebuf[1], buffer, size);
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c1, (deviceAddress << 1), writebuf, size + 1, 200);
    free(writebuf);
    return (status == HAL_OK) ? NULL : "I2C Transmit Error";
}

const char *noteI2CReceive(uint16_t deviceAddress, uint8_t *buffer, uint16_t size, uint32_t *available) {
    // First, request a read of (size + 2) bytes: [available_byte, good_byte, payload...]
    uint8_t hdr[2] = {0, (uint8_t)size};  // 0 = reserved, size = requested bytes
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c1, (deviceAddress << 1), hdr, 2, 200);
    if (status != HAL_OK) {
        *available = 0;
        return "I2C Receive: transmit request failed";
    }

    uint8_t *readbuf = malloc(size + 2);
    if (readbuf == NULL) {
        *available = 0;
        return "I2C Receive: malloc failed";
    }
    status = HAL_I2C_Master_Receive(&hi2c1, (deviceAddress << 1), readbuf, size + 2, 200);
    if (status != HAL_OK) {
        free(readbuf);
        *available = 0;
        return "I2C Receive: receive failed";
    }

    *available = readbuf[0];   // number of bytes actually available in Notecard
    if (readbuf[1] != size) {  // sanity check: should equal requested size
        free(readbuf);
        return "I2C Receive: length mismatch";
    }
    memcpy(buffer, &readbuf[2], size);
    free(readbuf);
    return NULL;
}

// Platform time management hooks
void noteDelayMs(uint32_t ms) {
    HAL_Delay(ms);
}

uint32_t noteMillis(void) {
    return HAL_GetTick();
}

// =========================-===========
// GETTING INCOMING DATA FROM THE NOTECARD
// Function to check for and process inbound notes from Notehub
void processInboundNotes(void)
{
    while (1)
    {
        // Retrieve and delete the next inbound note from data.qi
        J *get_req = NoteNewRequest("note.get");
        if (get_req == NULL) return;

        JAddStringToObject(get_req, "file", "data.qi");
        JAddBoolToObject(get_req, "delete", true);

        J *note_rsp = NoteRequestResponse(get_req);
        if (note_rsp == NULL) return;

        // If Notecard returns an error (e.g. "{no-note}" when queue is empty), stop looping
        if (NoteResponseError(note_rsp)) {
            NoteDeleteResponse(note_rsp);
            return;
        }

        J *inbound_body = JGetObject(note_rsp, "body");
        if (inbound_body != NULL) {
            /*
            const char *command = JGetString(inbound_body, "led");
            if (command != NULL) {
                if (strcmp(command, "on") == 0) {
                    HAL_GPIO_WritePin(MULTI_RED_GPIO_Port, MULTI_RED_Pin, GPIO_PIN_SET);
                    HAL_GPIO_WritePin(MULTI_GREEN_GPIO_Port, MULTI_GREEN_Pin, GPIO_PIN_SET);
                } else if (strcmp(command, "off") == 0) {
                    HAL_GPIO_WritePin(MULTI_RED_GPIO_Port, MULTI_RED_Pin, GPIO_PIN_RESET);
                    HAL_GPIO_WritePin(MULTI_GREEN_GPIO_Port, MULTI_GREEN_Pin, GPIO_PIN_RESET);
                }
            }
            */
            // Add future inbound command parsing here
        }
        NoteDeleteResponse(note_rsp);
    }
}

// =========================================================================
// QUEUE DUST TELEMETRY TO NOTECARD & TRIGGER CLOUD SYNC
// =========================================================================
void Notecard_SendDustReading(const ParticleReading_t *reading)
{
    if (reading == NULL || !reading->valid) return;

    // 1. Add note to sensors.qo using the compact template defined in Block B
    J *req = NoteNewRequest("note.add");
    if (req != NULL) {
        JAddStringToObject(req, "file", "sensors.qo");

        J *body = JCreateObject();
        if (body != NULL) {
            JAddNumberToObject(body, "rec",    reading->record_number);
            JAddNumberToObject(body, "ts",     RTC_GetEpochSeconds());
            JAddNumberToObject(body, "pm2_5",  reading->pm2_5);
            JAddNumberToObject(body, "pm10",   reading->pm10);
            JAddNumberToObject(body, "counts", reading->total_counts);
            JAddNumberToObject(body, "flow",   reading->flow_rate_ml_min);
            JAddNumberToObject(body, "dur_s",  reading->sample_seconds);
            JAddNumberToObject(body, "err",    reading->error_status);
            JAddItemToObject(req, "body", body);
        }

        // Sends request and automatically frees req, body, and response
        NoteRequest(req);
    }

    // 2. Trigger background cellular/Wi-Fi sync to Notehub.io
    J *sync_req = NoteNewCommand("hub.sync");
    if (sync_req != NULL) {
        NoteRequest(sync_req);
    }

    // Process any inbound notes from the cloud (matches original Block C line 739)
    processInboundNotes();

    // 3. Flash MULTI_BLUE once to show telemetry was queued to Notecard
    HAL_GPIO_WritePin(MULTI_BLUE_GPIO_Port, MULTI_BLUE_Pin, GPIO_PIN_SET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(MULTI_BLUE_GPIO_Port, MULTI_BLUE_Pin, GPIO_PIN_RESET);
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_IRDA_Init();
  MX_I2C1_Init();
  MX_LPUART1_UART_Init();
  MX_RTC_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */

  HAL_GPIO_WritePin(LED_A_GPIO_Port, LED_A_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_C_GPIO_Port, LED_C_Pin, GPIO_PIN_RESET);

  HAL_GPIO_WritePin(MULTI_RED_GPIO_Port, MULTI_RED_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MULTI_GREEN_GPIO_Port, MULTI_GREEN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MULTI_BLUE_GPIO_Port, MULTI_BLUE_Pin, GPIO_PIN_RESET);


  // LPUART1
  HAL_GPIO_WritePin(LPUART1_DE_GPIO_Port, LPUART1_DE_Pin, GPIO_PIN_RESET); // Recv mode

  // USART2
  HAL_GPIO_WritePin(USART2_SD_DE_GPIO_Port, USART2_SD_DE_Pin, GPIO_PIN_RESET); // Recv mode

  // EEPROM
  HAL_GPIO_WritePin(SPI2_CS_BAR_GPIO_Port, SPI2_CS_BAR_Pin, GPIO_PIN_SET);

  // Power Distribution (3p3_SW --> 3p0_GEN) -- POWER UP both USART2 and LPUART1 as well as external EEPROM (M95P32-EXMNT/E)
  // We should not have this powered on by default during final deployment.
  // It should only be on when it needs to be on (e.g. communicating and/or saving data to the EEPROM)

  // Enable 3p0_GEN power rail to the RS-485 transceiver (U19)
  HAL_GPIO_WritePin(GEN_PWR_EN_GPIO_Port, GEN_PWR_EN_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(POWERUP_NC_EN_GPIO_Port, POWERUP_NC_EN_Pin, GPIO_PIN_SET);
  HAL_Delay(100);




  // Reset the Note Card
  HAL_GPIO_WritePin(RESET_NC_RST_BAR_GPIO_Port, RESET_NC_RST_BAR_Pin, GPIO_PIN_RESET);
  HAL_Delay(1000);
  HAL_GPIO_WritePin(RESET_NC_RST_BAR_GPIO_Port, RESET_NC_RST_BAR_Pin, GPIO_PIN_SET);

  // CRITICAL: Wait for Notecard to fully boot (needs ~5 seconds)
  // MT: Uplink Block disabled A_MINUS_1
  HAL_Delay(8000);

  // Three quick blinks = hardware started correctly
  for (int i = 0; i < 5; i++)
  {
      HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_SET);
      HAL_Delay(100);
      HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_RESET);
      HAL_Delay(100);
  }

  // Initialize the RTC
  RTC_Init();
  HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
  SysState.rtc_wakeup_pending = 0;

  // Start Receive Interrupts of both USART2 and LPUART1
  Start_Uart2_Rx();  // Start USART2 Receive Interrupts
  Start_Lpuart1_Rx(); // Start LPUART1 Receive Interrupts

  // This was to fix the Notecarrier power-off during Start Sampling issue,
  // where even after powering back on, Stop Sampling did not take effect.
  // Instead it was back to back Start Sampling followed by a very momentary Stop Sampling
  // and back to Start Sampling loop.
  // Note: This was with the Particle Plus powered independently with its 12V jack.
  //           Likely would not occur if the power supply is controlled by the Notecarrier as it would power on/off
  //           all peripherals like the Particle Plus for each Sampling session.
  // 1. Clear any leftover RTC wakeup flag from cold boot
  HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
  __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
  SysState.rtc_wakeup_pending = 0;

  // 2. Power/prepare Particles Plus and configure sample timing
  Particle_PowerAndPrepare(DEFAULT_SAMPLE_DURATION_SEC);
  HAL_Delay(250);

  // Particle Sensor 7 Seconds On-Off Test
  if (Particle_Run7SecTest())
  {
      HAL_Delay(300); // Allow sensor to commit startup sample record to flash
      Particle_LatchAndReadLatest(&LatestDustReading);

      // Green/LED_A solid = Start & Stop commands both acknowledged!
      HAL_GPIO_WritePin(LED_A_GPIO_Port, LED_A_Pin, GPIO_PIN_SET);
  }
  else
  {
      // Red LED = No Modbus response or timeout
      HAL_GPIO_WritePin(MULTI_RED_GPIO_Port, MULTI_RED_Pin, GPIO_PIN_SET);
  }



// MT: Uplink Block disabled A
  // =========================================================================
  // Register I2C translators with note-c framework
  // =========================================================================
  NoteSetFn(malloc, free, noteDelayMs, noteMillis);
  NoteSetFnI2C(NOTE_I2C_ADDR, NOTE_I2C_MAX_SIZE, noteI2CReset, noteI2CTransmit, noteI2CReceive);

  // =========================================================================
  // Verify I2C communication with Notecard
  // =========================================================================
  HAL_Delay(500);  // Small delay before first ping

  if (!noteI2CReset(NOTE_I2C_ADDR)) {
      // Fast flashing = Cannot talk to Notecard over I2C
      while(1) {
    	  HAL_GPIO_TogglePin(MULTI_RED_GPIO_Port, MULTI_RED_Pin);
          HAL_Delay(100);
      }
  }

  // Slow blink 3 times = I2C communication verified
  for (int i = 0; i < 3; i++) {
      HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_SET);
      HAL_Delay(300);
      HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_RESET);
      HAL_Delay(300);
  }

  // =========================================================================
  // Configure Notecard once at startup
  // =========================================================================

  // Set Product and mode (runs once)
  J *config_req = NoteNewRequest("hub.set");
  if (config_req) {
      JAddStringToObject(config_req, "product", "com.yahoo.nanogens:dust_station_matrix_v1");
      JAddStringToObject(config_req, "mode", "periodic");      // Low-power periodic mode
      JAddNumberToObject(config_req, "outbound", 0);           // Manual sync via hub.sync
      JAddNumberToObject(config_req, "inbound", 0);            // Manual sync via hub.sync
      NoteRequest(config_req);  // Waits for confirmation and frees both req & rsp
  }
  J *location_req = NoteNewRequest("card.location.mode");
  if (location_req) {
      JAddStringToObject(location_req, "mode", "periodic");   // Saves battery when stationary
      JAddNumberToObject(location_req, "seconds", 300);       // Match your 5-minute STM32 loop
      NoteRequest(location_req);
  }

  // 3. Process any inbound notes already waiting on the Notecard, then arm ATTN (PB5)
  processInboundNotes();

  J *attn_req = NoteNewRequest("card.attn");
  if (attn_req) {
      JAddStringToObject(attn_req, "mode", "arm,rearm,files");
      const char *filesToWatch[] = {"data.qi"};
      JAddItemToObject(attn_req, "files", JCreateStringArray(filesToWatch, 1));
      NoteRequest(attn_req);
  }

  // Clear any old test data
  J *delete_req = NoteNewRequest("file.delete");
  if (delete_req) {
      JAddStringToObject(delete_req, "file", "sensors.qo");
      NoteRequest(delete_req);
  }

  // Verify Notecard is ready
  J *version_req = NoteNewRequest("card.version");
  if (version_req) {
      J *version_rsp = NoteRequestResponse(version_req);
      if (version_rsp) {
          // 5 quick blinks = ready
          for (int i = 0; i < 5; i++) {
              HAL_GPIO_WritePin(MULTI_GREEN_GPIO_Port, MULTI_GREEN_Pin, GPIO_PIN_SET);
              HAL_Delay(50);
              HAL_GPIO_WritePin(MULTI_GREEN_GPIO_Port, MULTI_GREEN_Pin, GPIO_PIN_RESET);
              HAL_Delay(50);
          }
          NoteDeleteResponse(version_rsp);
      }
  }


  SysState.last_activity_tick = HAL_GetTick();

  SysState.next_sample_epoch = RTC_GetEpochSeconds() + SysState.sample_interval_sec;
  SysState.last_activity_tick = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  // MT: Uplink Block B
    // Create a compact binary template for sensors.qo matching ParticleReading_t
    J *template_req = NoteNewRequest("note.template");
    if (template_req) {
        JAddStringToObject(template_req, "file", "sensors.qo");

        // Define the schema using Notecard type hints:
        // 24 = uint32_t, 22 = uint16_t, 14.1 = float (4-byte IEEE-754)
        J *body = JCreateObject();
        if (body) {
            JAddNumberToObject(body, "rec",    24);    // uint32_t: Record number
            JAddNumberToObject(body, "ts",     24);    // uint32_t: RTC epoch timestamp (s since 2000-01-01)
            JAddNumberToObject(body, "pm2_5",  14.1);  // float:    PM2.5 concentration (ug/m3)
            JAddNumberToObject(body, "pm10",   14.1);  // float:    PM10 concentration (ug/m3)
            JAddNumberToObject(body, "counts", 14.1);  // float:    Total particle counts (#/cm3)
            JAddNumberToObject(body, "flow",   22);    // uint16_t: Average pump flow rate (mL/min)
            JAddNumberToObject(body, "dur_s",  22);    // uint16_t: Sample duration (seconds)
            JAddNumberToObject(body, "err",    22);    // uint16_t: Sensor error status bitmask
            JAddItemToObject(template_req, "body", body);
        }

        // Send the template request (automatically frees template_req, body, and response)
        NoteRequest(template_req);
    }
    HAL_Delay(500);  // Give Notecard time to apply the template
    Notecard_SendDustReading(&LatestDustReading); // <-- ADD THIS LINE (uploads startup test reading)
    SysState.last_activity_tick = HAL_GetTick();

  // Testing LPUART1 transmission
  /*
  uint8_t data[] = "Hello World\r\n";
  while(1)
  {
    HAL_UART_Transmit(&hlpuart1, data, sizeof(data) - 1, HAL_MAX_DELAY);
    HAL_Delay(500);
  }
  */

  uint32_t sample_start_tick = 0;
  uint32_t sample_duration_ms = DEFAULT_SAMPLE_DURATION_SEC * 1000UL;

  while (1)
  {
      ProcessMsg();

	  //HAL_GPIO_TogglePin(LED_A_GPIO_Port, LED_A_Pin);
	  //HAL_GPIO_TogglePin(LED_B_GPIO_Port, LED_B_Pin);
	  //HAL_GPIO_TogglePin(LED_C_GPIO_Port, LED_C_Pin);
	  //HAL_Delay(1000);


	  // Testing LPUART1  output ===========================
	  // Enable RS-485 Transmitter
	  // HAL_GPIO_WritePin(LPUART1_DE_GPIO_Port, LPUART1_DE_Pin, GPIO_PIN_SET);
	  // Send message
	  // HAL_UART_Transmit(&hlpuart1, tx_message, sizeof(tx_message) - 1, 100);
	  // Return to RS-485 Receiver mode
	  // HAL_GPIO_WritePin(LPUART1_DE_GPIO_Port, LPUART1_DE_Pin, GPIO_PIN_RESET);

	  // Testing UART2 output ===========================
	  // Ensure IrDA transceiver shutdown/DE pin (PA4) is LOW (Active/Normal mode)
	  //HAL_GPIO_WritePin(USART2_SD_DE_GPIO_Port, USART2_SD_DE_Pin, GPIO_PIN_RESET);
	  // Send 1 byte using blocking IrDA transmit
	  //HAL_IRDA_Transmit(&hirda2, &irda_test, 1, 100);

      // 1. Start a scheduled dust sample when the target RTC epoch arrives (non-blocking)
      if ((!SysState.sampling_active) &&
          (SysState.next_sample_epoch != 0) &&
          (RTC_GetEpochSeconds() >= SysState.next_sample_epoch))
      {
          if (Particle_PowerAndPrepare(DEFAULT_SAMPLE_DURATION_SEC) && Particle_StartSampling())
          {
              SysState.sampling_active = 1;
              sample_start_tick = HAL_GetTick();
              HAL_GPIO_WritePin(LED_C_GPIO_Port, LED_C_Pin, GPIO_PIN_SET);
          }
          else
          {
              // Retry in 5 seconds if Modbus prep/start failed
              SysState.next_sample_epoch = RTC_GetEpochSeconds() + 5;
          }
      }


      // 2. Monitor active sample non-blockingly until sensor finishes & saves record (status == 2)
      if (SysState.sampling_active)
      {
          uint32_t elapsed_ms = HAL_GetTick() - sample_start_tick;
          if (elapsed_ms >= sample_duration_ms)
          {
              uint16_t status = 0;
              Particle_ReadU16(PP_REG_SAMPLING_STATUS, &status);

              // Sensor finished its countdown naturally (status == 2) OR 15s safety timeout hit
              if (status == 2 || elapsed_ms >= (sample_duration_ms + 8000))
              {
                  if (status != 2)
                  {
                      Particle_StopSampling();
                  }
                  HAL_Delay(300); // Allow sensor to commit new record to flash

                  // Latch and read the newly saved record (record_number + PM2.5, PM10, counts, flow)
                  Particle_LatchAndReadLatest(&LatestDustReading);
                  Notecard_SendDustReading(&LatestDustReading);

                  HAL_GPIO_WritePin(LED_C_GPIO_Port, LED_C_Pin, GPIO_PIN_RESET);

                  // Schedule next sample anchored to the end of this completed sample
                  SysState.next_sample_epoch = RTC_GetEpochSeconds() + SysState.sample_interval_sec;
                  SysState.sampling_active = 0;
                  SysState.last_activity_tick = HAL_GetTick();
              }
          }
      }

      // 3. Handle Notecard ATTN wakeup flag (PB5 falling edge from inbound data.qi)
      if (SysState.attn_pending)
      {
          SysState.attn_pending = 0;

          // Flash MULTI_BLUE once to confirm PB5 ATTN wakeup trigger
          HAL_GPIO_WritePin(MULTI_BLUE_GPIO_Port, MULTI_BLUE_Pin, GPIO_PIN_SET);
          HAL_Delay(100);
          HAL_GPIO_WritePin(MULTI_BLUE_GPIO_Port, MULTI_BLUE_Pin, GPIO_PIN_RESET);

          // Check and process inbound commands in data.qi
          processInboundNotes();

          // Re-arm the Notecard ATTN pin so PB5 returns LOW until the next inbound note
          J *rearm_req = NoteNewRequest("card.attn");
          if (rearm_req != NULL) {
              JAddStringToObject(rearm_req, "mode", "arm,rearm,files");
              const char *filesToWatch[] = {"data.qi"};
              JAddItemToObject(rearm_req, "files", JCreateStringArray(filesToWatch, 1));
              NoteRequest(rearm_req);
          }

          SysState.last_activity_tick = HAL_GetTick();
      }

      // Check all 4 conditions before entering STOP mode:
      // a) Inactivity timer expired (>= INACTIVITY_SLEEP_MS)
      // b) No sampling cycle actively running
      // c) Sample interval > 5s (if <= 5s, stay awake with GEN_PWR_EN ON)
      // d) No pending Notecard ATTN interrupt
      if (
    	  ((HAL_GetTick() - SysState.last_activity_tick) >= INACTIVITY_SLEEP_MS) &&
          (!SysState.sampling_active) &&
          (SysState.sample_interval_sec > FAST_SAMPLE_THRESHOLD_SEC) &&
          (!SysState.attn_pending))
      {
          Enter_Stop_Mode();
      }


/*
// MT: Uplink Block disabled C
      // Create and send the note (no reconfiguration!)
	  J *req = NoteNewRequest("note.add");
	  if (req != NULL) {
	      J *body = JCreateObject();
	      JAddNumberToObject(body, "press", press_value);
	      JAddNumberToObject(body, "temp", temp_value);
	      JAddNumberToObject(body, "cond", cond_value);
	      JAddItemToObject(req, "body", body); // Attached to req
	      NoteRequest(req);
	      NoteDeleteResponse(req);             // Cleans up BOTH req and body automatically!

	      // 🗑️ REMOVED: if (body != NULL) JDelete(body);
	  }

      // Sync (sends outbound data and checks for inbound)
      J *sync_req = NoteNewCommand("hub.sync");
      if (sync_req) {
          NoteRequest(sync_req);
          NoteDeleteResponse(sync_req);
      }

      // Process any inbound notes from the cloud
      processInboundNotes();

      // Flash LED once to show transaction attempted
      HAL_GPIO_WritePin(MULTI_BLUE_GPIO_Port, MULTI_BLUE_Pin, GPIO_PIN_SET);
      HAL_Delay(100);
      HAL_GPIO_WritePin(MULTI_BLUE_GPIO_Port, MULTI_BLUE_Pin, GPIO_PIN_RESET);

      // Update test values
      press_value += 1;
      temp_value += 2;
      cond_value += 4;

      // Wait 5 mins before next reading
      // Note: 5 mins minimum is needed for it to lock GPS satellites
      //HAL_Delay(300000);



      // =========================================================================
      // Put STM32 into Low-Power STOP Mode instead of HAL_Delay
      // =========================================================================

      // Clear RTC flags and set the 5-minute backup alarm
      __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
      HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 300, RTC_WAKEUPCLOCK_CK_SPRE_16BITS); // 300 seconds = 5 minutes exactly

      HAL_SuspendTick();
      HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);

      // -------------- THE STM32 SLEEPS UNTIL RTC TIMER OR PB5 EXTI FALLS --------------

      HAL_ResumeTick();
      HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
      SystemClock_Config(); // Restore full speed clocks (HSE/PLL)

      // Check if the Notecard's ATTN pin woke us up - 2 possible reasons: (a) Incoming data (ATTN_P interrupt from Note Card) or (b) 5 minute STM32 Wakeup timer which we have set
      J *attn_check = NoteNewRequest("card.attn");
      if (attn_check)
      {
          J *attn_rsp = NoteRequestResponse(attn_check);
          if (attn_rsp) {
              // If "files" is true in the response, we have inbound data waiting!
              if (JGetBool(attn_rsp, "files")) {
                  processInboundNotes(); // Process commands (like toggling LEDs) immediately
              }
              NoteDeleteResponse(attn_rsp);
          }
          NoteDeleteResponse(attn_check);
      }
*/
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.LSEState = RCC_LSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_4;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_LPUART1
                              |RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_RTC;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_SYSCLK;
  PeriphClkInit.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_SYSCLK;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_SYSCLK;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00503D58;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief LPUART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPUART1_UART_Init(void)
{

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 115200;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_IRDA_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  hirda2.Instance = USART2;
  hirda2.Init.BaudRate = 115200;
  hirda2.Init.WordLength = IRDA_WORDLENGTH_8B;
  hirda2.Init.Parity = IRDA_PARITY_NONE;
  hirda2.Init.Mode = IRDA_MODE_TX_RX;
  hirda2.Init.Prescaler = 1;
  hirda2.Init.PowerMode = IRDA_POWERMODE_NORMAL;
  if (HAL_IRDA_Init(&hirda2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */
  // Prevent CubeMX's auto-generated HAL_RTC_SetTime and HAL_RTC_SetDate
  // from resetting the clock to 00:00:00 on every reboot by checking the backup register first
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == 0x32F2)
  {
	  return; // Skip overwriting time/date if RTC was already synced
  }
  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the WakeUp
  */
  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 0, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BUFFEN_OE_BAR_GPIO_Port, BUFFEN_OE_BAR_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, BATT_MON_EN_Pin|USART2_SD_DE_Pin|POWERUP_NC_EN_Pin|GEN_PWR_EN_Pin
                          |LED_B_Pin|LED_A_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, SPI2_CS_BAR_Pin|MULTI_GREEN_Pin|RESET_NC_RST_BAR_Pin|MULTI_BLUE_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LPUART1_DE_Pin|LED_C_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(MULTI_RED_GPIO_Port, MULTI_RED_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : BUFFEN_OE_BAR_Pin */
  GPIO_InitStruct.Pin = BUFFEN_OE_BAR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUFFEN_OE_BAR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BATT_MON_EN_Pin */
  GPIO_InitStruct.Pin = BATT_MON_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BATT_MON_EN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BATT_MON_Pin */
  GPIO_InitStruct.Pin = BATT_MON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BATT_MON_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USART2_SD_DE_Pin */
  GPIO_InitStruct.Pin = USART2_SD_DE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(USART2_SD_DE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : POWERUP_NC_EN_Pin GEN_PWR_EN_Pin LED_B_Pin LED_A_Pin
                           MULTI_RED_Pin */
  GPIO_InitStruct.Pin = POWERUP_NC_EN_Pin|GEN_PWR_EN_Pin|LED_B_Pin|LED_A_Pin
                          |MULTI_RED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI2_CS_BAR_Pin MULTI_GREEN_Pin RESET_NC_RST_BAR_Pin MULTI_BLUE_Pin
                           LED_C_Pin */
  GPIO_InitStruct.Pin = SPI2_CS_BAR_Pin|MULTI_GREEN_Pin|RESET_NC_RST_BAR_Pin|MULTI_BLUE_Pin
                          |LED_C_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : IR_WAKE_UP_Pin */
  GPIO_InitStruct.Pin = IR_WAKE_UP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(IR_WAKE_UP_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LPUART1_DE_Pin */
  GPIO_InitStruct.Pin = LPUART1_DE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LPUART1_DE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ATTN_EXT15_Pin */
  GPIO_InitStruct.Pin = ATTN_EXT15_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ATTN_EXT15_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SDOWN_BUFF_Pin */
  GPIO_InitStruct.Pin = SDOWN_BUFF_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SDOWN_BUFF_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI2_3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == LPUART1) {
        Lpuart1_ContinueSend();
    }
#ifndef IR_USART2_SEL
    else if (huart->Instance == USART2) {
        ContinueSend();
    }
#endif
}

void HAL_IRDA_TxCpltCallback(IRDA_HandleTypeDef *hirda)
{
    if (hirda->Instance == USART2) {
#ifndef IR_USART2_SEL
        ContinueSend();
#endif
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == LPUART1) {
        uint8_t rx = Lpuart1.bufferrx[0];
        HAL_UART_Receive_IT(&hlpuart1, Lpuart1.bufferrx, 1);
        Parse_Lpuart1_Byte(rx);
    }
#ifndef IR_USART2_SEL
    else if (huart->Instance == USART2) {
    	SysState.last_activity_tick = HAL_GetTick(); // 10 seconds of no activity after getting this time, we move into STOP mode
        uint8_t rx = Usart2.bufferrx[0];
        HAL_UART_Receive_IT(&husart2, Usart2.bufferrx, 1);
        Parse_Usart2_Byte(rx);
    }
#endif
}

void HAL_IRDA_RxCpltCallback(IRDA_HandleTypeDef *hirda)
{
    if (hirda->Instance == USART2)
    {
        uint8_t rx = Usart2.bufferrx[0];
        if (rx != 0x00) // Ignore ambient 0x00 IR noise; reset inactivity timer on real packet bytes
        {
            SysState.last_activity_tick = HAL_GetTick();
        }
        HAL_IRDA_Receive_IT(&hirda2, Usart2.bufferrx, 1);
        Parse_Usart2_Byte(rx);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == LPUART1) {
        __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);
        volatile uint32_t temp = LPUART1->RDR;
        (void)temp;
        HAL_UART_Receive_IT(&hlpuart1, Lpuart1.bufferrx, 1);
    }
#ifndef IR_USART2_SEL
    else if (huart->Instance == USART2) {
        __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);
        volatile uint32_t temp = USART2->RDR;
        (void)temp;
        HAL_UART_Receive_IT(&husart2, Usart2.bufferrx, 1);
    }
#endif
}

void HAL_IRDA_ErrorCallback(IRDA_HandleTypeDef *hirda)
{
    if (hirda->Instance == USART2) {
        __HAL_IRDA_CLEAR_FLAG(hirda, IRDA_FLAG_ORE | IRDA_FLAG_NE | IRDA_FLAG_FE | IRDA_FLAG_PE);
        volatile uint32_t temp = USART2->ISR;
        temp = USART2->RDR;
        (void)temp;
        HAL_IRDA_Receive_IT(&hirda2, Usart2.bufferrx, 1);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == IR_WAKE_UP_Pin) // PB2 falling edge from USART2 IrDA/Pogo
    {
        HAL_NVIC_DisableIRQ(EXTI2_3_IRQn);
        SysState.wakeup_source = WAKEUP_USART2_IR;
    }
    else if (GPIO_Pin == ATTN_EXT15_Pin) // PB5 falling edge from Notecard
    {
        SysState.wakeup_source = WAKEUP_NOTECARD_ATTN;
        SysState.attn_pending = 1;
        SysState.last_activity_tick = HAL_GetTick();
    }
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
    SysState.wakeup_source = WAKEUP_RTC_TIMER;
    SysState.rtc_wakeup_pending = 1;
    SysState.last_activity_tick = HAL_GetTick();
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
