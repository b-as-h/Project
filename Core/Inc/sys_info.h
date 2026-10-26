/**
 * @file sys_info.h
 * @brief 系统信息与开机自检：复位原因、内存占用、栈水位、主循环负载、外设自检。
 *
 * 数据来源全部是片上资源，不额外占用外设：
 * - RCC->CSR   : 复位原因
 * - 链接符号   : Flash / RAM 静态占用
 * - 栈填充扫描 : 栈峰值
 * - DWT->CYCCNT: 主循环频率与最大单帧耗时
 * - I2C 探测   : OLED / MPU6050 在位检测
 */

#ifndef SYS_INFO_H
#define SYS_INFO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 复位原因。 */
typedef enum {
    SYS_RST_UNKNOWN = 0,
    SYS_RST_POWER_ON,  /**< 上电复位 */
    SYS_RST_PIN,       /**< NRST 引脚复位 */
    SYS_RST_SOFTWARE,  /**< 软件复位 */
    SYS_RST_IWDG,      /**< 独立看门狗 */
    SYS_RST_WWDG,      /**< 窗口看门狗 */
    SYS_RST_LOW_POWER  /**< 低功耗唤醒 */
} Sys_ResetCause;

/** 自检结果。 */
typedef enum {
    SYS_TEST_UNKNOWN = 0,
    SYS_TEST_PASS,
    SYS_TEST_FAIL
} Sys_TestResult;

/** 系统信息快照。 */
typedef struct {
    Sys_ResetCause reset_cause;

    uint32_t flash_used;   /**< 已用 Flash 字节数（含 .data 初值） */
    uint32_t flash_total;  /**< Flash 总容量 */
    uint32_t ram_static;   /**< .data + .bss */
    uint32_t ram_total;    /**< RAM 总容量 */
    uint32_t stack_total;  /**< 链接脚本预留栈大小 */
    uint32_t stack_peak;   /**< 实测栈峰值 */

    uint16_t loop_hz;      /**< 每秒主循环次数 */
    uint16_t loop_max_ms;  /**< 最近 1 秒内最长一次循环耗时 */

    Sys_TestResult oled;
    Sys_TestResult mpu;
    Sys_TestResult ds18b20;
    Sys_TestResult dht11;
    Sys_TestResult bluetooth;
} Sys_Info;

extern Sys_Info sys_info;

/** 读取复位原因、填充栈、计算内存占用、启动 DWT。应在时钟配置之后尽早调用。 */
void SysInfo_Init(void);

/** 探测 I2C 设备并刷新自检结果（会阻塞约 20ms，仅在进入自检页时调用）。 */
void SysInfo_SelfTest(void);

/** 主循环每轮调用一次，统计循环频率与最大帧耗时。 */
void SysInfo_LoopTick(void);

/** 复位原因文本，例如 "POR"。 */
const char *SysInfo_ResetCauseText(void);

/** 上电至今的秒数。 */
uint32_t SysInfo_UptimeSeconds(void);

#ifdef __cplusplus
}
#endif

#endif /* SYS_INFO_H */
