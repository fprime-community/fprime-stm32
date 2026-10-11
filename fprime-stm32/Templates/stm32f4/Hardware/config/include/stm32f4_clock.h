/**
 ******************************************************************************
 * @file    stm32f4_clock.h
 * @brief   Clock initialization for STM32F4.
 ******************************************************************************
 */
#ifndef STM32F4_CLOCK_H_
#define STM32F4_CLOCK_H_

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

void FprimeStm32_ClockInit(void);
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32F4_CLOCK_H_ */