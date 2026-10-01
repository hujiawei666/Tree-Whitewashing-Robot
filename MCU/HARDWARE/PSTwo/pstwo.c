#include "pstwo.h"
#include "usart.h"
/*********************************************************
Copyright (C), 2015-2025, YFRobot.
www.yfrobot.com
File：PS2驱动程序
Author：pinggai    Version:1.0     Data:2015/05/16
Description: PS2驱动程序
**********************************************************/	 
u16 Handkey;
u8 Comd[2]={0x01,0x42};	//开始命令。请求数据
u8 Data[9]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}; //数据存储数组
u8 scan[9]={0x01,0x42,0x00,0x00,0x00,0x00,0x00,0x00,0x00};//{0x01,0x42,0x00,0x5A,0x5A,0x5A,0x5A,0x5A,0x5A};	// 类型读取

u16 MASK[]={
    PSB_SELECT,
    PSB_L3,
    PSB_R3 ,
    PSB_START,
    PSB_PAD_UP,
    PSB_PAD_RIGHT,
    PSB_PAD_DOWN,
    PSB_PAD_LEFT,
    PSB_L2,
    PSB_R2,
    PSB_L1,
    PSB_R1 ,
    PSB_GREEN,
    PSB_RED,
    PSB_BLUE,
    PSB_PINK,
	  PSB_TRIANGLE,
	  PSB_CIRCLE,
	  PSB_CROSS,
	  PSB_SQUARE
	};	//按键值与按键明

//手柄接口初始化    输入  DAT->PB15 
//                  输出  CMD->PB14    CS->PB12  CLK->PB13
void PS2_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);  	/*开启GPIOAB的外设时钟*/
	
	GPIO_InitStructure.GPIO_Pin = PS2_MISO_PIN | PS2_SCK_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;      /*设置引脚模式为通用推挽输出*/
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
	GPIO_Init(PS2_SCK_PORT, &GPIO_InitStructure);	

	GPIO_InitStructure.GPIO_Pin = PS2_CS_PIN ;	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;      /*设置引脚模式为通用推挽输出*/
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
	GPIO_Init(PS2_CS_PORT, &GPIO_InitStructure);	


	GPIO_InitStructure.GPIO_Pin = PS2_MOSI_PIN;	//DI 15
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;     
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
	GPIO_Init(PS2_MOSI_PORT, &GPIO_InitStructure);
	
	DO_H;
	CLK_H;
	CS_H; 	 											  
}




//读取手柄数据
u8 PS2_ReadData(u8 command)
{

	u8 i,j=1;
	u8 res=0; 
    for(i=0; i<=7; i++)          
    {
		if(command&0x01)
			DO_H;
		else
			DO_L;
		command = command >> 1;
		delay_us(6);//10
		CLK_L;
		delay_us(6); //10
		if(DI) 
			res = res + j;
		j = j << 1; 
		CLK_H;
		delay_us(6);//10	 
    }
    DO_H;
	  delay_us(55);//50
    return res;	
}



//对读出来的 PS2 的数据进行处理
//按下为 0， 未按下为 1
unsigned char PS2_DataKey()
{
	u8 index = 0, i = 0;

	PS2_ClearData();
	CS_L;
	for(i=0;i<9;i++)	//更新扫描按键
	{
		Data[i] = PS2_ReadData(scan[i]);	
	} 
	CS_H;
	

	Handkey=(Data[4]<<8)|Data[3];     //这是16个按键  按下为0， 未按下为1
	for(index=0;index<16;index++)
	{	    
		if((Handkey&(1<<(MASK[index]-1)))==0)
			return index+1;
	}
	return 0;          //没有任何按键按下
}


//向手柄发送命令
void PS2_Cmd(u8 CMD)
{
	volatile u16 ref=0x01;
	Data[1] = 0;
	for(ref=0x01;ref<0x0100;ref<<=1)
	{
		if(ref&CMD)
		{
			DO_H;                   //输出以为控制位
		}
		else DO_L;

		CLK_H;                        //时钟拉高
		delay_us(50);
		CLK_L;
		delay_us(50);
		CLK_H;
		if(DI)
			Data[1] = ref|Data[1];
	}
}
//判断是否为红灯模式
//返回值；0，红灯模式
//		  其他，其他模式
u8 PS2_RedLight(void)
{
	CS_L;
	PS2_Cmd(Comd[0]);  //开始命令
	PS2_Cmd(Comd[1]);  //请求数据
	CS_H;
	if( Data[1] == 0X73)   return 0 ;
	else return 1;

}
////读取手柄数据
//void PS2_ReadData(void)
//{
//	volatile u8 byte=0;
//	volatile u16 ref=0x01;

//	CS_L;

//	PS2_Cmd(Comd[0]);  //开始命令
//	PS2_Cmd(Comd[1]);  //请求数据

//	for(byte=2;byte<9;byte++)          //开始接受数据
//	{
//		for(ref=0x01;ref<0x100;ref<<=1)
//		{
//			CLK_H;
//			CLK_L;
//			delay_us(50);
//			CLK_H;
//		      if(DI)
//		      Data[byte] = ref|Data[byte];
//		}
//        delay_us(50);
//	}
//	CS_H;	
//}

////对读出来的PS2的数据进行处理      只处理了按键部分         默认数据是红灯模式  只有一个按键按下时
////按下为0， 未按下为1
//u8 PS2_DataKey()
//{
//	u8 index;

//	PS2_ClearData();
//	PS2_ReadData();

//	Handkey=(Data[4]<<8)|Data[3];     //这是16个按键  按下为0， 未按下为1
//	for(index=0;index<16;index++)
//	{	    
//		if((Handkey&(1<<(MASK[index]-1)))==0)
//		return index+1;
//	}
//	return 0;          //没有任何按键按下
//}

//得到一个摇杆的模拟量	 范围0~256
u8 PS2_AnologData(u8 button)
{
	return Data[button];
}

//清除数据缓冲区
void PS2_ClearData()
{
	u8 a;
	for(a=0;a<9;a++)
		Data[a]=0x00;
}


