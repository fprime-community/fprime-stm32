/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    tim2_clock.cpp
 * @brief   Free-running microsecond clock built on TIM2.
 ******************************************************************************
 */
/* USER CODE END Header */
#include "tim2_clock.h"
#include "tim.h"

/**
 * \brief number of TIM2 update (overflow) events observed so far
 *
 * Incremented from HAL_TIM_PeriodElapsedCallback(), which executes in the
 * TIM2 interrupt context. Marked volatile because it is written from an ISR
 * and read from ordinary (non-interrupt) code in Stm32_GetSystemMicroseconds().
 */
static volatile uint32_t g_tim2_overflows = 0;

void Stm32_Tim2ClockInit(void) {
    MX_TIM2_Init();
    (void)HAL_TIM_Base_Start_IT(&htim2);
}

/**
 * \brief HAL callback invoked on every TIM2 update (overflow) event
 *
 * TIM2 is a 32-bit up-counter clocked at 1 MHz, so it overflows roughly
 * every 4294.967296 seconds. Each overflow represents 2^32 microseconds,
 * which is folded into the high bits of the 64-bit microsecond count
 * assembled by Stm32_GetSystemMicroseconds().
 */
extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
    if (htim->Instance == TIM2) {
        g_tim2_overflows++;
    }
}

uint64_t Stm32_GetSystemMicroseconds(void) {
    uint32_t overflows_1 = 0;
    uint32_t overflows_2 = 0;
    uint32_t ticks = 0;

    // Double-read pattern: guards against a TIM2 overflow interrupt firing
    // between reading the overflow count and reading the 32-bit counter
    // register, which would otherwise assemble a torn (inconsistent) value.
    do {
        overflows_1 = g_tim2_overflows;
        ticks = TIM2->CNT;
        overflows_2 = g_tim2_overflows;
    } while (overflows_1 != overflows_2);

    return (static_cast<uint64_t>(overflows_1) << 32) | static_cast<uint64_t>(ticks);
}
