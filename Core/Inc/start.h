#ifndef __START_H
#define __START_H

#include "main.h"
#include "oled.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>
#include "tim.h"

/* 变量声明 */
extern char oled_buf[20];
extern char oled_uart_buf[20];
extern char oled_switch_buf[3];
extern uint8_t oled_uart_flag;
extern char uart_re[2];
extern uint8_t uart_flag;
extern uint8_t blue_switch;
extern uint32_t key_lasttime;
extern uint8_t key_last_state;
extern uint32_t Count;

/* 函数声明 */
void Key_Process(void);
void OLED_Display(void);
void UART_Process(void);
void Encoder(void);
void Servo(void);

#endif
