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
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
#define Temperature_Humidity_Pin GPIO_PIN_7
#define Temperature_Humidity_GPIO_Port GPIOA
#define Light_do_Pin GPIO_PIN_0
#define Light_do_GPIO_Port GPIOB
#define Light_ao_Pin GPIO_PIN_1
#define Light_ao_GPIO_Port GPIOB
#define confirm_key_Pin GPIO_PIN_10
#define confirm_key_GPIO_Port GPIOB
#define Bule_switch_Pin GPIO_PIN_11
#define Bule_switch_GPIO_Port GPIOB
#define oled_ui_Pin GPIO_PIN_12
#define oled_ui_GPIO_Port GPIOB
#define Blue_en_Pin GPIO_PIN_8
#define Blue_en_GPIO_Port GPIOA
#define Blue_Tx_Pin GPIO_PIN_9
#define Blue_Tx_GPIO_Port GPIOA
#define Blue_Rx_Pin GPIO_PIN_10
#define Blue_Rx_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
