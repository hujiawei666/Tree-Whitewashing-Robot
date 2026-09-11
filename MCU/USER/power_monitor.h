
#ifndef __POWER_MONITOR_H
#define __POWER_MONITOR_H

#include "stm32f4xx.h"
#include <stdint.h>

// 功耗监视器结构
typedef struct {
    float current;       // 当前电流 (mA)
    float power;         // 当前功耗 (mW)
    float max_power;     // 最大功耗 (mW)
    float shunt_resistor;// 采样电阻值 (Ω)
    float gain;          // 放大倍数
} PowerMonitor;

void PowerMonitor_Init(PowerMonitor* monitor, float shunt, float gain);
void PowerMonitor_Update(PowerMonitor* monitor, uint16_t adc_value, float voltage);

extern float adc_voltage;  // 新增：全局ADC电压变量
#endif
