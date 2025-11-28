#include "NuMicro.h"
#include "gsensor.h"
#include "i2c.h"
#include <math.h>

/* Store the configured full-scale range so power mode functions know which
  register value to write when entering/exiting PD. */
static Gsensor_FSR g_current_fsr = FSR_2G;

/* Initialize I2C peripheral (I2C0) and any G-sensor related board settings
   This consolidates I2C_Init into the G-sensor module per request. */
void GsensorInit(uint32_t busHz, Gsensor_FSR fsr)
{
  /* Enable module clock for I2C0 should be done by SYS_Init in main; just open I2C here */
  I2C_Open(I2C0, busHz);
  g_current_fsr = fsr;
  MXC400_to_wakeup(g_current_fsr);
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

  I2C_ReadMultiBytesOneReg(I2C0, GSENSOR_ADDR, 0x03, data_reg, 6);

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
  I2C_ReadMultiBytesOneReg(I2C0, GSENSOR_ADDR, 0x09, &temp_reg, 1);

  *temp = (int16_t)temp_reg;
}

/* MXC400 Controll address 0x0D
  bit6:5 full-scale-range  0x00:2G 0x01:4G 0x10:8G
  bit4 : Clksel always 0
  bit1:0 : normal 0x00 , 0x01: power down
 */
void MXC400_to_PD(Gsensor_FSR fsr)
{
  uint8_t reg_value = 0x01; // Power down
  switch (fsr)
  {
  case FSR_2G:
    reg_value |= 0x00 << 5;
    break;
  case FSR_4G:
    reg_value |= 0x01 << 5;
    break;
  case FSR_8G:
    reg_value |= 0x02 << 5;
    break;
  default:
    reg_value |= 0x00 << 5;
    break;
  }
  I2C_WriteByteOneReg(I2C0, GSENSOR_ADDR, 0x0D, reg_value);
}

/* MXC400 set full-scale range 2G/4G/8G  address 0x0D
 */

void MXC400_to_wakeup(Gsensor_FSR fsr)
{
  uint8_t reg_value = 0x00; // Wakeup
  switch (fsr)
  {
  case FSR_2G:
    reg_value |= 0x00 << 5;
    break;
  case FSR_4G:
    reg_value |= 0x01 << 5;
    break;
  case FSR_8G:
    reg_value |= 0x02 << 5;
    break;
  default:
    reg_value |= 0x00 << 5;
    break;
  }
  I2C_WriteByteOneReg(I2C0, GSENSOR_ADDR, 0x0D, reg_value);
}

void GsensorPowerDown()
{
  MXC400_to_PD(g_current_fsr);
}

void GsensorWakeup()
{
  MXC400_to_wakeup(g_current_fsr);
}

void GsensorReadAxis(int16_t *axis)
{
  ReadGsensorAxis(axis);
}

/* Helper: convert raw axis counts to magnitude in g using current FSR */
float Gsensor_CalcMagnitude_g_from_raw(int16_t *axis)
{
  float counts_per_g;
  switch (g_current_fsr)
  {
  case FSR_2G:
    counts_per_g = 1024.0f; /* 1024 counts per g at 2G full-scale */
    break;
  case FSR_4G:
    counts_per_g = 512.0f; /* approximate */
    break;
  case FSR_8G:
    counts_per_g = 256.0f; /* approximate */
    break;
  default:
    counts_per_g = 1024.0f;
    break;
  }

  float xg = ((float)axis[0]) / counts_per_g;
  float yg = ((float)axis[1]) / counts_per_g;
  float zg = ((float)axis[2]) / counts_per_g;

  return sqrtf(xg * xg + yg * yg + zg * zg);
}
