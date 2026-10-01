/**
 ******************************************************************************
 * @file    app_main.c
 * @brief   应用层 —— 迁移自 USER/main.c(815 行)
 *
 * 【本次迁移做的事】
 *
 *   1. 初始化段里的 xxx_Init() 全部换掉(细节见 app_init()):
 *        delay_init(168)          -> delay_init()          (DWT 版,不再需要传主频)
 *        uart_init(115200)        -> bsp_usart1_init()
 *        uart3_init(115200)       -> bsp_usart3_init()
 *        PS2_Init()               -> bsp_ps2_init()
 *        SG90_Init()              -> bsp_servo_init()
 *        DC_MOTOR_GPIO_Config()   -> bsp_motor_init()
 *        TIM7_Int_Init(999,83)    -> bsp_tick_init()
 *        TIM5_Cap_Init(...)       -> bsp_ultrasonic_init()
 *        TIM8_OPM_RCR_Init(...)   -> bsp_stepper_init()
 *        Driver_Init()            -> 删除(只配 GPIO,CubeMX 已做)
 *        Hcsr04_Init()            -> 删除(同上)
 *        BEEP_Init/KEY_Init/LED_Init() -> 删除(同上)
 *        NVIC_PriorityGroupConfig() -> 删除(CubeMX 统一设为 Group_4)
 *
 *   2. 引脚操作 API:
 *        GPIO_WriteBit(GPIOE, GPIO_Pin_3, Bit_SET) -> HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET)
 *        GPIO_WriteBit(GPIOE, GPIO_Pin_7, Bit_RESET) -> ... GPIO_PIN_RESET
 *      位带别名(BEEP / Left_TRIG_Send 等)不变。
 *
 *   3. main() 拆成 app_init() + app_loop(),由 CubeMX 的 main.c 通过
 *      USER CODE 区调用(那样才不会被重新生成覆盖)。
 *
 *   4. TIM7_IRQHandler 删除 —— 已迁到 bsp_tick.c 的 bsp_tick_on_period_elapsed()。
 *
 *   5. system_time_ms 删除定义 —— 现在归 bsp_tick.c 所有,这里通过 bsp_tick.h 取用。
 *
 * 【业务逻辑本身一字未改】:状态机、HMI 协议、PS2 处理、超声波轮询流程全部照搬。
 ******************************************************************************
 */

#include "app_main.h"

#include "bsp_pin.h"
#include "bsp_tick.h"
#include "bsp_usart1.h"
#include "bsp_usart3.h"
#include "bsp_ps2.h"
#include "bsp_motor.h"
#include "bsp_servo.h"
#include "bsp_stepper.h"
#include "bsp_ultrasonic.h"
#include "delay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- 业务参数(原 main.c 顶部,值未改) ---- */
#define MAX_ALIGN_ATTEMPTS 3        /* 视觉对准最大尝试次数 */
#define AUTO_MODE_TIMEOUT  120000   /* 自动模式超时(120 秒) */
#define SAFE_DISTANCE      100      /* 安全距离(cm) */
#define SAFE_ANGLE         25       /* 舵机安全角度 */

/* 前置声明 */
static void HIM_chuan_shu(void);
static void release_auto_resources(void);
static void emergency_stop(void);
static void app_ps2_deal(void);
static void auto_mode_state_machine(void);
static void hcsr04_nonblock(void);
static int  measure_distance(u8 sensor);

/* ---- 系统状态枚举 ---- */
typedef enum {
    MANUAL_MODE,          /* 手动模式 */
    AUTO_SEARCHING,       /* 自动:搜索树木 */
    AUTO_APPROACHING,     /* 自动:接近树木 */
    AUTO_ALIGNING,        /* 自动:视觉对准 */
    AUTO_LIFTING,         /* 自动:抬升定位 */
    AUTO_GRIPPING,        /* 自动:夹紧 */
    AUTO_PAINTING,        /* 自动:涂白 */
    AUTO_RETREATING,      /* 自动:退回安全位 */
    AUTO_ROTATING         /* 自动:旋转 */
} SystemState;

/* ---- 超声波轮询状态 ---- */
typedef enum {
    US_IDLE,
    US_LEFT_TRIG,  US_LEFT_WAIT,
    US_FRONT_TRIG, US_FRONT_WAIT,
    US_RIGHT_TRIG, US_RIGHT_WAIT
} UltraState;

typedef enum { SCAN_LEFT, SCAN_RIGHT } ScanDirection;

/* ===========================================================================
 * 全局变量(与原工程同名)
 * =========================================================================== */
static ScanDirection current_scan_direction = SCAN_LEFT;
static uint32_t      last_scan_time = 0;
static uint32_t      scan_duration  = 500;     /* 扫描换向间隔(ms) */
//static u8            ProtocolString[80] = {0};
//static uint8_t       i, j;
static uint8_t       q = SAFE_ANGLE;           /* 舵机角度 */
static u8            len;
//static uint32_t      last_count = 0;
static volatile uint8_t is_motor_moving = 0;

static SystemState current_state = MANUAL_MODE;
static UltraState  us_state      = US_IDLE;
static uint32_t    us_last_time  = 0;

/* 视觉与距离数据 */
static int vision_center_diff = 0;    /* K230 视觉中心偏移 */
static int tree_distance      = 0;    /* 前方树干距离 */
//static int target_height      = 30000;
static int painting_time      = 12000;

/* 运动参数(可用 HMI 调) */
static uint16_t shudu        = 300;    /* 电机速度 */
static uint16_t maicong      = 10000;  /* 脉冲总数 */
static uint16_t pinlv        = 3000;   /* 脉冲频率 */
static uint16_t duojichanshu = 3;      /* 舵机角度变化量 */

/* 三路超声波回波计数值 */
static long long ch1 = 0, ch2 = 0, ch3 = 0;

static uint32_t auto_mode_start_time = 0;
static uint8_t  tree_count = 2;

/* ===========================================================================
 * HMI(串口屏)发送 —— 走 printf,printf 经 fputc 重定向到 USART1
 * =========================================================================== */
static void HMI_send_string(char *name, char *showdata)
{
    printf("%s=\"%s\"\xff\xff\xff", name, showdata);
}

static void HMI_send_number(char *name, int num)
{
    printf("%s=%d\xff\xff\xff", name, num);
}

/* ===========================================================================
 * 距离换算
 * =========================================================================== */
static int measure_distance(u8 sensor)
{
    switch (sensor) {
        case Left:  return (int)(ch1 * 0.017);
        case Front: return (int)(ch2 * 0.017);
        case Right: return (int)(ch3 * 0.017);
        default:    return 0;
    }
}

/* ===========================================================================
 * 超声波非阻塞轮询(逻辑逐字保留)
 * =========================================================================== */
static void hcsr04_nonblock(void)
{
    switch (us_state) {
        case US_IDLE:
            us_state = US_LEFT_TRIG;
            break;

        case US_LEFT_TRIG:
            Trig_start(Left);
            us_state = US_LEFT_WAIT;
            us_last_time = system_time_ms;
            break;

        case US_LEFT_WAIT:
            if (TIM5CH1_CAPTURE_STA & 0X80) {
                ch1  = TIM5CH1_CAPTURE_STA & 0X3F;
                ch1 *= 0XFFFFFFFF;
                ch1 += TIM5CH1_CAPTURE_VAL;
                TIM5CH1_CAPTURE_STA = 0;
                us_state = US_FRONT_TRIG;
            } else if (system_time_ms - us_last_time > 50) {
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
            if (TIM5CH2_CAPTURE_STA & 0X80) {
                ch2  = TIM5CH2_CAPTURE_STA & 0X3F;
                ch2 *= 0XFFFFFFFF;
                ch2 += TIM5CH2_CAPTURE_VAL;
                TIM5CH2_CAPTURE_STA = 0;
                us_state = US_RIGHT_TRIG;
            } else if (system_time_ms - us_last_time > 50) {
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
            if (TIM5CH3_CAPTURE_STA & 0X80) {
                ch3  = TIM5CH3_CAPTURE_STA & 0X3F;
                ch3 *= 0XFFFFFFFF;
                ch3 += TIM5CH3_CAPTURE_VAL;
                TIM5CH3_CAPTURE_STA = 0;
                us_state = US_IDLE;
            } else if (system_time_ms - us_last_time > 50) {
                ch3 = 0;
                us_state = US_IDLE;
            }
            break;

        default:
            break;
    }
}

/* ===========================================================================
 * 资源释放 / 急停
 * =========================================================================== */
static void release_auto_resources(void)
{
    Car_Stop();
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET);   /* 关水泵 */
    Set_Servo_Angle(SAFE_ANGLE);
}

static void emergency_stop(void)
{
    release_auto_resources();
    current_state = MANUAL_MODE;
    HMI_send_string("box.t3.txt", "紧急停止");
}

/* ===========================================================================
 * PS2 手柄处理(逻辑逐字保留)
 * =========================================================================== */
static void app_ps2_deal(void)
{
    u8 PS2_KEY;

    delay_ms(30);                       /* 去抖 */
    PS2_KEY = PS2_DataKey();

    switch (PS2_KEY) {
        case 0:             Car_Stop();  break;
        case PSB_PAD_UP:    Car_Go();    break;
        case PSB_PAD_RIGHT: Car_Right(); break;
        case PSB_PAD_DOWN:  Car_Back();  break;
        case PSB_PAD_LEFT:  Car_Left();  break;
        case PSB_L2: HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET); break;  /* 关水泵 */
        case PSB_R2: q += duojichanshu;  break;
        case PSB_L1: HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);   break;  /* 开水泵 */
        case PSB_R1: q -= 5;             break;
        case PSB_TRIANGLE: Locate_Rle(maicong, pinlv, CW);  break;
        case PSB_CROSS:    Locate_Rle(maicong, pinlv, CCW); break;
        default: break;
    }
}

/* ===========================================================================
 * HMI 指令解析('3'~'9' / '@' 调参数;'A' 触发一次性自动涂白流程)
 * =========================================================================== */
static void HIM_chuan_shu(void)
{
    if (USART_RX_STA & 0x8000) {
        len = USART_RX_STA & 0x3fff;

        if (USART_RX_BUF[0] == '3') {
            shudu += 50;
            if (shudu > 800) shudu = 800;
            DC_MOTOR_PWM_Config(shudu);
            HMI_send_number("guide.n0.val", shudu);
        }
        else if (USART_RX_BUF[0] == '4') {
            shudu -= 50;
            if (shudu < 100) shudu = 100;
            DC_MOTOR_PWM_Config(shudu);
            HMI_send_number("guide.n0.val", shudu);
        }
        else if (USART_RX_BUF[0] == '5') {
            pinlv += 200;
            HMI_send_number("prog.n0.val", pinlv);
        }
        else if (USART_RX_BUF[0] == '6') {
            pinlv -= 200;
            if (pinlv < 400) pinlv = 400;
            HMI_send_number("prog.n0.val", pinlv);
        }
        else if (USART_RX_BUF[0] == '7') {
            maicong += 800;
            HMI_send_number("prog.n1.val", maicong);
        }
        else if (USART_RX_BUF[0] == '8') {
            maicong -= 800;
            if (maicong < 400) maicong = 400;
            HMI_send_number("prog.n1.val", maicong);
        }
        else if (USART_RX_BUF[0] == '9') {
            duojichanshu += 1;
            HMI_send_number("duojichuanshu.n0.val", duojichanshu);
        }
        else if (USART_RX_BUF[0] == '@') {
            duojichanshu -= 1;
            if (duojichanshu < 1) duojichanshu = 1;
            HMI_send_number("duojichuanshu.n0.val", duojichanshu);
        }
        else if (USART_RX_BUF[0] == 'A') {
            /* ---- 一次性完整自动涂白流程(两棵树)---- */
            /* ===== 第 1 棵 ===== */
            Set_Servo_Angle(25);
            HMI_send_string("box.t3.txt", "搜索树木");
            HMI_send_number("box.n1.val", 152);
            DC_MOTOR_PWM_Config(300);
            Car_Go();
            HMI_send_number("box.n1.val", 148);
            delay_ms(2000);

            HMI_send_string("box.t3.txt", "接近树木");
            HMI_send_number("box.n1.val", 77);
            DC_MOTOR_PWM_Config(180);
            Car_Go();
            delay_ms(1500);

            HMI_send_string("box.t3.txt", "视觉对齐");
            HMI_send_number("box.n1.val", 31);
            DC_MOTOR_PWM_Config(100);
            Car_Left();
            delay_ms(1000);
            HMI_send_number("box.n1.val", 33);
            Car_Right();
            delay_ms(1000);
            Car_Left();
            delay_ms(800);
            HMI_send_number("box.n1.val", 29);
            Car_Right();
            delay_ms(800);
            Car_Stop();
            HMI_send_number("box.n1.val", 32);

            HMI_send_string("box.t3.txt", "抬升定位");
            Locate_Rle(18000, 3000, CCW);
            delay_ms(8000);

            HMI_send_string("box.t3.txt", "夹紧树木");
            Set_Servo_Angle(90);
            delay_ms(400);

            HMI_send_string("box.t3.txt", "开始涂白");
            HMI_send_number("box.n1.val", 34);
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);   /* 开水泵 */
            Enable_Acceleration(1);
            Locate_Rle(54000, 3000, CW);
            delay_ms(12000);
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET); /* 关水泵 */
            Set_Servo_Angle(25);

            HMI_send_string("box.t3.txt", "运行返回");
            DC_MOTOR_PWM_Config(200);
            Car_Back();
            Locate_Rle(36400, 3000, CCW);
            delay_ms(3500);
            HMI_send_number("box.n1.val", 163);
            Car_Back();
            delay_ms(1000);

            DC_MOTOR_PWM_Config(100);
            Car_Right();
            delay_ms(5000);
            Car_Stop();
            delay_ms(2000);

            /* ===== 第 2 棵 ===== */
            Set_Servo_Angle(25);
            HMI_send_string("box.t3.txt", "搜索树木");
            HMI_send_number("box.n1.val", 152);
            DC_MOTOR_PWM_Config(300);
            Car_Go();
            HMI_send_number("box.n1.val", 148);
            delay_ms(2000);

            HMI_send_string("box.t3.txt", "接近树木");
            HMI_send_number("box.n1.val", 77);
            DC_MOTOR_PWM_Config(180);
            Car_Go();
            delay_ms(1500);

            HMI_send_string("box.t3.txt", "视觉对齐");
            HMI_send_number("box.n1.val", 31);
            DC_MOTOR_PWM_Config(100);
            Car_Left();
            delay_ms(1000);
            HMI_send_number("box.n1.val", 31);
            Car_Right();
            delay_ms(1000);
            Car_Left();
            delay_ms(800);
            HMI_send_number("box.n1.val", 28);
            Car_Right();
            delay_ms(800);
            Car_Stop();
            HMI_send_number("box.n1.val", 36);

            HMI_send_string("box.t3.txt", "抬升定位");
            Locate_Rle(18000, 3000, CCW);
            delay_ms(8000);

            HMI_send_string("box.t3.txt", "夹紧树木");
            Set_Servo_Angle(90);
            delay_ms(400);

            HMI_send_string("box.t3.txt", "开始涂白");
            HMI_send_number("box.n1.val", 33);
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);
            Enable_Acceleration(1);
            Locate_Rle(54000, 3000, CW);
            delay_ms(12000);
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET);
            Set_Servo_Angle(25);

            HMI_send_string("box.t3.txt", "运行返回");
            DC_MOTOR_PWM_Config(200);
            Car_Back();
            Locate_Rle(36400, 3000, CCW);
            delay_ms(3500);
            HMI_send_number("box.n1.val", 173);
            Car_Back();
            delay_ms(1000);

            Car_Stop();
            HMI_send_string("box.t3.txt", "自动流程完成");
        }

        USART_RX_STA = 0;
    }
}

/* ===========================================================================
 * 自动模式状态机(逻辑逐字保留)
 * =========================================================================== */
static void auto_mode_state_machine(void)
{
    static uint32_t state_start_time   = 0;
    static uint8_t  alignment_attempts = 0;
    static uint32_t last_vision_time   = 0;

    /* 自动模式总超时 */
    if (system_time_ms - auto_mode_start_time > AUTO_MODE_TIMEOUT) {
        emergency_stop();
        return;
    }

    switch (current_state) {
        case AUTO_SEARCHING:
            HMI_send_string("box.t3.txt", "搜索树木");
            DC_MOTOR_PWM_Config(150);
            Car_Go();

            if (USART3_RX_STA & 0x8000) {
                char buf[16];
                strncpy(buf, (char *)USART3_RX_BUF, USART3_RX_STA & 0x3FFF);
                buf[USART3_RX_STA & 0x3FFF] = '\0';
                vision_center_diff = atoi(buf);
                USART3_RX_STA = 0;
                last_vision_time = system_time_ms;

                if (vision_center_diff != 9999) {   /* 9999 = 未检测到目标 */
                    HMI_send_string("box.t3.txt", "检测到树木");
                    current_state = AUTO_APPROACHING;
                    state_start_time = system_time_ms;
                }
            } else if (system_time_ms - last_vision_time > 5000) {
                last_vision_time = system_time_ms;   /* 视觉数据超时,刷新计时 */
            }
            break;

        case AUTO_APPROACHING:
            HMI_send_string("box.t3.txt", "接近树木");
            tree_distance = measure_distance(Front);
            HMI_send_number("box.n1.val", tree_distance);

            if (tree_distance < 33) {
                Car_Stop();
                current_state = AUTO_ALIGNING;
                state_start_time = system_time_ms;
                alignment_attempts = 0;
                HMI_send_string("box.t3.txt", "开始视觉精确对准");
                break;
            }

            if (USART3_RX_STA & 0x8000) {
                char buf[16];
                strncpy(buf, (char *)USART3_RX_BUF, USART3_RX_STA & 0x3FFF);
                buf[USART3_RX_STA & 0x3FFF] = '\0';
                vision_center_diff = atoi(buf);
                USART3_RX_STA = 0;
                last_vision_time = system_time_ms;

                if (vision_center_diff == 9999) {     /* 目标丢失 */
                    HMI_send_string("box.t3.txt", "树木丢失");
                    current_state = AUTO_SEARCHING;
                    break;
                }

                if (abs(vision_center_diff) > 20) {
                    DC_MOTOR_PWM_Config(150);
                    if (vision_center_diff < 0) {
                        DC_MOTOR_PWM_Config(100);
                        Car_Go_Left();
                    } else {
                        DC_MOTOR_PWM_Config(100);
                        Car_Go_Right();
                    }
                } else {
                    DC_MOTOR_PWM_Config(150);
                    Car_Go();
                }
            }

            if (system_time_ms - state_start_time > 15000) {
                HMI_send_string("box.t3.txt", "接近超时");
                current_state = AUTO_SEARCHING;
            } else if (system_time_ms - last_vision_time > 2000) {
                HMI_send_string("box.t3.txt", "视觉信号丢失");
                current_state = AUTO_SEARCHING;
            }
            break;

        case AUTO_ALIGNING:
            HMI_send_string("box.t3.txt", "视觉精确对准");
            if (USART3_RX_STA & 0x8000) {
                char buf[16];
                strncpy(buf, (char *)USART3_RX_BUF, USART3_RX_STA & 0x3FFF);
                buf[USART3_RX_STA & 0x3FFF] = '\0';
                vision_center_diff = atoi(buf);
                USART3_RX_STA = 0;
                last_vision_time = system_time_ms;

                if (vision_center_diff == 9999) {     /* 未检测到目标 -> 摆动扫描 */
                    alignment_attempts++;
                    if (last_scan_time == 0) {
                        last_scan_time = system_time_ms;
                    }
                    if (system_time_ms - last_scan_time > scan_duration) {
                        current_scan_direction = (current_scan_direction == SCAN_LEFT) ? SCAN_RIGHT : SCAN_LEFT;
                        last_scan_time = system_time_ms;
                    }
                    DC_MOTOR_PWM_Config(80);
                    if (current_scan_direction == SCAN_LEFT) {
                        Car_Left();
                    } else {
                        Car_Right();
                    }
                } else if (abs(vision_center_diff) > 10) {
                    DC_MOTOR_PWM_Config(80);
                    if (vision_center_diff < 0) {
                        Car_Left();
                    } else {
                        Car_Right();
                    }
                    state_start_time = system_time_ms;
                } else {
                    Car_Stop();
                    alignment_attempts = 0;
                    current_state = AUTO_LIFTING;
                    state_start_time = system_time_ms;
                    HMI_send_string("box.t3.txt", "视觉对准完成");
                }

                if (alignment_attempts > MAX_ALIGN_ATTEMPTS) {
                    HMI_send_string("box.t3.txt", "对准失败");
                    current_state = AUTO_SEARCHING;
                }
            } else if (system_time_ms - state_start_time > 5000) {
                HMI_send_string("box.t3.txt", "视觉超时");
                current_state = AUTO_SEARCHING;
            }
            break;

        case AUTO_LIFTING:
            Enable_Acceleration(1);
            Locate_Rle(10000, 3000, CCW);
            is_motor_moving = 1;
            HMI_send_string("box.t3.txt", "抬升定位");

            if (system_time_ms - state_start_time > 3000) {
                is_motor_moving = 0;
                current_state = AUTO_GRIPPING;
                state_start_time = system_time_ms;
            }
            break;

        case AUTO_GRIPPING:
            HMI_send_string("box.t3.txt", "夹紧树木");
            if (system_time_ms - state_start_time == 0) {
                Set_Servo_Angle(25);
            } else if (system_time_ms - state_start_time > 1000) {
                Set_Servo_Angle(90);
                delay_ms(500);
                current_state = AUTO_PAINTING;
                state_start_time = system_time_ms;
            }
            break;

        case AUTO_PAINTING:
            HMI_send_string("box.t3.txt", "开始涂白");
            delay_ms(500);
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);   /* 开水泵 */
            delay_ms(1600);
            Enable_Acceleration(1);
            Locate_Rle(40000, 2800, CW);
            is_motor_moving = 1;
            delay_ms(5000);

            if (system_time_ms - state_start_time > (uint32_t)painting_time ||
                system_time_ms - state_start_time > 12000) {
                HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET);  /* 关水泵 */
                Set_Servo_Angle(25);
                is_motor_moving = 0;
                state_start_time = system_time_ms;
                current_state = AUTO_RETREATING;
            }
            break;

        case AUTO_RETREATING:
            HMI_send_string("box.t3.txt", "运行返回");
            tree_distance = measure_distance(Front);
            HMI_send_number("box.n1.val", tree_distance);

            if (system_time_ms - state_start_time <= 5000) {
                Enable_Acceleration(1);
                Locate_Rle(18000, 3000, CCW);
                delay_ms(3000);
                is_motor_moving = 1;
            } else {
                if (is_motor_moving) {
                    is_motor_moving = 0;
                }
                DC_MOTOR_PWM_Config(300);
                Car_Back();

                if (system_time_ms - state_start_time > 10000) {
                    Car_Stop();

                    if (tree_count > 1) {
                        tree_count--;
                        current_state = AUTO_ROTATING;
                        state_start_time = system_time_ms;
                        HMI_send_string("box.t3.txt", "准备涂下一棵树");
                    } else {
                        Enable_Acceleration(1);
                        Locate_Rle(5000, 3000, CCW);

                        Car_Turn_Right();
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
            HMI_send_string("box.t3.txt", "旋转");
            DC_MOTOR_PWM_Config(200);
            Car_Turn_Right();

            if (system_time_ms - state_start_time > 3000) {
                Car_Stop();
                current_state = AUTO_SEARCHING;
                state_start_time = system_time_ms;
                HMI_send_string("box.t3.txt", "搜索下一棵树");
            }
            break;

        default:
            break;
    }
}

/* ===========================================================================
 * 应用初始化(原 main() 开头的初始化段)
 * =========================================================================== */
void app_init(void)
{
    /* 延时模块要用 DWT,必须在时钟配置好之后初始化 */
    delay_init();

    /* 各外设启动(GPIO/时钟/参数都已由 CubeMX 配好,这里只是"开动") */
    bsp_usart1_init();
    bsp_usart3_init();
    bsp_ps2_init();
    bsp_servo_init();
    bsp_motor_init();
    bsp_tick_init();
    bsp_ultrasonic_init();
    bsp_stepper_init();

    /* 电机与驱动器参数 */
    DC_MOTOR_PWM_Config(shudu);
    Enable_Acceleration(1);
    Set_Acceleration(3500);

    /* 原工程在这里写 GPIOC/GPIOE 的 Pin_7 —— 该脚随后被 TIM8 复用为 CH2,
     * 所以这行实际不产生引脚动作。忠实保留。 */
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_7, GPIO_PIN_RESET);

    /* HMI 初始界面 */
    HMI_send_number("guide.n0.val", shudu);           delay_us(10);
    HMI_send_number("prog.n0.val", pinlv);            delay_us(10);
    HMI_send_number("prog.n1.val", maicong);          delay_us(10);
    HMI_send_number("duojichuanshu.n0.val", duojichanshu); delay_us(10);
    HMI_send_string("moshi.t2.txt", "模式1");
    HMI_send_string("box.t3.txt", "状态信息");

    BEEP = 1;
    delay_ms(200);
    BEEP = 0;

    system_time_ms = 0;
    auto_mode_start_time = 0;
}

/* ===========================================================================
 * 主循环体(原 while(1) 的内容)
 * =========================================================================== */
void app_loop(void)
{
    static uint32_t last_system_time = 0;
    static uint32_t last_ps2_check_time = 0;   /* 原工程是 main() 里的 static 局部 */
    u8 key;

    /* 等下一个 1ms 节拍 —— 这是原工程"定时器驱动"模式的节拍同步。
     * 交付 B 重构任务时,这里会换成 vTaskDelayUntil。 */
    while (system_time_ms == last_system_time) {
        /* 空转等节拍 */
    }
    last_system_time = system_time_ms;

    /* ---- HMI 模式切换指令 ---- */
    if (USART_RX_STA & 0x8000) {
        len = USART_RX_STA & 0x3fff;

        if (USART_RX_BUF[0] == '2') {           /* 切到自动模式 */
            delay_ms(1000);
            HMI_send_string("moshi.t2.txt", "模式2");
            delay_us(5);
            HMI_send_string("box.t3.txt", "开始运行");
            delay_us(5);
            current_state = AUTO_SEARCHING;
            auto_mode_start_time = system_time_ms;
            tree_count = 2;
        }
        else if (USART_RX_BUF[0] == '1') {      /* 切到手动模式 */
            current_state = MANUAL_MODE;
            release_auto_resources();
            HMI_send_string("moshi.t2.txt", "模式1");
            HMI_send_string("box.t3.txt", "手动运行");
        }
    }

    /* ---- HMI 参数指令 ---- */
    HIM_chuan_shu();

    /* ---- 自动模式下的 PS2 中断检测(每 100ms 查一次)---- */
    if (current_state != MANUAL_MODE) {
        if (system_time_ms - last_ps2_check_time >= 100) {
            last_ps2_check_time = system_time_ms;
            key = PS2_DataKey();
            if (key == PSB_PAD_UP || key == PSB_PAD_DOWN ||
                key == PSB_PAD_LEFT || key == PSB_PAD_RIGHT) {
                current_state = MANUAL_MODE;
                release_auto_resources();
                HMI_send_string("moshi.t2.txt", "模式1");
                HMI_send_string("box.t3.txt", "手动预备");
            }
        }
    }

    /* ---- 模式分派 ---- */
    if (current_state == MANUAL_MODE) {
        DC_MOTOR_PWM_Config(300);
        app_ps2_deal();
        Set_Servo_Angle(q);
        if (q >= 90) {
            q = 90;
        } else if (q <= SAFE_ANGLE) {
            q = SAFE_ANGLE;
        }
    } else {
        auto_mode_state_machine();
    }

    /* ---- 超声波轮询 ---- */
    hcsr04_nonblock();
}
