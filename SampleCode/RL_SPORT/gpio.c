/* gpio.c - Button and G-sensor GPIO implementation */
#include "gpio.h"
#include "NuMicro.h"
#include "system_status.h"

void Init_Buttons_Gsensor(void)
{
    /* PB GPIO */
    GPIO_SetMode(PB, BIT7, GPIO_MODE_INPUT);
    GPIO_SetMode(PB, BIT8, GPIO_MODE_INPUT);
    GPIO_SetMode(PB, BIT15, GPIO_MODE_INPUT);
    GPIO_EnableInt(PB, 7, GPIO_INT_FALLING);
    GPIO_EnableInt(PB, 15, GPIO_INT_FALLING);

    GPIO_SET_DEBOUNCE_TIME(GPIO_DBCTL_DBCLKSRC_LIRC, GPIO_DBCTL_DBCLKSEL_512);
    GPIO_ENABLE_DEBOUNCE(PB, BIT15);
    NVIC_EnableIRQ(GPB_IRQn);

    /* PC GPIO */
    GPIO_SetMode(PC, BIT5, GPIO_MODE_INPUT); // G sensor interrupt
    GPIO_EnableInt(PC, 5, GPIO_INT_FALLING);
    NVIC_EnableIRQ(GPC_IRQn);
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

void Board_ConfigMultiFuncPins(void)
{
    /* Configure I2C and UART multi-function pins (previously in SYS_Init) */
    SYS->GPB_MFPL = (SYS->GPB_MFPL & ~(SYS_GPB_MFPL_PB4MFP_Msk | SYS_GPB_MFPL_PB5MFP_Msk)) |
                    (SYS_GPB_MFPL_PB4MFP_I2C0_SDA | SYS_GPB_MFPL_PB5MFP_I2C0_SCL);

    SYS->GPB_MFPH &= ~(SYS_GPB_MFPH_PB12MFP_Msk | SYS_GPB_MFPH_PB13MFP_Msk);
    SYS->GPB_MFPH |= (SYS_GPB_MFPH_PB12MFP_UART0_RXD | SYS_GPB_MFPH_PB13MFP_UART0_TXD);
    SYS->GPA_MFPH &= ~(SYS_GPA_MFPH_PA8MFP_Msk | SYS_GPA_MFPH_PA9MFP_Msk);
    SYS->GPA_MFPH |= (SYS_GPA_MFPH_PA8MFP_UART1_RXD | SYS_GPA_MFPH_PA9MFP_UART1_TXD);
}

void Board_ReleaseIOPD(void)
{
    /* Release I/O hold status for SPD Mode (was in InitSystem) */
    CLK->IOPDCTL = 1;
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

void GPB_IRQHandler(void)
{
    // Set flags only, handle in main loop
    if (GPIO_GET_INT_FLAG(PB, BIT7))
    {
        GPIO_CLR_INT_FLAG(PB, BIT7);
        if (Sys_GetGameState() == GAME_START)
        {
            Sys_IncrementJumpTimes();
        }
        Sys_SetJumpFlag(1);
    }
    if (GPIO_GET_INT_FLAG(PB, BIT8))
    {
        GPIO_CLR_INT_FLAG(PB, BIT8);
        // 可加 g_jump_flag2 = 1; 若有需要
    }
    if (GPIO_GET_INT_FLAG(PB, BIT15))
    {
        GPIO_CLR_INT_FLAG(PB, BIT15);
        Sys_SetKeyAFlag(1);
    }
    PB->INTSRC = PB->INTSRC;
}

void GPC_IRQHandler(void)
{
    // Set flag only, handle in main loop
    if (GPIO_GET_INT_FLAG(PC, BIT5))
    {
        GPIO_CLR_INT_FLAG(PC, BIT5);
        g_sys.gsensor_flag = 1;
    }
    PC->INTSRC = PC->INTSRC;
}
