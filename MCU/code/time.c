#include "time.h"
#include "stm32f4xx.h"                  // Device header
#include "sys.h"
#include "usart.h"
#include "delay.h"
#include "usart.h"
//#include "led.h"
//#include "key.h"
#include "pstwo.h"
#include "DC_MOTOR.h"
#include "driver.h"
#include "steering.h"




void TIM7_Int_Init(u16 arr,u16 psc)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7,ENABLE);  ///使能TIM7时钟
	
  TIM_TimeBaseInitStructure.TIM_Period = arr; 	//自动重装载值
	TIM_TimeBaseInitStructure.TIM_Prescaler=psc;  //定时器分频
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up; //向上计数模式
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1; 
	
	TIM_TimeBaseInit(TIM7,&TIM_TimeBaseInitStructure);//初始化TIM7
	
	TIM_ITConfig(TIM7,TIM_IT_Update,ENABLE); //允许定时器7更新中断
	TIM_Cmd(TIM7,ENABLE); //使能定时器7
	
	NVIC_InitStructure.NVIC_IRQChannel=TIM7_IRQn; //定时器7中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=0x01; //抢占优先级1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=0x03; //子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}


u8  TIM5CH1_CAPTURE_STA=0;	//通道1输入捕获状态		    				
u32	TIM5CH1_CAPTURE_VAL;		//通道1输入捕获值

u8  TIM5CH2_CAPTURE_STA=0;	//通道2输入捕获状态
u32	TIM5CH2_CAPTURE_VAL;		//通道2输入捕获值

u8  TIM5CH3_CAPTURE_STA=0;	//通道3输入捕获状态
u32	TIM5CH3_CAPTURE_VAL;		//通道3输入捕获值

TIM_ICInitTypeDef  TIM5_ICInitStructure;


//定时器5输入捕获通道配置初始化
//arr：自动重装值(TIM2,TIM5是32位的!!)
//psc：时钟预分频数
//Echo1---PA0
//Echo2---PA1
//Echo3---PA2
void TIM5_Cap_Init(u32 arr,u16 psc)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5,ENABLE);  		//TIM5时钟使能    
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE); 	//使能PORTA时钟	
	
	//IO引脚初始化
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2; //GPIOA0、GPIOA1、GPIOA2
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用功能
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;	//速度100MHz
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; //推挽复用输出
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN; //下拉
	GPIO_Init(GPIOA,&GPIO_InitStructure); //初始化GPIOA0、GPIOA1、GPIOA2
	
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource0,GPIO_AF_TIM5); //PA0复用位定时器5
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource1,GPIO_AF_TIM5); //PA1复用位定时器5
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource2,GPIO_AF_TIM5); //PA2复用位定时器5
	
	TIM_TimeBaseStructure.TIM_Prescaler=psc;  //定时器分频
	TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up; //向上计数模式
	TIM_TimeBaseStructure.TIM_Period=arr;   //自动重装载值
	TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1; 
	
	TIM_TimeBaseInit(TIM5,&TIM_TimeBaseStructure);
	
	//初始化TIM5输入捕获参数
	TIM5_ICInitStructure.TIM_Channel = TIM_Channel_1; //CC1S=01 	选择输入端 IC1映射到TI1上
  TIM5_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;	//上升沿捕获
  TIM5_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI; //映射到TI1上
  TIM5_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;	 //配置输入分频,不分频 
  TIM5_ICInitStructure.TIM_ICFilter = 0x00;//IC1F=0000 配置输入滤波器 不滤波
  TIM_ICInit(TIM5, &TIM5_ICInitStructure);
	
	//初始化TIM5输入捕获参数
	TIM5_ICInitStructure.TIM_Channel = TIM_Channel_2; //CC2S=01 	选择输入端 IC2映射到TI2上
  TIM5_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;	//上升沿捕获
  TIM5_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI; //映射到TI2上
  TIM5_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;	 //配置输入分频,不分频 
  TIM5_ICInitStructure.TIM_ICFilter = 0x00;//IC2F=0000 配置输入滤波器 不滤波
  TIM_ICInit(TIM5, &TIM5_ICInitStructure);
			
	//初始化TIM5输入捕获参数
	TIM5_ICInitStructure.TIM_Channel = TIM_Channel_3; //CC3S=01 	选择输入端 IC3映射到TI3上
  TIM5_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;	//上升沿捕获
  TIM5_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI; //映射到TI3上
  TIM5_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;	 //配置输入分频,不分频 
  TIM5_ICInitStructure.TIM_ICFilter = 0x00;//IC1F=0000 配置输入滤波器 不滤波
  TIM_ICInit(TIM5, &TIM5_ICInitStructure);
	
  NVIC_InitStructure.NVIC_IRQChannel = TIM5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=2;//抢占优先级3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority =0;		//子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器、	

	TIM_ITConfig(TIM5,TIM_IT_Update|TIM_IT_CC1|TIM_IT_CC2|TIM_IT_CC3,ENABLE);//允许更新中断 ,允许CCxIE捕获中断		
	
  TIM_Cmd(TIM5,ENABLE ); 	//使能定时器5
	
	
}


//捕获状态
//[7]:0,没有成功的捕获;1,成功捕获到一次.
//[6]:0,还没捕获到低电平;1,已经捕获到低电平了.
//[5:0]:捕获低电平后溢出的次数(对于32位定时器来说,1us计数器加1,溢出时间:4294秒)
//定时器5中断服务程序	 
void TIM5_IRQHandler(void)
{ 		    	
	/********************************通道1**************************************/
 	if((TIM5CH1_CAPTURE_STA&0X80)==0)//还未成功捕获	
	{
		if(TIM_GetITStatus(TIM5, TIM_IT_Update) != RESET)//溢出
		{	     
			if(TIM5CH1_CAPTURE_STA&0X40)//已经捕获到高电平了
			{
				if((TIM5CH1_CAPTURE_STA&0X3F)==0X3F)//高电平太长了
				{
					TIM5CH1_CAPTURE_STA|=0X80;		//标记成功捕获了一次
					TIM5CH1_CAPTURE_VAL=0XFFFFFFFF;
				}else TIM5CH1_CAPTURE_STA++;
			}	 
		}
		if(TIM_GetITStatus(TIM5, TIM_IT_CC1) != RESET)//捕获1发生捕获事件
		{	
			if(TIM5CH1_CAPTURE_STA&0X40)		//捕获到一个下降沿 		
			{	  			
				TIM5CH1_CAPTURE_STA|=0X80;		//标记成功捕获到一次高电平脉宽
			  TIM5CH1_CAPTURE_VAL=TIM_GetCapture1(TIM5);//获取当前的捕获值.
	 			TIM_OC1PolarityConfig(TIM5,TIM_ICPolarity_Rising); //CC1P=0 设置为上升沿捕获
			}else  								//还未开始,第一次捕获上升沿
			{
				TIM5CH1_CAPTURE_STA=0;			//清空
				TIM5CH1_CAPTURE_VAL=0;
				TIM5CH1_CAPTURE_STA|=0X40;		//标记捕获到了上升沿
				TIM_Cmd(TIM5,DISABLE ); 	//关闭定时器5
	 			TIM_SetCounter(TIM5,0);
	 			TIM_OC1PolarityConfig(TIM5,TIM_ICPolarity_Falling);		//CC1P=1 设置为下降沿捕获
				TIM_Cmd(TIM5,ENABLE ); 	//使能定时器5
			}		    
		}
		//TIM_ClearITPendingBit(TIM5, TIM_IT_CC1|TIM_IT_Update); //清除中断标志位		
 	}

	
/******************************通道2*****************************************/	

		if((TIM5CH2_CAPTURE_STA&0X80)==0)//还未成功捕获
		{
			if(TIM_GetITStatus(TIM5, TIM_IT_Update) != RESET)//溢出
			{
				if(TIM5CH2_CAPTURE_STA&0X40)//已经捕获到高电平了			
				{
					if((TIM5CH2_CAPTURE_STA&0X3F)==0X3F)//高电平太长了
					{
						TIM5CH2_CAPTURE_STA|=0X80;		//标记成功捕获了一次
						TIM5CH2_CAPTURE_VAL=0XFFFFFFFF;
					}else TIM5CH2_CAPTURE_STA++;
				}	 
			}
			if(TIM_GetITStatus(TIM5, TIM_IT_CC2) != RESET)//捕获1发生捕获事件
			{
				if(TIM5CH2_CAPTURE_STA&0X40)		//捕获到一个下降沿 		
				{
					TIM5CH2_CAPTURE_STA|=0X80;		//标记成功捕获到一次高电平脉宽
					TIM5CH2_CAPTURE_VAL=TIM_GetCapture2(TIM5);//获取当前的捕获值.
					TIM_OC2PolarityConfig(TIM5,TIM_ICPolarity_Rising); //CC2P=0 设置为上升沿捕获
				}else  								//还未开始,第一次捕获上升沿
				{
					TIM5CH2_CAPTURE_STA=0;			//清空
					TIM5CH2_CAPTURE_VAL=0;
					TIM5CH2_CAPTURE_STA|=0X40;		//标记捕获到了上升沿
					TIM_Cmd(TIM5,DISABLE ); 	//关闭定时器5
					TIM_SetCounter(TIM5,0);
					TIM_OC2PolarityConfig(TIM5,TIM_ICPolarity_Falling);		//CC2P=1 设置为下降沿捕获
					TIM_Cmd(TIM5,ENABLE ); 	//使能定时器5
				}		    
			}
//			TIM_ClearITPendingBit(TIM4, TIM_IT_CC2|TIM_IT_Update); //清除中断标志位
		}
		
	
/***********************************通道3************************************/

		if((TIM5CH3_CAPTURE_STA&0X80)==0)//还未成功捕获
		{
			if(TIM_GetITStatus(TIM5, TIM_IT_Update) != RESET)//溢出
			{
				if(TIM5CH3_CAPTURE_STA&0X40)//已经捕获到高电平了			
				{
					if((TIM5CH3_CAPTURE_STA&0X3F)==0X3F)//高电平太长了
					{
						TIM5CH3_CAPTURE_STA|=0X80;		//标记成功捕获了一次
						TIM5CH3_CAPTURE_VAL=0XFFFFFFFF;
					}else TIM5CH3_CAPTURE_STA++;
				}	 
			}
			if(TIM_GetITStatus(TIM5, TIM_IT_CC3) != RESET)//捕获1发生捕获事件
			{
				if(TIM5CH3_CAPTURE_STA&0X40)		//捕获到一个下降沿 		
				{
					TIM5CH3_CAPTURE_STA|=0X80;		//标记成功捕获到一次高电平脉宽
					TIM5CH3_CAPTURE_VAL=TIM_GetCapture3(TIM5);//获取当前的捕获值.
					TIM_OC3PolarityConfig(TIM5,TIM_ICPolarity_Rising); //CC3P=0 设置为上升沿捕获
				}else  								//还未开始,第一次捕获上升沿
				{
					TIM5CH3_CAPTURE_STA=0;			//清空
					TIM5CH3_CAPTURE_VAL=0;
					TIM5CH3_CAPTURE_STA|=0X40;		//标记捕获到了上升沿
					TIM_Cmd(TIM5,DISABLE ); 	//关闭定时器5
					TIM_SetCounter(TIM5,0);
					TIM_OC3PolarityConfig(TIM5,TIM_ICPolarity_Falling);		//CC3P=1 设置为下降沿捕获
					TIM_Cmd(TIM5,ENABLE ); 	//使能定时器5
				}		    
			}
//			TIM_ClearITPendingBit(TIM4, TIM_IT_CC3|TIM_IT_Update); //清除中断标志位
		}
		
		TIM_ClearITPendingBit(TIM5, TIM_IT_CC1|TIM_IT_CC2|TIM_IT_CC3|TIM_IT_Update); //清除中断标志位
}


