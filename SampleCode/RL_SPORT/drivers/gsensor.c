#include "NuMicro.h"
#include "gsensor.h"
#include "i2c.h"
#include "../project_config.h"
#include <math.h>

<<<<<<< HEAD
/* Store the configured full-scale range so power mode functions know which
  register value to write when entering/exiting PD. */
static Gsensor_FSR g_current_fsr = FSR_2G;
static volatile uint8_t g_gsensor_ready = 0u;
static volatile uint32_t g_gsensor_read_fail_count = 0u;

static uint8_t Gsensor_Probe(void)
{
    uint8_t temp_reg = 0u;
    uint32_t rx = RL_I2C_ReadMultiBytesOneReg(I2C0, GSENSOR_ADDR, 0x09, &temp_reg, 1u);
    return (rx == 1u) ? 1u : 0u;
}

/* Initialize I2C peripheral (I2C0) and any G-sensor related board settings
   This consolidates I2C_Init into the G-sensor module per request. */
void Gsensor_Init(uint32_t busHz, Gsensor_FSR fsr)
{
    /* Enable module clock for I2C0 should be done by SYS_Init in main; just open I2C here */
    I2C_Open(I2C0, busHz);
    g_current_fsr = fsr;

    g_gsensor_ready = 0u;
    g_gsensor_read_fail_count = 0u;

    for (uint32_t retry = 0u; retry < GSENSOR_INIT_RETRY_COUNT; retry++)
    {
        MXC400_to_wakeup(g_current_fsr);
        if (Gsensor_Probe())
        {
            g_gsensor_ready = 1u;
            break;
        }
    }

    if (!g_gsensor_ready)
=======
#ifndef GSENSOR_MXC400_I2C_ADDR
#define GSENSOR_MXC400_I2C_ADDR 0x15u
#endif

/* SC7U22 register map / constants */
#define SC7U22_REG_WHO_AM_I 0x01u
#define SC7U22_REG_COM_CFG 0x04u
#define SC7U22_REG_DRDY_STATUS 0x0Bu
#define SC7U22_REG_RAW_DATA 0x0Cu
#define SC7U22_REG_AOI_CFG 0x30u
#define SC7U22_REG_AOI_VTH 0x32u
#define SC7U22_REG_AOI_TTH 0x33u
#define SC7U22_REG_ACC_CONF 0x40u
#define SC7U22_REG_ACC_RANGE 0x41u
#define SC7U22_REG_GYR_CONF 0x42u
#define SC7U22_REG_GYR_RANGE 0x43u
#define SC7U22_REG_PWR_CTRL 0x7Du
#define SC7U22_REG_BANK_SEL 0x7Fu

#define SC7U22_WHO_AM_I_VALUE 0x6Au
#define SC7U22_PWR_ENABLE_DEFAULT 0x0Eu
#define SC7U22_COM_CFG_DEFAULT 0x50u
#define SC7U22_DRDY_ACC_GYR_MASK 0x03u
#define SC7U22_ACC_HP_MASK 0x80u
#define SC7U22_GYR_HP_MASK 0x40u
#define SC7U22_ODR_50HZ 0x07u
#define SC7U22_GYR_RANGE_2000DPS 0x00u

/* MXC400 register map */
#define MXC400_REG_AXIS_BASE 0x03u
#define MXC400_REG_TEMP_OUT 0x09u
#define MXC400_REG_CTRL 0x0Du

static Gsensor_DeviceType g_device_type = GSENSOR_DEVICE_NONE;
static uint8_t g_sensor_addr = 0u;
static uint8_t g_last_device_id = 0u;
static Gsensor_FSR g_current_fsr = FSR_2G;
static uint32_t g_i2c_bus_hz = 100000u;

static volatile uint8_t g_gsensor_ready = 0u;
static volatile uint32_t g_gsensor_read_fail_count = 0u;

static void Gsensor_DelayMs(uint32_t ms)
{
    if (ms > 0u)
    {
        CLK_SysTickDelay(ms * 1000u);
    }
}

static uint8_t Gsensor_WriteRegAddr(uint8_t addr, uint8_t reg, uint8_t value)
{
    return (RL_I2C_WriteByteOneReg(I2C0, addr, reg, value) == 0u) ? 1u : 0u;
}

static uint8_t Gsensor_ReadRegAddr(uint8_t addr, uint8_t reg, uint8_t *value)
{
    if (value == NULL)
    {
        return 0u;
    }

    return (RL_I2C_ReadByteOneReg(I2C0, addr, reg, value) == 0u) ? 1u : 0u;
}

static uint8_t Gsensor_ReadRegsAddr(uint8_t addr, uint8_t reg, uint8_t *data, uint32_t len)
{
    if ((data == NULL) || (len == 0u))
    {
        return 0u;
    }

    return (RL_I2C_ReadMultiBytesOneReg(I2C0, addr, reg, data, len) == len) ? 1u : 0u;
}

static uint8_t SC7U22_FsrToRangeReg(Gsensor_FSR fsr)
{
    static const uint8_t s_sc7u22_range_reg[] = {0x00u, 0x01u, 0x02u};
    return (fsr <= FSR_8G) ? s_sc7u22_range_reg[fsr] : s_sc7u22_range_reg[FSR_2G];
}

static uint8_t SC7U22_SelectBank(uint8_t bank)
{
    if (g_sensor_addr == 0u)
    {
        return 0u;
    }

    return Gsensor_WriteRegAddr(g_sensor_addr, SC7U22_REG_BANK_SEL, bank);
}

static uint8_t SC7U22_ReadWhoAmI(uint8_t *device_id)
{
    uint8_t local_id = 0u;

    if (!SC7U22_SelectBank(0x00u))
    {
        return 0u;
    }

    if (!Gsensor_ReadRegAddr(g_sensor_addr, SC7U22_REG_WHO_AM_I, &local_id))
    {
        return 0u;
    }

    g_last_device_id = local_id;
    if (device_id != NULL)
    {
        *device_id = local_id;
    }

    return (local_id == SC7U22_WHO_AM_I_VALUE) ? 1u : 0u;
}

static uint8_t SC7U22_WaitAddressReady(uint8_t addr)
{
    uint32_t elapsed_ms = 0u;
    uint32_t poll_ms = GSENSOR_SC7U22_ACK_POLL_INTERVAL_MS;

    if (poll_ms == 0u)
    {
        poll_ms = 1u;
    }

    while (elapsed_ms <= GSENSOR_SC7U22_ACK_WAIT_TIMEOUT_MS)
    {
        if (RL_I2C_ProbeAddress(I2C0, addr) != 0u)
        {
            return 1u;
        }

        Gsensor_DelayMs(poll_ms);
        elapsed_ms += poll_ms;
    }

    return 0u;
}

static uint8_t SC7U22_PowerDown(void)
{
    uint8_t pwr_ctrl = 0xFFu;

    if (!SC7U22_SelectBank(0x00u))
    {
        return 0u;
    }

    Gsensor_DelayMs(1u);
    if (!Gsensor_WriteRegAddr(g_sensor_addr, SC7U22_REG_PWR_CTRL, 0x00u))
    {
        return 0u;
    }

    Gsensor_DelayMs(20u);
    if (!Gsensor_ReadRegAddr(g_sensor_addr, SC7U22_REG_PWR_CTRL, &pwr_ctrl))
    {
        return 0u;
    }

    return (pwr_ctrl == 0x00u) ? 1u : 0u;
}

static uint8_t SC7U22_ConfigureAtAddress(uint8_t addr, Gsensor_FSR fsr)
{
    uint8_t device_id = 0u;
    uint8_t acc_range_reg = SC7U22_FsrToRangeReg(fsr);
    uint8_t acc_conf = (uint8_t)(SC7U22_ACC_HP_MASK | SC7U22_ODR_50HZ);
    uint8_t gyr_conf = (uint8_t)(SC7U22_GYR_HP_MASK | SC7U22_ODR_50HZ);
    uint8_t verify = 0u;

    g_sensor_addr = addr;

    if (!SC7U22_WaitAddressReady(addr))
    {
        return 0u;
    }

    if (!SC7U22_ReadWhoAmI(&device_id))
    {
        return 0u;
    }

    if (!SC7U22_PowerDown())
    {
        return 0u;
    }

    if (!SC7U22_SelectBank(0x00u))
    {
        return 0u;
    }

    Gsensor_DelayMs(10u);

    if (!Gsensor_WriteRegAddr(g_sensor_addr, SC7U22_REG_PWR_CTRL, SC7U22_PWR_ENABLE_DEFAULT))
    {
        return 0u;
    }
    Gsensor_DelayMs(5u);
    if (!Gsensor_WriteRegAddr(g_sensor_addr, SC7U22_REG_PWR_CTRL, SC7U22_PWR_ENABLE_DEFAULT))
    {
        return 0u;
    }
    Gsensor_DelayMs(10u);

    if (!Gsensor_WriteRegAddr(g_sensor_addr, SC7U22_REG_ACC_CONF, acc_conf))
    {
        return 0u;
    }
    if (!Gsensor_WriteRegAddr(g_sensor_addr, SC7U22_REG_ACC_RANGE, acc_range_reg))
    {
        return 0u;
    }
    if (!Gsensor_WriteRegAddr(g_sensor_addr, SC7U22_REG_GYR_CONF, gyr_conf))
    {
        return 0u;
    }
    if (!Gsensor_WriteRegAddr(g_sensor_addr, SC7U22_REG_GYR_RANGE, SC7U22_GYR_RANGE_2000DPS))
    {
        return 0u;
    }
    if (!Gsensor_WriteRegAddr(g_sensor_addr, SC7U22_REG_GYR_RANGE, SC7U22_GYR_RANGE_2000DPS))
    {
        return 0u;
    }
    if (!Gsensor_WriteRegAddr(g_sensor_addr, SC7U22_REG_COM_CFG, SC7U22_COM_CFG_DEFAULT))
    {
        return 0u;
    }

    if (!Gsensor_WriteRegAddr(g_sensor_addr, SC7U22_REG_AOI_CFG, 0x00u))
    {
        return 0u;
    }
    if (!Gsensor_WriteRegAddr(g_sensor_addr, SC7U22_REG_AOI_VTH, 0xFFu))
    {
        return 0u;
    }
    if (!Gsensor_WriteRegAddr(g_sensor_addr, SC7U22_REG_AOI_TTH, 0xFFu))
    {
        return 0u;
    }

    if (!Gsensor_ReadRegAddr(g_sensor_addr, SC7U22_REG_ACC_RANGE, &verify) || (verify != acc_range_reg))
    {
        return 0u;
    }
    if (!Gsensor_ReadRegAddr(g_sensor_addr, SC7U22_REG_GYR_RANGE, &verify) || (verify != SC7U22_GYR_RANGE_2000DPS))
    {
        return 0u;
    }

    g_last_device_id = device_id;
    return 1u;
}

static uint8_t SC7U22_ReadSixAxisRaw(int16_t *acc_axis, int16_t *gyro_axis)
{
    uint8_t raw_data[12] = {0};
    uint8_t status = 0u;
    uint32_t wait_count = 0u;

    if ((acc_axis == NULL) || (gyro_axis == NULL))
    {
        return 0u;
    }

    if (!SC7U22_SelectBank(0x00u))
    {
        return 0u;
    }

    while (wait_count < 20u)
    {
        if (!Gsensor_ReadRegAddr(g_sensor_addr, SC7U22_REG_DRDY_STATUS, &status))
        {
            return 0u;
        }

        if ((status & SC7U22_DRDY_ACC_GYR_MASK) == SC7U22_DRDY_ACC_GYR_MASK)
        {
            break;
        }

        wait_count++;
        Gsensor_DelayMs(1u);
    }

    if (!Gsensor_ReadRegsAddr(g_sensor_addr, SC7U22_REG_RAW_DATA, raw_data, sizeof(raw_data)))
    {
        return 0u;
    }

    for (uint32_t i = 0u; i < 3u; i++)
    {
        acc_axis[i] = (int16_t)((uint16_t)raw_data[(2u * i)] << 8 | raw_data[(2u * i) + 1u]);
        gyro_axis[i] = (int16_t)((uint16_t)raw_data[(2u * (i + 3u))] << 8 | raw_data[(2u * (i + 3u)) + 1u]);
    }

    return 1u;
}

static const uint8_t s_mxc_fsr_reg_bits[] = {0x00u, 0x20u, 0x40u};

static uint8_t MXC400_FsrToCtrlReg(Gsensor_FSR fsr, uint8_t pd_bit)
{
    uint8_t bits = (fsr <= FSR_8G) ? s_mxc_fsr_reg_bits[fsr] : s_mxc_fsr_reg_bits[FSR_2G];
    return (uint8_t)(bits | pd_bit);
}

static uint8_t MXC400_ProbeAtAddress(uint8_t addr)
{
    uint8_t temp_reg = 0u;
    return (RL_I2C_ReadMultiBytesOneReg(I2C0, addr, MXC400_REG_TEMP_OUT, &temp_reg, 1u) == 1u) ? 1u : 0u;
}

static uint8_t MXC400_ReadAxisRaw(int16_t *axis)
{
    uint8_t data_reg[6] = {0};

    if (axis == NULL)
    {
        return 0u;
    }

    if (RL_I2C_ReadMultiBytesOneReg(I2C0, g_sensor_addr, MXC400_REG_AXIS_BASE, data_reg, sizeof(data_reg)) != sizeof(data_reg))
    {
        return 0u;
    }

    for (uint32_t i = 0u; i < 3u; i++)
    {
        axis[i] = (int16_t)(((uint16_t)data_reg[(2u * i)] << 8) | data_reg[(2u * i) + 1u]) >> 4;
    }

    return 1u;
}

static uint8_t Gsensor_GetSc7PreferredAddress(void)
{
    if ((GSENSOR_SC7U22_I2C_ADDR == 0x18u) || (GSENSOR_SC7U22_I2C_ADDR == 0x19u))
    {
        return GSENSOR_SC7U22_I2C_ADDR;
    }

    return 0x19u;
}

static uint8_t Gsensor_GetSc7AlternateAddress(uint8_t preferred)
{
    return (preferred == 0x18u) ? 0x19u : 0x18u;
}

static uint8_t Gsensor_AutoDetectAndConfigure(void)
{
    uint8_t sc7_preferred_addr = Gsensor_GetSc7PreferredAddress();
    uint8_t sc7_alternate_addr = Gsensor_GetSc7AlternateAddress(sc7_preferred_addr);

    for (uint32_t retry = 0u; retry < GSENSOR_INIT_RETRY_COUNT; retry++)
    {
        if (SC7U22_ConfigureAtAddress(sc7_preferred_addr, g_current_fsr))
        {
            g_device_type = GSENSOR_DEVICE_SC7U22;
            g_gsensor_ready = 1u;
            return 1u;
        }

        if ((sc7_alternate_addr != sc7_preferred_addr) &&
            SC7U22_ConfigureAtAddress(sc7_alternate_addr, g_current_fsr))
        {
            g_device_type = GSENSOR_DEVICE_SC7U22;
            g_gsensor_ready = 1u;
            return 1u;
        }

        g_sensor_addr = GSENSOR_MXC400_I2C_ADDR;
        MXC400_to_wakeup(g_current_fsr);
        if (MXC400_ProbeAtAddress(g_sensor_addr))
        {
            g_device_type = GSENSOR_DEVICE_MXC400;
            g_last_device_id = 0u;
            g_gsensor_ready = 1u;
            return 1u;
        }

        Gsensor_DelayMs(GSENSOR_SC7U22_INIT_RETRY_DELAY_MS);
    }

    g_device_type = GSENSOR_DEVICE_NONE;
    g_sensor_addr = 0u;
    g_last_device_id = 0u;
    g_gsensor_ready = 0u;
    return 0u;
}

static void Gsensor_RequestRecovery(void)
{
    uint32_t interval = GSENSOR_RECOVERY_RETRY_INTERVAL;

    if (interval == 0u)
    {
        interval = 1u;
    }

    g_gsensor_read_fail_count++;
    g_gsensor_ready = 0u;

    if ((g_gsensor_read_fail_count % interval) == 0u)
    {
        GsensorWakeup();
    }
}

Gsensor_DeviceType GsensorGetDeviceType(void)
{
    return g_device_type;
}

const char *GsensorGetDeviceName(void)
{
    switch (g_device_type)
    {
    case GSENSOR_DEVICE_SC7U22:
        return "SC7U22";
    case GSENSOR_DEVICE_MXC400:
        return "MXC400";
    default:
        return "NONE";
    }
}

uint8_t GsensorGetI2CAddress(void)
{
    return g_sensor_addr;
}

void Gsensor_Init(uint32_t busHz, Gsensor_FSR fsr)
{
    g_i2c_bus_hz = (busHz == 0u) ? GSENSOR_I2C_BUS_HZ : busHz;
    g_current_fsr = fsr;

    g_device_type = GSENSOR_DEVICE_NONE;
    g_sensor_addr = 0u;
    g_last_device_id = 0u;
    g_gsensor_ready = 0u;
    g_gsensor_read_fail_count = 0u;

    I2C_Init(g_i2c_bus_hz);

    if (!Gsensor_AutoDetectAndConfigure())
>>>>>>> 增加-6-axis-sensor-SC7U22
    {
        DBG_PRINT("[GSENSOR] Init failed after retries\n");
    }
}

<<<<<<< HEAD
/* Read 3-axis data from MXC400 (6 bytes starting at reg 0x03). Caller provides int16_t axis[3].
   The original code stored 12-bit values left-aligned in 16-bit, so shift accordingly. */
static void ReadGsensorAxis(int16_t *axis)
{
    uint8_t data_reg[6];
    int i;
    /* MXC400xXC Gsensor address
        X_OUT_Low 0x03 ; X_OUT_High 0x04
        Y_OUT_Low 0x05 ; Y_OUT_High 0x06
        Z_OUT_Low 0x07 ; Z_OUT_High 0x08  */

    uint32_t rx = RL_I2C_ReadMultiBytesOneReg(I2C0, GSENSOR_ADDR, 0x03, data_reg, 6);
    if (rx != 6u)
=======
void MXC400_to_PD(Gsensor_FSR fsr)
{
    uint8_t target_addr = (g_device_type == GSENSOR_DEVICE_MXC400 && g_sensor_addr != 0u) ? g_sensor_addr : GSENSOR_MXC400_I2C_ADDR;
    (void)RL_I2C_WriteByteOneReg(I2C0, target_addr, MXC400_REG_CTRL, MXC400_FsrToCtrlReg(fsr, 0x01u));
}

void MXC400_to_wakeup(Gsensor_FSR fsr)
{
    uint8_t target_addr = (g_device_type == GSENSOR_DEVICE_MXC400 && g_sensor_addr != 0u) ? g_sensor_addr : GSENSOR_MXC400_I2C_ADDR;
    (void)RL_I2C_WriteByteOneReg(I2C0, target_addr, MXC400_REG_CTRL, MXC400_FsrToCtrlReg(fsr, 0x00u));
}

void GsensorPowerDown(void)
{
    if (g_device_type == GSENSOR_DEVICE_SC7U22)
    {
        (void)SC7U22_PowerDown();
    }
    else if (g_device_type == GSENSOR_DEVICE_MXC400)
    {
        MXC400_to_PD(g_current_fsr);
    }

    g_gsensor_ready = 0u;
}

void GsensorWakeup(void)
{
    uint8_t wakeup_ok = 0u;

    if ((g_device_type == GSENSOR_DEVICE_SC7U22) && (g_sensor_addr != 0u))
    {
        wakeup_ok = SC7U22_ConfigureAtAddress(g_sensor_addr, g_current_fsr);
    }
    else if ((g_device_type == GSENSOR_DEVICE_MXC400) && (g_sensor_addr != 0u))
    {
        MXC400_to_wakeup(g_current_fsr);
        wakeup_ok = MXC400_ProbeAtAddress(g_sensor_addr);
    }

    if (!wakeup_ok)
    {
        I2C_Init(g_i2c_bus_hz);
        wakeup_ok = Gsensor_AutoDetectAndConfigure();
    }

    g_gsensor_ready = wakeup_ok;
}

void GsensorReadAxis(int16_t *axis)
{
    if (axis == NULL)
    {
        return;
    }

    if (g_device_type == GSENSOR_DEVICE_SC7U22)
    {
        int16_t gyro_axis[3] = {0};
        if (!SC7U22_ReadSixAxisRaw(axis, gyro_axis))
        {
            axis[0] = 0;
            axis[1] = 0;
            axis[2] = 0;
            Gsensor_RequestRecovery();
            return;
        }
    }
    else if (g_device_type == GSENSOR_DEVICE_MXC400)
    {
        if (!MXC400_ReadAxisRaw(axis))
        {
            axis[0] = 0;
            axis[1] = 0;
            axis[2] = 0;
            Gsensor_RequestRecovery();
            return;
        }
    }
    else
>>>>>>> 增加-6-axis-sensor-SC7U22
    {
        axis[0] = 0;
        axis[1] = 0;
        axis[2] = 0;
<<<<<<< HEAD

        g_gsensor_read_fail_count++;
        g_gsensor_ready = 0u;

        if ((g_gsensor_read_fail_count % GSENSOR_RECOVERY_RETRY_INTERVAL) == 0u)
        {
            MXC400_to_wakeup(g_current_fsr);
        }
=======
        Gsensor_RequestRecovery();
>>>>>>> 增加-6-axis-sensor-SC7U22
        return;
    }

    g_gsensor_ready = 1u;
    g_gsensor_read_fail_count = 0u;
<<<<<<< HEAD

    for (i = 0; i < 3; i++)
    {
        axis[i] = (int16_t)(data_reg[2 * i] << 8 | data_reg[2 * i + 1]) >> 4;
    }
}

/* Read Temperature data from MXC400 (1 bytes starting at reg 0x09). */
static void ReadGsensorTemp(int16_t *temp)
{
    uint8_t temp_reg;
    /* MXC400xXC Gsensor address
        TEMP_OUT 0x09 */
    uint32_t rx = RL_I2C_ReadMultiBytesOneReg(I2C0, GSENSOR_ADDR, 0x09, &temp_reg, 1);
    (void)rx;
    *temp = (int16_t)temp_reg;
}

/* MXC400 Control register 0x0D
  bit6:5 full-scale-range  0x00:2G 0x01:4G 0x10:8G
  bit4 : Clksel always 0
  bit1:0 : normal 0x00 , 0x01: power down */

/* FSR enum -> control-register FSR bits lookup */
static const uint8_t s_fsr_reg_bits[] = {0x00, 0x20, 0x40}; /* FSR_2G, FSR_4G, FSR_8G */

static uint8_t fsr_to_ctrl_reg(Gsensor_FSR fsr, uint8_t pd_bit)
{
    uint8_t bits = (fsr <= FSR_8G) ? s_fsr_reg_bits[fsr] : 0x00;
    return bits | pd_bit;
}

void MXC400_to_PD(Gsensor_FSR fsr)
{
    uint8_t wr = RL_I2C_WriteByteOneReg(I2C0, GSENSOR_ADDR, 0x0D, fsr_to_ctrl_reg(fsr, 0x01));
    (void)wr;
}

void MXC400_to_wakeup(Gsensor_FSR fsr)
{
    uint8_t wr = RL_I2C_WriteByteOneReg(I2C0, GSENSOR_ADDR, 0x0D, fsr_to_ctrl_reg(fsr, 0x00));
    (void)wr;
}

void GsensorPowerDown()
{
    MXC400_to_PD(g_current_fsr);
    g_gsensor_ready = 0u;
}

void GsensorWakeup()
{
    MXC400_to_wakeup(g_current_fsr);
    g_gsensor_ready = Gsensor_Probe();
}

void GsensorReadAxis(int16_t *axis)
{
    ReadGsensorAxis(axis);
}

/* Counts-per-g lookup indexed by Gsensor_FSR.
   MXC4005XC: 12-bit two's complement (-2048..+2047), so ±Ng maps to ±2048 counts.
   FSR_2G: 2048/2 = 1024 counts/g, FSR_4G: 512, FSR_8G: 256. */
static const float s_fsr_cpg[] = {1024.0f, 512.0f, 256.0f}; /* FSR_2G, FSR_4G, FSR_8G */

float Gsensor_CalcMagnitude_g_from_raw(int16_t *axis)
{
    float cpg = (g_current_fsr <= FSR_8G) ? s_fsr_cpg[g_current_fsr] : 1024.0f;
    float xg = (float)axis[0] / cpg;
    float yg = (float)axis[1] / cpg;
    float zg = (float)axis[2] / cpg;
=======
}

uint8_t GsensorReadSixAxis(int16_t *acc_axis, int16_t *gyro_axis)
{
    if ((acc_axis == NULL) || (gyro_axis == NULL))
    {
        return 0u;
    }

    if (g_device_type == GSENSOR_DEVICE_SC7U22)
    {
        if (!SC7U22_ReadSixAxisRaw(acc_axis, gyro_axis))
        {
            acc_axis[0] = 0;
            acc_axis[1] = 0;
            acc_axis[2] = 0;
            gyro_axis[0] = 0;
            gyro_axis[1] = 0;
            gyro_axis[2] = 0;
            Gsensor_RequestRecovery();
            return 0u;
        }
    }
    else if (g_device_type == GSENSOR_DEVICE_MXC400)
    {
        if (!MXC400_ReadAxisRaw(acc_axis))
        {
            acc_axis[0] = 0;
            acc_axis[1] = 0;
            acc_axis[2] = 0;
            gyro_axis[0] = 0;
            gyro_axis[1] = 0;
            gyro_axis[2] = 0;
            Gsensor_RequestRecovery();
            return 0u;
        }

        gyro_axis[0] = 0;
        gyro_axis[1] = 0;
        gyro_axis[2] = 0;
    }
    else
    {
        acc_axis[0] = 0;
        acc_axis[1] = 0;
        acc_axis[2] = 0;
        gyro_axis[0] = 0;
        gyro_axis[1] = 0;
        gyro_axis[2] = 0;
        Gsensor_RequestRecovery();
        return 0u;
    }

    g_gsensor_ready = 1u;
    g_gsensor_read_fail_count = 0u;
    return 1u;
}

uint8_t GsensorReadDeviceId(uint8_t *device_id)
{
    if (device_id == NULL)
    {
        return 0u;
    }

    if (g_device_type == GSENSOR_DEVICE_SC7U22)
    {
        if (!SC7U22_ReadWhoAmI(device_id))
        {
            *device_id = g_last_device_id;
            return 0u;
        }

        return 1u;
    }

    if (g_device_type == GSENSOR_DEVICE_MXC400)
    {
        *device_id = 0u;
        g_last_device_id = 0u;
        return MXC400_ProbeAtAddress(g_sensor_addr);
    }

    *device_id = 0u;
    return 0u;
}

static const float s_mxc_cpg[] = {1024.0f, 512.0f, 256.0f};       /* FSR_2G, FSR_4G, FSR_8G */
static const float s_sc7u22_cpg[] = {16384.0f, 8192.0f, 4096.0f}; /* FSR_2G, FSR_4G, FSR_8G */

float Gsensor_CalcMagnitude_g_from_raw(int16_t *axis)
{
    const float *cpg_table = s_mxc_cpg;
    float cpg = 1024.0f;
    float xg;
    float yg;
    float zg;

    if (axis == NULL)
    {
        return 0.0f;
    }

    if (g_device_type == GSENSOR_DEVICE_SC7U22)
    {
        cpg_table = s_sc7u22_cpg;
    }

    if (g_current_fsr <= FSR_8G)
    {
        cpg = cpg_table[g_current_fsr];
    }

    xg = (float)axis[0] / cpg;
    yg = (float)axis[1] / cpg;
    zg = (float)axis[2] / cpg;
>>>>>>> 增加-6-axis-sensor-SC7U22

    return sqrtf(xg * xg + yg * yg + zg * zg);
}
