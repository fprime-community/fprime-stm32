/**
 ******************************************************************************
 * @file    tim2_clock.h
 * @author  ivanlara
 * @brief   Free-running microsecond clock built on TIM2.
 ******************************************************************************
 */
#ifndef __TIM2_CLOCK_H__
#define __TIM2_CLOCK_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief start the TIM2 free-running microsecond clock
 *
 * Initializes TIM2 (MX_TIM2_Init) and starts it in interrupt mode
 * (HAL_TIM_Base_Start_IT) so that the overflow counter used by
 * Stm32_GetSystemMicroseconds is kept up to date. Must be called once,
 * after HAL_Init(), before any code relies on Stm32_GetSystemMicroseconds().
 */
void Stm32_Tim2ClockInit(void);

/**
 * \brief read the current free-running microsecond count
 *
 * Assembles a 64-bit microsecond count from the 32-bit TIM2 counter and the
 * overflow counter incremented by HAL_TIM_PeriodElapsedCallback(). Uses a
 * double-read pattern so that a TIM2 update interrupt firing between reading
 * the overflow count and the counter register cannot produce a corrupted
 * (torn) result.
 *
 * \return microseconds elapsed since Stm32_Tim2ClockInit() was called
 */
uint64_t Stm32_GetSystemMicroseconds(void);

#ifdef __cplusplus
}
#endif

#endif /* __TIM2_CLOCK_H__ */
