#include "NuMicro.h"
#include "i2c.h"
#include "../project_config.h"

#include <stddef.h>

#if I2C_DIAG_LOG_ENABLE
#include <stdio.h>
#define I2C_DIAG_PRINT(fmt, ...) printf("[I2C] " fmt, ##__VA_ARGS__)
#else
#define I2C_DIAG_PRINT(fmt, ...)
#endif

static uint32_t g_rl_i2c_bus_hz = 100000u;
static uint32_t g_i2c_diag_log_count = 0u;

void I2C_Init(uint32_t u32BusHz)
{
    g_rl_i2c_bus_hz = (u32BusHz == 0u) ? 100000u : u32BusHz;
    I2C_Open(I2C0, g_rl_i2c_bus_hz);
}

static uint32_t RL_I2C_GetReadyTimeoutCount(void)
{
    uint32_t core_hz = SystemCoreClock;
    uint32_t count;

    if (core_hz < 1000000u)
    {
        core_hz = 1000000u;
    }

    count = core_hz / 1000u;
    if (count < 1000u)
    {
        count = 1000u;
    }

    return count;
}

static void RL_I2C_WaitStopDone(I2C_T *i2c)
{
    uint32_t timeout_count = RL_I2C_GetReadyTimeoutCount();

    while ((i2c->CTL0 & I2C_CTL0_STO_Msk) != 0u)
    {
        if (timeout_count-- == 0u)
        {
            g_I2C_i32ErrCode = I2C_TIMEOUT_ERR;
            break;
        }
    }
}

static void RL_I2C_LogFailure(const char *op,
                              I2C_T *i2c,
                              uint8_t slave,
                              uint8_t reg,
                              uint32_t status,
                              uint32_t attempt)
{
    if ((g_i2c_diag_log_count < 40u) || ((g_i2c_diag_log_count % 100u) == 0u))
    {
        I2C_DIAG_PRINT("op=%s a=%lu sla=0x%02X reg=0x%02X st=0x%02lX err=%ld ctl0=0x%08lX\n",
                       op,
                       (unsigned long)attempt,
                       (unsigned)slave,
                       (unsigned)reg,
                       (unsigned long)status,
                       (long)g_I2C_i32ErrCode,
                       (unsigned long)i2c->CTL0);
    }

    g_i2c_diag_log_count++;
}

static void RL_I2C_RecoverBus(I2C_T *i2c)
{
    if (i2c == I2C0)
    {
        SYS_ResetModule(I2C0_RST);
    }
#if defined(I2C1) && defined(I2C1_RST)
    else if (i2c == I2C1)
    {
        SYS_ResetModule(I2C1_RST);
    }
#endif
#if defined(I2C2) && defined(I2C2_RST)
    else if (i2c == I2C2)
    {
        SYS_ResetModule(I2C2_RST);
    }
#endif

    I2C_Open(i2c, g_rl_i2c_bus_hz);

    if (I2C_GET_TIMEOUT_FLAG(i2c) != 0u)
    {
        I2C_ClearTimeoutFlag(i2c);
    }
}

uint8_t RL_I2C_WriteByteOneReg(I2C_T *i2c, uint8_t u8SlaveAddr, uint8_t u8DataAddr, uint8_t data)
{
    for (uint32_t attempt = 0u; attempt < I2C_XFER_RETRY_COUNT; attempt++)
    {
        uint8_t xfering = 1u;
        uint8_t err = 0u;
        uint8_t ctrl = 0u;
        uint8_t wrote_data = 0u;
        uint32_t last_status = 0xFFFFFFFFu;

        g_I2C_i32ErrCode = 0;
        I2C_START(i2c);

        while ((xfering != 0u) && (err == 0u))
        {
            uint32_t timeout_count = RL_I2C_GetReadyTimeoutCount();
            I2C_WAIT_READY(i2c)
            {
                if (timeout_count-- == 0u)
                {
                    g_I2C_i32ErrCode = I2C_TIMEOUT_ERR;
                    err = 1u;
                    I2C_SET_CONTROL_REG(i2c, I2C_CTL_STO_SI);
                    break;
                }
            }

            if (err != 0u)
            {
                break;
            }

            last_status = (uint32_t)I2C_GET_STATUS(i2c);
            switch (last_status)
            {
            case 0x08u:
                I2C_SET_DATA(i2c, (uint8_t)(u8SlaveAddr << 1u));
                ctrl = I2C_CTL_SI;
                break;

            case 0x18u:
                I2C_SET_DATA(i2c, u8DataAddr);
                ctrl = I2C_CTL_SI;
                break;

            case 0x28u:
                if (wrote_data == 0u)
                {
                    I2C_SET_DATA(i2c, data);
                    wrote_data = 1u;
                    ctrl = I2C_CTL_SI;
                }
                else
                {
                    ctrl = I2C_CTL_STO_SI;
                    xfering = 0u;
                }
                break;

            case 0x20u:
            case 0x30u:
            case 0x38u:
            default:
                ctrl = I2C_CTL_STO_SI;
                err = 1u;
                break;
            }

            I2C_SET_CONTROL_REG(i2c, ctrl);
        }

        if (I2C_GET_TIMEOUT_FLAG(i2c) != 0u)
        {
            I2C_ClearTimeoutFlag(i2c);
        }

        RL_I2C_WaitStopDone(i2c);

        if ((err == 0u) && (xfering == 0u))
        {
            return 0u;
        }

        RL_I2C_LogFailure("W1", i2c, u8SlaveAddr, u8DataAddr, last_status, attempt + 1u);
        RL_I2C_RecoverBus(i2c);
    }

    return 1u;
}

uint8_t RL_I2C_ProbeAddress(I2C_T *i2c, uint8_t u8SlaveAddr)
{
    for (uint32_t attempt = 0u; attempt < I2C_XFER_RETRY_COUNT; attempt++)
    {
        uint8_t ack = 0u;
        uint8_t done = 0u;
        uint8_t err = 0u;
        uint8_t ctrl = 0u;
        uint32_t last_status = 0xFFFFFFFFu;

        I2C_START(i2c);
        while ((done == 0u) && (err == 0u))
        {
            uint32_t timeout_count = RL_I2C_GetReadyTimeoutCount();
            I2C_WAIT_READY(i2c)
            {
                if (timeout_count-- == 0u)
                {
                    g_I2C_i32ErrCode = I2C_TIMEOUT_ERR;
                    err = 1u;
                    break;
                }
            }

            if (err != 0u)
            {
                I2C_SET_CONTROL_REG(i2c, I2C_CTL_STO_SI);
                break;
            }

            last_status = (uint32_t)I2C_GET_STATUS(i2c);
            switch (last_status)
            {
            case 0x08u:
                I2C_SET_DATA(i2c, (uint8_t)(u8SlaveAddr << 1u));
                ctrl = I2C_CTL_SI;
                break;

            case 0x18u:
                ack = 1u;
                done = 1u;
                ctrl = I2C_CTL_STO_SI;
                break;

            case 0x20u:
                done = 1u;
                ctrl = I2C_CTL_STO_SI;
                break;

            case 0x38u:
            default:
                err = 1u;
                ctrl = I2C_CTL_STO_SI;
                break;
            }

            I2C_SET_CONTROL_REG(i2c, ctrl);
        }

        if (I2C_GET_TIMEOUT_FLAG(i2c) != 0u)
        {
            I2C_ClearTimeoutFlag(i2c);
        }

        RL_I2C_WaitStopDone(i2c);

        if ((err == 0u) && (ack != 0u))
        {
            return 1u;
        }

        RL_I2C_LogFailure("PRB", i2c, u8SlaveAddr, 0xFFu, last_status, attempt + 1u);
        RL_I2C_RecoverBus(i2c);
    }

    return 0u;
}

uint8_t RL_I2C_ReadByteOneReg(I2C_T *i2c, uint8_t u8SlaveAddr, uint8_t u8DataAddr, uint8_t *data)
{
    uint8_t value = 0u;
    uint8_t ret = 1u;

    if (data == NULL)
    {
        return 1u;
    }

    for (uint32_t attempt = 0u; attempt < I2C_XFER_RETRY_COUNT; attempt++)
    {
        uint8_t xfering = 1u;
        uint8_t err = 0u;
        uint8_t ctrl = 0u;
        uint32_t rx_len = 0u;
        uint32_t last_status = 0xFFFFFFFFu;

        g_I2C_i32ErrCode = 0;
        I2C_START(i2c);

        while ((xfering != 0u) && (err == 0u))
        {
            uint32_t timeout_count = RL_I2C_GetReadyTimeoutCount();
            I2C_WAIT_READY(i2c)
            {
                if (timeout_count-- == 0u)
                {
                    g_I2C_i32ErrCode = I2C_TIMEOUT_ERR;
                    err = 1u;
                    break;
                }
            }

            if (err != 0u)
            {
                I2C_SET_CONTROL_REG(i2c, I2C_CTL_STO_SI);
                break;
            }

            last_status = (uint32_t)I2C_GET_STATUS(i2c);
            switch (last_status)
            {
            case 0x08u:
                I2C_SET_DATA(i2c, (uint8_t)(u8SlaveAddr << 1u));
                ctrl = I2C_CTL_SI;
                break;

            case 0x18u:
                I2C_SET_DATA(i2c, u8DataAddr);
                ctrl = I2C_CTL_SI;
                break;

            case 0x28u:
                ctrl = I2C_CTL_STA_SI;
                break;

            case 0x10u:
                I2C_SET_DATA(i2c, (uint8_t)((u8SlaveAddr << 1u) | 0x01u));
                ctrl = I2C_CTL_SI;
                break;

            case 0x40u:
                ctrl = I2C_CTL_SI;
                break;

            case 0x58u:
                value = (uint8_t)I2C_GET_DATA(i2c);
                rx_len = 1u;
                ctrl = I2C_CTL_STO_SI;
                xfering = 0u;
                break;

            case 0x20u:
            case 0x30u:
            case 0x48u:
            case 0x38u:
            default:
                ctrl = I2C_CTL_STO_SI;
                err = 1u;
                break;
            }

            I2C_SET_CONTROL_REG(i2c, ctrl);
        }

        if ((err == 0u) && (xfering == 0u) && (rx_len == 1u))
        {
            *data = value;
            ret = 0u;
            RL_I2C_WaitStopDone(i2c);
            break;
        }

        ret = 1u;
        if (I2C_GET_TIMEOUT_FLAG(i2c) != 0u)
        {
            I2C_ClearTimeoutFlag(i2c);
        }

        RL_I2C_WaitStopDone(i2c);
        RL_I2C_LogFailure("R1", i2c, u8SlaveAddr, u8DataAddr, last_status, attempt + 1u);
        RL_I2C_RecoverBus(i2c);
    }

    return ret;
}

uint32_t RL_I2C_ReadMultiBytesOneReg(I2C_T *i2c, uint8_t u8SlaveAddr, uint8_t u8DataAddr, uint8_t rdata[], uint32_t u32rLen)
{
    if ((rdata == NULL) || (u32rLen == 0u))
    {
        return 0u;
    }

    for (uint32_t attempt = 0u; attempt < I2C_XFER_RETRY_COUNT; attempt++)
    {
        uint8_t xfering = 1u;
        uint8_t err = 0u;
        uint8_t ctrl = 0u;
        uint32_t rx_len = 0u;
        uint32_t last_status = 0xFFFFFFFFu;

        g_I2C_i32ErrCode = 0;
        I2C_START(i2c);

        while ((xfering != 0u) && (err == 0u))
        {
            uint32_t timeout_count = RL_I2C_GetReadyTimeoutCount();
            I2C_WAIT_READY(i2c)
            {
                if (timeout_count-- == 0u)
                {
                    g_I2C_i32ErrCode = I2C_TIMEOUT_ERR;
                    err = 1u;
                    I2C_SET_CONTROL_REG(i2c, I2C_CTL_STO_SI);
                    break;
                }
            }

            if (err != 0u)
            {
                break;
            }

            last_status = (uint32_t)I2C_GET_STATUS(i2c);
            switch (last_status)
            {
            case 0x08u:
                I2C_SET_DATA(i2c, (uint8_t)(u8SlaveAddr << 1u));
                ctrl = I2C_CTL_SI;
                break;

            case 0x18u:
                I2C_SET_DATA(i2c, u8DataAddr);
                ctrl = I2C_CTL_SI;
                break;

            case 0x28u:
                ctrl = I2C_CTL_STA_SI;
                break;

            case 0x10u:
                I2C_SET_DATA(i2c, (uint8_t)((u8SlaveAddr << 1u) | 0x01u));
                ctrl = I2C_CTL_SI;
                break;

            case 0x40u:
                ctrl = (u32rLen > 1u) ? I2C_CTL_SI_AA : I2C_CTL_SI;
                break;

            case 0x50u:
                rdata[rx_len++] = (uint8_t)I2C_GET_DATA(i2c);
                if (rx_len < (u32rLen - 1u))
                {
                    ctrl = I2C_CTL_SI_AA;
                }
                else
                {
                    ctrl = I2C_CTL_SI;
                }
                break;

            case 0x58u:
                rdata[rx_len++] = (uint8_t)I2C_GET_DATA(i2c);
                ctrl = I2C_CTL_STO_SI;
                xfering = 0u;
                break;

            case 0x20u:
            case 0x30u:
            case 0x48u:
            case 0x38u:
            default:
                ctrl = I2C_CTL_STO_SI;
                err = 1u;
                break;
            }

            I2C_SET_CONTROL_REG(i2c, ctrl);
        }

        if (I2C_GET_TIMEOUT_FLAG(i2c) != 0u)
        {
            I2C_ClearTimeoutFlag(i2c);
        }

        RL_I2C_WaitStopDone(i2c);

        if ((err == 0u) && (xfering == 0u) && (rx_len == u32rLen))
        {
            return rx_len;
        }

        RL_I2C_LogFailure("RM", i2c, u8SlaveAddr, u8DataAddr, last_status, attempt + 1u);
        RL_I2C_RecoverBus(i2c);
    }

    return 0u;
}