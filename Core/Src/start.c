#include "start.h"

/* OLED相关变量定义 */
unsigned int oled_lasttime = 0;
unsigned int oled_uart_lasttime = 0;
char oled_buf[20];
char oled_uart_buf[20];
char oled_switch_buf[3] = "OFF";
char oled_encoder_buf[20] = "";
uint8_t oled_uart_flag = 0;

/* 按键相关变量定义 */
uint32_t key_lasttime = 0;
uint8_t key_last_state = 1; // 记录上一次按键状态，1=未按下(GPIO_PIN_SET)

uint32_t Count = 0; // 旋转编码器

#define count_MAX 20
uint8_t duty = 0;
/**
 * @brief OLED显示刷新
 */
void OLED_Display(void)
{
    if (HAL_GetTick() - oled_lasttime >= 50)
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
        sprintf(oled_encoder_buf, "Encoder: %d", Count);
        OLED_PrintString(0, 0, oled_buf, &font16x16, OLED_COLOR_NORMAL);
        OLED_PrintString(0, 15, oled_uart_buf, &font16x16, OLED_COLOR_NORMAL);
        OLED_PrintString(0, 30, oled_encoder_buf, &font16x16, OLED_COLOR_NORMAL);
        OLED_ShowFrame();
        oled_lasttime = HAL_GetTick();
    }
}

/**
 * @brief 按键检测与蓝牙开关控制
 */
void Key_Process(void)
{
    uint8_t key_current = HAL_GPIO_ReadPin(Bule_switch_GPIO_Port, Bule_switch_Pin);
    // 检测下降沿（从未按下变为按下）+ 消抖延时500ms
    if (key_current == GPIO_PIN_RESET && key_last_state == GPIO_PIN_SET && HAL_GetTick() - key_lasttime >= 500)
    {
        blue_switch = !blue_switch; // 0不接受蓝牙数据
        if (blue_switch)
        {
            uart_flag = 0;                                                   // 清除之前缓存的标志，避免开启蓝牙时误触发
            HAL_GPIO_WritePin(Blue_en_GPIO_Port, Blue_en_Pin, GPIO_PIN_SET); // 写1在蓝牙EN引脚让蓝牙引脚工作
            sprintf(oled_switch_buf, "ON");
        }
        else
        {
            HAL_GPIO_WritePin(Blue_en_GPIO_Port, Blue_en_Pin, GPIO_PIN_RESET);
            sprintf(oled_switch_buf, "OFF");
        }
        key_lasttime = HAL_GetTick();
    }
    key_last_state = key_current;
}

/**
 * @brief 串口数据处理（回传）
 */
void UART_Process(void)
{
    if (uart_flag == 1 && blue_switch == 1)
    {
        uart_flag = 0;
        HAL_UART_Transmit_IT(&huart1, (uint8_t *)uart_re, 2);
        oled_uart_flag = 1;
    }
}

void Encoder(void) // 旋转编码器
{
    Count = __HAL_TIM_GET_COUNTER(&htim2);
}

void Servo(void) // 舵机控制   2.5%~12.5%占空比
//                              0~180度
{
    if (Count > count_MAX)
    {
        if (Count > 60000)
        {
            Count = 0;
            __HAL_TIM_SET_COUNTER(&htim2, Count);
        }
        else
        {
            Count = count_MAX;
            __HAL_TIM_SET_COUNTER(&htim2, count_MAX);
        }
    }
    duty = (10 * Count / (float)count_MAX + 2.5) / 100 * 2000;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, duty);
    // HAL_Delay(10);
}
