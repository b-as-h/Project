/**
 * @file fmt.c
 * @brief 轻量整数 / 定点格式化实现。
 */

#include "fmt.h"

char *Fmt_U32(char *dst, uint32_t value)
{
    char tmp[10];
    uint8_t n = 0;

    do {
        tmp[n++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);

    while (n > 0U) {
        *dst++ = tmp[--n];
    }
    *dst = '\0';
    return dst;
}

char *Fmt_I32(char *dst, int32_t value)
{
    uint32_t magnitude;

    if (value < 0) {
        *dst++ = '-';
        /* 先转成无符号再取反，避免 INT32_MIN 溢出。 */
        magnitude = (uint32_t)(-(int64_t)value);
    } else {
        magnitude = (uint32_t)value;
    }
    return Fmt_U32(dst, magnitude);
}

char *Fmt_Fixed(char *dst, int32_t scaled, uint8_t decimals)
{
    uint32_t divisor = 1U;
    uint32_t magnitude;
    uint32_t fraction;
    uint32_t probe;
    uint8_t i;

    if (decimals > 4U) {
        decimals = 4U;
    }
    for (i = 0U; i < decimals; ++i) {
        divisor *= 10U;
    }

    if (scaled < 0) {
        *dst++ = '-';
        magnitude = (uint32_t)(-(int64_t)scaled);
    } else {
        magnitude = (uint32_t)scaled;
    }

    dst = Fmt_U32(dst, magnitude / divisor);
    if (decimals == 0U) {
        return dst;
    }

    *dst++ = '.';
    fraction = magnitude % divisor;

    /* 补足前导零：fraction=5、divisor=100 时需要输出 "05"。 */
    for (probe = divisor / 10U; probe > 1U; probe /= 10U) {
        if (fraction < probe) {
            *dst++ = '0';
        } else {
            break;
        }
    }
    dst = Fmt_U32(dst, fraction);
    return dst;
}

char *Fmt_Pad2(char *dst, uint32_t value)
{
    if (value < 10U) {
        *dst++ = '0';
    }
    return Fmt_U32(dst, value);
}

char *Fmt_Hex8(char *dst, uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";

    *dst++ = hex[(value >> 4) & 0x0FU];
    *dst++ = hex[value & 0x0FU];
    *dst = '\0';
    return dst;
}

char *Fmt_Str(char *dst, const char *src)
{
    while (*src != '\0') {
        *dst++ = *src++;
    }
    *dst = '\0';
    return dst;
}

char *Fmt_TimeHMS(char *dst, uint32_t seconds)
{
    dst = Fmt_Pad2(dst, (seconds / 3600U) % 100U);
    *dst++ = ':';
    dst = Fmt_Pad2(dst, (seconds / 60U) % 60U);
    *dst++ = ':';
    return Fmt_Pad2(dst, seconds % 60U);
}

char *Fmt_Size(char *dst, uint32_t bytes)
{
    if (bytes < 1024U) {
        dst = Fmt_U32(dst, bytes);
        return Fmt_Str(dst, "B");
    }
    dst = Fmt_Fixed(dst, (int32_t)((bytes * 10U + 512U) / 1024U), 1);
    return Fmt_Str(dst, "K");
}
