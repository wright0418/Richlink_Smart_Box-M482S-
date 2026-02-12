/**
 * @file gpio.h
 * @brief Button and G-sensor GPIO initialization helpers
 *
 * This header exposes board-level GPIO helpers used by `main.c` and
 * other modules. It focuses on configuring buttons, G-sensor pins and
 * wake-up pin behavior for low-power modes.
 */
#ifndef _GPIO_H_
#define _GPIO_H_

#include <stdint.h>

/**
 * @brief Initialize push-buttons and G-sensor GPIOs.
 *
 * Configures PB7, PB8, PB15 (buttons) and PC5 (G-sensor interrupt)
 * as inputs, enables appropriate interrupt triggering and debounce for
 * the user key (PB15) and Hall sensor (PB7). Also enables NVIC IRQs
 * for GPB/GPC lines.
 * 
 * Hall Sensor (PB7): Falling edge triggered, counted as 2 per jump.
 * Debounce (20ms) applied to filter electrical noise.
 */
void Init_Buttons_Gsensor(void);

/**
 * @brief Reset Hall sensor edge counter (called on game stop or disconnect).
 *
 * Clears the internal PB7 falling-edge counter used for 2-edge jump
 * detection. Must be called when transitioning from GAME_START or on
 * state reset to ensure clean counting on next game session.
 */
void GPIO_ResetHallEdgeCount(void);

/**
 * @brief Configure pins for SPD (Super Power Down) input state.
 *
 * Sets the listed pins to the low-leakage input configuration required
 * while the MCU is in SPD mode. This function does not enable wake-up
 * logic; it only configures the GPIO states.
 */
void InitSpdPins(void);

/**
 * @brief Configure Deep Power-Down (DPD) wake-up pin.
 * @param edgeType Edge selection for wake-up (platform-specific flag
 *                 forwarded to the clock/power controller).
 *
 * Configures the MCU pin used to wake from DPD and programs the
 * hardware to trigger on the specified edge type.
 */
void Gpio_ConfigDPDWakeup(uint32_t edgeType);

/**
 * @brief Configure SPD (Super Power Down) wake-up pins.
 *
 * Typically selects PB15 (or other board-specific pins) as the SPD
 * wake-up source and programs debounce/wakeup filtering as required.
 */
void Gpio_ConfigSPDWakeup(void);

/**
 * @brief Simple GPIO helpers for direct pin manipulation.
 *
 * These macros perform a direct set/clear on a port DOUT register and are
 * provided as convenience helpers. Prefer using module-level APIs where
 * possible rather than manipulating DOUT directly.
 */
#define GPIO_SET(port, bit) ((port)->DOUT |= (bit))
#define GPIO_CLR(port, bit) ((port)->DOUT &= ~(bit))

/**
 * @brief Enable Schmitt trigger on I2C pins (board helper).
 *
 * Some boards require the I2C SCL/SDA Schmitt trigger to be enabled to
 * meet signal integrity requirements. This helper configures the pin
 * attribute accordingly.
 */
void EnableI2C_Schmitt(void);

/** Board-level helpers used by startup code in main.c */
/** Board-level helpers used by startup code in main.c */
void Board_ConfigPCLKDiv(void);
void Board_ConfigMultiFuncPins(void);
void Board_ReleaseIOPD(void);

/**
 * @brief Initialize Power Lock GPIO (PA11) and assert lock (high).
 */
void PowerLock_Init(void);

/**
 * @brief Control Power Lock GPIO (PA11).
 * @param on 1 to lock (high), 0 to release (low).
 */
void PowerLock_Set(uint8_t on);

/**
 * @brief Initialize USB charge detect pin (PA12) as input.
 */
void USBDetect_Init(void);

/**
 * @brief Read USB charge detect status (PA12).
 * @return 1 if PA12 is high, 0 otherwise.
 */
uint8_t USBDetect_IsHigh(void);

#endif // _GPIO_H_
