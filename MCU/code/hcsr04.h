#ifndef _hcsr04_H
#define _hcsr04_H
#include "sys.h"

//-------------------¶Ë¿Úºê¶¨Òå------------------------

#define Left	1
#define Front	2
#define Right	3

#define Left_TRIG_Send	 PBout(2)
#define Front_TRIG_Send	 PBout(3)
#define Right_TRIG_Send	 PBout(4)

void Hcsr04_Init(void);
void Trig_start(u8 pos);

#endif

