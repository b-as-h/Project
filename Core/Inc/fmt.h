/**
 * @file fmt.h
 * @brief 轻量整数 / 定点格式化工具。
 *
 * 用整数运算替代 printf 的 %f，避免链接 newlib 的浮点格式化代码：
 * - newlib-nano 默认不支持 %f（除非加 -u _printf_float），原 sprintf("%.2f")
 *   会输出空串；
 * - 一旦打开浮点格式化，代码体积会增加约 3~5 KB。
 *
 * 所有函数返回写入结束后的字符串末尾指针（指向 '\0'），便于链式拼接：
 * @code
 *   char buf[24];
 *   char *p = Fmt_Str(buf, "T:");
 *   p = Fmt_Fixed(p, ds18b20_temp, 2);   // 2534 -> "25.34"
 *   Fmt_Str(p, "C");
 * @endcode
 */

#ifndef FMT_H
#define FMT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 无符号十进制。 */
char *Fmt_U32(char *dst, uint32_t value);

/** 有符号十进制，含 '-'。 */
char *Fmt_I32(char *dst, int32_t value);

/**
 * @brief 定点小数格式化。
 * @param scaled   已放大 10^decimals 倍的整数，例如 2534 配 decimals=2 得到 "25.34"
 * @param decimals 小数位数，0~4
 */
char *Fmt_Fixed(char *dst, int32_t scaled, uint8_t decimals);

/** 两位补零十进制，例如 5 -> "05"。 */
char *Fmt_Pad2(char *dst, uint32_t value);

/** 大写十六进制，固定两位，例如 0x3C -> "3C"。 */
char *Fmt_Hex8(char *dst, uint8_t value);

/** 拷贝字符串。 */
char *Fmt_Str(char *dst, const char *src);

/** 秒数转 "HH:MM:SS"。 */
char *Fmt_TimeHMS(char *dst, uint32_t seconds);

/** 字节数转 "34.4K" / "512B"。 */
char *Fmt_Size(char *dst, uint32_t bytes);

#ifdef __cplusplus
}
#endif

#endif /* FMT_H */
