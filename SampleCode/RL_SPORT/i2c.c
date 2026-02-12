#include "NuMicro.h"
#include "i2c.h"
#include "project_config.h"

/* Simple wrapper to initialize I2C0 with requested bus speed. Keeps main.c smaller. */
void I2C_Init(uint32_t u32BusHz)
{
    /* Enable I2C0 module clock handled in SYS_Init (main.c), here open peripheral */
    I2C_Open(I2C0, u32BusHz);
}

/* Debug log gate: only BoardTest should enable this. */
static volatile uint8_t g_rl_i2c_debug_log_enable = 0u;

void RL_I2C_SetDebugLog(uint8_t enable)
{
    g_rl_i2c_debug_log_enable = (enable != 0u) ? 1u : 0u;
}

uint8_t RL_I2C_GetDebugLog(void)
{
    return (uint8_t)g_rl_i2c_debug_log_enable;
}

static const char *RL_I2C_StatusToStr(uint32_t status)
{
    /*
     * I2C status code reference (Master mode, STATUS0):
     * - 0x08: START transmitted
     * - 0x10: Repeated START transmitted
     * - 0x18: SLA+W transmitted, ACK received
     * - 0x20: SLA+W transmitted, NACK received
     * - 0x28: Data transmitted, ACK received
     * - 0x30: Data transmitted, NACK received
     * - 0x38: Arbitration lost
     * - 0x40: SLA+R transmitted, ACK received
     * - 0x48: SLA+R transmitted, NACK received
     * - 0x50: Data received, ACK returned
     * - 0x58: Data received, NACK returned
     * - 0x00: Bus error
     */
    switch (status)
    {
    case 0x00u:
        return "BUS_ERR: Illegal START/STOP";
    case 0x08u:
        return "START sent";
    case 0x10u:
        return "Repeated START sent";
    case 0x18u:
        return "SLA+W ACK";
    case 0x20u:
        return "SLA+W NACK";
    case 0x28u:
        return "DATA_TX ACK";
    case 0x30u:
        return "DATA_TX NACK";
    case 0x38u:
        return "Arbitration lost";
    case 0x40u:
        return "SLA+R ACK";
    case 0x48u:
        return "SLA+R NACK";
    case 0x50u:
        return "DATA_RX ACK";
    case 0x58u:
        return "DATA_RX NACK";
    case 0xF8u:
        /* Common meaning across many I2C controllers: no relevant status, bus idle (SI=0). */
        return "IDLE: No relevant state (SI=0)";
    default:
        return "Unknown";
    }
}

static void RL_I2C_LogIfAbnormal(I2C_T *i2c,
                                 const char *op,
                                 uint8_t u8SlaveAddr,
                                 uint8_t u8DataAddr,
                                 int32_t ret,
                                 uint32_t expected)
{
    if (!RL_I2C_GetDebugLog())
    {
        return;
    }

    /* Treat non-zero error code, timeout flag, or bad return value as abnormal. */
    uint32_t status = (uint32_t)I2C_GET_STATUS(i2c);
    uint32_t to = (uint32_t)I2C_GET_TIMEOUT_FLAG(i2c);

    uint32_t ret_mismatch;
    if (expected == 0u)
    {
        /* For write-style APIs: 0 = success, non-zero = fail */
        ret_mismatch = (ret != 0);
    }
    else
    {
        /* For read-style APIs: return value should equal expected length */
        ret_mismatch = ((ret < 0) || ((uint32_t)ret != expected));
    }

    if ((to != 0u) || ret_mismatch)
    {
        const char *sts_str = RL_I2C_StatusToStr(status);

        DBG_PRINT("[I2C] %s slave=0x%02X reg=0x%02X ret=%ld exp=%lu status=0x%02lX(%s) to=%lu\n",
                  op,
                  (unsigned int)u8SlaveAddr,
                  (unsigned int)u8DataAddr,
                  (long)ret,
                  (unsigned long)expected,
                  (unsigned long)status,
                  sts_str,
                  (unsigned long)to);

        /* If timeout flag is set, clear it to avoid affecting subsequent transfers. */
        if (to)
        {
            I2C_ClearTimeoutFlag(i2c);
        }
    }
}

uint8_t RL_I2C_WriteByteOneReg(I2C_T *i2c, uint8_t u8SlaveAddr, uint8_t u8DataAddr, uint8_t data)
{
    uint8_t ret = I2C_WriteByteOneReg(i2c, u8SlaveAddr, u8DataAddr, data);

    /* StdDriver convention: 0 = success, non-zero = fail */
    RL_I2C_LogIfAbnormal(i2c, "WR1", u8SlaveAddr, u8DataAddr, (int32_t)ret, 0u);
    return ret;
}

uint32_t RL_I2C_ReadMultiBytesOneReg(I2C_T *i2c, uint8_t u8SlaveAddr, uint8_t u8DataAddr, uint8_t rdata[], uint32_t u32rLen)
{
    uint32_t rx = I2C_ReadMultiBytesOneReg(i2c, u8SlaveAddr, u8DataAddr, rdata, u32rLen);

    /* StdDriver convention: returns received length; expect == requested length */
    RL_I2C_LogIfAbnormal(i2c, "RDm", u8SlaveAddr, u8DataAddr, (int32_t)rx, u32rLen);
    return rx;
}
