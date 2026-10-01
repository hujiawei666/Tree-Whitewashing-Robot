#ifndef TIME_H_
#define TIME_H_
#include "sys.h"

// static  uint8_t duoji_flag;  //全局变量舵机标志位
 static uint32_t timer_count;           //
// static uint8_t jiao_du;           //
//extern uint8_t ;  //舵机角度

	void TIM7_Int_Init(uint16_t arr,uint16_t psc);

extern uint8_t i,j;


extern u8  TIM5CH1_CAPTURE_STA;		//输入捕获状态		
extern u32 TIM5CH1_CAPTURE_VAL;		//输入捕获值  
extern u8  TIM5CH2_CAPTURE_STA;		//输入捕获状态		
extern u32 TIM5CH2_CAPTURE_VAL;		//输入捕获值  
extern u8  TIM5CH3_CAPTURE_STA;		//输入捕获状态		
extern u32 TIM5CH3_CAPTURE_VAL;		//输入捕获值  

void TIM5_Cap_Init(u32 arr,u16 psc);

#endif


