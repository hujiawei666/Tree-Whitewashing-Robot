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
#include "stm32f4xx_hal.h"

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
#define KEY2_Pin GPIO_PIN_2
#define KEY2_GPIO_Port GPIOE
#define PUMP_Pin GPIO_PIN_3
#define PUMP_GPIO_Port GPIOE
#define KEY0_Pin GPIO_PIN_4
#define KEY0_GPIO_Port GPIOE
#define DRIVER_DIR_Pin GPIO_PIN_5
#define DRIVER_DIR_GPIO_Port GPIOE
#define DRIVER_OE_Pin GPIO_PIN_6
#define DRIVER_OE_GPIO_Port GPIOE
#define LED1_Pin GPIO_PIN_13
#define LED1_GPIO_Port GPIOC
#define INA1_Pin GPIO_PIN_0
#define INA1_GPIO_Port GPIOC
#define INB2_Pin GPIO_PIN_1
#define INB2_GPIO_Port GPIOC
#define INA3_Pin GPIO_PIN_2
#define INA3_GPIO_Port GPIOC
#define INB4_Pin GPIO_PIN_3
#define INB4_GPIO_Port GPIOC
#define INA5_Pin GPIO_PIN_4
#define INA5_GPIO_Port GPIOC
#define INB6_Pin GPIO_PIN_5
#define INB6_GPIO_Port GPIOC
#define BEEP_Pin GPIO_PIN_0
#define BEEP_GPIO_Port GPIOB
#define LED0_Pin GPIO_PIN_1
#define LED0_GPIO_Port GPIOB
#define TRIG_LEFT_Pin GPIO_PIN_2
#define TRIG_LEFT_GPIO_Port GPIOB
#define PS2_CS_Pin GPIO_PIN_12
#define PS2_CS_GPIO_Port GPIOB
#define PS2_CLK_Pin GPIO_PIN_13
#define PS2_CLK_GPIO_Port GPIOB
#define PS2_DO_Pin GPIO_PIN_14
#define PS2_DO_GPIO_Port GPIOB
#define PS2_DI_Pin GPIO_PIN_15
#define PS2_DI_GPIO_Port GPIOB
#define INA7_Pin GPIO_PIN_8
#define INA7_GPIO_Port GPIOC
#define INB8_Pin GPIO_PIN_9
#define INB8_GPIO_Port GPIOC
#define TRIG_FRONT_Pin GPIO_PIN_3
#define TRIG_FRONT_GPIO_Port GPIOB
#define TRIG_RIGHT_Pin GPIO_PIN_4
#define TRIG_RIGHT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
