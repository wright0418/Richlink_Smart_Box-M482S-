#include "NuMicro.h"
#include "gsensor.h"
#include "i2c.h"
#include "project_config.h"
#include <math.h>

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
  {
    DBG_PRINT("[GSENSOR] Init failed after retries\n");
  }
}

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
  {
    axis[0] = 0;
    axis[1] = 0;
    axis[2] = 0;

    g_gsensor_read_fail_count++;
    g_gsensor_ready = 0u;

    if ((g_gsensor_read_fail_count % GSENSOR_RECOVERY_RETRY_INTERVAL) == 0u)
    {
      MXC400_to_wakeup(g_current_fsr);
    }
    return;
  }

  g_gsensor_ready = 1u;
  g_gsensor_read_fail_count = 0u;

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
   Empirically verified on MXC4005XC hardware: flat board Z=2047 at 1g indicates
   2048 counts/g for FSR_2G (not the theoretical 1024 counts/g derived from range). */
static const float s_fsr_cpg[] = {2048.0f, 1024.0f, 512.0f}; /* FSR_2G, FSR_4G, FSR_8G */

float Gsensor_CalcMagnitude_g_from_raw(int16_t *axis)
{
  float cpg = (g_current_fsr <= FSR_8G) ? s_fsr_cpg[g_current_fsr] : 2048.0f;
  float xg = (float)axis[0] / cpg;
  float yg = (float)axis[1] / cpg;
  float zg = (float)axis[2] / cpg;

  return sqrtf(xg * xg + yg * yg + zg * zg);
}
