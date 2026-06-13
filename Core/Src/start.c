#include "start.h"

/* OLED相关变量定义 */
unsigned int oled_lasttime = 0;
unsigned int oled_uart_lasttime = 0;
char oled_buf[20];
char oled_uart_buf[20];
char oled_switch_buf[3] = "OFF";
uint8_t oled_uart_flag = 0;

/**
 * @brief OLED显示刷新
 */
void start_oled_display(void)
{
    if (HAL_GetTick() - oled_lasttime >= 1000)
    {
        OLED_NewFrame();
        if (oled_uart_flag == 1 && HAL_GetTick() - oled_uart_lasttime >= 100) // 正在发送
        {
            sprintf(oled_uart_buf, "UART1:%s   ING", oled_switch_buf);
            oled_uart_lasttime = HAL_GetTick();
            oled_uart_flag = 0;
        }
        else if (oled_uart_flag == 0 && HAL_GetTick() - oled_uart_lasttime >= 1000)
        {
            sprintf(oled_uart_buf, "UART1:%s   ED", oled_switch_buf);
        }
        sprintf(oled_buf, "OLED_Test:%d", HAL_GetTick() / 1000);
        OLED_PrintString(0, 0, oled_buf, &font16x16, OLED_COLOR_NORMAL);
        OLED_PrintString(0, 15, oled_uart_buf, &font16x16, OLED_COLOR_NORMAL);
        OLED_ShowFrame();
        oled_lasttime = HAL_GetTick();
    }
}

/**
 * @brief 串口数据处理（回传）
 */
void start_uart_process(void)
{
    if (uart_flag == 1 && blue_switch == 1)
    {
        uart_flag = 0;
        HAL_UART_Transmit_IT(&huart1, (uint8_t *)uart_re, 2);
        oled_uart_flag = 1;
    }
}
