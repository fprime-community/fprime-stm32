// ======================================================================
// \title  Stm32Config.hpp
// \author ivanlara
// \brief  Configuration file for STM32 peripherals
// ======================================================================
#ifndef STM32_CONFIG_HPP
#define STM32_CONFIG_HPP

// ======================================================================
// USART/UART peripheral instances
// ======================================================================

#ifndef USART_ENABLED
#define USART_ENABLED (true)
#endif  // USART_ENABLED

#ifndef USART1_UART_INSTANCE
#define USART1_UART_INSTANCE (false) //!< Indicates whether the USART1 peripheral instance is enabled
#endif  // USART1_UART_INSTANCE

#ifndef USART2_UART_INSTANCE
#define USART2_UART_INSTANCE (true) //!< Indicates whether the USART2 peripheral instance is enabled
#endif  // USART2_UART_INSTANCE

#ifndef USART3_UART_INSTANCE
#define USART3_UART_INSTANCE (false) //!< Indicates whether the USART3 peripheral instance is enabled
#endif  // USART3_UART_INSTANCE

#ifndef UART4_INSTANCE
#define UART4_INSTANCE (false) //!< Indicates whether the UART4 peripheral instance is enabled
#endif  // UART4_INSTANCE

#ifndef UART5_INSTANCE
#define UART5_INSTANCE (false) //!< Indicates whether the UART5 peripheral instance is enabled
#endif  // UART5_INSTANCE

#ifndef USART6_UART_INSTANCE
#define USART6_UART_INSTANCE (false) //!< Indicates whether the USART6 peripheral instance is enabled
#endif  // USART6_UART_INSTANCE

#ifndef UART7_INSTANCE
#define UART7_INSTANCE (false) //!< Indicates whether the UART7 peripheral instance is enabled
#endif  // UART7_INSTANCE

#ifndef UART8_INSTANCE
#define UART8_INSTANCE (false) //!< Indicates whether the UART8 peripheral instance is enabled
#endif  // UART8_INSTANCE

// ======================================================================
// I2C peripheral instances
// ======================================================================

#ifndef I2C_ENABLED
#define I2C_ENABLED (false)
#endif  // I2C_ENABLED

#ifndef I2C1_INSTANCE
#define I2C1_INSTANCE (false) //!< Indicates whether the I2C1 peripheral instance is enabled
#endif  // I2C1_INSTANCE

#ifndef I2C2_INSTANCE
#define I2C2_INSTANCE (false) //!< Indicates whether the I2C2 peripheral instance is enabled
#endif  // I2C2_INSTANCE

#ifndef I2C3_INSTANCE
#define I2C3_INSTANCE (false) //!< Indicates whether the I2C3 peripheral instance is enabled
#endif  // I2C3_INSTANCE

#ifndef I2C4_INSTANCE
#define I2C4_INSTANCE (false) //!< Indicates whether the I2C4 peripheral instance is enabled
#endif  // I2C4_INSTANCE

// ======================================================================
// SPI peripheral instances
// ======================================================================

#ifndef SPI_ENABLED
#define SPI_ENABLED (false)
#endif  // SPI_ENABLED

#ifndef SPI1_INSTANCE
#define SPI1_INSTANCE (false) //!< Indicates whether the SPI1 peripheral instance is enabled
#endif  // SPI1_INSTANCE

#ifndef SPI2_INSTANCE
#define SPI2_INSTANCE (false) //!< Indicates whether the SPI2 peripheral instance is enabled
#endif  // SPI2_INSTANCE

#ifndef SPI3_INSTANCE
#define SPI3_INSTANCE (false) //!< Indicates whether the SPI3 peripheral instance is enabled
#endif  // SPI3_INSTANCE

#ifndef SPI4_INSTANCE
#define SPI4_INSTANCE (false) //!< Indicates whether the SPI4 peripheral instance is enabled
#endif  // SPI4_INSTANCE

#ifndef SPI5_INSTANCE
#define SPI5_INSTANCE (false) //!< Indicates whether the SPI5 peripheral instance is enabled
#endif  // SPI5_INSTANCE

#ifndef SPI6_INSTANCE
#define SPI6_INSTANCE (false) //!< Indicates whether the SPI6 peripheral instance is enabled
#endif  // SPI6_INSTANCE

// ======================================================================
// TIM peripheral instances
// ======================================================================

#ifndef TIM_ENABLED
#define TIM_ENABLED (true)
#endif  // TIM_ENABLED

#ifndef TIM1_INSTANCE
#define TIM1_INSTANCE (false) //!< Indicates whether the TIM1 peripheral instance is enabled
#endif  // TIM1_INSTANCE

#ifndef TIM2_INSTANCE
#define TIM2_INSTANCE (true) //!< Indicates whether the TIM2 peripheral instance is enabled
#endif  // TIM2_INSTANCE

#ifndef TIM3_INSTANCE
#define TIM3_INSTANCE (false) //!< Indicates whether the TIM3 peripheral instance is enabled
#endif  // TIM3_INSTANCE

#ifndef TIM4_INSTANCE
#define TIM4_INSTANCE (false) //!< Indicates whether the TIM4 peripheral instance is enabled
#endif  // TIM4_INSTANCE

#ifndef TIM5_INSTANCE
#define TIM5_INSTANCE (false) //!< Indicates whether the TIM5 peripheral instance is enabled
#endif  // TIM5_INSTANCE

#ifndef TIM6_INSTANCE
#define TIM6_INSTANCE (false) //!< Indicates whether the TIM6 peripheral instance is enabled
#endif  // TIM6_INSTANCE

#ifndef TIM7_INSTANCE
#define TIM7_INSTANCE (false) //!< Indicates whether the TIM7 peripheral instance is enabled
#endif  // TIM7_INSTANCE

#ifndef TIM8_INSTANCE
#define TIM8_INSTANCE (false) //!< Indicates whether the TIM8 peripheral instance is enabled
#endif  // TIM8_INSTANCE

#ifndef TIM12_INSTANCE
#define TIM12_INSTANCE (false) //!< Indicates whether the TIM12 peripheral instance is enabled
#endif  // TIM12_INSTANCE

#ifndef TIM13_INSTANCE
#define TIM13_INSTANCE (false) //!< Indicates whether the TIM13 peripheral instance is enabled
#endif  // TIM13_INSTANCE

#ifndef TIM14_INSTANCE
#define TIM14_INSTANCE (false) //!< Indicates whether the TIM14 peripheral instance is enabled
#endif  // TIM14_INSTANCE

#ifndef TIM15_INSTANCE
#define TIM15_INSTANCE (false) //!< Indicates whether the TIM15 peripheral instance is enabled
#endif  // TIM15_INSTANCE

#ifndef TIM16_INSTANCE
#define TIM16_INSTANCE (false) //!< Indicates whether the TIM16 peripheral instance is enabled
#endif  // TIM16_INSTANCE

#ifndef TIM17_INSTANCE
#define TIM17_INSTANCE (false) //!< Indicates whether the TIM17 peripheral instance is enabled
#endif  // TIM17_INSTANCE

#endif  // STM32_CONFIG_HPP