#ifndef __START_H
#define __START_H

#include "main.h"
#include "oled.h"
#include "usart.h"
#include "adc.h"
#include <string.h>
#include <stdio.h>
#include "tim.h"

/* 变量声明 */
extern char oled_buf[20];
extern char oled_uart_buf[20];
extern char oled_switch_buf[4];
extern uint8_t oled_uart_flag;
extern char uart_re[2];
extern uint8_t uart_flag;
extern uint8_t blue_switch;
extern uint32_t key_lasttime_0;
extern uint32_t key_lasttime_1;
extern uint8_t key_last_state_0;
extern uint8_t key_last_state_1;
extern uint32_t Count;
extern uint8_t oled_ui;
extern uint8_t Servo_lock;
extern uint32_t servo_last_count;
extern uint8_t Lock;

/* 密码相关变量 */
#define PASSWORD_LEN 4
#define MAX_ERROR_COUNT 5
typedef enum
{
    LOCK_IDLE,  // 待机态
    LOCK_INPUT, // 输入态（转动选择数字）
    LOCK_CHECK, // 验证态
    LOCK_ALARM  // 报警态
} LockState;
extern LockState lock_state;
extern uint8_t password[PASSWORD_LEN]; // 正确密码
extern uint8_t input[PASSWORD_LEN];    // 用户输入
extern uint8_t input_index;            // 当前输入第几位
extern uint8_t select_num;             // 当前选择的数字(0-9)
extern uint8_t error_count;            // 错误次数

/* 函数声明 */
void Key_Process(void);
void OLED_Display(void);
void UART_Process(void);
void Encoder(void);
void Servo(void);
void Sensor_Process(void);

#endif
