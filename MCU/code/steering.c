#include "steering.h"
unsigned int ServoAngle;
unsigned int Servo_Angle = 0;//舵机角度
/******************************************************************
 * 函数名称:SG90_Init
 * 函数说明:PWM配置
 * 函数返回: pre定时器预分频    per周期
 * 函数返回：无
 * 备注:PWM频率=84 000 000 /( (pre+1) * (per+1) )

配置占空比 范围 0 ~ (per-1)

//    t = 0.5ms舵机转动0°
//    t = 1.0ms舵机转动45°
//    t = 1.5ms舵机转动90°
//    t = 2.0ms舵机转动135v
//    t = 2.5ms舵机转动180°
//    举例：90°,PWM周期20ms,所以占空比1.5ms/20ms = 7.5%
******************************************************************/
void SG90_Init(void)
{
    /* 配置定时器参数 */
    // 频率f =系统时钟 / ( (prescaler+1) * (period+1) )
    // 频率f = 84,000,000/ (8400 * 200)  = 50hz
    // 周期T = 1/f = 1/50 = 0.02S = 20ms
    
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_OCInitTypeDef  TIM_OCInitStructure;
	
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);        
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);   
	
    //配置PA6/PA7为复用功能TIM3_CH1,CH2
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6|GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;  
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
 
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_TIM3); //连接通道1和2
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_TIM3); 
 
    //配置TIM3
    TIM_TimeBaseStructure.TIM_Period = 200-1; //PWM周期为20ms
    TIM_TimeBaseStructure.TIM_Prescaler =8400-1; //预分频 = 84HMz /8400=10KHz
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; // ????
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //TIM??????
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure); 
 
    //???TIM3 Channel1 PWM??
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1; //???????:TIM????????2
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; //??????
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High; //????
		
    TIM_OC1Init(TIM3, &TIM_OCInitStructure);  //??T??????????TIM3 OC1
    TIM_OC2Init(TIM3, &TIM_OCInitStructure);  //??T??????????TIM3 OC1

    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);  //??TIM3?CCR1????????
    TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);  //??TIM3?CCR1????????

    TIM_Cmd(TIM3, ENABLE);  //??TIM3
}
 
 
/******************************************************************
 * 函数:Set_Servo_Angle
 * 功能:设置舵机角度
 * 参数:angle=0-180
 * 返回:无
******************************************************************/
void Set_Servo_Angle(unsigned int angle)
{
    if(angle > 180)
    {
        angle = 180; 
    }
 
  
   ServoAngle = (unsigned int)((0.5 + (angle / 180.0) * 2.0) / 20.0 * 200);
 
    TIM_SetCompare1(TIM3, ServoAngle);
		TIM_SetCompare2(TIM3, ServoAngle);

}
 
 
/******************************************************************
 * 函数:Get_Servo_Angle
 * 功能:获取当前设置的舵机角度
 * 返回:当前舵机角度
******************************************************************/
unsigned int Get_Servo_Angle(void)
{
    return Servo_Angle;      
}
