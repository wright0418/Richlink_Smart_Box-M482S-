
#include "SL_SC7U22_AOI_Angle_Driver.h"
#include "math.h"
#include "delay.h"
#include "usart.h"

#if SL_Sensor_Algo_Release_Enable==0x00
#include "usart.h"
#include "led.h"
#endif


//I2C SPI选择
//#define SL_SC7U22_SPI_EN_I2C_DISABLE   0x00 //必须和SL_SPI_IIC_INTERFACE相反；
#define SL_SPI_IIC_INTERFACE           0x01 //必须和SL_SC7A22H_SPI_EN_I2C_DISABLE 相反；
//原始数据高通输出使能控制宏定义
#define SL_SC7U22_RAWDATA_HPF_ENABLE   0x00
//中断脚默认输出电平控制宏定义
#define SL_SC7U22_INT_DEFAULT_LEVEL    0x01
//SDO  上拉电阻控制
#define SL_SC7U22_SDO_PullUP_ENABLE    0x01
//AOI唤醒中断检测使能 
#define SL_SC7U22_AOI_Wake_Up_ENABLE   0x00

//FIFO_STREAM使能//FIFO_WTM使能
//#define SL_SC7U22_FIFO_STREAM_WTM  	   0x01//0X00=STREAM MODE 0X01=FIFO MODE

static void sl_delay(unsigned char sl_i)
{
    unsigned int sl_j = 10;

	sl_j = sl_j*1000*sl_i;
    while(sl_j--);
}


unsigned char SL_SC7U22_Check(void)
{
	unsigned char reg_value=0;
	
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, SC7U22_WHO_AM_I, 1, &reg_value);
#if SL_Sensor_Algo_Release_Enable==0x00
	USART_printf( USART1, "0x%x=0x%x\r\n",SC7U22_WHO_AM_I,reg_value);
#endif
	if(reg_value==0x6A)
		return 0x01;//SC7U22
	else
		return 0x00;//其他芯片
}

unsigned char SL_SC7U22_Config(void)
{
	unsigned char Check_Flag=0;
	unsigned char reg_value=0;
	
#if SL_SPI_IIC_INTERFACE==0x00 //SPI
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x90
	sl_delay(1);
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x4A, 0x66);
	sl_delay(1);
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7F, 0x83);//goto 0x6F
	sl_delay(1);
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x6F, 0x04);//I2C disable
	sl_delay(1);
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x6F
	sl_delay(1);
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x4A, 0x00);
	sl_delay(1);
#endif
	Check_Flag=SL_SC7U22_Check();
//	Check_Flag= SL_SC7U22_SOFT_RESET();
//	Check_Flag=1;//强制初始化
#if SL_Sensor_Algo_Release_Enable==0x00
	PWM_PB4(0);
	USART_printf( USART1, "SL_SC7U22_Check=0x%x\r\n",Check_Flag);
#endif	
	if(Check_Flag==1)
	{
		Check_Flag= SL_SC7U22_POWER_DOWN();
	}
#if SL_Sensor_Algo_Release_Enable==0x00
	USART_printf( USART1, "SL_SC7U22_POWER_DOWN=0x%x\r\n",Check_Flag);
#endif
	if(Check_Flag==1)
	{
		Check_Flag= SL_SC7U22_SOFT_RESET();
	}
#if SL_Sensor_Algo_Release_Enable==0x00
	USART_printf( USART1, "SL_SC7U22_SOFT_RESET=0x%x\r\n",Check_Flag);
#endif
	if(Check_Flag==1)
	{
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
		delay_nms(10);//10ms		
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7D, 0x0E);//PWR_CTRL ENABLE ACC+GYR+TEMP
		delay_nms(10);//10ms
	
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x40, 0x06);//ACC_CONF 0x07=50Hz
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x41, 0x01);//ACC_RANGE  ±8G
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x42, 0x86);//GYR_CONF 0x87=50Hz
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x43, 0x00);//GYR_RANGE 2000dps
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x43, 0x00);//GYR_RANGE 2000dps
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x04, 0x50);//COM_CFG

#if SL_SC7U22_RAWDATA_HPF_ENABLE ==0x01
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE,0x7F, 0x83);//goto 0x83
		sl_delay(1);
		SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x26, 1, &reg_value);
		reg_value=reg_value|0xA0;
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x26, reg_value);//HPF_CFG  rawdata hpf
#endif

#if SL_SC7U22_AOI_Wake_Up_ENABLE==0x01
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x30, 0x2A);//XYZ-ENABLE
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x32, 0x01);//VTH
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x33, 0x01);//TTH
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x3F, 0x30);//HPF FOR AOI1&AOI2
#endif

#if SL_SC7U22_FIFO_ENABLE==0x01
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x1E,0x1D);//
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x1D,0x00);//
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x1D,0x20);//
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x1C,0x37);//
#endif





#if SL_SC7U22_SDO_PullUP_ENABLE ==0x01
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE,0x7F, 0x8C);//goto 0x8C
		sl_delay(1);
		SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x30, 1, &reg_value);
		reg_value=reg_value&0xFE;//CS PullUP_enable
		reg_value=reg_value&0xFD;//SDO PullUP_enable
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x30, reg_value);

		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE,0x7F, 0x00);//goto 0x00
		delay_nms(1);
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE,0x7F, 0x00);//goto 0x00
		delay_nms(1);
#else
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE,0x7F, 0x8C);//goto 0x8C
		sl_delay(1);
		SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x30, 1, &reg_value);
		reg_value=reg_value&0xFE;//CS PullUP_enable
		reg_value=reg_value|0x02;//SDO PullUP_disable	
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x30, reg_value);

		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE,0x7F, 0x00);//goto 0x00
		sl_delay(1);
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE,0x7F, 0x00);//goto 0x00
		sl_delay(1);
#endif

		return 1;
	}
	else
		return 0;
}


//读取时间戳
unsigned int SL_SC7U22_TimeStamp_Read(void)
{
	unsigned char  time_data[3];
	unsigned int time_stamp;
	
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x18, 1, &time_data[0]);
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x19, 1, &time_data[1]);
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x20, 1, &time_data[2]);

	time_stamp=(unsigned int)(time_data[0]<<16|time_data[1]<<8|time_data[2]);
	
	return time_stamp;
}

#if SL_SC7U22_FIFO_ENABLE ==0x00
//100Hz 10ms read once
void SL_SC7U22_RawData_Read(signed short * acc_data_buf,signed short * gyr_data_buf)
{
	unsigned char  raw_data[12];
	unsigned char  drdy_satus=0x00;
	unsigned short drdy_cnt=0;
	
	while((drdy_satus&0x03)!=0x03)//acc+gyro
//	while((drdy_satus&0x01)!=0x01)//acc
	{
		drdy_satus=0x00;
		sl_delay(1);
		SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x0B, 1, &drdy_satus);
		drdy_cnt++;
		if(drdy_cnt>30000) break;
	}
	
#if SL_Sensor_Algo_Release_Enable==0x00
//	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x30, 1, &drdy_satus);
//	USART_printf( USART1, "RawData:0x40=%x\r\n",drdy_satus);
//	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x40, 1, &drdy_satus);
//	USART_printf( USART1, "RawData:0x40=%x\r\n",drdy_satus);	
//	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x06, 1, &drdy_satus);
//	USART_printf( USART1, "RawData:0x06=%x\r\n",drdy_satus);
//	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x07, 1, &drdy_satus);
//	USART_printf( USART1, "RawData:0x07=%x\r\n",drdy_satus);
//	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x7D, 1, &drdy_satus);
//	USART_printf( USART1, "RawData:0x7D=%x\r\n",drdy_satus);
//	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x31, 1, &drdy_satus);
//	USART_printf( USART1, "RawData:0x31=%x\r\n",drdy_satus);
//	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x02, 1, &drdy_satus);
//	USART_printf( USART1, "RawData:0x02=%x\r\n",drdy_satus);
//	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x03, 1, &drdy_satus);
//	USART_printf( USART1, "RawData:0x03=%x\r\n",drdy_satus);
#endif
	
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x0C, 12, &raw_data[0]);	

	acc_data_buf[0] =(signed short)((((unsigned char)raw_data[0])* 256)  + ((unsigned char)raw_data[1]));//ACCX-16位
	acc_data_buf[1] =(signed short)((((unsigned char)raw_data[2])* 256)  + ((unsigned char)raw_data[3]));//ACCY-16位
	acc_data_buf[2] =(signed short)((((unsigned char)raw_data[4])* 256)  + ((unsigned char)raw_data[5]));//ACCZ-16位
	gyr_data_buf[0] =(signed short)((((unsigned char)raw_data[6])* 256)  + ((unsigned char)raw_data[7]));//GYRX-16位
	gyr_data_buf[1] =(signed short)((((unsigned char)raw_data[8])* 256)  + ((unsigned char)raw_data[9]));//GYRY-16位
	gyr_data_buf[2] =(signed short)((((unsigned char)raw_data[10])* 256) + ((unsigned char)raw_data[11]));//GYRZ-16位
	
#if SL_Sensor_Algo_Release_Enable==0x00
	USART_printf( USART1, "RawData:AX=%d,AY=%d,AZ=%d,GX=%d,GY=%d,GZ=%d\r\n",acc_data_buf[0],acc_data_buf[1],acc_data_buf[2],gyr_data_buf[0],gyr_data_buf[1],gyr_data_buf[2]);
#endif 
	
}
#else

#if SL_Sensor_Algo_Release_Enable==0x00
#define SL_SC7U22_WAIT_FIFO_LEN_ENABLE 0x00//0x01
#else
#define SL_SC7U22_WAIT_FIFO_LEN_ENABLE 0x00
#endif
unsigned char  Acc_FIFO_Num;
unsigned char  Gyr_FIFO_Num;

unsigned char  SL_SC7U22_FIFO_DATA[1024];

unsigned short SL_SC7U22_FIFO_Read(signed short *accx_buf,signed short *accy_buf,signed short *accz_buf,signed short *gyrx_buf,signed short *gyry_buf,signed short *gyrz_buf)
{
	int16_t Acc_x = 0, Acc_y = 0, Acc_z = 0;
	int16_t Gyr_x = 0, Gyr_y = 0, Gyr_z = 0;		
	unsigned char  fifo_num1=0;
	unsigned char  fifo_num2=0;
	unsigned short fifo_num=0;
	unsigned short fifo_len=0;
	unsigned short temp = 0;
	unsigned short i = 0 ;
    unsigned char header[2];
	unsigned short j;
	
#if SL_Sensor_Algo_Release_Enable==0x00	//user can set to zero
#if SL_SC7U22_WAIT_FIFO_LEN_ENABLE==0x00
	while((fifo_num1&0x20)!=0x20)
	{
		sl_delay(200);
		SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x1F,1,&fifo_num1);
	}
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x1F,1,&fifo_num1);
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x20,1,&fifo_num2);
	if((fifo_num1&0x10)==0x10)
	{
		fifo_num=2048;
	}
	else
	{
		fifo_num=(fifo_num1&0x0F)*256+fifo_num2;
	}
#else
	while(fifo_num2<194)//32
	{
		SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x1F,1,&fifo_num1);
		SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x20,1,&fifo_num2);
		sl_delay(20);
		fifo_wait++;
		if(fifo_wait>30000) break;
	}
	fifo_wait=0;
	fifo_num=fifo_num2;
#endif
	
#else
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x1F,1,&fifo_num1);
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x20,1,&fifo_num2);
	if((fifo_num1&0x10)==0x10)
	{
		fifo_num=2048;
	}
	else
	{
		fifo_num=(fifo_num1&0x0F)*256+fifo_num2;
	}
#endif
	
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x21, fifo_num*2, SL_SC7U22_FIFO_DATA);//单地址连续读取 BYTE NUM
//	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x1D, 0x00);//BY PASS MODE
//	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x1D, 0x20);//Stream MODE
	printf("SC7U22_FIFO_NUM1:%d\n",fifo_num);
#if SL_Sensor_Algo_Release_Enable==0x00
//	USART_printf(USART1,"0x1F:0x%x 0x20:0x%x\n",fifo_num1,fifo_num2);
//	USART_printf(USART1,"SC7U22_FIFO_NUM1:%d\n",fifo_num);
#endif
	fifo_len=0;
	
	i = 0;
	Acc_FIFO_Num=0;
	Gyr_FIFO_Num=0;
	while(i < fifo_num*2)
	{	
		//header process 1
		header[0] = SL_SC7U22_FIFO_DATA[i + 0];
		header[1] = SL_SC7U22_FIFO_DATA[i + 1];
		i = i + 2;
		
		//timestamp process 2
		if(header[1] & 0x80)
		{
			i = i + 4;//every frame  include the timestamp, 4 bytes
		}
		//acc process 3
		if(header[0] & 0x04)
		{
			accx_buf[Acc_FIFO_Num]	=	((s16)(SL_SC7U22_FIFO_DATA[i + 0] * 256 + SL_SC7U22_FIFO_DATA[i + 1])) ;
			accy_buf[Acc_FIFO_Num]	=	((s16)(SL_SC7U22_FIFO_DATA[i + 2] * 256 + SL_SC7U22_FIFO_DATA[i + 3])) ;
			accz_buf[Acc_FIFO_Num]	=	((s16)(SL_SC7U22_FIFO_DATA[i + 4] * 256 + SL_SC7U22_FIFO_DATA[i + 5])) ;
			printf("AccNum : %d  ,Acc_x : %4d, Acc_y : %4d, Acc_z : %4d,\r\n",Acc_FIFO_Num, accx_buf[Acc_FIFO_Num], accy_buf[Acc_FIFO_Num], accz_buf[Acc_FIFO_Num]);	
			i = i + 6;
			Acc_FIFO_Num++;
		}
		//gyro process 3
		if(header[0] & 0x02)
		{			
			gyrx_buf[Gyr_FIFO_Num]	=	((s16)(SL_SC7U22_FIFO_DATA[i + 0] * 256 + SL_SC7U22_FIFO_DATA[i + 1])) ;
			gyry_buf[Gyr_FIFO_Num]	=	((s16)(SL_SC7U22_FIFO_DATA[i + 2] * 256 + SL_SC7U22_FIFO_DATA[i + 3])) ;
			gyrz_buf[Gyr_FIFO_Num]	=	((s16)(SL_SC7U22_FIFO_DATA[i + 4] * 256 + SL_SC7U22_FIFO_DATA[i + 5])) ;	
			printf("GyrNum : %d, Gyr_x : %4d, Gyr_y : %4d, Gyr_z : %4d,\r\n",Gyr_FIFO_Num, gyrx_buf[Gyr_FIFO_Num], gyry_buf[Gyr_FIFO_Num], gyrz_buf[Gyr_FIFO_Num]);	
			i = i + 6;
			Gyr_FIFO_Num++;
		}
		
		//temperature process 1
		if(header[0] & 0x01)
		{	
			i = i + 2;
		}
	}		

	
	return fifo_len;
}
#endif


unsigned char SL_SC7U22_POWER_DOWN(void)
{
	unsigned char SL_Read_Reg  = 0xff;
	
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
	sl_delay(20);
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7D, 0x00);//POWER DOWN
	sl_delay(200);
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x7D, 1,&SL_Read_Reg);
	if(SL_Read_Reg==0x00)   return  1;
	else                    return  0;
}


unsigned char SL_SC7U22_SOFT_RESET(void)
{
	unsigned char SL_Read_Reg  = 0xff;
	
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
	delay_nms(1);
#if SL_Sensor_Algo_Release_Enable==0x00
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x04, 1,&SL_Read_Reg);
	USART_printf( USART1, "SL_SC7U22_SOFT_RESET1 0x04=0x%x\r\n",SL_Read_Reg);
	SL_Read_Reg = 0xff;
#endif
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x04, 0x10);//BOOT
#if SL_Sensor_Algo_Release_Enable==0x00	
	PWM_PB4(1);
#endif
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x4A, 0xA5);//SOFT_RESET
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x4A, 0xA5);//SOFT_RESET
	delay_nms(200);
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x04, 1,&SL_Read_Reg);
#if SL_Sensor_Algo_Release_Enable==0x00	
	PWM_PB4(0);
	USART_printf( USART1, "SL_SC7U22_SOFT_RESET2 0x08=0x%x\r\n",SL_Read_Reg);
#endif
	if(SL_Read_Reg==0x50)   return  1;
	else                    return  0;
	
}


/****acc_enable ==0 close acc;acc_enable ==1 open acc******/
/****gyro_enable==0 close acc;gyro_enable==1 open acc******/
unsigned char SL_SC7U22_Open_Close_SET(unsigned char acc_enable,unsigned char gyro_enable)
{
	unsigned char SL_Read_Reg  = 0xff;
	unsigned char SL_Read_Check= 0xff;
	
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
	sl_delay(1);
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x7D, 1,&SL_Read_Reg);
	
	if(acc_enable==0)
	{
		SL_Read_Reg=SL_Read_Reg&0xFB;//Bit.ACC_EN=0
	}
	else if(acc_enable==1)
	{
		SL_Read_Reg=SL_Read_Reg|0x04;//Bit.ACC_EN=1
	}
	if(gyro_enable==0)
	{
		SL_Read_Reg=SL_Read_Reg&0xFD;//Bit.GYR_EN=0
	}
	else if(gyro_enable==1)
	{
		SL_Read_Reg=SL_Read_Reg|0x02;//Bit.GYR_EN=1
	}
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7D, SL_Read_Reg);//PWR_CTRL ENABLE ACC+GYR+TEMP
	sl_delay(5);//5ms
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7D, SL_Read_Reg);//PWR_CTRL ENABLE ACC+GYR+TEMP
	sl_delay(20);//10ms
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x7D, 1,&SL_Read_Check);
	if(SL_Read_Reg!=SL_Read_Check)
	{
#if SL_Sensor_Algo_Release_Enable==0x00	
		USART_printf( USART1, "SL_Read_Reg=0x%x  SL_Read_Check=0x%x\r\n",SL_Read_Reg,SL_Read_Check);
#endif
		return 0;
	}
	return 1;
}


/*******open INT******/
unsigned char SL_SC7U22_IN_SLEEP_SET(unsigned char acc_odr,unsigned char vth,unsigned char tth,unsigned char int_io)
{
	unsigned char SL_Read_Reg  = 0xff;
	unsigned char SL_Acc_Odr_Reg  = 0xff;
	
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
	sl_delay(1);
	if(int_io==1)
	{
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x06, 0x02);//AOI1-INT1
	}
	else if(int_io==2)
	{
		SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x08, 0x02);//AOI1-INT2
	}
	
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x04, 1, &SL_Read_Reg);
#if SL_SC7U22_INT_DEFAULT_LEVEL ==0x01
	SL_Read_Reg=SL_Read_Reg|0x04;
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x04, SL_Read_Reg);//defalut high level&& push-pull
#else
	reg_value=reg_value&0xDF;
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x06, SL_Read_Reg);//defalut low  level&& push-pull	
#endif
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x30, 0x2A);//AIO1-Enable
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x32, vth);//VTH
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x33, tth);//TTH
	
	if(acc_odr==12)
	{
		SL_Acc_Odr_Reg=0x05;
	}
	else if(acc_odr==25)
	{
		SL_Acc_Odr_Reg=0x06;
	}
	else if(acc_odr==50)
	{
		SL_Acc_Odr_Reg=0x07;
	}
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x40, SL_Acc_Odr_Reg);//ACC_CONF
	delay_nms(5);//5ms
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7D, 0x04);//acc open and gyro close
	delay_nms(5);//5ms
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7D, 0x04);//acc open and gyro close
	sl_delay(200);
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x7D, 1,&SL_Read_Reg);
	
	if(SL_Read_Reg!=0x04)
	{
#if SL_Sensor_Algo_Release_Enable==0x00	
		USART_printf( USART1, "SL_Read_Reg=0x%x 0x04\r\n",SL_Read_Reg);
#endif
		return 0;
	}
	return 1;
}

/*******ODR SET:25 50 100 200******************/
/*******acc range:2 4 8 16*********************/
/*******gyro range:125 250 500 1000 2000*******/
/*******acc_hp_en: 0=disable 1=enable**********/
/*******gyro_hp_en:0=disable 1=enable**********/
unsigned char SL_SC7U22_WakeUp_SET(unsigned char odr_mode,unsigned char acc_range,unsigned char acc_hp_en,unsigned short gyro_range,unsigned char gyro_hp_en)
{
	unsigned char SL_Odr_Reg        = 0x00;
	unsigned char SL_acc_mode_Reg   = 0x00;
	unsigned char SL_gyro_mode_Reg  = 0x00;	
	unsigned char SL_acc_range_Reg  = 0x00;
	unsigned char SL_gyro_range_Reg = 0x00;
	unsigned char SL_Read_Check     = 0xff;
	
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7F, 0x00);//goto 0x00
	sl_delay(1);
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7D, 0x06);//PWR_CTRL ENABLE ACC+GYR
	sl_delay(5);
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x7D, 0x06);//PWR_CTRL ENABLE ACC+GYR
	sl_delay(200);
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x30, 0x00);//AIO1-disable
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x32, 0xff);//vth
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x33, 0xff);//tth

	if(odr_mode==25)
	{
		SL_Odr_Reg=0x06;
	}
	else if(odr_mode==50)
	{
		SL_Odr_Reg=0x07;
	}
	else if(odr_mode==100)
	{
		SL_Odr_Reg=0x08;
	}
	else if(odr_mode==200)
	{
		SL_Odr_Reg=0x09;
	}
	if(acc_hp_en==1)
		SL_acc_mode_Reg=0x80;

	SL_acc_mode_Reg=SL_acc_mode_Reg|SL_Odr_Reg;
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x40, SL_acc_mode_Reg);//ACC_CONF

	if(gyro_hp_en==1)
		SL_gyro_mode_Reg=0x40;
	else if(gyro_hp_en==2)
		SL_gyro_mode_Reg=0x80;
	else if(gyro_hp_en==3)
		SL_gyro_mode_Reg=0xC0;
	
	SL_gyro_mode_Reg=SL_gyro_mode_Reg|SL_Odr_Reg;
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x42, SL_gyro_mode_Reg);//GYR_CONF
	
	if(acc_range==2)
	{
		SL_acc_range_Reg=0x00;
	}
	else if(acc_range==4)
	{
		SL_acc_range_Reg=0x01;
	}
	else if(acc_range==8)
	{
		SL_acc_range_Reg=0x02;
	}		
	else if(acc_range==16)
	{
		SL_acc_range_Reg=0x03;
	}		
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x41, SL_acc_range_Reg);//ACC_RANGE
	
	if(gyro_range==2000)
	{
		SL_gyro_range_Reg=0x00;
	}
	else if(gyro_range==1000)
	{
		SL_gyro_range_Reg=0x01;
	}
	else if(gyro_range==500)
	{
		SL_gyro_range_Reg=0x02;
	}		
	else if(gyro_range==250)
	{
		SL_gyro_range_Reg=0x03;
	}
	else if(gyro_range==125)
	{
		SL_gyro_range_Reg=0x04;
	}
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x43, SL_gyro_range_Reg);//GYR_RANGE 2000dps
	SL_SC7U22_I2c_Spi_Write(SL_SPI_IIC_INTERFACE, 0x43, SL_gyro_range_Reg);//GYR_RANGE 2000dps
#if SL_Sensor_Algo_Release_Enable==0x00
//	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x40, 1, &SL_Read_Check);
//	USART_printf( USART1, "RawData:0x40=%x\r\n",SL_Read_Check);
//	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x41, 1, &SL_Read_Check);
//	USART_printf( USART1, "RawData:0x41=%x\r\n",SL_Read_Check);
//	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x42, 1, &SL_Read_Check);
//	USART_printf( USART1, "RawData:0x42=%x\r\n",SL_Read_Check);
//	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x43, 1, &SL_Read_Check);
//	USART_printf( USART1, "RawData:0x43=%x\r\n",SL_Read_Check);
#endif
	
	SL_SC7U22_I2c_Spi_Read(SL_SPI_IIC_INTERFACE, 0x43, 1,&SL_Read_Check);
	if(SL_Read_Check!=SL_gyro_range_Reg)
	{
#if SL_Sensor_Algo_Release_Enable==0x00	
		USART_printf( USART1, "SL_Read_Check=0x%x SL_gyro_range_Reg=0x%x\r\n",SL_Read_Check,SL_gyro_range_Reg);
#endif
		return 0;
	}
	return 1;
}


#if SL_SC7U22_FIFO_ENABLE ==0x00
//Kalman
//-------------------------------------------------------
float angle[3]={0,0,0}, angle_dot[3]={0,0,0}; 		//
float angle0[3]={0,0,0}, angle_dot0[3]={0,0,0}; 		//
//-------------------------------------------------------
//float Q_angle=0.0003, Q_gyro=0.001, R_angle=0.005, dt=0.005;//5ms ST
//float Q_angle=0.00001, Q_gyro=0.00001, R_angle=0.005, dt=0.0025;//5ms ST
float Q_angle=0.0003, Q_gyro=0.001, R_angle=0.005, dt=0.01;//10ms

float P[2][2]  = {{ 1, 0 },{ 0, 1 }};
float P0[2][2] = {{ 1, 0 },{ 0, 1 }};
float P1[2][2] = {{ 1, 0 },{ 0, 1 }};						
float P2[2][2] = {{ 1, 0 },{ 0, 1 }};				
float Pdot2[4] ={0,0,0,0};
float Pdot1[4] ={0,0,0,0};
float Pdot0[4] ={0,0,0,0};
const float C_0 = 1.0;
const float C_1 = 1.0;
const float C_2 = 1.0;
float q_bias[3]={0,0,0}, angle_err[3]={0,0,0}, PCt_0[3]={0,0,0}, PCt_1[3]={0,0,0},PCt_2[3]={0,0,0}, E[3]={0,0,0}, K_0[3]={0,0,0}, K_1[3]={0,0,0}, t_0[3]={0,0,0}, t_1[3]={0,0,0};
float q_bias0[3]={0,0,0}, angle_err0[3]={0,0,0}, PCt0_0[3]={0,0,0}, PCt0_1[3]={0,0,0}, E0[3]={0,0,0}, K0_0[3]={0,0,0}, K0_1[3]={0,0,0}, t0_0[3]={0,0,0}, t0_1[3]={0,0,0};
//-------------------------------------------------------

static signed short SL_GetAbsShort(signed short v_Val_s16r)
{
	if(v_Val_s16r==(-32768))
		return 32767;
	return (v_Val_s16r < 0) ? -v_Val_s16r : v_Val_s16r;
}

unsigned char SL_SC7U22_Error_Flag=0;
unsigned char SL_SC7U22_Error_cnt=0;
unsigned char SL_SC7U22_Error_cnt2=0;
signed short  Temp_Accgyro[6] ={0};
signed short  Error_Accgyro[6]={0};
signed int    Sum_Avg_Accgyro[6] ={0};
unsigned char  SL_SC7U22_Angle_Output(unsigned char calibration_en,signed short *acc_gyro_input,float *Angle_output, unsigned char yaw_rst)
{
	unsigned short acc_gyro_delta[2];
	unsigned char sl_i=0;
	float angle_acc[3]={0};
	float gyro_val[3]={0};
	
	if(calibration_en==0) 
	{
		SL_SC7U22_Error_Flag=1;
	}
	
	if(SL_SC7U22_Error_Flag==0)
	{
		acc_gyro_delta[0]=0;
		acc_gyro_delta[1]=0;
		for(sl_i=0;sl_i<3;sl_i++)
		{
			acc_gyro_delta[0]+=SL_GetAbsShort(acc_gyro_input[sl_i]-Temp_Accgyro[sl_i]);
			acc_gyro_delta[1]+=SL_GetAbsShort(acc_gyro_input[3+sl_i]-Temp_Accgyro[3+sl_i]);
		}
		for(sl_i=0;sl_i<6;sl_i++)
		{
			Temp_Accgyro[sl_i]=acc_gyro_input[sl_i];
		}
		
		if((acc_gyro_delta[0]/8<80)&&(acc_gyro_delta[1]<20)&&(SL_GetAbsShort(acc_gyro_input[0])<3000)&&(SL_GetAbsShort(acc_gyro_input[1])<3000)&&(SL_GetAbsShort(acc_gyro_input[2]-8192)<3000))//acc<80mg  gyro<20 lsb
		{
			if(SL_SC7U22_Error_cnt<200)
				SL_SC7U22_Error_cnt++;
		}
		else
		{
			SL_SC7U22_Error_cnt=0;
		}
	
		if(SL_SC7U22_Error_cnt>190)
		{
			for(sl_i=0;sl_i<6;sl_i++)
			{
				Sum_Avg_Accgyro[sl_i]+=acc_gyro_input[sl_i];
			}
			SL_SC7U22_Error_cnt2++;
			if(SL_SC7U22_Error_cnt2>49)
			{
				SL_SC7U22_Error_Flag=1;
				SL_SC7U22_Error_cnt2=0;
				SL_SC7U22_Error_cnt=0;
				for(sl_i=0;sl_i<6;sl_i++)
				{
					Sum_Avg_Accgyro[sl_i]=Sum_Avg_Accgyro[sl_i]/50;
				}
				
				Error_Accgyro[0]=0-Sum_Avg_Accgyro[0];
				Error_Accgyro[1]=0-Sum_Avg_Accgyro[1];
				Error_Accgyro[2]=8192-Sum_Avg_Accgyro[2];
				Error_Accgyro[3]=0-Sum_Avg_Accgyro[3];
				Error_Accgyro[4]=0-Sum_Avg_Accgyro[4];
				Error_Accgyro[5]=0-Sum_Avg_Accgyro[5];
#if SL_Sensor_Algo_Release_Enable==0x00
				USART_printf(USART1,"AVG_Recode AX:%d,AY:%d,AZ:%d,GX:%d,GY:%d,GZ:%d\r\n",Sum_Avg_Accgyro[0],Sum_Avg_Accgyro[1],Sum_Avg_Accgyro[2],Sum_Avg_Accgyro[3],Sum_Avg_Accgyro[4],Sum_Avg_Accgyro[5]);
				USART_printf(USART1,"Error_Recode AX:%d,AY:%d,AZ:%d,GX:%d,GY:%d,GZ:%d\r\n",Error_Accgyro[0],Error_Accgyro[1],Error_Accgyro[2],Error_Accgyro[3],Error_Accgyro[4],Error_Accgyro[5]);		
#endif
			}
		}
		else
		{
			SL_SC7U22_Error_cnt2=0;
			for(sl_i=0;sl_i<6;sl_i++)
			{
				Sum_Avg_Accgyro[sl_i]=0;
			}
		}
		
		return 0;
	}
	
	
	if(SL_SC7U22_Error_Flag==1)
	{
		for(sl_i=0;sl_i<6;sl_i++)
		{
			Temp_Accgyro[sl_i]=acc_gyro_input[sl_i]+Error_Accgyro[sl_i];
		}//-error
		
#if 1   //output calibration data
		for(sl_i=0;sl_i<6;sl_i++)
		{
			acc_gyro_input[sl_i]=Temp_Accgyro[sl_i];
		}
#endif		
		angle_acc[0]=(float)Temp_Accgyro[0]/8192;//ax
		angle_acc[1]=(float)Temp_Accgyro[1]/8192;//ay       
		angle_acc[2]=(float)Temp_Accgyro[2]/8192;//az

		if(angle_acc[0]>1.0)    angle_acc[0]= 1.0;
		if(angle_acc[0]<-1.0)   angle_acc[0]=-1.0;
		if(angle_acc[1]>1.0)    angle_acc[1]= 1.0;
		if(angle_acc[1]<-1.0)   angle_acc[1]=-1.0;
		if(angle_acc[2]>1.0)    angle_acc[2]= 1.0;
		if(angle_acc[2]<-1.0)   angle_acc[2]=-1.0;

		angle_acc[0] = asinf(angle_acc[0])*57.32484;//Pitch:-90~+90
		if((angle_acc[2]<0.001)&&(angle_acc[2]>-0.001))
		{
			if(angle_acc[2]>0) 
				angle_acc[2]=0.001;
			else
				angle_acc[2]=-0.001;
		}	
		angle_acc[1] =-atanf(angle_acc[1]/angle_acc[2])*57.32484;//Roll: -180~+180
		
		if(angle_acc[2]>=0)
		{
			angle_acc[1]=angle_acc[1];
		}
		else
		{
			if(angle_acc[1]>=0)
				angle_acc[1]=-180+angle_acc[1];
			else
				angle_acc[1]=180+angle_acc[1];
		}
	
		gyro_val[0]=Temp_Accgyro[4]*0.061;//lsb-->dps
		gyro_val[1]=Temp_Accgyro[3]*0.061;//lsb-->dps
		gyro_val[2]=Temp_Accgyro[5]*0.061;//lsb-->dps
		
		/**************Pitch**************/
		angle0[0] += (gyro_val[0]-q_bias0[0]) * dt;
		Pdot0[0] = Q_angle - P0[0][1] - P0[1][0]+P[1][1]*dt;
		Pdot0[1] = - P0[1][1];
		Pdot0[2] = - P0[1][1];
		Pdot0[3] = Q_gyro;
		
		P0[0][0] += Pdot0[0] * dt;
		P0[0][1] += Pdot0[1] * dt;
		P0[1][0] += Pdot0[2] * dt;
		P0[1][1] += Pdot0[3] * dt;	
		//P(K|K-1) 
		
		PCt0_0[0] = C_0 * P0[0][0];
		PCt0_1[0] = C_0 * P0[1][0];
		
		E0[0] = R_angle + C_0 * PCt0_0[0];
		if(E0[0]==0)
		{
			E0[0]=0.0001;
		}

		K0_0[0] = PCt0_0[0] / E0[0];
		K0_1[0] = PCt0_1[0] / E0[0];
		
		angle_err0[0] = angle_acc[0] - angle0[0];
		angle0[0]	+= K0_0[0] * angle_err0[0];
		q_bias0[0] += K0_1[0] * angle_err0[0];
		angle_dot0[0] = gyro_val[0]-q_bias0[0];

		t0_0[0] = PCt0_0[0];
		t0_1[0] = C_0 * P0[0][1];

		P0[0][0] -= K0_0[0] * t0_0[0];
		P0[0][1] -= K0_0[0] * t0_1[0];
		P0[1][0] -= K0_1[0] * t0_0[0];
		P0[1][1] -= K0_1[0] * t0_1[0];
		Angle_output[0]=angle0[0];
		
		/**************Roll**************/
		angle0[1] += (gyro_val[1]-q_bias0[1]) * dt;
		Pdot1[0]=Q_angle - P1[0][1] - P1[1][0]+P[1][1]*dt;
		Pdot1[1] = - P1[1][1];
		Pdot1[2] = - P1[1][1];
		Pdot1[3]=Q_gyro;
		
		P1[0][0] += Pdot1[0] * dt;
		P1[0][1] += Pdot1[1] * dt;
		P1[1][0] += Pdot1[2] * dt;
		P1[1][1] += Pdot1[3] * dt;
		
		PCt0_0[1] = C_1 * P1[0][0];
		PCt0_1[1] = C_1 * P1[1][0];
		
		E0[1] = R_angle + C_1 * PCt0_0[1];
		if(E0[1]==0)
		{
			E0[1]=0.0001;
		}
		
		K0_0[1] = PCt0_0[1] / E0[1];
		K0_1[1] = PCt0_1[1] / E0[1];
		
		angle_err0[1] = angle_acc[1] - angle0[1];
		angle0[1]	+= K0_0[1] * angle_err0[1];
		q_bias0[1] += K0_1[1] * angle_err0[1];
		angle_dot0[1] = gyro_val[1]-q_bias0[1];

		t0_0[1] = PCt0_0[1];
		t0_1[1] = C_1 * P1[0][1];

		P1[0][0] -= K0_0[1] * t0_0[1];
		P1[0][1] -= K0_0[1] * t0_1[1];
		P1[1][0] -= K0_1[1] * t0_0[1];
		P1[1][1] -= K0_1[1] * t0_1[1];
		Angle_output[1]=angle0[1];
		
		if(yaw_rst==1)
			Angle_output[2]=0;
			
		if(SL_GetAbsShort(Temp_Accgyro[5])>8)
			Angle_output[2]+=gyro_val[2]*dt;
		
		return 1;
	}
	
	return 2;//error status
}



#endif

