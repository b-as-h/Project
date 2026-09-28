/**
 * @file input.c
 * @author BASH
 * @brief 旋转编码器 + 确认键输入实现。
 */

#include "input.h"
#include "main.h"
#include "tim.h"
#include "gpio.h"

/*
 * 硬件定义 —— CubeMX 已配置 GPIO。
 * 编码器: TIM2 CH1/CH2 = PA0/PA1（CubeMX Encoder 模式）
 * 按键:   PB10 = EC11 确认键（内部上拉，按下=低电平）
 *         消抖由 BASH_UI 内部处理（UI_KEY_DEBOUNCE_MS）。
 */

#define CONFIRM_PORT  GPIOB
#define CONFIRM_PIN   GPIO_PIN_10

static int16_t input_last_encoder;  /**< 上次读取的编码器计数值。 */

void Input_Init(void)
{
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    input_last_encoder = (int16_t)__HAL_TIM_GET_COUNTER(&htim2);
}

UI_Input Input_Read(void)
{
    UI_Input input;
    int16_t current;
    int16_t delta;

    input.keys = 0;
    input.encoder_delta = 0;

    /* 编码器增量 */
    current = (int16_t)__HAL_TIM_GET_COUNTER(&htim2);
    delta = (int16_t)(current - input_last_encoder);
    if (delta != 0) {
        input.encoder_delta = delta;
        input_last_encoder = current;
    }

    /* PB10 确认键：内部上拉，按下读到低电平，取反后 1=按下 */
    if (HAL_GPIO_ReadPin(CONFIRM_PORT, CONFIRM_PIN) == GPIO_PIN_RESET) {
        input.keys |= UI_KEY_OK;
    }

    return input;
}
