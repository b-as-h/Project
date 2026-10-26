/**
 * @file sys_info.c
 * @brief 系统信息与开机自检实现。
 */

#include "sys_info.h"
#include "main.h"
#include "i2c.h"
#include "start.h"
#include "bt.h"

/* 链接脚本导出的内存边界符号（取地址使用，本身不占空间）。 */
extern uint32_t _etext;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _ebss;
extern uint32_t _sstack;
extern uint32_t _estack;

#define FLASH_BASE_ADDR 0x08000000U
#define FLASH_TOTAL     (64U * 1024U)
#define RAM_BASE_ADDR   0x20000000U

/** 栈填充魔数：扫描时未被改写的区域即为未使用栈。 */
#define STACK_PAINT 0xA5U

/** 填充栈时给当前栈帧留出的安全余量。 */
#define STACK_PAINT_MARGIN 96U

Sys_Info sys_info;

static uint32_t boot_tick;
static uint32_t loop_last_cycle;
static uint32_t loop_window_start;
static uint16_t loop_count;
static uint16_t loop_max_ms;
static uint8_t dwt_ready;

/* ========== 内部工具 ========== */

static void sys_dwt_enable(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    dwt_ready = 1U;
}

static void sys_read_reset_cause(void)
{
    uint32_t csr = RCC->CSR;

    if ((csr & RCC_CSR_PORRSTF) != 0U) {
        sys_info.reset_cause = SYS_RST_POWER_ON;
    } else if ((csr & RCC_CSR_PINRSTF) != 0U) {
        sys_info.reset_cause = SYS_RST_PIN;
    } else if ((csr & RCC_CSR_SFTRSTF) != 0U) {
        sys_info.reset_cause = SYS_RST_SOFTWARE;
    } else if ((csr & RCC_CSR_IWDGRSTF) != 0U) {
        sys_info.reset_cause = SYS_RST_IWDG;
    } else if ((csr & RCC_CSR_WWDGRSTF) != 0U) {
        sys_info.reset_cause = SYS_RST_WWDG;
    } else if ((csr & RCC_CSR_LPWRRSTF) != 0U) {
        sys_info.reset_cause = SYS_RST_LOW_POWER;
    } else {
        sys_info.reset_cause = SYS_RST_UNKNOWN;
    }

    /* 清标志，保证下次读到的是本次运行期间发生的新复位。 */
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

static void sys_read_memory(void)
{
    uint32_t text_end = (uint32_t)&_etext;
    uint32_t data_start = (uint32_t)&_sdata;
    uint32_t data_end = (uint32_t)&_edata;

    sys_info.flash_total = FLASH_TOTAL;
    sys_info.flash_used = (text_end - FLASH_BASE_ADDR) + (data_end - data_start);

    sys_info.ram_static = (uint32_t)&_ebss - data_start;
    sys_info.ram_total = (uint32_t)&_estack - RAM_BASE_ADDR;
    sys_info.stack_total = (uint32_t)&_estack - (uint32_t)&_sstack;
}

static void sys_paint_stack(void)
{
    uint8_t *p = (uint8_t *)&_sstack;
    uint8_t *limit = (uint8_t *)(__get_MSP() - STACK_PAINT_MARGIN);

    while (p < limit) {
        *p++ = STACK_PAINT;
    }
}

static uint32_t sys_stack_peak(void)
{
    const uint8_t *p = (const uint8_t *)&_sstack;
    const uint8_t *end = (const uint8_t *)&_estack;

    while (p < end && *p == STACK_PAINT) {
        ++p;
    }
    return (uint32_t)(end - p);
}

/* ========== 公共接口 ========== */

void SysInfo_Init(void)
{
    boot_tick = HAL_GetTick();

    sys_read_reset_cause();
    sys_read_memory();
    sys_paint_stack();
    sys_dwt_enable();

    sys_info.oled = SYS_TEST_UNKNOWN;
    sys_info.mpu = SYS_TEST_UNKNOWN;
    sys_info.ds18b20 = SYS_TEST_UNKNOWN;
    sys_info.dht11 = SYS_TEST_UNKNOWN;
    sys_info.bluetooth = SYS_TEST_UNKNOWN;

    loop_last_cycle = DWT->CYCCNT;
    loop_window_start = loop_last_cycle;
}

void SysInfo_SelfTest(void)
{
    /* HAL_I2C_IsDeviceReady 会阻塞，最多 3 次 × 20ms，仅在进页面时调用。 */
    sys_info.oled = (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(0x3CU << 1), 3U, 20U) == HAL_OK)
                        ? SYS_TEST_PASS
                        : SYS_TEST_FAIL;
    sys_info.mpu = (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(0x68U << 1), 3U, 20U) == HAL_OK)
                       ? SYS_TEST_PASS
                       : SYS_TEST_FAIL;

    sys_info.ds18b20 = ds18b20_valid ? SYS_TEST_PASS : SYS_TEST_FAIL;
    sys_info.dht11 = dht11_data.valid ? SYS_TEST_PASS : SYS_TEST_FAIL;
    sys_info.bluetooth = BT_IsOnline() ? SYS_TEST_PASS : SYS_TEST_FAIL;

    sys_info.stack_peak = sys_stack_peak();
}

void SysInfo_LoopTick(void)
{
    uint32_t now;
    uint32_t delta;
    uint32_t elapsed_ms;

    if (dwt_ready == 0U) {
        return;
    }

    now = DWT->CYCCNT;
    delta = now - loop_last_cycle;
    loop_last_cycle = now;

    elapsed_ms = delta / (SystemCoreClock / 1000U);
    if (elapsed_ms > loop_max_ms) {
        loop_max_ms = (uint16_t)(elapsed_ms > 0xFFFFU ? 0xFFFFU : elapsed_ms);
    }
    if (loop_count < 0xFFFFU) {
        ++loop_count;
    }

    if ((now - loop_window_start) >= SystemCoreClock) {
        sys_info.loop_hz = loop_count;
        sys_info.loop_max_ms = loop_max_ms;
        sys_info.stack_peak = sys_stack_peak();
        loop_count = 0U;
        loop_max_ms = 0U;
        loop_window_start = now;
    }
}

const char *SysInfo_ResetCauseText(void)
{
    switch (sys_info.reset_cause) {
    case SYS_RST_POWER_ON:  return "POR";
    case SYS_RST_PIN:       return "PIN";
    case SYS_RST_SOFTWARE:  return "SW";
    case SYS_RST_IWDG:      return "IWDG";
    case SYS_RST_WWDG:      return "WWDG";
    case SYS_RST_LOW_POWER: return "LPWR";
    default:                return "UNKN";
    }
}

uint32_t SysInfo_UptimeSeconds(void)
{
    return (HAL_GetTick() - boot_tick) / 1000U;
}
