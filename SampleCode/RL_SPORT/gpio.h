/**
 * @file gpio.h
 * @brief Button and G-sensor GPIO initialization helpers
 *
 * This header exposes board-level GPIO helpers used by `main.c` and
 * other modules. It focuses on configuring buttons, G-sensor pins and
 * power-down wake-up pin behavior.
 */
#ifndef _GPIO_H_
#define _GPIO_H_

#include <stdint.h>

/**
 * @brief Initialize push-buttons and G-sensor GPIOs.
 *
 * Sets up PB7, PB8, PB15 and PC5 as required by the board. Enables
 * interrupts and debounce for key input (keyA on PB15).
 */
void Init_Buttons_Gsensor(void);

/**
 * @brief Configure pins used for SPD power-down (PB7/PB8/PB15/PC5).
 */
void InitSpdPins(void);

/**
 * @brief Configure wake-up pin for Deep Power-Down (DPD).
 * @param edgeType Edge selection for wake-up (platform-specific).
 */
void Gpio_ConfigDPDWakeup(uint32_t edgeType);

/**
 * @brief Configure wake-up pins for SPD mode.
 */
void Gpio_ConfigSPDWakeup(void);

/* GPIO operation macros (module-owned, exposed for others) */
#define GPIO_SET(port, bit)   ((port)->DOUT |= (bit))
#define GPIO_CLR(port, bit)   ((port)->DOUT &= ~(bit))

/**
 * @brief Enable I2C pin Schmitt trigger (board helper).
 */
void EnableI2C_Schmitt(void);

/** Board-level helpers used by startup code in main.c */
void Board_ConfigPCLKDiv(void);
void Board_ConfigMultiFuncPins(void);
void Board_ReleaseIOPD(void);

#endif // _GPIO_H_
