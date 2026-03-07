#include "NuMicro.h"
#include "adc.h"
#include "project_config.h"

void Adc_InitBattery(void)
{
    /* Configure EADC in single-end mode and sample module 0 on channel 1 (PB1) */
    EADC_Open(EADC, EADC_CTL_DIFFEN_SINGLE_END);
    EADC_ConfigSampleModule(EADC, 0, EADC_SOFTWARE_TRIGGER, 1);
    EADC_CLR_INT_FLAG(EADC, EADC_STATUS2_ADIF0_Msk);
}

uint16_t Adc_ReadBatteryRaw(void)
{
    uint32_t timeout = ADC_CONV_TIMEOUT;

    EADC_START_CONV(EADC, BIT0);
    while (timeout--)
    {
        if (EADC_GET_DATA_VALID_FLAG(EADC, BIT0))
        {
            return (uint16_t)EADC_GET_CONV_DATA(EADC, 0);
        }
    }

    return 0;
}

uint16_t Adc_ReadBatteryRawAvg(uint8_t samples)
{
    if (samples == 0)
    {
        samples = 1;
    }

    uint32_t sum = 0;
    for (uint8_t i = 0; i < samples; ++i)
    {
        sum += Adc_ReadBatteryRaw();
    }

    return (uint16_t)(sum / samples);
}

float Adc_ConvertRawToBatteryV(uint16_t raw)
{
    float v_adc = ((float)raw / ADC_FULL_SCALE) * ADC_VREF_V;
    return v_adc / ADC_DIVIDER_RATIO;
}

uint8_t Adc_IsBatteryLow(uint16_t raw)
{
    float vbat = Adc_ConvertRawToBatteryV(raw);
    return (vbat <= ADC_BATT_LOW_V) ? 1u : 0u;
}
