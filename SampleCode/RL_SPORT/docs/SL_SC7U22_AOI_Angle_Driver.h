/**************************************************
Copyright (c) 2022 Silan MEMS. All Rights Reserved.
@Silan MEMS Sensor Product Line
@Code Author:Zhou Min
**************************************************/

#ifndef __SL_SC7U22_AOI_Angle_DRIVER_H__
#define __SL_SC7U22_AOI_Angle_DRIVER_H__


/********添加IIC头文件******************/
//#include "i2c.h"
/***************************************/

//传感器调试打印功能是否打开
#define SL_Sensor_Algo_Release_Enable  0x01
//是否打开FIFO工作模式，默认STREAM模式
#define SL_SC7U22_FIFO_ENABLE          0x00


/***使用驱动前请根据实际接线情况配置******/
/**SC7U22的SDO 脚接地：  0****************/
/**SC7U22的SDO 脚接电源：1****************/
#define SL_SC7U22_SDO_VDD_GND            1
/*****************************************/
/***使用驱动前请根据实际IIC情况进行配置***/
/**SC7U22的IIC 接口地址类型 7bits：  0****/
/**SC7U22的IIC 接口地址类型 8bits：  1****/
#define SL_SC7U22_IIC_7BITS_8BITS        0
/*****************************************/
#if SL_SC7U22_SDO_VDD_GND==0
#define SL_SC7U22_IIC_7BITS_ADDR        0x18
#define SL_SC7U22_IIC_8BITS_WRITE_ADDR  0x30
#define SL_SC7U22_IIC_8BITS_READ_ADDR   0x31
#else
#define SL_SC7U22_IIC_7BITS_ADDR        0x19
#define SL_SC7U22_IIC_8BITS_WRITE_ADDR  0x32
#define SL_SC7U22_IIC_8BITS_READ_ADDR   0x33
#endif
#if SL_SC7U22_IIC_7BITS_8BITS==0
#define SL_SC7U22_IIC_ADDRESS        SL_SC7U22_IIC_7BITS_ADDR
#else
#define SL_SC7U22_IIC_WRITE_ADDRESS  SL_SC7U22_IIC_8BITS_WRITE_ADDR
#define SL_SC7U22_IIC_READ_ADDRESS   SL_SC7U22_IIC_8BITS_READ_ADDR
#endif

/********客户需要定义IIC/SPI接口封包函数****************/
extern unsigned char SL_SC7U22_I2c_Spi_Write(unsigned char sl_spi_iic, unsigned char reg, unsigned char dat);
extern unsigned char SL_SC7U22_I2c_Spi_Read(unsigned char sl_spi_iic, unsigned char reg, unsigned short len, unsigned char* buf);
/**SL_SC7U22_I2c_Spi_Write 函数中， sl_spi_iic:0=spi  1=i2c  Reg：寄存器地址   dat：寄存器的配置值******************/
/**SL_SC7U22_I2c_Spi_Write 函数 是一个单次写的函数******************************************************************/
/***SL_SC7U22_I2c_Spi_Read 函数中， sl_spi_iic:0=spi  1=i2c Reg 同上，len:读取数据长度，buf:存储数据首地址（指针）***/
/***SL_SC7U22_I2c_Spi_Read 函数 是可以进行单次读或多次连续读取的函数************************************************/

/*
unsigned char SL_SC7U22_I2c_Spi_Write(unsigned char sl_spi_iic, unsigned char reg, unsigned char dat)
{
	if(sl_spi_iic==1)
		i2cWrite(0x19,reg,dat);
	else
		SPI_SENSOR_WriteReg(reg,dat);
	 return 1;
}
unsigned char SL_SC7U22_I2c_Spi_Read(unsigned char sl_spi_iic, unsigned char reg, unsigned short len, unsigned char* buf)
{
	if(sl_spi_iic==1)
		i2cRead(0x19,reg,len,buf);
	else
		SPI_SENSOR_ReadBuf(reg|0x80,buf,len);//SC7A20E SC7A20H
	 return 1;
}
*/

/*************I2C通讯检查函数******************/
unsigned char SL_SC7U22_Check(void);
/*************返回数据情况如下*****************/
/**return : 1   IIC通讯正常IC在线**************/
/**return : 0   IIC通讯异常或IC不在线**********/

/*************驱动初始化函数*******************/
unsigned char SL_SC7U22_Config(void);
/*************返回数据情况如下*****************/
/**return : 1    IIC通讯正常IC在线*************/
/**return : 0;   IIC通讯异常或IC不在线*********/

/*************SC7U22 Sensor Time**************/
unsigned int SL_SC7U22_TimeStamp_Read(void);
/*************返回数据情况如下*****************/
/**return : Internal Sensor Time***************/

#if SL_SC7U22_FIFO_ENABLE ==0x00
/******定时读取数据寄存器的数据，相当于是从400Hz的FIFO数据中抽取数据******/
void SL_SC7U22_RawData_Read(signed short* acc_data_buf, signed short* gyr_data_buf);
/************* 输入XYZ三轴数据变量的地址*****************/
/************* *acc_data_buf:    ACC轴数据***********************/
/************* *gyr_data_buf:    GYR轴数据***********************/

#else
/******定时读取数据寄存器的FIFO数据******/
unsigned short SL_SC7U22_FIFO_Read(signed short* accx_buf, signed short* accy_buf, signed short* accz_buf, signed short* gyrx_buf, signed short* gyry_buf, signed short* gyrz_buf);
/*************输入XYZ三轴数组首地址**************************/
/*************accx_buf[0]:    ACC_X轴第一个数据**************/
/*************accy_buf[0]:    ACC_Y轴第一个数据**************/
/*************accz_buf[0]:    ACC_Z轴第一个数据**************/
/*************gyrx_buf[0]:    GYR_X轴第一个数据**************/
/*************gyry_buf[0]:    GYR_Y轴第一个数据**************/
/*************gyrz_buf[0]:    GYR_Z轴第一个数据**************/
/****************返回数据情况如下****************************/
/**return : len         表示数组长度*************************/
#endif

/*********进入传感器关闭模式*************/
unsigned char SL_SC7U22_POWER_DOWN(void);
/**0: 进入模式失败***********************/
/**1: 进入模式成功***********************/

/*********SC7U22 RESET***************/
unsigned char SL_SC7U22_SOFT_RESET(void);
/**0: OK*****************************/
/**1: ERROR**************************/

/*************GSensor and GyroSensor打开和关闭函数*********/
unsigned char SL_SC7U22_Open_Close_SET(unsigned char acc_enable,unsigned char gyro_enable);
/**acc_enable:  0=关闭ACC Sensor; 1=打开ACC Sensor*********/
/**gyro_enable: 0=关闭GYRO Sensor; 1=打开GYRO Sensor*******/
/**return: 0=操作失败；1=操作成功**************************/

/*********进入休眠模式，开中断函数*************/
unsigned char SL_SC7U22_IN_SLEEP_SET(unsigned char acc_odr,unsigned char vth,unsigned char tth,unsigned char int_io);
/**acc_odr:  12/25/50**************************************/
/**vth:  运动检测，幅度阈值设置****************************/
/**vth:  运动检测，满足幅度阈值后最小持续时长设置**********/
/**int_io:  1=INT1；2=INT2*********************************/
/**return: 0=操作失败；1=操作成功**************************/

/*********进入工作模式，配置传感器，关闭中断函数***********/
unsigned char SL_SC7U22_WakeUp_SET(unsigned char odr_mode,unsigned char acc_range,unsigned char acc_hp_en,unsigned short gyro_range,unsigned char gyro_hp_en);
/**odr_mode:  25HZ/50Hz/100Hz/200Hz ACC+GYRO***************/
/**acc_range: ±2G/±4G/±8G/±16G*****************************/
/**acc_hp_en: 0=disable high performance mode;1=enable*****/
/**gyro_range: ±125dps/±250dps/±500dps/±1000dps/±2000dps***/
/**gyro_hp_en: 0=disable hp mode;1=enable hp mode; ********/
/**return: 0=操作失败；1=操作成功**************************/

/*********SC7U22 Angle Cauculate***************/
unsigned char SL_SC7U22_Angle_Output(unsigned char calibration_en,signed short *acc_gyro_input,float *Angle_output, unsigned char yaw_rst);
/**in calibration_en: 1=enable 0=disable***********************/
/**in/out acc_gyro_input[0]: ACC-X*****************************/
/**in/out acc_gyro_input[1]: ACC-Y*****************************/
/**in/out acc_gyro_input[2]: ACC-Z*****************************/
/**in/out acc_gyro_input[3]: GYR-X*****************************/
/**in/out acc_gyro_input[4]: GYR-Y*****************************/
/**in/out acc_gyro_input[5]: GYR-Z*****************************/
/**output Angle_output[0]:   Pitch*****************************/
/**output Angle_output[1]:   Roll******************************/
/**output Angle_output[2]:   Yaw*******************************/
/**input  yaw_rst:   reset yaw value***************************/


/**reg map*******************************/
#define SC7U22_WHO_AM_I         0x01


#endif // SC7U22

