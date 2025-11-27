#include "NuMicro.h"
#include "gsensor.h"

/* Initialize I2C peripheral (I2C0) and any G-sensor related board settings
   This consolidates I2C_Init into the G-sensor module per request. */
void GsensorInit(uint32_t busHz)
{
  /* Enable module clock for I2C0 should be done by SYS_Init in main; just open I2C here */
  I2C_Open(I2C0, busHz);
}

/* Read 3-axis data from MXC400 (6 bytes starting at reg 0x03). Caller provides int16_t axis[3].
   The original code stored 12-bit values left-aligned in 16-bit, so shift accordingly. */
static void ReadGsensorAxis(int16_t *axis)
{
  uint8_t data_reg[6];
  int i;

  I2C_ReadMultiBytesOneReg(I2C0, GSENSOR_ADDR, 0x03, data_reg, 6);

  for (i = 0; i < 3; i++)
  {
    axis[i] = (int16_t)(data_reg[2 * i] << 8 | data_reg[2 * i + 1]) >> 4;
  }
}

void MXC400_to_PD()
{
  I2C_WriteByteOneReg(I2C0, GSENSOR_ADDR, 0x0D, 0x01);
}

void MXC400_to_wakeup()
{
  I2C_WriteByteOneReg(I2C0, GSENSOR_ADDR, 0x0D, 0x00);
}

void GsensorPowerDown()
{
  MXC400_to_PD();
}

void GsensorWakeup()
{
  MXC400_to_wakeup();
}

void GsensorReadAxis(int16_t* axis)
{
  ReadGsensorAxis(axis);
}
