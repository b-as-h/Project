/**
 * @file    oled.h
 * @brief   SSD1306 OLED驱动头文件 (I2C接口, 128x64分辨率)
 * @note    适用于波特律动·keysking的STM32学习套件
 * @note    使用流程:
 *          1. 初始化I2C后调用 OLED_Init() 初始化屏幕
 *          2. 调用 OLED_NewFrame() 清空显存, 开始绘制新一帧
 *          3. 调用 OLED_DrawXXX() / OLED_PrintXXX() 系列函数绘制内容到显存
 *          4. 调用 OLED_ShowFrame() 将显存数据刷新到屏幕
 *
 * @example 基本使用示例:
 * @code
 *   OLED_Init();                          // 初始化OLED
 *   OLED_NewFrame();                      // 开始新一帧
 *   OLED_DrawCircle(64, 32, 20, OLED_COLOR_NORMAL);  // 画圆
 *   OLED_PrintString(0, 0, "你好", &font16x16, OLED_COLOR_NORMAL); // 显示中文
 *   OLED_ShowFrame();                     // 刷新到屏幕
 * @endcode
 */

#ifndef __OLED_H__
#define __OLED_H__

#include "font.h"
#include "main.h"
#include "string.h"

/**
 * @brief OLED颜色/显示模式枚举
 * @note  控制像素点亮/灭, 或者整体正常/反色显示
 */
typedef enum {
    OLED_COLOR_NORMAL  = 0, ///< 正常模式: 黑底白字 (像素点亮=白色)
    OLED_COLOR_REVERSED = 1 ///< 反色模式: 白底黑字 (像素点亮=黑色)
} OLED_ColorMode;

// ========================== 初始化与显示控制 ==========================

/**
 * @brief  初始化OLED屏幕 (SSD1306)
 * @note   必须在I2C初始化之后调用
 * @note   建议在上电后延时20ms再调用, 等待OLED完成上电
 * @note   调用后屏幕自动清屏并开启显示
 */
void OLED_Init(void);

/**
 * @brief  开启OLED显示
 * @note   使能电荷泵并点亮屏幕, 从睡眠模式唤醒
 */
void OLED_DisPlay_On(void);

/**
 * @brief  关闭OLED显示
 * @note   关闭电荷泵并关闭屏幕, 进入低功耗模式
 * @note   屏幕关闭后显存内容仍保留, 重新开启后可恢复显示
 */
void OLED_DisPlay_Off(void);

// ====================== 显存操作与帧刷新 ======================

/**
 * @brief  清空显存, 开始绘制新的一帧
 * @note   每次绘制前必须调用, 否则会与上一帧内容叠加
 * @note   此函数只操作显存(OLED_GRAM数组), 不会刷新到屏幕
 */
void OLED_NewFrame(void);

/**
 * @brief  将显存内容刷新到OLED屏幕
 * @note   此函数通过I2C将OLED_GRAM数组的数据发送到SSD1306
 * @note   刷新整个屏幕(128x64像素), 约需发送1024字节
 */
void OLED_ShowFrame(void);

/**
 * @brief  设置单个像素点
 * @param  x      横坐标 (列), 取值范围: 0 ~ 127
 * @param  y      纵坐标 (行), 取值范围: 0 ~ 63
 * @param  color  像素颜色模式
 *                - OLED_COLOR_NORMAL:  点亮像素(白色)
 *                - OLED_COLOR_REVERSED: 熄灭像素(黑色)
 * @note   坐标原点在屏幕左上角, x向右增大, y向下增大
 * @note   超出屏幕范围的坐标会被忽略
 */
void OLED_SetPixel(uint8_t x, uint8_t y, OLED_ColorMode color);

// ========================= 图形绘制函数 =========================

/**
 * @brief  绘制直线段 (Bresenham算法)
 * @param  x1    起点横坐标, 取值范围: 0 ~ 127
 * @param  y1    起点纵坐标, 取值范围: 0 ~ 63
 * @param  x2    终点横坐标, 取值范围: 0 ~ 127
 * @param  y2    终点纵坐标, 取值范围: 0 ~ 63
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  画白线
 *               - OLED_COLOR_REVERSED: 画黑线(擦除)
 */
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, OLED_ColorMode color);

/**
 * @brief  绘制矩形边框 (空心)
 * @param  x     左上角横坐标, 取值范围: 0 ~ 127
 * @param  y     左上角纵坐标, 取值范围: 0 ~ 63
 * @param  w     矩形宽度(像素), 取值范围: 1 ~ 128
 * @param  h     矩形高度(像素), 取值范围: 1 ~ 64
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  白色边框
 *               - OLED_COLOR_REVERSED: 黑色边框(擦除)
 */
void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, OLED_ColorMode color);

/**
 * @brief  绘制填充矩形 (实心)
 * @param  x     左上角横坐标, 取值范围: 0 ~ 127
 * @param  y     左上角纵坐标, 取值范围: 0 ~ 63
 * @param  w     矩形宽度(像素), 取值范围: 1 ~ 128
 * @param  h     矩形高度(像素), 取值范围: 1 ~ 64
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  白色填充
 *               - OLED_COLOR_REVERSED: 黑色填充(擦除)
 */
void OLED_DrawFilledRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, OLED_ColorMode color);

/**
 * @brief  绘制三角形边框 (空心)
 * @param  x1    第一个顶点横坐标, 取值范围: 0 ~ 127
 * @param  y1    第一个顶点纵坐标, 取值范围: 0 ~ 63
 * @param  x2    第二个顶点横坐标, 取值范围: 0 ~ 127
 * @param  y2    第二个顶点纵坐标, 取值范围: 0 ~ 63
 * @param  x3    第三个顶点横坐标, 取值范围: 0 ~ 127
 * @param  y3    第三个顶点纵坐标, 取值范围: 0 ~ 63
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  白色边框
 *               - OLED_COLOR_REVERSED: 黑色边框(擦除)
 */
void OLED_DrawTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3, OLED_ColorMode color);

/**
 * @brief  绘制填充三角形 (实心)
 * @param  x1    第一个顶点横坐标, 取值范围: 0 ~ 127
 * @param  y1    第一个顶点纵坐标, 取值范围: 0 ~ 63
 * @param  x2    第二个顶点横坐标, 取值范围: 0 ~ 127
 * @param  y2    第二个顶点纵坐标, 取值范围: 0 ~ 63
 * @param  x3    第三个顶点横坐标, 取值范围: 0 ~ 127
 * @param  y3    第三个顶点纵坐标, 取值范围: 0 ~ 63
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  白色填充
 *               - OLED_COLOR_REVERSED: 黑色填充(擦除)
 * @warning 三个顶点的y坐标不能有任意两个相同, 否则会导致除零错误!
 */
void OLED_DrawFilledTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3, OLED_ColorMode color);

/**
 * @brief  绘制圆形边框 (空心, Bresenham算法)
 * @param  x     圆心横坐标, 取值范围: 0 ~ 127
 * @param  y     圆心纵坐标, 取值范围: 0 ~ 63
 * @param  r     圆的半径(像素), 建议范围: 1 ~ 32
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  白色圆
 *               - OLED_COLOR_REVERSED: 黑色圆(擦除)
 */
void OLED_DrawCircle(uint8_t x, uint8_t y, uint8_t r, OLED_ColorMode color);

/**
 * @brief  绘制填充圆 (实心, Bresenham算法)
 * @param  x     圆心横坐标, 取值范围: 0 ~ 127
 * @param  y     圆心纵坐标, 取值范围: 0 ~ 63
 * @param  r     圆的半径(像素), 建议范围: 1 ~ 32
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  白色实心圆
 *               - OLED_COLOR_REVERSED: 黑色实心圆(擦除)
 */
void OLED_DrawFilledCircle(uint8_t x, uint8_t y, uint8_t r, OLED_ColorMode color);

/**
 * @brief  绘制椭圆边框 (中点椭圆算法)
 * @param  x     椭圆中心横坐标, 取值范围: 0 ~ 127
 * @param  y     椭圆中心纵坐标, 取值范围: 0 ~ 63
 * @param  a     椭圆X轴半径(水平方向), 建议范围: 1 ~ 64
 * @param  b     椭圆Y轴半径(垂直方向), 建议范围: 1 ~ 32
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  白色椭圆
 *               - OLED_COLOR_REVERSED: 黑色椭圆(擦除)
 */
void OLED_DrawEllipse(uint8_t x, uint8_t y, uint8_t a, uint8_t b, OLED_ColorMode color);

/**
 * @brief  绘制图片
 * @param  x     起始点横坐标(左上角), 取值范围: 0 ~ 127
 * @param  y     起始点纵坐标(左上角), 取值范围: 0 ~ 63
 * @param  img   指向Image结构体的指针, 包含图片宽度、高度和数据
 *               - 使用波特律动LED取模工具生成: https://led.baud-dance.com
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  正常显示
 *               - OLED_COLOR_REVERSED: 反色显示
 */
void OLED_DrawImage(uint8_t x, uint8_t y, const Image *img, OLED_ColorMode color);

// ========================= 文字绘制函数 =========================

/**
 * @brief  绘制单个ASCII字符
 * @param  x     起始点横坐标(左上角), 取值范围: 0 ~ 127
 * @param  y     起始点纵坐标(左上角), 取值范围: 0 ~ 63
 * @param  ch    要显示的ASCII字符, 取值范围: ' '(空格, 0x20) ~ '~'(0x7E)
 * @param  font  指向ASCIIFont结构体的指针, 定义字符宽高和字模数据
 *               - 可选: &afont8x6 (8行6列), &afont12x6, &afont16x8, &afont24x12
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  正常显示(黑底白字)
 *               - OLED_COLOR_REVERSED: 反色显示(白底黑字)
 */
void OLED_PrintASCIIChar(uint8_t x, uint8_t y, char ch, const ASCIIFont *font, OLED_ColorMode color);

/**
 * @brief  绘制ASCII字符串
 * @param  x     起始点横坐标(左上角), 取值范围: 0 ~ 127
 * @param  y     起始点纵坐标(左上角), 取值范围: 0 ~ 63
 * @param  str   要显示的ASCII字符串, 以'\0'结尾
 *               - 支持的字符范围: ' '(0x20) ~ '~'(0x7E)
 * @param  font  指向ASCIIFont结构体的指针
 *               - 可选: &afont8x6, &afont12x6, &afont16x8, &afont24x12
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  正常显示(黑底白字)
 *               - OLED_COLOR_REVERSED: 反色显示(白底黑字)
 * @note   字符从左到右依次绘制, 自动按字体宽度推进光标
 * @note   不会自动换行, 超出屏幕宽度的部分会被截断
 */
void OLED_PrintASCIIString(uint8_t x, uint8_t y, char *str, const ASCIIFont *font, OLED_ColorMode color);

/**
 * @brief  绘制字符串 (支持中英文混合)
 * @param  x     起始点横坐标(左上角), 取值范围: 0 ~ 127
 * @param  y     起始点纵坐标(左上角), 取值范围: 0 ~ 63
 * @param  str   要显示的字符串, 以'\0'结尾
 *               - 支持UTF-8编码的中文和ASCII字符混合显示
 *               - 中文使用Font结构体中的字库
 *               - ASCII字符使用Font结构体中指定的缺省ASCII字体
 * @param  font  指向Font结构体的指针, 包含中文字库和ASCII字体
 *               - 可选: &font16x16 (16x16中文 + 8x6 ASCII)
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  正常显示(黑底白字)
 *               - OLED_COLOR_REVERSED: 反色显示(白底黑字)
 * @note   为保证中文正常显示:
 *         1. 编译器字符集必须设置为UTF-8
 *         2. 中文字模需使用波特律动LED取模工具生成: https://led.baud-dance.com
 * @note   字符从左到右依次绘制, 不会自动换行
 */
void OLED_PrintString(uint8_t x, uint8_t y, char *str, const Font *font, OLED_ColorMode color);

#endif // __OLED_H__
