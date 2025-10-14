#include "modbus_crc.h"

#include "NuMicro.h"

#include <stddef.h>

#define MODBUS_CRC_TEST_VECTOR_LENGTH (5U)

static const uint8_t g_modbus_crc_test_vector[MODBUS_CRC_TEST_VECTOR_LENGTH] = {0x01U, 0x04U, 0x02U, 0xFFU, 0xFFU};
static const uint16_t g_modbus_crc_test_expected = 0x80B8U;

static struct
{
    bool hardware_tested;
    bool hardware_available;
    modbus_crc_method_t active_method;
    uint32_t hardware_attr;
} g_modbus_crc_state = {false, false, MODBUS_CRC_METHOD_AUTO, 0U};

static bool modbus_crc_is_locked(void)
{
    return (SYS->REGLCTL == 0U);
}

static void modbus_crc_enable_clock(void)
{
    bool was_locked = modbus_crc_is_locked();
    if (was_locked)
    {
        SYS_UnlockReg();
    }

    CLK_EnableModuleClock(CRC_MODULE);

    if (was_locked)
    {
        SYS_LockReg();
    }
}

static void modbus_crc_hw_configure(uint32_t attr)
{
    CRC_Open(CRC_16, attr, 0xFFFFU, CRC_CPU_WDATA_8);
}

static uint16_t modbus_crc16_compute_software(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;
    for (uint16_t i = 0U; i < length; ++i)
    {
        crc ^= (uint16_t)data[i];
        for (uint8_t bit = 0U; bit < 8U; ++bit)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc = (uint16_t)((crc >> 1U) ^ 0xA001U);
            }
            else
            {
                crc >>= 1U;
            }
        }
    }
    return crc;
}

static uint16_t modbus_crc16_compute_hardware(const uint8_t *data, uint16_t length)
{
    if ((data == NULL) || (length == 0U))
    {
        return 0xFFFFU;
    }

    bool was_locked = modbus_crc_is_locked();
    if (was_locked)
    {
        SYS_UnlockReg();
    }

    modbus_crc_hw_configure(g_modbus_crc_state.hardware_attr);

    if (was_locked)
    {
        SYS_LockReg();
    }

    for (uint16_t i = 0U; i < length; ++i)
    {
        CRC_WRITE_DATA((uint32_t)data[i]);
    }

    return (uint16_t)CRC_GetChecksum();
}

static bool modbus_crc_try_hardware_attributes(void)
{
    modbus_crc_enable_clock();

    for (uint32_t combination = 0U; combination < 16U; ++combination)
    {
        uint32_t attributes = 0U;
        if ((combination & 0x1U) != 0U)
        {
            attributes |= CRC_WDATA_RVS;
        }
        if ((combination & 0x2U) != 0U)
        {
            attributes |= CRC_WDATA_COM;
        }
        if ((combination & 0x4U) != 0U)
        {
            attributes |= CRC_CHECKSUM_RVS;
        }
        if ((combination & 0x8U) != 0U)
        {
            attributes |= CRC_CHECKSUM_COM;
        }

        bool was_locked = modbus_crc_is_locked();
        if (was_locked)
        {
            SYS_UnlockReg();
        }

        modbus_crc_hw_configure(attributes);

        if (was_locked)
        {
            SYS_LockReg();
        }

        for (uint16_t i = 0U; i < MODBUS_CRC_TEST_VECTOR_LENGTH; ++i)
        {
            CRC_WRITE_DATA((uint32_t)g_modbus_crc_test_vector[i]);
        }

        uint16_t checksum = (uint16_t)CRC_GetChecksum();
        if (checksum == g_modbus_crc_test_expected)
        {
            g_modbus_crc_state.hardware_attr = attributes;
            return true;
        }
    }

    return false;
}

static void modbus_crc_init_if_needed(void)
{
    if (g_modbus_crc_state.hardware_tested)
    {
        return;
    }

    g_modbus_crc_state.hardware_available = modbus_crc_try_hardware_attributes();
    g_modbus_crc_state.hardware_tested = true;
    g_modbus_crc_state.active_method = g_modbus_crc_state.hardware_available ? MODBUS_CRC_METHOD_HARDWARE : MODBUS_CRC_METHOD_SOFTWARE;
}

uint16_t modbus_crc16_compute(const uint8_t *data, uint16_t length, modbus_crc_method_t method)
{
    if (data == NULL)
    {
        return 0U;
    }

    modbus_crc_init_if_needed();

    modbus_crc_method_t effective = method;
    if (method == MODBUS_CRC_METHOD_AUTO)
    {
        effective = g_modbus_crc_state.hardware_available ? MODBUS_CRC_METHOD_HARDWARE : MODBUS_CRC_METHOD_SOFTWARE;
    }
    else if ((method == MODBUS_CRC_METHOD_HARDWARE) && !g_modbus_crc_state.hardware_available)
    {
        effective = MODBUS_CRC_METHOD_SOFTWARE;
    }

    g_modbus_crc_state.active_method = effective;

    if (effective == MODBUS_CRC_METHOD_HARDWARE)
    {
        return modbus_crc16_compute_hardware(data, length);
    }

    return modbus_crc16_compute_software(data, length);
}

bool modbus_crc16_validate_frame(const uint8_t *frame, uint16_t length, modbus_crc_method_t method)
{
    if ((frame == NULL) || (length < 3U))
    {
        return false;
    }

    uint16_t payload_length = (uint16_t)(length - 2U);
    const uint8_t *crc_field = &frame[payload_length];
    uint16_t expected = (uint16_t)(crc_field[0] | ((uint16_t)crc_field[1] << 8));
    uint16_t computed = modbus_crc16_compute(frame, payload_length, method);
    return (computed == expected);
}

void modbus_crc16_append(uint8_t *frame, uint16_t length, modbus_crc_method_t method)
{
    if (frame == NULL)
    {
        return;
    }

    uint16_t crc = modbus_crc16_compute(frame, length, method);
    frame[length] = (uint8_t)(crc & 0xFFU);
    frame[(uint16_t)(length + 1U)] = (uint8_t)(crc >> 8U);
}

void modbus_crc_get_status(modbus_crc_status_t *status)
{
    if (status == NULL)
    {
        return;
    }

    modbus_crc_init_if_needed();
    status->hardware_tested = g_modbus_crc_state.hardware_tested;
    status->hardware_available = g_modbus_crc_state.hardware_available;
    status->active_method = g_modbus_crc_state.active_method;
}

bool modbus_crc_run_self_test(void)
{
    uint16_t reference = modbus_crc16_compute_software(g_modbus_crc_test_vector, MODBUS_CRC_TEST_VECTOR_LENGTH);
    if (reference != g_modbus_crc_test_expected)
    {
        return false;
    }

    uint16_t computed = modbus_crc16_compute(g_modbus_crc_test_vector, MODBUS_CRC_TEST_VECTOR_LENGTH, MODBUS_CRC_METHOD_AUTO);
    return (computed == g_modbus_crc_test_expected);
}
