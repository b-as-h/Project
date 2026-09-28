/**
 * @file input.h
 * @author BASH
 * @brief 旋转编码器 + 确认键输入接口。
 */

#ifndef INPUT_H
#define INPUT_H

#include "bash_ui.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化输入外设（TIM2 编码器 + PB10 确认键）。
 * TIM2 需在 CubeMX 中配置为 Encoder 模式后再调用。
 */
void Input_Init(void);

/**
 * 读取编码器增量和按键状态，返回 UI_Input。
 * encoder_delta: 正值=Down, 负值=Up, 0=无变化。
 * keys: bit2=OK（按下=1）。
 */
UI_Input Input_Read(void);

#ifdef __cplusplus
}
#endif

#endif /* INPUT_H */
