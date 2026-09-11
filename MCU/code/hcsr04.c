#include "hcsr04.h"
#include "delay.h"


//超声波发送引脚定义初始化
//Trig1--PB2
//Trig2--PB3
//Trig3--PB4
void Hcsr04_Init(void)
{
  GPIO_InitTypeDef  GPIO_InitStructure;

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);//使能GPIOB时钟

  //初始化设置
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;//普通输出模式
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;//上拉
  GPIO_Init(GPIOB, &GPIO_InitStructure);//初始化
	
  GPIO_ResetBits(GPIOB,GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4);//GPIOG2\3\4置低
	
}

//超声波模块启动函数
//pos：通道
//1:左	2:前	3:右
void Trig_start(u8 pos)
{
	switch(pos)
	{
		case 1:	{
							Left_TRIG_Send=0;
							delay_ms(1);
							Left_TRIG_Send=1;
							delay_us(10);
							Left_TRIG_Send=0;
							delay_ms(1);
						}
		break;
		
		case 2:	{
							Front_TRIG_Send =0;
							delay_ms(1);
							Front_TRIG_Send =1;
							delay_us(10);
							Front_TRIG_Send =0;
							delay_ms(1);
						}
		break;
		
		case 3:	{
							Right_TRIG_Send=0;
							delay_ms(1);
							Right_TRIG_Send=1;
							delay_us(10);
							Right_TRIG_Send=0;
							delay_ms(1);
						}
		break;		
	}
}

