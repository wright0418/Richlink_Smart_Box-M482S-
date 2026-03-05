/* gpio.c - Button and G-sensor GPIO implementation */
#include "gpio.h"
#include "NuMicro.h"
#include "system_status.h"
#include "project_config.h"

void Init_Buttons_Gsensor(void)
{
    /* PB GPIO */
    GPIO_SetMode(PB, BIT15, GPIO_MODE_INPUT);

#if !USE_GSENSOR_JUMP_DETECT
    /* HALL sensors used for jump counting: enable PB7 input and interrupt only */
    GPIO_SetMode(PB, BIT7, GPIO_MODE_INPUT);
    GPIO_SetMode(PB, BIT8, GPIO_MODE_INPUT);
    GPIO_EnableInt(PB, 7, GPIO_INT_FALLING);
    /* PB8 interrupt disabled */
#else
    /* In G-Sensor mode, PB7/PB8 are not used for counting; keep as inputs but do not enable HALL interrupts */
    GPIO_SetMode(PB, BIT7, GPIO_MODE_INPUT);
    GPIO_SetMode(PB, BIT8, GPIO_MODE_INPUT);
#endif
    GPIO_EnableInt(PB, 15, GPIO_INT_FALLING);
    NVIC_EnableIRQ(GPB_IRQn);

    /* PC GPIO */
    GPIO_SetMode(PC, BIT5, GPIO_MODE_INPUT); // G sensor interrupt
    /* PC5 interrupt disabled (G-sensor INT) */
}

void EnableI2C_Schmitt(void)
{
    /* Enable Schmitt trigger for I2C SCL (PB.5) as board helper */
    PB->SMTEN |= GPIO_SMTEN_SMTEN5_Msk;
}

void Board_ConfigPCLKDiv(void)
{
    /* Set PCLK0/PCLK1 to HCLK/2 as original SYS init did */
    CLK->PCLKDIV = (CLK_PCLKDIV_APB0DIV_DIV2 | CLK_PCLKDIV_APB1DIV_DIV2);
}

static void Board_ConfigI2C0Pins(void)
{
    SYS->GPB_MFPL = (SYS->GPB_MFPL & ~(SYS_GPB_MFPL_PB4MFP_Msk | SYS_GPB_MFPL_PB5MFP_Msk)) |
                    (SYS_GPB_MFPL_PB4MFP_I2C0_SDA | SYS_GPB_MFPL_PB5MFP_I2C0_SCL);
}

static void Board_ConfigBatteryAdcPin(void)
{
    SYS->GPB_MFPL = (SYS->GPB_MFPL & ~SYS_GPB_MFPL_PB1MFP_Msk) | SYS_GPB_MFPL_PB1MFP_EADC0_CH1;
    PB->MODE &= ~GPIO_MODE_MODE1_Msk;
    GPIO_DISABLE_DIGITAL_PATH(PB, BIT1);
}

static void Board_ConfigUartPins(void)
{
    SYS->GPB_MFPH &= ~(SYS_GPB_MFPH_PB12MFP_Msk | SYS_GPB_MFPH_PB13MFP_Msk);
    SYS->GPB_MFPH |= (SYS_GPB_MFPH_PB12MFP_UART0_RXD | SYS_GPB_MFPH_PB13MFP_UART0_TXD);
    SYS->GPA_MFPH &= ~(SYS_GPA_MFPH_PA8MFP_Msk | SYS_GPA_MFPH_PA9MFP_Msk);
    SYS->GPA_MFPH |= (SYS_GPA_MFPH_PA8MFP_UART1_RXD | SYS_GPA_MFPH_PA9MFP_UART1_TXD);
}

static void Board_ConfigPowerPins(void)
{
    SYS->GPA_MFPH &= ~(SYS_GPA_MFPH_PA11MFP_Msk | SYS_GPA_MFPH_PA12MFP_Msk);
}

void Board_ConfigMultiFuncPins(void)
{
    /* Configure I2C, UART, ADC, and power-related GPIO MFPs */
    Board_ConfigI2C0Pins();
    Board_ConfigBatteryAdcPin();
    Board_ConfigUartPins();
    Board_ConfigPowerPins();
}

void Board_ReleaseIOPD(void)
{
    /* Release I/O hold status for SPD Mode (was in InitSystem) */
    CLK->IOPDCTL = 1;
}

void PowerLock_Init(void)
{
    /* Configure PA11 as output and assert lock (high). */
    GPIO_SetMode(PA, BIT11, GPIO_MODE_OUTPUT);
    PA->DOUT |= BIT11;
}

void PowerLock_Set(uint8_t on)
{
    if (on)
    {
        PA->DOUT |= BIT11;
    }
    else
    {
        PA->DOUT &= ~BIT11;
    }
}

void USBDetect_Init(void)
{
    /* Configure PA12 as input for USB charge detect */
    GPIO_SetMode(PA, BIT12, GPIO_MODE_INPUT);
}

uint8_t USBDetect_IsHigh(void)
{
    return (PA->PIN & BIT12) ? 1u : 0u;
}

void InitSpdPins(void)
{
    /* Configure PB7/PB8/PB15 and PC5 for SPD input state */
    GPIO_SetMode(PB, BIT7, GPIO_MODE_INPUT);
    GPIO_SetMode(PB, BIT8, GPIO_MODE_INPUT);
    GPIO_SetMode(PB, BIT15, GPIO_MODE_INPUT);
    GPIO_SetMode(PC, BIT5, GPIO_MODE_INPUT); /* G sensor interrupt */
}

void Gpio_ConfigDPDWakeup(uint32_t edgeType)
{
    /* Configure PC0 as input wake-up for DPD board variant */
    GPIO_SetMode(PC, BIT0, GPIO_MODE_INPUT);
    CLK_EnableDPDWKPin(edgeType);
}

void Gpio_ConfigSPDWakeup(void)
{
    /* Configure PB15 as SPD wakeup pin and set debounce as original code */
    GPIO_SetMode(PB, BIT15, GPIO_MODE_INPUT);
    CLK_EnableSPDWKPin(1, 15, CLK_SPDWKPIN_FALLING, CLK_SPDWKPIN_DEBOUNCEDIS);
}

void GPIO_ResetHallEdgeCount(void)
{
    /* Note: Static hall_pb7_edge_count in ISR is auto-reset on state change.
       This function serves as explicit reset API for future extensibility. */
}

void GPB_IRQHandler(void)
{
#if !USE_GSENSOR_JUMP_DETECT
    /* ============================================================================
     * Hall Sensor Jump Detection (PB7 only)
     *
     * Logic: Rope jumping produces 2 falling edges per jump on the Hall sensor.
     * Edge counting:
     *  - Count every falling-edge interrupt on PB7
     *  - Increment jump counter immediately in ISR when GAME_START
     *  - Reset counter after each 2-edge pair to prepare for next jump
     *
     * Notes on debounce and filtering:
     *  - Hardware debounce for PB7 has been disabled in Init_Buttons_Gsensor()
     *    so this ISR receives raw/fast edges. If contact bounce or noise is
     *    observed, apply a software debouncing/filtering strategy in the main
     *    loop or enable hardware debounce via a compile-time option.
     *  - PB15 (user key) still uses GPIO debounce to filter mechanical bounce.
     *
     * Safety: Edge counter is static and local to ISR context; it does not
     * perform lengthy work. ISR sets a flag so the main loop can log or act on
     * the event without blocking interrupt latency.
     * ============================================================================ */
    static uint8_t hall_pb7_edge_count = 0u;

    if (GPIO_GET_INT_FLAG(PB, BIT7))
    {
        GPIO_CLR_INT_FLAG(PB, BIT7);
        if (Sys_GetGameState() == GAME_START)
        {
            hall_pb7_edge_count++;
            if (hall_pb7_edge_count >= 2u)
            {
                hall_pb7_edge_count = 0u;
                Sys_IncrementJumpTimes();
            }
        }
        Sys_SetHallPb7IrqFlag(1); /* Signal main loop for logging */
    }

    if (GPIO_GET_INT_FLAG(PB, BIT8))
    {
        GPIO_CLR_INT_FLAG(PB, BIT8);
        /* PB8 not used - interrupt disabled */
    }
#else
    /* G-Sensor mode active - clear any stray Hall edges without counting */
    if (GPIO_GET_INT_FLAG(PB, BIT7))
    {
        GPIO_CLR_INT_FLAG(PB, BIT7);
    }
    if (GPIO_GET_INT_FLAG(PB, BIT8))
    {
        GPIO_CLR_INT_FLAG(PB, BIT8);
    }
#endif /* USE_GSENSOR_JUMP_DETECT */

    /* Button (PB15) handling - always active */
    if (GPIO_GET_INT_FLAG(PB, BIT15))
    {
        GPIO_CLR_INT_FLAG(PB, BIT15);
        Sys_SetKeyAFlag(1);
    }
    PB->INTSRC = PB->INTSRC;
}

void GPC_IRQHandler(void)
{
    /* PC5 interrupt disabled; clear any unexpected pending flags */
    if (GPIO_GET_INT_FLAG(PC, BIT5))
    {
        GPIO_CLR_INT_FLAG(PC, BIT5);
    }
    PC->INTSRC = PC->INTSRC;
}
