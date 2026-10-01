#include "beep.h"
#include "power_monitor.h"
#include <math.h>
#include "delay.h"
#include "usart2.h"

// 新增：全局ADC电压变量
float adc_voltage = 0.0f;

// 初始化功耗监视器
void PowerMonitor_Init(PowerMonitor* monitor, float shunt, float gain) {
    monitor->current = 0.0f;       // 当前电流 (mA)
    monitor->power = 0.0f;         // 当前功耗 (mW)
    monitor->max_power = 0.0f;     // 最大功耗 (mW)
    monitor->shunt_resistor = shunt;// 采样电阻值 (Ω)
    monitor->gain = gain;          // 放大倍数
}

// 更新功耗数据
void PowerMonitor_Update(PowerMonitor* monitor, uint16_t adc_value, float voltage) {
    // ADC参考电压3.3V，12位分辨率
    const float vref = 3.3f;
    const float adc_max = 4095.0f;
    
    // 计算采样电压
    adc_voltage = (adc_value / adc_max) * vref;  // 新增：存储ADC电压值
    
    // 计算负载电流 (mA)
    if (monitor->shunt_resistor > 0 && monitor->gain > 0) {
        monitor->current = (adc_voltage / monitor->gain) / monitor->shunt_resistor * 1000.0f;
    } else {
        monitor->current = 0.0f;
    }
    
    // 计算功耗 (mW)
    monitor->power = voltage * monitor->current;
    
    // 更新最大功耗
    if (monitor->power > monitor->max_power) {
        monitor->max_power = monitor->power;
    BEEP = 1;                   // 蜂鸣器短鸣
    delay_ms(100);
    BEEP = 0;
	USART2_SendString("2/r/n");

		
    }
}
