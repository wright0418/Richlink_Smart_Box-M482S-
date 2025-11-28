#include "powerdown.h"
#include "project_config.h"
#include "led.h"
#include "gsensor.h"
#include "ble.h"
#include "power_mgmt.h"
#include "system_status.h"
#include "timer.h"
#include "buzzer.h"

#define BEEP_FREQ_HZ 4000
#define BEEP_DURATION_MS 100
#define BEEP_GAP_MS 500
#define BEEP_REPEAT_COUNT 3
#define WAIT_BLE_DISCONNECT_MS 1000

void PowerDown_PerformSequence(uint8_t ble_connected)
{
    /* Turn off LED */
    SetGreenLedMode(1, 0);
    DBG_PRINT("Enter to Power-Down (no movement) ......\n");

    /* Put G-sensor to power-down */
    GsensorPowerDown();

    if (ble_connected)
    {
        BLE_DISCONNECT();
        /* Wait up to WAIT_BLE_DISCONNECT_MS for BLE to disconnect */
        uint32_t wait_start = get_ticks_ms();
        while (!is_timeout(wait_start, WAIT_BLE_DISCONNECT_MS))
        {
            if (Sys_GetBleState() == BLE_DISCONNECTED)
                break;
            delay_ms(50);
        }
    }

    BLE_to_DLPS();
    delay_ms(100);

    /* Play notification beeps */
    for (int i = 0; i < BEEP_REPEAT_COUNT; i++)
    {
        BuzzerPlay(BEEP_FREQ_HZ, BEEP_DURATION_MS);
        delay_ms(BEEP_GAP_MS);
    }

    /* Enter power-down mode (DPD or SPD based on board config) */
#ifdef DPD_PC0
    PowerMgmt_EnterDPD(PWR_WAKEUP_RISING);
#else
    PowerMgmt_EnterSPD(PWR_MODE_SPD0);
#endif

    /* Should not reach here */
    while (1)
    {
    }
}
