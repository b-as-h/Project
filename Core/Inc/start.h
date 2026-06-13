#ifndef __START_H
#define __START_H

#include "main.h"
#include "oled.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

/* 变量声明 */
extern char oled_buf[20];
extern char oled_uart_buf[20];
extern char oled_switch_buf[3];
extern uint8_t oled_uart_flag;
extern char uart_re[2];
extern uint8_t uart_flag;
extern uint8_t blue_switch;

/* 函数声明 */
void start_oled_display(void);
void start_uart_process(void);

#endif
