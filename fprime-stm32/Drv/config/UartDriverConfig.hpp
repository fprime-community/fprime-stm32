/**
 * \file UartDriverConfig.hpp
 * \author ivanlara
 * \brief Configuration header for the STM32 UART driver.
 */
#ifndef STM32_UART_DRIVER_CONFIG_HPP
#define STM32_UART_DRIVER_CONFIG_HPP

#include <config/Stm32Config.hpp>
#include <Fw/Types/BasicTypes.hpp>

namespace Stm32::Stm32UartDriverConfig {

// Ring and DMA staging capacities.
static constexpr FwSizeType TX_RING_SIZE = 4096;
static constexpr FwSizeType TX_STAGING_SIZE = 1024;
static constexpr FwSizeType RX_RING_SIZE = 4096;
static constexpr FwSizeType RX_STAGING_SIZE = 1024;

// Timeout calculation for an 8N1 UART frame.
static constexpr U32 TX_BITS_PER_BYTE = 10;
static constexpr U32 TX_TIMEOUT_MARGIN = 2;
static constexpr U32 TX_TIMEOUT_SLACK_US = 2000;

// Configuration invariants
static_assert(TX_RING_SIZE > 0);
static_assert(TX_STAGING_SIZE > 0);
static_assert(RX_RING_SIZE > 0);
static_assert(RX_STAGING_SIZE > 0);

static_assert(TX_STAGING_SIZE <= 0xFFFF);
static_assert(RX_STAGING_SIZE <= 0xFFFF);

static_assert(TX_BITS_PER_BYTE > 0);
static_assert(TX_TIMEOUT_MARGIN > 0);
static_assert(TX_TIMEOUT_SLACK_US > 0);

}  // namespace Stm32::Stm32UartDriverConfig

#endif