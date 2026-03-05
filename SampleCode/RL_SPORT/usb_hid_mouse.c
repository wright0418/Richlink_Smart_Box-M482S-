/**************************************************************************/ /**
                                                                              * @file     usb_hid_mouse.c
                                                                              * @brief    USB HID mouse test implementation for RL_SPORT
                                                                              ******************************************************************************/
#include <string.h>
#include "NuMicro.h"
#include "project_config.h"
#include "timer.h"
#include "usb_hid_mouse.h"
#include "usb_hid_mouse_internal.h"

static uint8_t g_usb_hid_active = 0u;

static signed char mouse_table[] = {-16, -16, -16, 0, 16, 16, 16, 0};
static uint8_t mouse_idx = 0;
static uint8_t move_len = 0;
static uint8_t mouse_mode = 1;

static volatile uint8_t g_u8EP2Ready = 0u;

static void UsbHidMouse_ConfigClock(void)
{
    /* USB requires a 48 MHz clock. Derive it from current PLL frequency. */
    uint32_t u32PllFreq = CLK_GetPLLClockFreq();
    uint32_t div = u32PllFreq / 48000000UL;
    if (div < 1UL)
    {
        div = 1UL;
    }
    if (div > 16UL)
    {
        div = 16UL;
    }

    /* USB clock source = PLL, divider = actual_PLL / 48 MHz */
    CLK->CLKSEL0 |= CLK_CLKSEL0_USBSEL_Msk;
    CLK->CLKDIV0 = (CLK->CLKDIV0 & ~CLK_CLKDIV0_USBDIV_Msk) | CLK_CLKDIV0_USB(div);

    /* Enable USBD module clock */
    CLK_EnableModuleClock(USBD_MODULE);

    /* Enable USB PHY in device mode */
    SYS->USBPHY = (SYS->USBPHY & ~SYS_USBPHY_USBROLE_Msk) | SYS_USBPHY_USBEN_Msk | SYS_USBPHY_SBO_Msk;

    printf("[USB] PLL=%luHz, USB div=%lu -> USB clk=%luHz\n",
           (unsigned long)u32PllFreq, (unsigned long)div,
           (unsigned long)(u32PllFreq / div));
}

static void UsbHidMouse_ConfigPins(void)
{
    /* PA12~PA15: USB VBUS/D-/D+/OTG_ID */
    PA->MODE &= ~(GPIO_MODE_MODE12_Msk | GPIO_MODE_MODE13_Msk | GPIO_MODE_MODE14_Msk | GPIO_MODE_MODE15_Msk);
    SYS->GPA_MFPH &= ~(SYS_GPA_MFPH_PA12MFP_Msk | SYS_GPA_MFPH_PA13MFP_Msk |
                       SYS_GPA_MFPH_PA14MFP_Msk | SYS_GPA_MFPH_PA15MFP_Msk);
    SYS->GPA_MFPH |= (SYS_GPA_MFPH_PA12MFP_USB_VBUS | SYS_GPA_MFPH_PA13MFP_USB_D_N |
                      SYS_GPA_MFPH_PA14MFP_USB_D_P | SYS_GPA_MFPH_PA15MFP_USB_OTG_ID);
}

void UsbHidMouse_TestStart(void)
{
    if (g_usb_hid_active)
    {
        return;
    }

    /* Protected register access needed for clock/PHY configuration */
    SYS_UnlockReg();

    UsbHidMouse_ConfigClock();
    UsbHidMouse_ConfigPins();

    SYS_ResetModule(USBD_RST);

    /* Follow official Nuvoton sequence: Open → Init → Start → NVIC */
    USBD_Open(&gsInfo, HID_ClassRequest, NULL);
    HID_Init();
    USBD_Start();

    NVIC_EnableIRQ(USBD_IRQn);

    SYS_LockReg();

    printf("[USB] ATTR=0x%03lX INTEN=0x%03lX SE0=0x%lX\n",
           (unsigned long)(USBD->ATTR),
           (unsigned long)(USBD->INTEN),
           (unsigned long)(USBD->SE0));

    g_usb_hid_active = 1u;
}

void UsbHidMouse_TestStop(void)
{
    if (!g_usb_hid_active)
    {
        return;
    }

    NVIC_DisableIRQ(USBD_IRQn);
    USBD_DISABLE_USB();
    USBD_DISABLE_PHY();
    CLK_DisableModuleClock(USBD_MODULE);
    g_usb_hid_active = 0u;
}

void UsbHidMouse_TestUpdate(void)
{
    if (!g_usb_hid_active)
    {
        return;
    }
    HID_UpdateMouseData();
}

uint8_t UsbHidMouse_TestIsActive(void)
{
    return g_usb_hid_active;
}

void USBD_IRQHandler(void)
{
    uint32_t volatile u32IntSts = USBD_GET_INT_FLAG();
    uint32_t volatile u32State = USBD_GET_BUS_STATE();

    if (u32IntSts & USBD_INTSTS_VBDETIF_Msk)
    {
        USBD_CLR_INT_FLAG(USBD_INTSTS_VBDETIF_Msk);

        if (USBD_IS_ATTACHED())
        {
            USBD_ENABLE_USB();
        }
        else
        {
            USBD_DISABLE_USB();
        }
    }

    if (u32IntSts & USBD_INTSTS_BUSIF_Msk)
    {
        USBD_CLR_INT_FLAG(USBD_INTSTS_BUSIF_Msk);

        if (u32State & USBD_ATTR_USBRST_Msk)
        {
            USBD_ENABLE_USB();
            USBD_SwReset();
        }
        if (u32State & USBD_ATTR_SUSPEND_Msk)
        {
            USBD_DISABLE_PHY();
        }
        if (u32State & USBD_ATTR_RESUME_Msk)
        {
            USBD_ENABLE_USB();
            USBD_ENABLE_PHY();
        }
    }

    if (u32IntSts & USBD_INTSTS_WAKEUP)
    {
        USBD_CLR_INT_FLAG(USBD_INTSTS_WAKEUP);
    }

    if (u32IntSts & USBD_INTSTS_USBIF_Msk)
    {
        if (u32IntSts & USBD_INTSTS_SETUP_Msk)
        {
            USBD_CLR_INT_FLAG(USBD_INTSTS_SETUP_Msk);
            USBD_STOP_TRANSACTION(EP0);
            USBD_STOP_TRANSACTION(EP1);
            USBD_ProcessSetupPacket();
        }

        if (u32IntSts & USBD_INTSTS_EP0)
        {
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP0);
            USBD_CtrlIn();
        }

        if (u32IntSts & USBD_INTSTS_EP1)
        {
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP1);
            USBD_CtrlOut();
        }

        if (u32IntSts & USBD_INTSTS_EP2)
        {
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP2);
            g_u8EP2Ready = 1u;
        }

        if (u32IntSts & USBD_INTSTS_EP3)
        {
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP3);
        }
        if (u32IntSts & USBD_INTSTS_EP4)
        {
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP4);
        }
        if (u32IntSts & USBD_INTSTS_EP5)
        {
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP5);
        }
        if (u32IntSts & USBD_INTSTS_EP6)
        {
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP6);
        }
        if (u32IntSts & USBD_INTSTS_EP7)
        {
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP7);
        }
    }
}

void HID_Init(void)
{
    USBD->STBUFSEG = SETUP_BUF_BASE;

    USBD_CONFIG_EP(EP0, USBD_CFG_CSTALL | USBD_CFG_EPMODE_IN | 0);
    USBD_SET_EP_BUF_ADDR(EP0, EP0_BUF_BASE);

    USBD_CONFIG_EP(EP1, USBD_CFG_CSTALL | USBD_CFG_EPMODE_OUT | 0);
    USBD_SET_EP_BUF_ADDR(EP1, EP1_BUF_BASE);

    USBD_CONFIG_EP(EP2, USBD_CFG_EPMODE_IN | INT_IN_EP_NUM);
    USBD_SET_EP_BUF_ADDR(EP2, EP2_BUF_BASE);

    g_u8EP2Ready = 1u;
}

void HID_ClassRequest(void)
{
    uint8_t buf[8];

    USBD_GetSetupPacket(buf);

    if (buf[0] & 0x80)
    {
        switch (buf[1])
        {
        case GET_REPORT:
        case GET_IDLE:
        case GET_PROTOCOL:
        default:
            USBD_SetStall(0);
            break;
        }
    }
    else
    {
        switch (buf[1])
        {
        case SET_REPORT:
            if (buf[3] == HID_RPT_TYPE_FEATURE)
            {
                USBD_SET_DATA1(EP1);
                USBD_SET_PAYLOAD_LEN(EP1, 0);
            }
            break;
        case SET_IDLE:
            USBD_SET_DATA1(EP0);
            USBD_SET_PAYLOAD_LEN(EP0, 0);
            break;
        case SET_PROTOCOL:
        default:
            USBD_SetStall(0);
            break;
        }
    }
}

void HID_UpdateMouseData(void)
{
    uint8_t *buf;

    if (!g_u8EP2Ready)
    {
        return;
    }

    buf = (uint8_t *)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP2));
    mouse_mode ^= 1u;

    if (mouse_mode)
    {
        if (move_len > 6u)
        {
            buf[0] = 0x00;
            buf[1] = (uint8_t)mouse_table[mouse_idx & 0x07u];
            buf[2] = (uint8_t)mouse_table[(mouse_idx + 2u) & 0x07u];
            buf[3] = 0x00;
            mouse_idx++;
            move_len = 0u;
        }
        g_u8EP2Ready = 0u;
        USBD_SET_PAYLOAD_LEN(EP2, 4);
    }
    move_len++;
}
