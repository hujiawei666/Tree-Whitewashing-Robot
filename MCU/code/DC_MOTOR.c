#include "DC_MOTOR.h"
#include "stm32f4xx.h"                  // Device header
#include "sys.h"
#include "usart.h"

extern unsigned int n;
extern int num;
int PWM_50 = 30;

void DC_MOTOR_GPIO_Config(void) //GPIO和TIM2初始化P,PWM
{		
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
	RCC_AHB1PeriphClockCmd( RCC_AHB1Periph_GPIOB, ENABLE);     //开启GPIO的外设时钟		
  RCC_AHB1PeriphClockCmd( RCC_AHB1Periph_GPIOC, ENABLE); 	
	
	GPIO_PinAFConfig(GPIOB,GPIO_PinSource6,GPIO_AF_TIM4);
	GPIO_PinAFConfig(GPIOB,GPIO_PinSource7,GPIO_AF_TIM4);
	GPIO_PinAFConfig(GPIOB,GPIO_PinSource8,GPIO_AF_TIM4);
	GPIO_PinAFConfig(GPIOB,GPIO_PinSource9,GPIO_AF_TIM4);

	//PB6：PWM1   PB7:PWM2   PB8：PWM3   PB9:PWM4  Tim4
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_8|GPIO_Pin_9; 
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF; //复用推挽输出模式，定时器功能为A0引脚复用功能
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz; //定义该引脚输出速度为50MHZ
	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
		TIM_TimeBaseStructure.TIM_Prescaler = 71;  //71+1= 72分频
		TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;//向上计数
		TIM_TimeBaseStructure.TIM_Period = 999;  //  peroid -->1ms，计1个数为1us
	//TIM_TimeBaseStructure.TIM_ClockDivision = 0x0;
	TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_RepetitionCounter=0;
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);
	
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1; //PWM模式1
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	
	TIM_OC1Init(TIM4, & TIM_OCInitStructure);
	TIM_OC2Init(TIM4, & TIM_OCInitStructure);
	TIM_OC3Init(TIM4, & TIM_OCInitStructure);
	TIM_OC4Init(TIM4, & TIM_OCInitStructure);
	
		//输出比较1-4预装载寄存器使能
	TIM_OC1PreloadConfig(TIM4,TIM_OCPreload_Enable);
	TIM_OC2PreloadConfig(TIM4,TIM_OCPreload_Enable);
	TIM_OC3PreloadConfig(TIM4,TIM_OCPreload_Enable);
	TIM_OC4PreloadConfig(TIM4,TIM_OCPreload_Enable);
	
	TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);  //使能TIM在CCR1上的预装载寄存器
	TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Enable);  //使能TIM在CCR1上的预装载寄存器
	TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Enable);  //使能TIM在CCR1上的预装载寄存器
	TIM_OC4PreloadConfig(TIM4, TIM_OCPreload_Enable);  //使能TIM在CCR1上的预装载寄存器

	TIM_ARRPreloadConfig(TIM4,ENABLE);//ARPE使能 
	TIM_Cmd(TIM4, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 |GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_8 | GPIO_Pin_9;  //选择要控制的GPIO引脚
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;//普通输出模式
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;     //设置引脚速率为50MHz 
	GPIO_Init(GPIOC, &GPIO_InitStructure);                //调用库函数，初始化GPIO	
}
 void PWM_Control(int PS2_RX,int PS2_RY)
{  
	static int num=200;
	if((num>=0)&&(PS2_RY==0)&&(PS2_RX==127||PS2_RX==128))
	{  
		 num+=PWM_50;
	   DC_MOTOR_PWM_Config(num);
		if(num==700)
		{
		 DC_MOTOR_PWM_Config(700);
		}
	}
   if((num>0)&&(PS2_RY==255)&&(PS2_RX==127||PS2_RX==128))
	 { num-=PWM_50;
		 DC_MOTOR_PWM_Config(num);
	 }
}

void DC_MOTOR_PWM_Config(int num)
{
  TIM_SetCompare1(TIM4,num);
	TIM_SetCompare2(TIM4,num);
	TIM_SetCompare3(TIM4,num);
	TIM_SetCompare4(TIM4,num);
	
}

void Analog_stick_model(int PS2_LX,int PS2_LY)
{
	 if((PS2_LX==0)&&(PS2_LY==127||PS2_LY==128))  //UP
	 {
			Car_Go();
		 
	 }
   else if((PS2_LX==255)&&(PS2_LY==128))             //DOWM 
	 {
			Car_Back(); 
		
	 }
	 else if((PS2_LX==127)&&(PS2_LY==0)) //LEFT
	 {
			Car_Left(); 
		 
	 }

  	else if((PS2_LX==127)&&(PS2_LY==255))     //RIGHT
	{
			Car_Right(); 
		
	}
	 else if((PS2_LX==0)&&(PS2_LY==0))          //左上斜
	{
			Car_Go_Left();
		
	}
	else if((PS2_LX==0)&&(PS2_LY==255))                //右上斜
	{		
		Car_Go_Right();
		
	}
	else if ((PS2_LX==255)&&(PS2_LY==0))                //左下斜
	{	
		Car_Back_Left();
	}
	else if((PS2_LX==255)&&(PS2_LY==255))              //右下斜
	{
		Car_Back_Right();
		
	}
}







//左前电机控制
void MOTOR_ZQ(char state)
{
	if(state == GO)//左电机前进
	{
		INA1(0); 
		INB2(1);
	}
	if(state == BACK)//左电机后退
	{
		INA1(1);
		INB2(0);
	}
	if(state == STOP)//停转
	{
		INA1(0);  
		INB2(0);
	}
}
//右前电机控制
void MOTOR_YQ(char state)
{
	if(state == GO)//右电机前进
	{
		INA3(0); 
		INB4(1);
	}
	if(state == BACK)//右电机后退
	{
		INA3(1);
		INB4(0);
	}
	if(state == STOP)//停转
	{
		INA3(0);  
		INB4(0);
	}
}


//左后电机控制
void MOTOR_ZH(char state)
{
	if(state == GO)  //左后电机前进
	{
		INA5(0);
		INB6(1);
	}
	if(state == BACK)//左后电机后退
	{
		INA5(1);
		INB6(0);
	}
	if(state == STOP)//左后电机刹车
	{
		INA5(0);
		INB6(0);
	}
}

//右后电机控制
void MOTOR_YH(char state)
{
	if(state == GO)  //右后电机前进
	{
		INA7(0);
		INB8(1);
	}
	if(state == BACK)//右后电机后退
	{
		INA7(1);
		INB8(0);
	}
	if(state == STOP)//右后电机刹车
	{
		INA7(0);
		INB8(0);
	}
}

void Car_Go(void)//前进
{
	//左电机前进     //右电机前进
	MOTOR_ZQ(GO);      MOTOR_YQ(GO);
	MOTOR_ZH(GO);      MOTOR_YH(GO);

}

void Car_Back(void)//后退
{
	//左电机后退     //右电机后退
	MOTOR_ZQ(BACK);    MOTOR_YQ(BACK);
	MOTOR_ZH(BACK);    MOTOR_YH(BACK);

}
void Car_Right(void)//右转
{
	//左电机后退     //右电机前进
	MOTOR_ZQ(GO);        MOTOR_YQ(BACK);
	MOTOR_ZH(BACK);      MOTOR_YH(GO);
}

void Car_Left(void)//左转
{
	//左电机前进     //右电机后退
	MOTOR_ZQ(BACK);    MOTOR_YQ(GO);
	MOTOR_ZH(GO);	     MOTOR_YH(BACK);
}


void Car_Turn_Right(void)//顺时针自转
{
	//左电机前进        //右电机后退
	MOTOR_ZQ(GO);      MOTOR_YQ(BACK);
	MOTOR_ZH(GO);      MOTOR_YH(BACK);
}

void Car_Turn_Left(void)//逆时针自转
{
	//左电机后退     //右电机前进
	MOTOR_ZQ(BACK);    MOTOR_YQ(GO);
	MOTOR_ZH(BACK);	  MOTOR_YH(GO);
}


void Car_Stop(void)//停止
{
	//左电机停止     //右电机停止
	MOTOR_ZQ(STOP);    MOTOR_YQ(STOP);
//	MOTOR_ZH(STOP);    MOTOR_YH(STOP);
}

void Car_Go_Right(void)//右斜上
{
	//左电机前进     //右电机前进
	MOTOR_ZQ(GO);      MOTOR_YQ(STOP);
	MOTOR_ZH(STOP);    MOTOR_YH(GO);
}

void Car_Go_Left(void)//左斜上
{
	//左电机前进     //右电机前进
	MOTOR_ZQ(STOP);   MOTOR_YQ(GO);
	MOTOR_ZH(GO);    	MOTOR_YH(STOP);
}

void Car_Back_Left(void)//左斜下
{
	//左电机前进     //右电机前进
	MOTOR_ZQ(BACK);     MOTOR_YQ(STOP);
	MOTOR_ZH(STOP);    	MOTOR_YH(BACK);
}

void Car_Back_Right(void)//右斜下
{
	//左电机前进     //右电机前进
	MOTOR_ZQ(STOP);     MOTOR_YQ(BACK);
	MOTOR_ZH(BACK);    	MOTOR_YH(STOP);
}

void Car_Left_Right(void)//后端右摆尾
{
	//左电机前进     //右电机前进
	MOTOR_ZQ(GO);      	MOTOR_YQ(BACK);
	MOTOR_ZH(STOP);    	MOTOR_YH(STOP);
}

void Car_Right_Left(void)//后端左摆尾
{
	//左电机前进     //右电机前进
	MOTOR_ZQ(BACK);      MOTOR_YQ(GO);
	MOTOR_ZH(STOP);      MOTOR_YH(STOP);
}

void Car_RRight(void)//顺时针自转
{
	//左电机前进     //右电机前进
	MOTOR_ZQ(GO);      MOTOR_YQ(BACK);
	MOTOR_ZH(GO);      MOTOR_YH(BACK);
}

