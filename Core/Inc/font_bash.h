/**
 * @file font_bash.h
 * @brief BASH_UI 使用的字体声明。
 *
 * font_6x10: u8g2 格式 ASCII 字体（用于菜单和页面文字）
 * zh16x16: 中文 16x16 位图字体数据（用于中文显示）
 */

#ifndef FONT_BASH_H
#define FONT_BASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 6x10 像素 ASCII 字体（u8g2 格式，用于菜单和页面文字）。 */
extern const uint8_t font_6x10[];

/** 中文 16x16 字体数据（每字符 36 字节：4字节UTF-8头 + 32字节位图）。 */
extern const uint8_t zh16x16[][36];

/** 中文字符数量 */
#define ZH16x16_COUNT 24

/**
 * @brief 绘制单个 ASCII 字符（6x8 位图）
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param c ASCII 字符 (32-127)
 */
void Font_DrawASCIIChar(int16_t x, int16_t y, char c);

/**
 * @brief 绘制单个中文字符（16x16）
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param utf8_code 3字节 UTF-8 编码
 */
void Font_DrawChineseChar(int16_t x, int16_t y, const uint8_t *utf8_code);

/**
 * @brief 绘制包含中文和 ASCII 的混合字符串
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param str UTF-8 编码的字符串
 */
void Font_DrawMixedString(int16_t x, int16_t y, const char *str);

/** 获取混合字符串在当前字体下的显示宽度。 */
uint16_t Font_GetMixedStringWidth(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* FONT_BASH_H */
