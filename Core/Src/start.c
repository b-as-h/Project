#include "start.h"

// 构思:
// 一屏:欢迎页,门禁锁
// 二屏:雷达页面
// 三屏:ADC环境监测
// 四屏:电机风扇

/* OLED相关变量定义 */
unsigned int oled_lasttime = 0;
unsigned int oled_uart_lasttime = 0;
char oled_buf[20];
char oled_buf_name[20];
char oled_buf_num[20];
char oled_num_result[10];
char oled_uart_buf[20] = "UART1:OFF   ED";
char oled_switch_buf[4] = "OFF";
char oled_encoder_buf[20] = "";
char oled_ui_buf[20] = "";
uint8_t oled_uart_flag = 0;
uint8_t oled_ui = 1;
uint8_t key_flag_0 = 0;
uint8_t key_flag_1 = 0;

/* 按键相关变量定义 */
uint32_t key_lasttime_0 = 0;
uint32_t key_lasttime_1 = 0;
uint8_t key_last_state_0 = 1; // 记录上一次按键状态，1=未按下(GPIO_PIN_SET)
uint8_t key_last_state_1 = 0; // 外接高电平,故反过来

uint32_t Count = 0; // 旋转编码器

#define count_MAX 20
uint8_t duty = 0;
/**
 * @brief OLED显示刷新
 */
void OLED_Display(void)
{
    if (HAL_GetTick() - oled_lasttime >= 100)
    {
        if (oled_ui == 1)
        {
            OLED_NewFrame();
            sprintf(oled_buf, "OLED_Test:%d", HAL_GetTick() / 100);
            sprintf(oled_ui_buf, "       ①");
            sprintf(oled_buf_name, "BASH");
            sprintf(oled_num_result, "Right"); //
            sprintf(oled_buf_num, "PassWord:");
            OLED_PrintString(0, 0, oled_ui_buf, &font16x16, OLED_COLOR_NORMAL);
            OLED_PrintString(0, 15, oled_buf, &font16x16, OLED_COLOR_NORMAL);
            OLED_PrintString(0, 0, oled_buf_name, &font16x16, OLED_COLOR_REVERSED);
            OLED_PrintString(0, 30, oled_buf_num, &font16x16, OLED_COLOR_NORMAL);
            OLED_PrintString(90, 30, oled_num_result, &font16x16, OLED_COLOR_REVERSED);
            OLED_ShowFrame();
            oled_lasttime = HAL_GetTick();
        }
        if (oled_ui == 2)
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
            sprintf(oled_ui_buf, "       ②");
            sprintf(oled_encoder_buf, "Angle: %dº", Count * 180 / count_MAX);
            OLED_PrintString(0, 0, oled_ui_buf, &font16x16, OLED_COLOR_NORMAL);
            OLED_PrintString(0, 15, oled_uart_buf, &font16x16, OLED_COLOR_NORMAL);
            OLED_PrintString(0, 30, oled_encoder_buf, &font16x16, OLED_COLOR_NORMAL);
            OLED_ShowFrame();
            oled_lasttime = HAL_GetTick();
        }
    }
}

/**
 * @brief 按键检测与蓝牙开关控制
 */
void Key_Process(void)
{
    uint8_t key_current_0 = HAL_GPIO_ReadPin(Bule_switch_GPIO_Port, Bule_switch_Pin);
    uint8_t key_current_1 = HAL_GPIO_ReadPin(oled_ui_GPIO_Port, oled_ui_Pin);
    // 检测下降沿（从未按下变为按下）+ 消抖延时500ms
    if (key_current_0 == GPIO_PIN_RESET && key_last_state_0 == GPIO_PIN_SET && HAL_GetTick() - key_lasttime_0 >= 500)
    {
        key_flag_0 = 1;
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
        key_lasttime_0 = HAL_GetTick();
    }

    key_last_state_0 = key_current_0;

    if (key_current_1 == GPIO_PIN_SET && key_last_state_1 == GPIO_PIN_RESET && HAL_GetTick() - key_lasttime_1 >= 500)
    {
        key_flag_1 = 1;
        oled_ui++;
        if (oled_ui > 2)
        {
            oled_ui = 1;
        }
        key_lasttime_1 = HAL_GetTick();
    }

    key_last_state_1 = key_current_1;
}

/**
 * @brief 串口数据处理（回传）
 */
void UART_Process(void)
{
    if (uart_flag == 1 && blue_switch == 1 && oled_ui == 2)
    {
        uart_flag = 0;
        HAL_UART_Transmit_IT(&huart1, (uint8_t *)uart_re, 2);
        oled_uart_flag = 1;
    }
}

void Encoder(void) // 旋转编码器
{
    if (oled_ui == 2)
    {
        Count = __HAL_TIM_GET_COUNTER(&htim2);
    }
}

void Servo(void) // 舵机控制   2.5%~12.5%占空比
//                              0~180度
{
    if (oled_ui == 2)
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
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, duty);
    }
}
