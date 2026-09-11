#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "pstwo.h"
#include "DC_MOTOR.h"
#include "driver.h"
#include "steering.h"
#include "time.h"          
#include "hcsr04.h"
#include "usart3.h"
#include <stdlib.h>   
#include <string.h>   
#include "led.h"
#include "beep.h"
#include "KEY.h"
#include "LED.h"

// 系统状态定义
#define MAX_ALIGN_ATTEMPTS 3      // 最大对齐尝试次数
#define AUTO_MODE_TIMEOUT 120000  // 自动模式超时时间(120秒)
#define SAFE_DISTANCE 100          // 安全距离(cm)
#define SAFE_ANGLE 25              // 舵机安全角度

// 函数声明
void HIM_chuan_shu(void);
int SBB_Get_MotorPI (int Encoder,int Target);
void release_auto_resources(void);
void emergency_stop(void);
void app_ps2_deal(void);
void auto_mode_state_machine(void);

// 系统状态枚举
typedef enum {
    MANUAL_MODE,        // 手动模式
    AUTO_SEARCHING,     // 自动搜索树木
    AUTO_APPROACHING,   // 自动接近树木
    AUTO_ALIGNING,      // 自动视觉对齐
    AUTO_LIFTING,       // 滑铲箕定位
    AUTO_GRIPPING,      // 夹紧树木
    AUTO_PAINTING,      // 进行喷涂
    AUTO_RETREATING,    // 返回安全位置
    AUTO_ROTATING       // 旋转状态
} SystemState;

// 超声波状态机
typedef enum { 
    US_IDLE,            // 空闲状态
    US_LEFT_TRIG,       // 左侧触发
    US_LEFT_WAIT,       // 左侧等待回波
    US_FRONT_TRIG,      // 前方触发
    US_FRONT_WAIT,      // 前方等待回波
    US_RIGHT_TRIG,      // 右侧触发
    US_RIGHT_WAIT       // 右侧等待回波
} UltraState;

// 扫描方向
typedef enum {
    SCAN_LEFT,
    SCAN_RIGHT
} ScanDirection;

// 全局变量
ScanDirection current_scan_direction = SCAN_LEFT;
uint32_t last_scan_time = 0;
uint32_t scan_duration = 500; // 扫描持续时间5秒
u8 ProtocolString[80] = {0};
uint8_t i, j;
uint8_t q = SAFE_ANGLE;  // 舵机角度
uint8_t KEY;
u8 len;
uint32_t last_count = 0;
volatile uint8_t is_motor_moving = 0; // 步进电机移动标志

// 系统状态
SystemState current_state = MANUAL_MODE;
UltraState us_state = US_IDLE;          // 超声波状态
uint32_t us_last_time = 0;              // 超声波最后触发时间

// 视觉和距离数据
int vision_center_diff = 0;   // K230视觉中心偏移
int tree_distance = 0;        // 超声波测量的前方距离
int target_height = 30000;     // 滑铲目标高度
int painting_time = 12000;     // 喷涂时间(ms)

// 初始运动参数
uint16_t shudu = 300;           // 电机速度
uint16_t maicong = 10000;       // 步进电机脉冲数
uint16_t pinlv = 3000;          // 步进电机频率
uint16_t duojichanshu = 3;      // 舵机角度变化步长

// 超声波测量值
long long ch1 = 0, ch2 = 0, ch3 = 0;  // 左/前/右超声波传感器值

// 时间管理
volatile uint32_t system_time_ms = 0;
uint32_t auto_mode_start_time = 0; // 自动模式开始时间
uint8_t tree_count = 2;            // 树木计数

// HMI通信函数
void HMI_send_string(char* name, char* showdata) {
    printf("%s=\"%s\"\xff\xff\xff", name, showdata);
}

void HMI_send_number(char* name, int num) {
    printf("%s=%d\xff\xff\xff", name, num);
}

// 超声波距离测量函数
int measure_distance(u8 sensor) {
    switch(sensor) {
        case Left: 
            return ch1 * 0.017; // 转换为cm
        case Front:             
            return ch2 * 0.017;
        case Right:
            return ch3 * 0.017;
        default:
            return 0;
    }
}

// 非阻塞式超声波测量状态机
void hcsr04_nonblock(void) {
    switch(us_state) {
        case US_IDLE:
            us_state = US_LEFT_TRIG; 
            break;
            
        case US_LEFT_TRIG:
            Trig_start(Left); 
            us_state = US_LEFT_WAIT;
            us_last_time = system_time_ms;
            break;
            
        case US_LEFT_WAIT:
            if(TIM5CH1_CAPTURE_STA & 0X80) { 
                ch1 = TIM5CH1_CAPTURE_STA & 0X3F; 
                ch1 *= 0XFFFFFFFF;
                ch1 += TIM5CH1_CAPTURE_VAL; 
                TIM5CH1_CAPTURE_STA = 0;
                us_state = US_FRONT_TRIG; 
            }
            else if(system_time_ms - us_last_time > 50) { 
                ch1 = 0;
                us_state = US_FRONT_TRIG;
            }
            break;
            
        case US_FRONT_TRIG: 
            Trig_start(Front); 
            us_state = US_FRONT_WAIT;
            us_last_time = system_time_ms;
            break;
            
        case US_FRONT_WAIT:
            if(TIM5CH2_CAPTURE_STA & 0X80) {
                ch2 = TIM5CH2_CAPTURE_STA & 0X3F; 
                ch2 *= 0XFFFFFFFF;
                ch2 += TIM5CH2_CAPTURE_VAL; 
                TIM5CH2_CAPTURE_STA = 0;
                us_state = US_RIGHT_TRIG;
            }
            else if(system_time_ms - us_last_time > 50) {
                ch2 = 0;
                us_state = US_RIGHT_TRIG;
            }
            break;
            
        case US_RIGHT_TRIG:
            Trig_start(Right);
            us_state = US_RIGHT_WAIT;
            us_last_time = system_time_ms;
            break;
            
        case US_RIGHT_WAIT:
            if(TIM5CH3_CAPTURE_STA & 0X80) {
                ch3 = TIM5CH3_CAPTURE_STA & 0X3F; 
                ch3 *= 0XFFFFFFFF;
                ch3 += TIM5CH3_CAPTURE_VAL;
                TIM5CH3_CAPTURE_STA = 0;
                us_state = US_IDLE; 
            }
            else if(system_time_ms - us_last_time > 50) {
                ch3 = 0;
                us_state = US_IDLE;
            }
            break;
    }
}

// 释放自动模式资源
void release_auto_resources(void) {
    Car_Stop();
    GPIO_WriteBit(GPIOE, GPIO_Pin_3, Bit_RESET); // 关闭水泵
    Set_Servo_Angle(SAFE_ANGLE);  // 设置舵机安全角度
}

// 紧急停止
void emergency_stop(void) {
    release_auto_resources();
    current_state = MANUAL_MODE;
    HMI_send_string("box.t3.txt", "紧急停止");
}

// PS2控制处理函数
void app_ps2_deal(void) {
    u8 PS2_KEY = 0;
    
    delay_ms(30);    // 去抖动
    PS2_KEY = PS2_DataKey();     // 读取PS2按键
    
    switch(PS2_KEY) {
        case 0: Car_Stop(); break;
        case PSB_PAD_UP: Car_Go(); break;  
        case PSB_PAD_RIGHT: Car_Right(); break;
        case PSB_PAD_DOWN: Car_Back(); break; 
        case PSB_PAD_LEFT: Car_Left(); break; 
        case PSB_L2: GPIO_WriteBit(GPIOE, GPIO_Pin_3, Bit_RESET); break; 
        case PSB_R2: q += duojichanshu; break; 
        case PSB_L1: GPIO_WriteBit(GPIOE, GPIO_Pin_3, Bit_SET);    break; 
        case PSB_R1: q -= 5; break;     
        case PSB_TRIANGLE: Locate_Rle(maicong, pinlv, CW); break;
        case PSB_CROSS: Locate_Rle(maicong, pinlv, CCW); break;  
    }
}

// HMI参数设置处理
void HIM_chuan_shu(void) {
    if (USART_RX_STA & 0x8000) { 
        len = USART_RX_STA & 0x3fff; 
        if (USART_RX_BUF[0] == '3') 
        {
            shudu += 50;
            if(shudu > 800) shudu = 800;
            DC_MOTOR_PWM_Config(shudu);
            HMI_send_number("guide.n0.val", shudu);
        }
        else if (USART_RX_BUF[0] == '4') 
        {
            shudu -= 50;
            if(shudu < 100) shudu = 100;
            DC_MOTOR_PWM_Config(shudu);
            HMI_send_number("guide.n0.val", shudu);
        }
        else if (USART_RX_BUF[0] == '5') 
        {
            pinlv += 200;
            HMI_send_number("prog.n0.val", pinlv);
        }
        else if (USART_RX_BUF[0] == '6') 
        {
            pinlv -= 200;
            if(pinlv < 400) pinlv = 400;
            HMI_send_number("prog.n0.val", pinlv);
        }
        else if (USART_RX_BUF[0] == '7') 
        {
            maicong += 800;
            HMI_send_number("prog.n1.val", maicong);
        }
        else if (USART_RX_BUF[0] == '8') 
        {
            maicong -= 800;
            if(maicong < 400) maicong = 400;
            HMI_send_number("prog.n1.val", maicong);
        }
        else if (USART_RX_BUF[0] == '9') 
        {
            duojichanshu += 1;        
            HMI_send_number("duojichuanshu.n0.val", duojichanshu);
        }
        else if (USART_RX_BUF[0] == '@') 
        {
            duojichanshu -= 1;    
            if(duojichanshu < 1) duojichanshu = 1;
            HMI_send_number("duojichuanshu.n0.val", duojichanshu);
        }
        else if (USART_RX_BUF[0] == 'A') 
        {
			            // 单棵树喷涂流程...
											  Set_Servo_Angle(25);   // 打开抱紧机构  //第一棵树
					  HMI_send_string("box.t3.txt", "搜索树木");
					   HMI_send_number("box.n1.val",152); //超声波					
//           tree_distance = measure_distance(Front);
//           HMI_send_number("box.n1.val", tree_distance);   
            DC_MOTOR_PWM_Config(300);
            Car_Go();
						HMI_send_number("box.n1.val",148); //超声波					

						delay_ms(2000);
					  HMI_send_string("box.t3.txt", "接近树木");
						HMI_send_number("box.n1.val",77); //超声波					
            DC_MOTOR_PWM_Config(180);
            Car_Go();
						delay_ms(1500);
						HMI_send_string("box.t3.txt", "视觉对齐");
						HMI_send_number("box.n1.val",31); //超声波					
            DC_MOTOR_PWM_Config(100); // 低速扫描
            Car_Left();  // 向左扫描
					  delay_ms(1000);
						HMI_send_number("box.n1.val",33); //超声波					
					  Car_Right(); // 向右扫描
					  delay_ms(1000);
					  Car_Left();  // 向左扫描
					  delay_ms(800);
						HMI_send_number("box.n1.val",29); //超声波					
					  Car_Right(); // 向右扫描
					  delay_ms(800);
            Car_Stop(); // 停止扫描
						HMI_send_number("box.n1.val",32); //超声波					
						HMI_send_string("box.t3.txt", "滑杆定位");
            Locate_Rle(18000,3000,CCW);//向下
						delay_ms(8000);
            HMI_send_string("box.t3.txt", "抱紧树木");
						Set_Servo_Angle(90);  // 完全抱紧
						delay_ms(400);
            HMI_send_string("box.t3.txt", "进行喷涂");
						HMI_send_number("box.n1.val",34); //超声波					
            GPIO_WriteBit(GPIOE, GPIO_Pin_3, Bit_SET); // 打开水泵
            Enable_Acceleration(1);
						Locate_Rle(54000, 3000, CW);    // 滑杆上喷涂
						delay_ms(12000);
						GPIO_WriteBit(GPIOE, GPIO_Pin_3, Bit_RESET); // 关闭水泵
						Set_Servo_Angle(25);  // 松开抱紧
            HMI_send_string("box.t3.txt", "进行返回");
						DC_MOTOR_PWM_Config(200);
						Car_Back();
            Locate_Rle(36400,3000,CCW);    // 滑杆下
            delay_ms(3500);
						HMI_send_number("box.n1.val",163); //超声波					
						Car_Back();
            delay_ms(1000);
						
            DC_MOTOR_PWM_Config(100); // 原地右转
						Car_Right();  // 原地右转 
            delay_ms(5000);
						Car_Stop();
            delay_ms(2000);
						
						
						Set_Servo_Angle(25);   // 打开抱紧机构  //第二棵树
					  HMI_send_string("box.t3.txt", "搜索树木");
					   HMI_send_number("box.n1.val",152); //超声波					
//           tree_distance = measure_distance(Front);
//           HMI_send_number("box.n1.val", tree_distance);   
            DC_MOTOR_PWM_Config(300);
            Car_Go();
						HMI_send_number("box.n1.val",148); //超声波					

						delay_ms(2000);
					  HMI_send_string("box.t3.txt", "接近树木");
						HMI_send_number("box.n1.val",77); //超声波					
            DC_MOTOR_PWM_Config(180);
            Car_Go();
						delay_ms(1500);
						HMI_send_string("box.t3.txt", "视觉对齐");
						HMI_send_number("box.n1.val",31); //超声波					
            DC_MOTOR_PWM_Config(100); // 低速扫描
            Car_Left();  // 向左扫描
					  delay_ms(1000);
						HMI_send_number("box.n1.val",31); //超声波					
					  Car_Right(); // 向右扫描
					  delay_ms(1000);
					  Car_Left();  // 向左扫描
					  delay_ms(800);
						HMI_send_number("box.n1.val",28); //超声波					
					  Car_Right(); // 向右扫描
					  delay_ms(800);
            Car_Stop(); // 停止扫描
						HMI_send_number("box.n1.val",36); //超声波					
						HMI_send_string("box.t3.txt", "滑杆定位");
            Locate_Rle(18000,3000,CCW);//向下
						delay_ms(8000);
            HMI_send_string("box.t3.txt", "抱紧树木");
						Set_Servo_Angle(90);  // 完全抱紧
						delay_ms(400);
            HMI_send_string("box.t3.txt", "进行喷涂");
						HMI_send_number("box.n1.val",33); //超声波					
            GPIO_WriteBit(GPIOE, GPIO_Pin_3, Bit_SET); // 打开水泵
            Enable_Acceleration(1);
						Locate_Rle(54000, 3000, CW);    // 滑杆上喷涂
						delay_ms(12000);
						GPIO_WriteBit(GPIOE, GPIO_Pin_3, Bit_RESET); // 关闭水泵
						Set_Servo_Angle(25);  // 松开抱紧
            HMI_send_string("box.t3.txt", "进行返回");
						DC_MOTOR_PWM_Config(200);
						Car_Back();
            Locate_Rle(36400,3000,CCW);    // 滑杆下
            delay_ms(3500);
						HMI_send_number("box.n1.val",173); //超声波					
						Car_Back();
            delay_ms(1000);

						
						
				Car_Stop(); // 停止
						HMI_send_string("box.t3.txt", "自动流程完成");

            // 自动喷涂流程...
        }
     USART_RX_STA = 0; 

    } 
}

// 自动模式状态机
void auto_mode_state_machine(void) {
    static uint32_t state_start_time = 0; 
    static uint8_t alignment_attempts = 0; 
    static uint32_t last_vision_time = 0; 
    
    // 自动模式超时检查
    if(system_time_ms - auto_mode_start_time > AUTO_MODE_TIMEOUT) {
        emergency_stop();
        return;
    }

    switch(current_state) {
        case AUTO_SEARCHING:
            HMI_send_string("box.t3.txt", "搜索树木");
            // 低速前进
            DC_MOTOR_PWM_Config(150); // 低速
            Car_Go();
            
            // 获取K230视觉数据
            if (USART3_RX_STA & 0x8000) {
                char buf[16];
                strncpy(buf, (char*)USART3_RX_BUF, USART3_RX_STA & 0x3FFF);
                buf[USART3_RX_STA & 0x3FFF] = '\0';
                vision_center_diff = atoi(buf);
                USART3_RX_STA = 0;
                last_vision_time = system_time_ms;

                // 检测到树木（非9999）
                if(vision_center_diff != 9999) {
                    HMI_send_string("box.t3.txt", "检测到树木");
                    current_state = AUTO_APPROACHING;
                    state_start_time = system_time_ms;
                }
            }
            // 视觉数据超时处理（5秒未收到数据）
            else if(system_time_ms - last_vision_time > 5000) {
                last_vision_time = system_time_ms;
            }
            break;
            
        case AUTO_APPROACHING:
            HMI_send_string("box.t3.txt", "接近树木");
            // 获取超声波测量的前方距离
            tree_distance = measure_distance(Front);
            HMI_send_number("box.n1.val", tree_distance);   // 实时更新距离显示
            
            // 如果距离小于330cm，进入对齐状态
            if(tree_distance < 33) {
                Car_Stop();
                current_state = AUTO_ALIGNING;
                state_start_time = system_time_ms;
                alignment_attempts = 0;
                HMI_send_string("box.t3.txt", "开始视觉精确对齐");
                break;
            }
            
            // 获取K230视觉数据
            if (USART3_RX_STA & 0x8000) {
                char buf[16];
                strncpy(buf, (char*)USART3_RX_BUF, USART3_RX_STA & 0x3FFF);
                buf[USART3_RX_STA & 0x3FFF] = '\0';
                vision_center_diff = atoi(buf);
                USART3_RX_STA = 0;
                last_vision_time = system_time_ms;

                // 树木丢失处理
                if(vision_center_diff == 9999) {
                    HMI_send_string("box.t3.txt", "树木丢失");
                    current_state = AUTO_SEARCHING;
                    break;
                }
                
                // 根据视觉偏移调整方向
                if(abs(vision_center_diff) > 20) {
                    DC_MOTOR_PWM_Config(150); // 中速前进
                    
                    if(vision_center_diff < 0) {

					DC_MOTOR_PWM_Config(100); 
                        Car_Go_Left(); // 向左前方前进
                    } else {
						DC_MOTOR_PWM_Config(100); 
                        Car_Go_Right(); // 向右前方前进
                    }
                } else {
                    DC_MOTOR_PWM_Config(150); // 直行前进
                    Car_Go();
                }
            }
            
            // 接近超时处理（15秒）
            if(system_time_ms - state_start_time > 15000) {
                HMI_send_string("box.t3.txt", "接近超时");
                current_state = AUTO_SEARCHING;
            }
            // 视觉数据超时处理（2秒未收到数据）
            else if(system_time_ms - last_vision_time > 2000) {
                HMI_send_string("box.t3.txt", "视觉信号丢失");
                current_state = AUTO_SEARCHING;
            }
            break;
            
        case AUTO_ALIGNING:
            HMI_send_string("box.t3.txt", "视觉精确对齐");
            // 处理K230视觉数据
            if (USART3_RX_STA & 0x8000) {
                char buf[16];
                strncpy(buf, (char*)USART3_RX_BUF, USART3_RX_STA & 0x3FFF);
                buf[USART3_RX_STA & 0x3FFF] = '\0';
                vision_center_diff = atoi(buf);
                USART3_RX_STA = 0;
                last_vision_time = system_time_ms;

                // 特殊值9999(未检测到目标)
                if(vision_center_diff == 9999) {
                    alignment_attempts++; 
                    
                    // 启动扫描模式
                    if (last_scan_time == 0) {
                        last_scan_time = system_time_ms;
                    }
                    
                    // 扫描方向切换
                    if (system_time_ms - last_scan_time > scan_duration) {
                        current_scan_direction = (current_scan_direction == SCAN_LEFT) ? SCAN_RIGHT : SCAN_LEFT;
                        last_scan_time = system_time_ms;
                    }
 
                    // 执行扫描动作
                    DC_MOTOR_PWM_Config(80); // 低速
                    if (current_scan_direction == SCAN_LEFT) {
                        Car_Left();  // 向左扫描
                    } else {
                        Car_Right(); // 向右扫描
                    }
                } 
                // 正常偏移值
                else if(abs(vision_center_diff) > 10) { // 更精确的对齐阈值
                    DC_MOTOR_PWM_Config(80); // 设置低速
                    
                    if(vision_center_diff < 0) {
                        Car_Left();
                    } else {
                        Car_Right();
                    }
                    state_start_time = system_time_ms;
                } 
                else {
                    Car_Stop();
                    alignment_attempts = 0; // 重置尝试计数
                    current_state = AUTO_LIFTING;
                    state_start_time = system_time_ms;
                    HMI_send_string("box.t3.txt", "视觉对齐完成");
                }
                
                // 超过最大尝试次数处理
                if(alignment_attempts > MAX_ALIGN_ATTEMPTS) {
                    HMI_send_string("box.t3.txt", "对齐失败");
                    current_state = AUTO_SEARCHING;
                }
            } 
            // 5秒未收到数据超时
            else if(system_time_ms - state_start_time > 5000) {
                HMI_send_string("box.t3.txt", "视觉超时");
                current_state = AUTO_SEARCHING;
            }
            break;

        case AUTO_LIFTING:
            Enable_Acceleration(1);
            Locate_Rle(10000,3000,CCW);  // 滑铲下降
            is_motor_moving = 1;
            HMI_send_string("box.t3.txt", "滑杆定位");
            
            // 箕升完成检查
            if(system_time_ms - state_start_time > 3000) {
                is_motor_moving = 0;
                current_state = AUTO_GRIPPING;
                state_start_time = system_time_ms;
            }
            break;
            
        case AUTO_GRIPPING:
            HMI_send_string("box.t3.txt", "抱紧树木");
            if(system_time_ms - state_start_time == 0) {
                Set_Servo_Angle(25);   // 松开
            }
            else if(system_time_ms - state_start_time > 1000) {
                Set_Servo_Angle(90);  // 夹紧
				delay_ms(500);
                current_state = AUTO_PAINTING;
                state_start_time = system_time_ms;
            }
            break;
            
        case AUTO_PAINTING:
            HMI_send_string("box.t3.txt", "进行喷涂");
			delay_ms(500);
            GPIO_WriteBit(GPIOE, GPIO_Pin_3, Bit_SET); // 打开水泵
			delay_ms(1600);		
            Enable_Acceleration(1);
            Locate_Rle(40000,2800,CW);    // 喷涂动作
            is_motor_moving = 1;
			delay_ms(5000);
            // 喷涂完成检查
            if(system_time_ms - state_start_time > painting_time || 
              (system_time_ms - state_start_time > 12000)) {
                GPIO_WriteBit(GPIOE, GPIO_Pin_3, Bit_RESET); // 关闭水泵
                Set_Servo_Angle(25);  // 松开
                is_motor_moving = 0;               
				  state_start_time = system_time_ms;
                current_state = AUTO_RETREATING;
            }
            break;
            
        case AUTO_RETREATING:
            HMI_send_string("box.t3.txt", "进行返回");
            tree_distance = measure_distance(Front);  // 更新前方距离
            HMI_send_number("box.n1.val", tree_distance);  // 显示实时距离

            // 滑杆下降完成检测
            if (system_time_ms - state_start_time <= 5000) {
//                if (!is_motor_moving) {
                      Enable_Acceleration(1);
                    Locate_Rle(18000, 3000, CCW);  // 滑杆下降
					delay_ms(3000);
                 is_motor_moving = 1;
//                }
            } 
            // 开始后退
            else {
                if (is_motor_moving) {
                    is_motor_moving = 0;  // 重置电机标志
                }
                DC_MOTOR_PWM_Config(300);
                Car_Back();
                
                // 退出条件检测
//                if (tree_distance > SAFE_DISTANCE || 
				 if ( 
                    (system_time_ms - state_start_time > 10000)) 
                {
                    Car_Stop();
                    
                    // 检查是否还有树需要喷涂
                    if (tree_count > 1) {
                        tree_count--; 
                        current_state = AUTO_ROTATING; 
                        state_start_time = system_time_ms; 
                        HMI_send_string("box.t3.txt", "准备喷涂下一棵树");
                    } else {
                        // 所有树喷涂完成
						                      Enable_Acceleration(1);						
				   Locate_Rle(5000, 3000, CCW);  // 滑杆下降

                        Car_Turn_Right(); // 原地右转
                        delay_ms(4000);
                        Car_Stop();
                        release_auto_resources();
                        current_state = MANUAL_MODE;
                        HMI_send_string("moshi.t2.txt", "模式1");
                        HMI_send_string("box.t3.txt", "自动流程完成");
                    }
                }
            }
            break;
            
        case AUTO_ROTATING: 
            HMI_send_string("box.t3.txt", "检测");
            
            // 开始旋转
            DC_MOTOR_PWM_Config(200); 
            Car_Turn_Right(); 
            
            // 旋转90度（持续时间约3000ms）
            if (system_time_ms - state_start_time > 3000) {
                Car_Stop(); 
                current_state = AUTO_SEARCHING; 
                state_start_time = system_time_ms;
                HMI_send_string("box.t3.txt", "搜索下一棵树");
            }
            break;
    }
}

// 主函数
int main(void) {
    uint32_t last_system_time = 0;
    static uint32_t last_ps2_check_time = 0; 
    BEEP_Init();                
    KEY_Init();
    LED_Init();                 
    u8 key; 
    
    // 外设初始化
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    delay_init(168);
    uart_init(115200);
    Hcsr04_Init();      
    PS2_Init();         
    SG90_Init();        
    DC_MOTOR_GPIO_Config(); 
    Driver_Init();      
    uart3_init(115200); 
    
    // 电机参数配置
    DC_MOTOR_PWM_Config(shudu);
    TIM8_OPM_RCR_Init(999,168-1);
    TIM7_Int_Init(999,83);  
    TIM5_Cap_Init(0XFFFFFFFF,84-1); 
    
    // 初始设置
    Enable_Acceleration(1);
    Set_Acceleration(3500);
    GPIO_WriteBit(GPIOE, GPIO_Pin_7,Bit_RESET);
    
    // 初始化HMI显示
    HMI_send_number("guide.n0.val", shudu);
    delay_us(10);
    HMI_send_number("prog.n0.val", pinlv);
    delay_us(10);
    HMI_send_number("prog.n1.val", maicong);
    delay_us(10);
    HMI_send_number("duojichuanshu.n0.val", duojichanshu);
    HMI_send_string("moshi.t2.txt", "模式1"); 
    HMI_send_string("box.t3.txt", "状态信息");    
    BEEP = 1;
    delay_ms(200);
    BEEP = 0;

    // 时间初始化
    system_time_ms = 0;
    auto_mode_start_time = 0;
    
    while(1) {
        // 系统时间更新
        while(system_time_ms == last_system_time) {
            // 等待时间更新
        }
        last_system_time = system_time_ms;
        
        // HMI指令处理
        if (USART_RX_STA & 0x8000) {
            len = USART_RX_STA & 0x3fff;
            
            // 模式切换指令
            if (USART_RX_BUF[0] == '2') {  // 切换到模式2
                delay_ms(1000);
                HMI_send_string("moshi.t2.txt", "模式2"); 
                delay_us(5);
                HMI_send_string("box.t3.txt", "开始搜索"); 
                delay_us(5);
                current_state = AUTO_SEARCHING;  
                auto_mode_start_time = system_time_ms; 
                tree_count = 2; 
            } 
            else if (USART_RX_BUF[0] == '1') {  // 切换回手动模式
                current_state = MANUAL_MODE;
                release_auto_resources(); 
                HMI_send_string("moshi.t2.txt", "模式1");
                HMI_send_string("box.t3.txt", "手动控制"); 
            }
        }

        // 处理HMI参数设置
        HIM_chuan_shu();

        // 自动模式下的PS2中断检查
        if (current_state != MANUAL_MODE) {
            if (system_time_ms - last_ps2_check_time >= 100) {
                last_ps2_check_time = system_time_ms;
                key = PS2_DataKey();
                // 方向键中断自动模式
                if (key == PSB_PAD_UP || key == PSB_PAD_DOWN || 
                    key == PSB_PAD_LEFT || key == PSB_PAD_RIGHT) {
                    current_state = MANUAL_MODE;  
                    release_auto_resources();     
                    HMI_send_string("moshi.t2.txt", "模式1");
                    HMI_send_string("box.t3.txt", "手动干预"); 
                }
            }
        }

        // 模式处理
        if (current_state == MANUAL_MODE) {
			
			DC_MOTOR_PWM_Config(300);
            app_ps2_deal(); 
            Set_Servo_Angle(q);  
            if(q >= 90) q = 90;        
            else if (q <= SAFE_ANGLE) q = SAFE_ANGLE; 
        } 
        else {
            auto_mode_state_machine(); 
        }

        // 执行超声波测量
        hcsr04_nonblock();
    }
}

// TIM7中断处理函数（系统时间基准）
void TIM7_IRQHandler(void) {
    if(TIM_GetITStatus(TIM7, TIM_IT_Update) == SET) {
        system_time_ms++; 
        
        TIM_ClearITPendingBit(TIM7, TIM_IT_Update); 
    }
}
