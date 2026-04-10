#include "NuMicro.h"
#include "adc.h"
#include "project_config.h"

/* Measured AVDD (V), updated by Adc_UpdateVdda(). Initialised to nominal 3.3V. */
static float s_vdda = 3.3f;

void Adc_UpdateVdda(void)
{
    uint32_t timeout = ADC_CONV_TIMEOUT;
    int32_t raw_bg = 0;

    /* Band-gap source needs extra settling time; use maximum extend window */
    EADC_SetExtendSampleTime(EADC, 16, 0x3F);
    EADC_START_CONV(EADC, BIT16);

    while (timeout--)
    {
        if (EADC_GET_DATA_VALID_FLAG(EADC, BIT16))
        {
            raw_bg = EADC_GET_CONV_DATA(EADC, 16);
            break;
        }
    }

    if (raw_bg > 0)
    {
        /* AVDD_actual = VBG_nominal * 4095 / raw_bg */
        s_vdda = (ADC_VBG_NOMINAL * ADC_FULL_SCALE) / (float)raw_bg;
    }
    /* else: keep previous cached value */
}

float Adc_GetVdda(void)
{
    return s_vdda;
}

uint8_t Adc_IsVddaLow(void)
{
    return (s_vdda < ADC_VDDA_LOW_V) ? 1u : 0u;
}

void Adc_Init(void)
{
    /* Only need EADC open for band-gap (sample module 16, fixed channel) */
    EADC_Open(EADC, EADC_CTL_DIFFEN_SINGLE_END);

    /* Prime the AVDD cache with an initial measurement */
    Adc_UpdateVdda();
}
