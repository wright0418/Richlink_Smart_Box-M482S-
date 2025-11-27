#include "NuMicro.h"
#include "i2c.h"

/* Simple wrapper to initialize I2C0 with requested bus speed. Keeps main.c smaller. */
void I2C_Init(uint32_t u32BusHz)
{
    /* Enable I2C0 module clock handled in SYS_Init (main.c), here open peripheral */
    I2C_Open(I2C0, u32BusHz);
}
