/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "defines.h"
#include "globals.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BUFFEN_OE_BAR_Pin GPIO_PIN_13
#define BUFFEN_OE_BAR_GPIO_Port GPIOC
#define BATT_MON_EN_Pin GPIO_PIN_0
#define BATT_MON_EN_GPIO_Port GPIOA
#define BATT_MON_Pin GPIO_PIN_1
#define BATT_MON_GPIO_Port GPIOA
#define USART2_SD_DE_Pin GPIO_PIN_4
#define USART2_SD_DE_GPIO_Port GPIOA
#define POWERUP_NC_EN_Pin GPIO_PIN_5
#define POWERUP_NC_EN_GPIO_Port GPIOA
#define GEN_PWR_EN_Pin GPIO_PIN_7
#define GEN_PWR_EN_GPIO_Port GPIOA
#define SPI2_CS_BAR_Pin GPIO_PIN_0
#define SPI2_CS_BAR_GPIO_Port GPIOB
#define IR_WAKE_UP_Pin GPIO_PIN_2
#define IR_WAKE_UP_GPIO_Port GPIOB
#define IR_WAKE_UP_EXTI_IRQn EXTI2_3_IRQn
#define LPUART1_DE_Pin GPIO_PIN_12
#define LPUART1_DE_GPIO_Port GPIOB
#define LED_B_Pin GPIO_PIN_8
#define LED_B_GPIO_Port GPIOA
#define LED_A_Pin GPIO_PIN_11
#define LED_A_GPIO_Port GPIOA
#define MULTI_RED_Pin GPIO_PIN_15
#define MULTI_RED_GPIO_Port GPIOA
#define MULTI_GREEN_Pin GPIO_PIN_3
#define MULTI_GREEN_GPIO_Port GPIOB
#define RESET_NC_RST_BAR_Pin GPIO_PIN_4
#define RESET_NC_RST_BAR_GPIO_Port GPIOB
#define ATTN_EXT15_Pin GPIO_PIN_5
#define ATTN_EXT15_GPIO_Port GPIOB
#define ATTN_EXT15_EXTI_IRQn EXTI4_15_IRQn
#define MULTI_BLUE_Pin GPIO_PIN_7
#define MULTI_BLUE_GPIO_Port GPIOB
#define LED_C_Pin GPIO_PIN_8
#define LED_C_GPIO_Port GPIOB
#define SDOWN_BUFF_Pin GPIO_PIN_9
#define SDOWN_BUFF_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
