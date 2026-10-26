/**
 * @file bt.h
 * @brief 蓝牙串口（USART1）通信：环形缓冲 + 行协议 + 周期遥测。
 *
 * 硬件：HC-05/HC-06 等透传模块，PA9=TX、PA10=RX，默认 9600 8N1。
 *
 * 协议为可读的 ASCII 行协议，每条命令以 '\n' 结尾，回复以 "\r\n" 结尾：
 * | 命令                | 说明                                  |
 * |---------------------|---------------------------------------|
 * | PING                | 心跳，回复 +PONG                      |
 * | GET                 | 读取一次全部传感器                     |
 * | SERVO,<0-180>       | 设置舵机角度                           |
 * | LOCK,<0/1>          | 舵机锁定 / 解锁                        |
 * | STREAM,<0/1>[,<ms>] | 开关周期遥测，默认 500ms               |
 * | BAUD,<rate>         | 切换波特率（9600~115200），掉电恢复 9600 |
 * | SYS                 | 系统信息（复位原因、内存、负载）        |
 * | VER                 | 固件版本                               |
 * | RESET               | 软复位                                 |
 *
 * 收发均为中断驱动，不阻塞主循环；发送失败或缓冲满时丢弃新数据。
 */

#ifndef BT_H
#define BT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化串口接收与协议状态。需在 MX_USART1_UART_Init() 之后调用。 */
void BT_Init(void);

/** 主循环调用：解析收到的命令行、处理周期遥测。 */
void BT_Process(void);

/** 最近 5 秒内是否收到过数据（用于判断手机/上位机是否连接）。 */
uint8_t BT_IsOnline(void);

/** 发送一个以 "\r\n" 结尾的字符串（非阻塞，写入发送环形缓冲）。 */
void BT_SendString(const char *text);

/** 当前串口波特率。 */
uint32_t BT_GetBaud(void);

#ifdef __cplusplus
}
#endif

#endif /* BT_H */
