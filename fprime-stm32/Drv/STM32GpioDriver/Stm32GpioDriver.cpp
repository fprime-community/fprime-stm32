// ======================================================================
// \title  Stm32GpioDriver.cpp
// \author ivanlara
// \brief  HAL boundary for the STM32 bare-metal GPIO pin driver (real
//         hardware implementation, STM32 targets only). This is the only
//         file in this driver allowed to include HAL headers -- see
//         Stm32GpioDriverStub.cpp for the host unit-test stand-in.
// ======================================================================

#include <fprime-stm32/Drv/STM32GpioDriver/Stm32GpioDriver.hpp>

// gpio.h is CubeMX's own always-present per-peripheral header (generated
// under this exact name for every STM32 family); it chains to the family
// HAL umbrella (via main.h), so including it instead of naming e.g.
// stm32h7xx_hal.h directly keeps this file portable across STM32 families.
#include "gpio.h"

namespace {

//! Map a HAL-free GpioPort identifier to the real CMSIS peripheral base.
GPIO_TypeDef* toHalPort(Stm32::GpioPort port) {
    GPIO_TypeDef* halPort = nullptr;
    switch (port) {
        case Stm32::GpioPort::A:
            #if defined(GPIOA)
            halPort = GPIOA;
            #endif
            break;
        case Stm32::GpioPort::B:
            #if defined(GPIOB)
            halPort = GPIOB;
            #endif
            break;
        case Stm32::GpioPort::C:
            #if defined(GPIOC)
            halPort = GPIOC;
            #endif
            break;
        case Stm32::GpioPort::D:
            #if defined(GPIOD)
            halPort = GPIOD;
            #endif
            break;
        case Stm32::GpioPort::E:
            #if defined(GPIOE)
            halPort = GPIOE;
            #endif
            break;
        case Stm32::GpioPort::F:
            #if defined(GPIOF)
            halPort = GPIOF;
            #endif
            break;
        case Stm32::GpioPort::G:
            #if defined(GPIOG)
            halPort = GPIOG;
            #endif
            break;
        case Stm32::GpioPort::H:
            #if defined(GPIOH)
            halPort = GPIOH;
            #endif
            break;
        case Stm32::GpioPort::I:
            #if defined(GPIOI)
            halPort = GPIOI;
            #endif
            break;
        case Stm32::GpioPort::J:
            #if defined(GPIOJ)
            halPort = GPIOJ;
            #endif
            break;
        case Stm32::GpioPort::K:
            #if defined(GPIOK)
            halPort = GPIOK;
            #endif
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(port));
            break;
    }
    return halPort;
}

//! Enable the AHB4 clock for whichever GPIO port is passed in
void enableGpioClock(GPIO_TypeDef* port) {
    FW_ASSERT(port != nullptr);

#if defined(GPIOA) && defined(__HAL_RCC_GPIOA_CLK_ENABLE)
    if (port == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
#endif
#if defined(GPIOB) && defined(__HAL_RCC_GPIOB_CLK_ENABLE)
    } else if (port == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
#endif
#if defined(GPIOC) && defined(__HAL_RCC_GPIOC_CLK_ENABLE)
    } else if (port == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
        #endif
#if defined(GPIOD) && defined(__HAL_RCC_GPIOD_CLK_ENABLE)
    } else if (port == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
        #endif
#if defined(GPIOE) && defined(__HAL_RCC_GPIOE_CLK_ENABLE)
    } else if (port == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
        #endif
#if defined(GPIOF) && defined(__HAL_RCC_GPIOF_CLK_ENABLE)
    } else if (port == GPIOF) {
        __HAL_RCC_GPIOF_CLK_ENABLE();
        #endif
#if defined(GPIOG) && defined(__HAL_RCC_GPIOG_CLK_ENABLE)
    } else if (port == GPIOG) {
        __HAL_RCC_GPIOG_CLK_ENABLE();
        #endif
#if defined(GPIOH) && defined(__HAL_RCC_GPIOH_CLK_ENABLE)
    } else if (port == GPIOH) {
        __HAL_RCC_GPIOH_CLK_ENABLE();
        #endif
#if defined(GPIOI) && defined(__HAL_RCC_GPIOI_CLK_ENABLE)
    } else if (port == GPIOI) {
        __HAL_RCC_GPIOI_CLK_ENABLE();
        #endif
#if defined(GPIOJ) && defined(__HAL_RCC_GPIOJ_CLK_ENABLE)
    } else if (port == GPIOJ) {
        __HAL_RCC_GPIOJ_CLK_ENABLE();
        #endif
#if defined(GPIOK) && defined(__HAL_RCC_GPIOK_CLK_ENABLE)
    } else if (port == GPIOK) {
        __HAL_RCC_GPIOK_CLK_ENABLE();
#endif
    } else {
        FW_ASSERT(false, reinterpret_cast<FwAssertArgType>(port));
    }
}
}  // namespace

namespace Stm32 {

bool Stm32GpioDriver ::hwConfigurePin(GpioPort port, U16 pin, Fw::Direction mode, Fw::Logic defaultState) {
    GPIO_TypeDef* halPort = toHalPort(port);

    FW_ASSERT(halPort != nullptr);

    enableGpioClock(halPort);

    if (mode == Fw::Direction::OUT) {
        HAL_GPIO_WritePin(halPort, pin, (defaultState == Fw::Logic::HIGH) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    GPIO_InitTypeDef init = {};
    init.Pin = pin;
    init.Mode = (mode == Fw::Direction::OUT) ? GPIO_MODE_OUTPUT_PP : GPIO_MODE_INPUT;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(halPort, &init);

    // Verify that the GPIO was successfully configured for the specified mode.
    const U32 position = POSITION_VAL(pin);
    const U32 actualModeBits = (halPort->MODER >> (position * 2U)) & GPIO_MODE;
    const U32 expectedModeBits = init.Mode & GPIO_MODE;
    return actualModeBits == expectedModeBits;
}

bool Stm32GpioDriver ::hwReadPin(GpioPort port, U16 pin) {
    return HAL_GPIO_ReadPin(toHalPort(port), pin) == GPIO_PIN_SET;
}

void Stm32GpioDriver ::hwWritePin(GpioPort port, U16 pin, bool high) {
    HAL_GPIO_WritePin(toHalPort(port), pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

}  // namespace Stm32