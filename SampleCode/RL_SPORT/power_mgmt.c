/**
 * @file power_mgmt.c
 * @brief Power management module implementation
 */
#include "power_mgmt.h"
#include "NuMicro.h"
#include "project_config.h"
#include "gpio.h"  /* For GPIO helpers */
#include "timer.h" /* For get_ticks if needed */

/* Wake flag masks (from clk_reg.h) for readability */
#ifndef CLK_PMUSTS_GPBWK_Msk
#define CLK_PMUSTS_GPAWK_Msk (1u << 8)
#define CLK_PMUSTS_GPBWK_Msk (1u << 9)
#define CLK_PMUSTS_GPCWK_Msk (1u << 10)
#define CLK_PMUSTS_GPDWK_Msk (1u << 11)
#endif

/* Forward declaration */
static void PowerMgmt_DebugWakeFlags(void);

/* Print which group woke the device (called after reset) */
static void PowerMgmt_DebugWakeFlags(void)
{
    uint32_t sts = CLK->PMUSTS;
    if (sts & CLK_PMUSTS_GPAWK_Msk)
        DBG_PRINT("[Power] Wake flag: GPA\n");
    if (sts & CLK_PMUSTS_GPBWK_Msk)
        DBG_PRINT("[Power] Wake flag: GPB\n");
    if (sts & CLK_PMUSTS_GPCWK_Msk)
        DBG_PRINT("[Power] Wake flag: GPC\n");
    if (sts & CLK_PMUSTS_GPDWK_Msk)
        DBG_PRINT("[Power] Wake flag: GPD\n");
}

/* Handle wake after SPD: release IO hold, clear flags, reconfigure key GPIO */
void PowerMgmt_HandleWake(void)
{
    uint32_t sts = CLK->PMUSTS;
    if (sts & (CLK_PMUSTS_GPAWK_Msk | CLK_PMUSTS_GPBWK_Msk | CLK_PMUSTS_GPCWK_Msk | CLK_PMUSTS_GPDWK_Msk))
    {
        DBG_PRINT("[Power] SPD wake detected (PMUSTS=0x%08lX)\n", (unsigned long)sts);
        /* Release IO hold */
        PowerMgmt_ReleaseIOHold();
        /* Clear all wake flags */
        CLK->PMUSTS |= (1u << 31); /* Set CLRWK bit */
        PowerMgmt_DebugWakeFlags();
        /* Reconfigure PB15 button interrupt (falling edge) */
        GPIO_SetMode(PB, BIT15, GPIO_MODE_INPUT);
        GPIO_EnableInt(PB, 15, GPIO_INT_FALLING);
        GPIO_SET_DEBOUNCE_TIME(GPIO_DBCTL_DBCLKSRC_LIRC, GPIO_DBCTL_DBCLKSEL_512);
        GPIO_ENABLE_DEBOUNCE(PB, BIT15);
        NVIC_EnableIRQ(GPB_IRQn);
    }
}

/* Local helper to wait for UARTs to finish transmission */
static void wait_uart_tx_empty(void)
{
    /* Wait for UART0 and UART1 to finish transmission */
    while (!UART_IS_TX_EMPTY(UART0))
    {
    }
    while (!UART_IS_TX_EMPTY(UART1))
    {
    }
}

void PowerMgmt_ConfigGpioForSPD(void)
{
    /* Set GPIO pins as input to reduce leakage current in SPD mode */
    GPIO_SetMode(PB, BIT3, GPIO_MODE_INPUT);  // GREEN LED PB3
    GPIO_SetMode(PB, BIT7, GPIO_MODE_INPUT);  // JUMP1
    GPIO_SetMode(PB, BIT8, GPIO_MODE_INPUT);  // JUMP2
    GPIO_SetMode(PB, BIT15, GPIO_MODE_INPUT); // KeyA
    GPIO_SetMode(PC, BIT5, GPIO_MODE_INPUT);  // G-sensor INT
    GPIO_SetMode(PC, BIT7, GPIO_MODE_INPUT);  // Buzzer
}

void PowerMgmt_ReleaseIOHold(void)
{
    /* Release I/O hold status (required after SPD wake-up) */
    SYS_UnlockReg();
    CLK->IOPDCTL = 1;
    SYS_LockReg();
}

void PowerMgmt_EnterDPD(WakeupEdge edge)
{
    DBG_PRINT("Enter to DPD Power-Down mode......\n");

    /* Wait for all UART transmissions to complete */
    wait_uart_tx_empty();

    SYS_UnlockReg();

    /* Select Deep Power-Down mode */
    CLK_SetPowerDownMode(CLK_PMUCTL_PDMSEL_DPD);

    /* Configure PC0 as input for DPD wake-up */
    GPIO_SetMode(PC, BIT0, GPIO_MODE_INPUT);

    /* Set wake-up pin trigger type */
    uint32_t edgeType = (edge == PWR_WAKEUP_RISING) ? CLK_DPDWKPIN_RISING : CLK_DPDWKPIN_FALLING;
    CLK_EnableDPDWKPin(edgeType);

    /* Enter to Power-down mode (system will reset on wake-up) */
    CLK_PowerDown();

    /* Wait for reset (should not reach here) */
    while (1)
    {
    }
}

void PowerMgmt_EnterSPD(PowerMode mode)
{
    uint32_t pd_mode;

    /* Map PowerMode enum to CLK_PMUCTL_PDMSEL values */
    switch (mode)
    {
    case PWR_MODE_SPD0:
        pd_mode = CLK_PMUCTL_PDMSEL_SPD0;
        break;
    case PWR_MODE_SPD1:
        pd_mode = CLK_PMUCTL_PDMSEL_SPD1;
        break;
    default:
        pd_mode = CLK_PMUCTL_PDMSEL_SPD0;
        break;
    }

    if ((SYS->CSERVER & SYS_CSERVER_VERSION_Msk) == 0x0)
        DBG_PRINT("Enter to SPD%d Power-Down mode......\n", (pd_mode - 4));
    else
        DBG_PRINT("Enter to SPD Power-Down mode......\n");

    /* Wait for all UART transmissions to complete */
    wait_uart_tx_empty();

    SYS_UnlockReg();

    /* Select Shallow Power-Down mode */
    CLK_SetPowerDownMode(pd_mode);

    /* Configure GPIOs for low power */
    PowerMgmt_ConfigGpioForSPD();

    /* Configure PB15 (KeyA) as SPD wake-up pin - */
    GPIO_SetMode(PB, BIT15, GPIO_MODE_INPUT);

    /* Try enabling PB15 (group 1, pin15) wake (falling edge press) */
    CLK_EnableSPDWKPin(1, 15, CLK_SPDWKPIN_FALLING, CLK_SPDWKPIN_DEBOUNCEDIS);

    /* Enter to Power-down mode (system will reset on wake-up) */
    CLK_PowerDown();

    /* Wait for reset (should not reach here) */
    while (1)
    {
    }
}

uint8_t PowerMgmt_DetectUsbCharge(void)
{
    USBDetect_Init();
    CLK_SysTickDelay(50000); /* 50 ms debounce */
    return USBDetect_IsHigh();
}

void PowerMgmt_RunChargeLoop(void)
{
    /* 在 LDROM 已經將  Power lock 設定為 High 為了可以進入 APROM 正常run code
        為了USB 充電被移除之後可以自動關機，需要將 power lock 關閉，讓 PA11 變為 low */

    PA->DOUT &= ~BIT11; // Clear power lock to allow charging
    while (1)
    {
        /* USB charging auto-boot mode: no power lock, no game */
    }
}
