/**
 * @file app_ui.c
 * @brief BASH_UI 应用层：页面定义、事件处理、自定义页面回调。
 *
 * 页面结构：
 * - 锁屏页（自定义）：密码输入，解锁后进入首页
 * - 首页（图标轮播）：传感器、舵机、MPU6050
 * - 传感器页（自定义）：显示光照、温度、湿度等
 * - 舵机页（自定义）：编码器控制角度，OK键锁定/解锁
 * - MPU6050页（自定义）：显示加速度、陀螺仪、温度
 */

#include "app_ui.h"
#include "font_bash.h"
#include "bash_oled.h"
#include "bash_ui.h"
#include "input.h"
#include "main.h"
#include "start.h"
#include "mpu6050.h"
#include "fmt.h"
#include "sys_info.h"
#include "bt.h"
#include <string.h>

/* ========== 占位图标 (32x32 XBM, 128 bytes each) ========== */

/* 传感器图标：温度计图案 */
static const uint8_t icon_sensor[128] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x80,
    0x00,
    0x00,
    0x00,
    0xC0,
    0x01,
    0x00,
    0x00,
    0x60,
    0x03,
    0x00,
    0x00,
    0x60,
    0x03,
    0x00,
    0x00,
    0x60,
    0x03,
    0x00,
    0x00,
    0x60,
    0x03,
    0x00,
    0x00,
    0x60,
    0x03,
    0x00,
    0x00,
    0x60,
    0x03,
    0x00,
    0x00,
    0x60,
    0x03,
    0x00,
    0x00,
    0x60,
    0x03,
    0x00,
    0x00,
    0x60,
    0x03,
    0x00,
    0x00,
    0x60,
    0x03,
    0x00,
    0x00,
    0x60,
    0x03,
    0x00,
    0x00,
    0xF0,
    0x07,
    0x00,
    0x00,
    0xF0,
    0x07,
    0x00,
    0x00,
    0xF0,
    0x07,
    0x00,
    0x80,
    0xF8,
    0x0F,
    0x00,
    0x80,
    0xF8,
    0x0F,
    0x00,
    0x80,
    0xF8,
    0x0F,
    0x00,
    0x80,
    0xF8,
    0x0F,
    0x00,
    0x80,
    0xF8,
    0x0F,
    0x00,
    0x80,
    0xF8,
    0x0F,
    0x00,
    0x80,
    0xF8,
    0x0F,
    0x00,
    0x80,
    0xF8,
    0x0F,
    0x00,
    0x00,
    0xF0,
    0x07,
    0x00,
    0x00,
    0xF0,
    0x07,
    0x00,
    0x00,
    0xF0,
    0x07,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

/* 舵机图标：齿轮图案 */
static const uint8_t icon_servo[128] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xE0,
    0x07,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xF8,
    0x1F,
    0x00,
    0x00,
    0xFC,
    0x3F,
    0x00,
    0x00,
    0x7E,
    0x7E,
    0x00,
    0x00,
    0x3E,
    0x7C,
    0x00,
    0x00,
    0x1F,
    0xF8,
    0x00,
    0x80,
    0x0F,
    0xF0,
    0x01,
    0xC0,
    0x07,
    0xE0,
    0x03,
    0xC0,
    0x07,
    0xE0,
    0x03,
    0xE0,
    0x07,
    0xE0,
    0x07,
    0xE0,
    0x07,
    0xE0,
    0x07,
    0xE0,
    0x07,
    0xE0,
    0x07,
    0xE0,
    0x07,
    0xE0,
    0x07,
    0xE0,
    0x07,
    0xE0,
    0x07,
    0xE0,
    0x07,
    0xE0,
    0x07,
    0xC0,
    0x07,
    0xE0,
    0x03,
    0xC0,
    0x07,
    0xE0,
    0x03,
    0x80,
    0x0F,
    0xF0,
    0x01,
    0x00,
    0x1F,
    0xF8,
    0x00,
    0x00,
    0x3E,
    0x7C,
    0x00,
    0x00,
    0x7E,
    0x7E,
    0x00,
    0x00,
    0xFC,
    0x3F,
    0x00,
    0x00,
    0xF8,
    0x1F,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xE0,
    0x07,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

/* MPU6050图标：芯片图案 */
static const uint8_t icon_mpu[128] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0xF8,
    0xFF,
    0xFF,
    0x1F,
    0xF8,
    0xFF,
    0xFF,
    0x1F,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xF8,
    0xFF,
    0xFF,
    0x1F,
    0xF8,
    0xFF,
    0xFF,
    0x1F,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xF8,
    0xFF,
    0xFF,
    0x1F,
    0xF8,
    0xFF,
    0xFF,
    0x1F,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xF8,
    0xFF,
    0xFF,
    0x1F,
    0xF8,
    0xFF,
    0xFF,
    0x1F,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0xF0,
    0x0F,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

/* 系统信息图标：圆圈 + 字母 i */
static const uint8_t icon_sysinfo[128] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x0F, 0x00,
    0x00, 0xFC, 0x3F, 0x00, 0x00, 0x0F, 0xF0, 0x00,
    0x80, 0x03, 0xC0, 0x01, 0xC0, 0xC1, 0x83, 0x03,
    0xE0, 0xC0, 0x03, 0x07, 0x60, 0xC0, 0x03, 0x06,
    0x30, 0xC0, 0x03, 0x0C, 0x30, 0x00, 0x00, 0x0C,
    0x18, 0x00, 0x00, 0x18, 0x18, 0xC0, 0x03, 0x18,
    0x18, 0xC0, 0x03, 0x18, 0x18, 0xC0, 0x03, 0x18,
    0x18, 0xC0, 0x03, 0x18, 0x18, 0xC0, 0x03, 0x18,
    0x18, 0xC0, 0x03, 0x18, 0x18, 0xC0, 0x03, 0x18,
    0x30, 0xC0, 0x03, 0x0C, 0x30, 0xC0, 0x03, 0x0C,
    0x60, 0xC0, 0x03, 0x06, 0xE0, 0xC0, 0x03, 0x07,
    0xC0, 0x01, 0x80, 0x03, 0x80, 0x03, 0xC0, 0x01,
    0x00, 0x0F, 0xF0, 0x00, 0x00, 0xFC, 0x3F, 0x00,
    0x00, 0xF0, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* ========== 密码系统变量 ========== */
typedef enum
{
    LOCK_IDLE,  /* 待机态 */
    LOCK_INPUT, /* 输入态（转动选择数字） */
    LOCK_ALARM  /* 报警态 */
} LockState;

static uint8_t unlock_pending = 0; /* 解锁待处理标志 */

#define PASSWORD_LEN 4
#define MAX_ERROR_COUNT 5

static LockState lock_state = LOCK_IDLE;
static uint8_t password[PASSWORD_LEN] = {1, 2, 3, 4}; /* 默认密码 */
static uint8_t pwd_input[PASSWORD_LEN] = {0};         /* 用户输入 */
static uint8_t input_index = 0;                       /* 当前输入第几位 */
static uint8_t select_num = 0;                        /* 当前选择的数字(0-9) */
static uint8_t error_count = 0;                       /* 错误次数 */

/* ========== 舵机控制变量 ========== */
/* 舵机角度由 start.c 的 servo_a10 统一维护，页面这边只负责转发编码器增量，
 * 不再保存/恢复 TIM2 计数器 —— 那正是之前"只能停在 0 和 180"的根源。 */

/* ========== 自定义页定时器 ========== */
static uint32_t lock_last_tick = 0;
static uint32_t sensor_last_tick = 0;
static uint32_t servo_last_tick = 0;
static uint32_t mpu_last_tick = 0;
/* 雷达页：编辑警报距离的状态 + 刷新计时 */
static uint8_t radar_edit = 0;
static uint32_t radar_last_tick = 0;
/* MPU 页两个视图：0=数据，1=寄存器诊断 */
#define MPU_VIEW_COUNT 2U
static uint8_t mpu_view = 0;
static uint32_t sys_last_tick = 0;

/* 系统信息页：3 个子视图，编码器切换 */
#define SYSINFO_VIEW_COUNT 3U
static uint8_t sys_view = 0;

static const char *App_UITestText(Sys_TestResult result)
{
    switch (result) {
    case SYS_TEST_PASS: return "OK";
    case SYS_TEST_FAIL: return "FAIL";
    default:            return "--";
    }
}

/* 雷达图标：半圆扫描盘 + 回波点 */
static const uint8_t icon_radar[128] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xE0, 0x0F, 0x80, 0x00, 0x1C, 0x71, 0xC0,
    0x00, 0x03, 0x81, 0x61, 0x80, 0x00, 0xC1, 0x33,
    0x40, 0x00, 0x81, 0x1C, 0x20, 0x00, 0x01, 0x0E,
    0x10, 0x04, 0x01, 0x17, 0x08, 0xCE, 0x87, 0x23,
    0x08, 0x34, 0xD9, 0x21, 0x04, 0x08, 0xE1, 0x40,
    0x04, 0x04, 0x70, 0x40, 0x04, 0x02, 0xB0, 0x40,
    0x02, 0x02, 0x98, 0x80, 0x02, 0x01, 0x0C, 0x81,
    0x02, 0x01, 0x06, 0x81, 0xFA, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* ========== 首页 ========== */
static const UI_HomeItem home_items[] = {
    {"Sensor", icon_sensor, APP_PAGE_SENSOR},
    {"Servo", icon_servo, APP_PAGE_SERVO},
    {"MPU6050", icon_mpu, APP_PAGE_MPU6050},
    {"System", icon_sysinfo, APP_PAGE_SYSINFO},
    {"Radar", icon_radar, APP_PAGE_RADAR},
};
static const UI_HomePage home_pages[] = {
    {home_items, sizeof(home_items) / sizeof(home_items[0])},
};

/* ========== UI表 ========== */
static const UI_PageRoute routes[] = {
    {UI_PAGE_CUSTOM, 0}, /* page 1: 锁屏（自定义） */
    {UI_PAGE_HOME, 0},   /* page 2: 首页（图标轮播） */
    {UI_PAGE_CUSTOM, 1}, /* page 3: 传感器（自定义） */
    {UI_PAGE_CUSTOM, 2}, /* page 4: 舵机（自定义） */
    {UI_PAGE_CUSTOM, 3}, /* page 5: MPU6050（自定义） */
    {UI_PAGE_CUSTOM, 4}, /* page 6: 系统信息（自定义） */
    {UI_PAGE_CUSTOM, 5}, /* page 7: 雷达（自定义） */
};

/* ========== 应用描述（非 const，以便动态修改 root_page） ========== */
static UI_App app_desc = {
    .root_page = APP_PAGE_LOCK, /* 上电默认锁屏 */
    .routes = routes,
    .route_count = sizeof(routes) / sizeof(routes[0]),
    .home_pages = home_pages,
    .home_page_count = sizeof(home_pages) / sizeof(home_pages[0]),
    .custom_page_count = 6, /* 锁屏、传感器、舵机、MPU6050、系统信息、雷达 */
};

/* ========== 密码验证 ========== */
static uint8_t Password_Verify(void)
{
    for (int i = 0; i < PASSWORD_LEN; i++)
    {
        if (pwd_input[i] != password[i])
        {
            return 0;
        }
    }
    return 1;
}

/* ========== BASH_UI 回调实现 ========== */

void UI_CustomOnEnter(UI_PageId page)
{
    switch (page)
    {
    case APP_PAGE_LOCK:
        lock_state = LOCK_IDLE;
        input_index = 0;
        select_num = 0;
        error_count = 0;
        lock_last_tick = 0;
        break;

    case APP_PAGE_SERVO:
        /* 舵机角度由 servo_a10 保存，进出页面都不需要动编码器计数器 */
        servo_last_tick = 0;
        break;

    case APP_PAGE_SENSOR:
        sensor_last_tick = 0;
        break;

    case APP_PAGE_MPU6050:
        mpu_last_tick = 0;
        mpu_view = 0;
        break;

    case APP_PAGE_SYSINFO:
        sys_view = 0;
        sys_last_tick = 0;
        /* 进入页面时做一次 I2C 在位检测（阻塞约 20ms，仅此一次） */
        SysInfo_SelfTest();
        break;

    case APP_PAGE_RADAR:
        radar_edit = 0;
        radar_last_tick = 0;
        /* 进页就开扫，否则"查看当前雷达数据"看到的是一动不动的旧值 */
        if (!Servo_lock)
        {
            Servo_SweepStart(60);
        }
        break;

    default:
        break;
    }
}

void UI_CustomOnLeave(UI_PageId page)
{
    /* 舵机角度由 servo_a10 保存，编码器计数器不再被任何页面改写，无需恢复。
     * 雷达页要额外收尾：把云台停住，否则回到首页它还在继续扫。 */
    if (page == APP_PAGE_RADAR)
    {
        Servo_SweepStop();
    }
}

void UI_CustomOnInput(UI_PageId page, UI_InputEvent event)
{
    switch (page)
    {
    case APP_PAGE_LOCK:
        /* 锁屏页输入处理 */
        if (event.action == UI_INPUT_OK && event.source == UI_INPUT_PRESS)
        {
            if (lock_state == LOCK_IDLE || lock_state == LOCK_ALARM)
            {
                /* 进入输入状态 */
                lock_state = LOCK_INPUT;
                input_index = 0;
                select_num = 0;
            }
            else if (lock_state == LOCK_INPUT)
            {
                /* 确认当前位数字 */
                pwd_input[input_index] = select_num;
                input_index++;
                select_num = 0;

                if (input_index >= PASSWORD_LEN)
                {
                    /* 输入完成，自动验证 */
                    if (Password_Verify())
                    {
                        /* 密码正确，设置待处理标志 */
                        lock_state = LOCK_IDLE;
                        error_count = 0;
                        input_index = 0;
                        unlock_pending = 1;
                    }
                    else
                    {
                        /* 密码错误 */
                        error_count++;
                        if (error_count >= MAX_ERROR_COUNT)
                        {
                            lock_state = LOCK_ALARM;
                        }
                        else
                        {
                            lock_state = LOCK_IDLE;
                        }
                        input_index = 0;
                    }
                }
            }
        }

        /* 编码器旋转选择数字 */
        if (event.source == UI_INPUT_ENCODER && lock_state == LOCK_INPUT)
        {
            int16_t delta = (event.action == UI_INPUT_DOWN) ? (int16_t)event.steps : -(int16_t)event.steps;
            int16_t new_num = (int16_t)select_num + delta;
            /* 处理环绕 */
            while (new_num < 0)
                new_num += 10;
            while (new_num >= 10)
                new_num -= 10;
            select_num = (uint8_t)new_num;
        }
        break;

    case APP_PAGE_SERVO:
        /* 舵机页输入处理 */
        if (event.action == UI_INPUT_OK && event.source == UI_INPUT_REPEAT)
        {
            /* 长按 OK 返回首页，短按仍只负责锁定/解锁。 */
            (void)UI_CustomRequestClose();
            break;
        }
        if (event.action == UI_INPUT_OK && event.source == UI_INPUT_RELEASE)
        {
            /* 切换锁定状态：锁定后编码器不再改变角度，位置原地保持，
             * 解锁时无需恢复 —— servo_a10 一直是最后命令的角度。 */
            Servo_lock = (uint8_t)!Servo_lock;
            if (Servo_lock)
            {
                Servo_SweepStop(); /* 锁定 = 立刻停住，扫描和插补一起取消 */
            }
            break;
        }

        /* 编码器调整角度：每格 5°，并取消正在进行的扫描。
         * 方向沿用原逻辑：DOWN（计数器增大）对应角度增大。 */
        if (event.source == UI_INPUT_ENCODER && !Servo_lock)
        {
            int16_t delta = (event.action == UI_INPUT_DOWN)
                                ? (int16_t)event.steps
                                : -(int16_t)event.steps;
            Servo_Step((int16_t)(delta * 5));
        }
        break;

    case APP_PAGE_SENSOR:
        /* OK键返回首页 */
        if (event.action == UI_INPUT_OK && event.source == UI_INPUT_PRESS)
        {
            UI_CustomRequestClose();
        }
        break;

    case APP_PAGE_MPU6050:
        /* OK 返回首页；编码器在"数据 / 寄存器"两个视图间切换 */
        if (event.action == UI_INPUT_OK && event.source == UI_INPUT_PRESS)
        {
            UI_CustomRequestClose();
            break;
        }
        /* 编码器切换视图：往下拨看寄存器，往上拨回数据。
         * 按方向直接赋值，不累加 steps，避免一格跳两次。 */
        if (event.source == UI_INPUT_ENCODER)
        {
            mpu_view = (event.action == UI_INPUT_DOWN) ? 1U : 0U;
            UI_Invalidate();
        }
        break;

    case APP_PAGE_SYSINFO:
        /* OK 返回首页；编码器切换 3 个子视图 */
        if (event.action == UI_INPUT_OK && event.source == UI_INPUT_PRESS)
        {
            UI_CustomRequestClose();
            break;
        }
        if (event.source == UI_INPUT_ENCODER)
        {
            int16_t delta = (event.action == UI_INPUT_DOWN) ? (int16_t)event.steps
                                                            : -(int16_t)event.steps;
            int16_t view = (int16_t)sys_view + delta;
            while (view < 0)
                view += (int16_t)SYSINFO_VIEW_COUNT;
            while (view >= (int16_t)SYSINFO_VIEW_COUNT)
                view -= (int16_t)SYSINFO_VIEW_COUNT;
            sys_view = (uint8_t)view;
            UI_Invalidate();
        }
        break;

    case APP_PAGE_RADAR:
        /* 长按 OK 返回首页 */
        if (event.action == UI_INPUT_OK && event.source == UI_INPUT_REPEAT)
        {
            (void)UI_CustomRequestClose();
            break;
        }
        /* 短按 OK 切换"编辑警报距离" */
        if (event.action == UI_INPUT_OK && event.source == UI_INPUT_RELEASE)
        {
            radar_edit = (uint8_t)!radar_edit;
            UI_Invalidate();
            break;
        }
        /* 编辑态下编码器调警报距离，每格 5cm，范围 5~400cm */
        if (event.source == UI_INPUT_ENCODER && radar_edit)
        {
            int16_t delta = (event.action == UI_INPUT_DOWN) ? (int16_t)event.steps
                                                            : -(int16_t)event.steps;
            int32_t v = (int32_t)Radar_AlarmCm + (int32_t)delta * 5;
            if (v < 5)
                v = 5;
            if (v > 400)
                v = 400;
            Radar_AlarmCm = (uint16_t)v;
            UI_Invalidate();
        }
        break;

    default:
        break;
    }
}

bool UI_CustomOnTick(UI_PageId page, uint32_t now_ms)
{
    switch (page)
    {
    case APP_PAGE_LOCK:
        /* 锁屏页每 100ms 刷新 */
        if (now_ms - lock_last_tick >= 100)
        {
            lock_last_tick = now_ms;
            return true;
        }
        return false;

    case APP_PAGE_SENSOR:
        /* 传感器页每 200ms 刷新（数据由 Sensor_Process 周期采样） */
        if (now_ms - sensor_last_tick >= 200)
        {
            sensor_last_tick = now_ms;
            return true;
        }
        return false;

    case APP_PAGE_SERVO:
        /* 舵机页每 100ms 刷新 */
        if (now_ms - servo_last_tick >= 100)
        {
            servo_last_tick = now_ms;
            return true;
        }
        return false;

    case APP_PAGE_MPU6050:
        /* MPU6050页每 100ms 刷新 */
        if (now_ms - mpu_last_tick >= 100)
        {
            mpu_last_tick = now_ms;
            return true;
        }
        return false;

    case APP_PAGE_SYSINFO:
        /* 系统信息页每 500ms 刷新 */
        if (now_ms - sys_last_tick >= 500)
        {
            sys_last_tick = now_ms;
            return true;
        }
        return false;

    case APP_PAGE_RADAR:
        /* 雷达页每 100ms 刷新（角度在扫描中一直在变） */
        if (now_ms - radar_last_tick >= 100)
        {
            radar_last_tick = now_ms;
            return true;
        }
        return false;

    default:
        return false;
    }
}

void UI_CustomOnDraw(UI_PageId page, int16_t x_offset,
                     int16_t clip_x, uint16_t clip_width)
{
    (void)clip_x;
    (void)clip_width;

    char buf[40];
    char *p;

    switch (page)
    {
    case APP_PAGE_LOCK:
        /* 锁屏页绘制 */
        OLED_SetDrawMode(OLED_DRAW_SET);

        /* 边框 */
        OLED_DrawFrame(x_offset + 0, 0, 128, 64);

        /* 标题 */
        Font_DrawMixedString(x_offset + 2, 2, "LOCKED");

        if (lock_state == LOCK_IDLE)
        {
            Font_DrawMixedString(x_offset + 2, 18, "Press OK");
            p = Fmt_Str(buf, "Err:");
            p = Fmt_I32(p, error_count);
            *p++ = '/';
            p = Fmt_I32(p, MAX_ERROR_COUNT);
            Font_DrawMixedString(x_offset + 2, 38, buf);
        }
        else if (lock_state == LOCK_INPUT)
        {
            Font_DrawMixedString(x_offset + 2, 16, "PWD:");
            char stars[PASSWORD_LEN + 1];
            for (int i = 0; i < input_index; i++)
                stars[i] = '*';
            stars[input_index] = '\0';
            Font_DrawMixedString(x_offset + 30, 16, stars);
            p = Fmt_Str(buf, "Num:");
            p = Fmt_I32(p, select_num);
            Font_DrawMixedString(x_offset + 2, 32, buf);
            Font_DrawMixedString(x_offset + 2, 48, "[OK]Confirm");
        }
        else if (lock_state == LOCK_ALARM)
        {
            /* 反显的顺序必须是「先画字，再 XOR 填充」。
             * OLED_DrawBox 是实心填充，先填后画的话字也是白色的，
             * 整行会糊成一块白板。 */
            Font_DrawMixedString(x_offset + 4, 18, "ERROR!");
            OLED_SetDrawMode(OLED_DRAW_XOR);
            OLED_DrawBox(x_offset + 2, 16, 80, 16);
            OLED_SetDrawMode(OLED_DRAW_SET);
            Font_DrawMixedString(x_offset + 2, 38, "Try again");
        }
        break;

    case APP_PAGE_SENSOR:
        /* 传感器页绘制 */
        OLED_SetDrawMode(OLED_DRAW_SET);

        /* 标题 + 光照图标 */
        Font_DrawMixedString(x_offset + 2, 0, "传感器");
        if (sensor_light_state)
        {
            Font_DrawMixedString(x_offset + 54, 0, "☪");
        }
        else
        {
            Font_DrawMixedString(x_offset + 54, 0, "☀");
        }

        /* 光照强度 */
        p = Fmt_Str(buf, "L:");
        p = Fmt_U32(p, sensor_light_percent);
        p = Fmt_Str(p, "%");
        Font_DrawMixedString(x_offset + 2, 18, buf);

        /* DS18B20 温度 + DHT11 湿度 */
        p = Fmt_Str(buf, "T:");
        if (ds18b20_valid)
        {
            p = Fmt_Fixed(p, ds18b20_temp, 2);
        }
        else
        {
            p = Fmt_Str(p, "--.--");
        }
        p = Fmt_Str(p, "C H:");
        if (dht11_data.valid)
        {
            p = Fmt_U32(p, dht11_data.humi);
        }
        else
        {
            p = Fmt_Str(p, "--");
        }
        p = Fmt_Str(p, "%");
        Font_DrawMixedString(x_offset + 2, 28, buf);

        /* 片内温度 + VDDA 电压 */
        p = Fmt_Str(buf, "C:");
        p = Fmt_Fixed(p, sensor_chip_temp_x10, 1);
        p = Fmt_Str(p, "C V:");
        p = Fmt_U32(p, sensor_vdd_mv);
        p = Fmt_Str(p, "mV");
        Font_DrawMixedString(x_offset + 2, 42, buf);

        /* 返回提示 */
        Font_DrawMixedString(x_offset + 2, 54, "[OK]Back");
        break;

    case APP_PAGE_SERVO:
        /* 舵机页绘制 */
        OLED_SetDrawMode(OLED_DRAW_SET);

        /* 标题 */
        Font_DrawMixedString(x_offset + 2, 0, "舵机控制");

        /* 锁定状态 */
        if (Servo_lock)
        {
            Font_DrawMixedString(x_offset + 70, 0, "已上锁");
        }
        else
        {
            Font_DrawMixedString(x_offset + 70, 0, "已解锁");
        }

        /* 当前角度（直接读软件位置，与实际 CCR 输出始终一致） */
        p = Fmt_Str(buf, "Angle: ");
        p = Fmt_I32(p, (int32_t)Servo_GetAngle());
        Font_DrawMixedString(x_offset + 2, 20, buf);

        /* 操作提示 */
        if (Servo_lock)
        {
            Font_DrawMixedString(x_offset + 2, 40, "[OK]Unlock");
        }
        else
        {
            Font_DrawMixedString(x_offset + 2, 40, "[OK]Lock");
        }
        Font_DrawMixedString(x_offset + 2, 52, "Hold OK:Back");
        break;

    case APP_PAGE_MPU6050:
        /* MPU6050页绘制 */
        OLED_SetDrawMode(OLED_DRAW_SET);

        /* 标题 + 视图序号 */
        p = Fmt_Str(buf, "MPU6050 ");
        p = Fmt_U32(p, (uint32_t)mpu_view + 1U);
        *p++ = '/';
        p = Fmt_U32(p, MPU_VIEW_COUNT);
        Font_DrawMixedString(x_offset + 2, 0, buf);

        if (mpu_view == 1U)
        {
            /* 视图 2：寄存器诊断。用来判断"能读不能采"到底卡在哪一环。 */
            if (!mpu6050_initialized)
            {
                Font_DrawMixedString(x_offset + 2, 24, "Not init");
                Font_DrawMixedString(x_offset + 2, 54, "[OK]Back");
                break;
            }

            p = Fmt_Str(buf, "W1:");
            p = Fmt_Hex8(p, mpu6050_pwr_mgmt);
            p = Fmt_Str(p, " W2:");
            p = Fmt_Hex8(p, mpu6050_pwr_mgmt2);
            Font_DrawMixedString(x_offset + 2, 11, buf);

            p = Fmt_Str(buf, "AC:");
            p = Fmt_Hex8(p, mpu6050_accel_config);
            p = Fmt_Str(p, " GC:");
            p = Fmt_Hex8(p, mpu6050_gyro_config);
            Font_DrawMixedString(x_offset + 2, 22, buf);

            p = Fmt_Str(buf, "AX:");
            p = Fmt_I32(p, (int32_t)mpu6050_data.Accel_X_RAW);
            p = Fmt_Str(p, " AY:");
            p = Fmt_I32(p, (int32_t)mpu6050_data.Accel_Y_RAW);
            Font_DrawMixedString(x_offset + 2, 33, buf);

            p = Fmt_Str(buf, "AZ:");
            p = Fmt_I32(p, (int32_t)mpu6050_data.Accel_Z_RAW);
            p = Fmt_Str(p, " E:");
            p = Fmt_U32(p, mpu6050_read_errors > 9999U ? 9999U : mpu6050_read_errors);
            Font_DrawMixedString(x_offset + 2, 44, buf);

            Font_DrawMixedString(x_offset + 2, 54, "[ENC] [OK]Back");
            break;
        }

        /* 诊断行：WHO_AM_I 回读值 + 初始化结果。
         * 无论成功还是失败这一行都有内容，页面不会再是空白一片。 */
        p = Fmt_Str(buf, "ID:");
        if (mpu6050_who_am_i == 0U)
        {
            p = Fmt_Str(p, "--");
        }
        else
        {
            p = Fmt_Hex8(p, mpu6050_who_am_i);
        }
        *p++ = ' ';
        switch (mpu6050_init_status)
        {
        case MPU6050_OK:        p = Fmt_Str(p, "OK");     break;
        case MPU6050_ERR_I2C:   p = Fmt_Str(p, "NOACK");  break;
        case MPU6050_ERR_ID:    p = Fmt_Str(p, "BADID");  break;
        case MPU6050_ERR_SLEEP: p = Fmt_Str(p, "SLEEP");  break;
        default:                p = Fmt_Str(p, "CFGERR"); break;
        }
        Font_DrawMixedString(x_offset + 2, 11, buf);

        if (!mpu6050_initialized)
        {
            Font_DrawMixedString(x_offset + 2, 26, "No data");
            Font_DrawMixedString(x_offset + 2, 54, "[OK]Back");
            break;
        }

        /* 加速度（单位 g，保留两位小数）。
         * Fmt_Fixed 的参数是"已放大 10^decimals 倍的整数"，
         * 2 位小数要 ×100，之前写的 ×1000 会把数值放大 10 倍。 */
        p = Fmt_Str(buf, "A:");
        p = Fmt_Fixed(p, (int32_t)(mpu6050_data.Ax * 100.0f), 2);
        *p++ = ',';
        p = Fmt_Fixed(p, (int32_t)(mpu6050_data.Ay * 100.0f), 2);
        *p++ = ',';
        p = Fmt_Fixed(p, (int32_t)(mpu6050_data.Az * 100.0f), 2);
        Font_DrawMixedString(x_offset + 2, 22, buf);

        /* 陀螺仪（单位 °/s，取整） */
        p = Fmt_Str(buf, "G:");
        p = Fmt_I32(p, (int32_t)mpu6050_data.Gx);
        *p++ = ',';
        p = Fmt_I32(p, (int32_t)mpu6050_data.Gy);
        *p++ = ',';
        p = Fmt_I32(p, (int32_t)mpu6050_data.Gz);
        Font_DrawMixedString(x_offset + 2, 33, buf);

        /* 温度 + 采样失败计数 + PWR_MGMT_1 回读值
         * P 正常应为 01（PLL 时钟、已唤醒）；40 表示还停在 SLEEP。 */
        p = Fmt_Str(buf, "T:");
        p = Fmt_Fixed(p, (int32_t)(mpu6050_data.Temperature * 10.0f), 1);
        p = Fmt_Str(p, "C E:");
        p = Fmt_U32(p, mpu6050_read_errors > 9999U ? 9999U : mpu6050_read_errors);
        p = Fmt_Str(p, " P:");
        p = Fmt_Hex8(p, mpu6050_pwr_mgmt);
        Font_DrawMixedString(x_offset + 2, 44, buf);

        /* 返回提示 */
        Font_DrawMixedString(x_offset + 2, 54, "[OK]Back");
        break;

    case APP_PAGE_SYSINFO:
        /* 系统信息：3 个子视图，编码器切换 */
        OLED_SetDrawMode(OLED_DRAW_SET);

        p = Fmt_Str(buf, (sys_view == 0U) ? "SYS "
                                          : ((sys_view == 1U) ? "RES " : "TEST "));
        p = Fmt_U32(p, (uint32_t)sys_view + 1U);
        *p++ = '/';
        p = Fmt_U32(p, SYSINFO_VIEW_COUNT);
        Font_DrawMixedString(x_offset + 2, 0, buf);
        OLED_DrawHLine(x_offset + 2, 12, 124);

        if (sys_view == 0U)
        {
            /* 视图 1：复位原因 / 运行时间 / 版本 / 蓝牙 */
            p = Fmt_Str(buf, "RST: ");
            p = Fmt_Str(p, SysInfo_ResetCauseText());
            Font_DrawMixedString(x_offset + 2, 14, buf);

            p = Fmt_Str(buf, "UP : ");
            p = Fmt_TimeHMS(p, SysInfo_UptimeSeconds());
            Font_DrawMixedString(x_offset + 2, 24, buf);

            p = Fmt_Str(buf, "VER: ");
            p = Fmt_Str(p, "1.4.0");
            Font_DrawMixedString(x_offset + 2, 34, buf);

            p = Fmt_Str(buf, "BT :");
            p = Fmt_U32(p, BT_GetBaud());
            p = Fmt_Str(p, BT_IsOnline() ? " ON" : " OFF");
            Font_DrawMixedString(x_offset + 2, 44, buf);

            Font_DrawMixedString(x_offset + 2, 54, "[ENC]Page [OK]Back");
        }
        else if (sys_view == 1U)
        {
            /* 视图 2：Flash / RAM / 栈 / 主循环负载 */
            p = Fmt_Str(buf, "FLASH:");
            p = Fmt_Size(p, sys_info.flash_used);
            *p++ = '/';
            p = Fmt_Size(p, sys_info.flash_total);
            Font_DrawMixedString(x_offset + 2, 14, buf);

            p = Fmt_Str(buf, "RAM  :");
            p = Fmt_Size(p, sys_info.ram_static);
            *p++ = '/';
            p = Fmt_Size(p, sys_info.ram_total);
            Font_DrawMixedString(x_offset + 2, 24, buf);

            p = Fmt_Str(buf, "STACK:");
            p = Fmt_U32(p, sys_info.stack_peak);
            *p++ = '/';
            p = Fmt_U32(p, sys_info.stack_total);
            Font_DrawMixedString(x_offset + 2, 34, buf);

            p = Fmt_Str(buf, "LOOP :");
            p = Fmt_U32(p, sys_info.loop_hz);
            p = Fmt_Str(p, "/s");
            Font_DrawMixedString(x_offset + 2, 44, buf);

            p = Fmt_Str(buf, "LMAX :");
            p = Fmt_U32(p, sys_info.loop_max_ms);
            p = Fmt_Str(p, "ms");
            Font_DrawMixedString(x_offset + 2, 54, buf);
        }
        else
        {
            /* 视图 3：外设自检结果 */
            p = Fmt_Str(buf, "OLED: ");
            p = Fmt_Str(p, App_UITestText(sys_info.oled));
            Font_DrawMixedString(x_offset + 2, 14, buf);

            p = Fmt_Str(buf, "MPU : ");
            p = Fmt_Str(p, App_UITestText(sys_info.mpu));
            Font_DrawMixedString(x_offset + 2, 24, buf);

            p = Fmt_Str(buf, "DS18: ");
            p = Fmt_Str(p, App_UITestText(sys_info.ds18b20));
            Font_DrawMixedString(x_offset + 2, 34, buf);

            p = Fmt_Str(buf, "DHT : ");
            p = Fmt_Str(p, App_UITestText(sys_info.dht11));
            Font_DrawMixedString(x_offset + 2, 44, buf);

            p = Fmt_Str(buf, "BT  : ");
            p = Fmt_Str(p, App_UITestText(sys_info.bluetooth));
            Font_DrawMixedString(x_offset + 2, 54, buf);
        }
        break;

    case APP_PAGE_RADAR:
    {
        int16_t dist;

        OLED_SetDrawMode(OLED_DRAW_SET);

        /* 中文字模是 16x16，一屏最多排 4 行且行距必须给满 16px，否则会压在一起。
         * 这里用 3 行数据 + 1 行 ASCII 提示，行距 17px 留出 1px 缝；
         * 数值是 6x8 的 ASCII，下移 4px 才和汉字垂直居中对齐。 */
        Font_DrawMixedString(x_offset + 92, 4, "RADAR");

        /* 当前云台角度 */
        Font_DrawMixedString(x_offset + 2, 0, "角度");
        p = Fmt_I32(buf, (int32_t)Servo_GetAngle());
        Font_DrawMixedString(x_offset + 40, 4, buf);

        /* 当前距离：超声波未接入时 Radar_GetDistanceCm() 返回 -1 */
        Font_DrawMixedString(x_offset + 2, 17, "距离");
        dist = Radar_GetDistanceCm();
        if (dist < 0)
        {
            p = Fmt_Str(buf, "--");
        }
        else
        {
            p = Fmt_I32(buf, (int32_t)dist);
            p = Fmt_Str(p, "cm");
        }
        Font_DrawMixedString(x_offset + 40, 21, buf);

        /* 警报距离：编辑态整行反显。
         * 顺序不能反 —— OLED_DrawBox 是实心填充，先填后画会把字也画成白色，
         * 整行糊成一片白。正确做法是先画字，再用 XOR 填充把字反色。 */
        Font_DrawMixedString(x_offset + 2, 34, "警报");
        p = Fmt_U32(buf, Radar_AlarmCm);
        p = Fmt_Str(p, "cm");
        Font_DrawMixedString(x_offset + 40, 38, buf);

        if (radar_edit)
        {
            OLED_SetDrawMode(OLED_DRAW_XOR);
            OLED_DrawBox(x_offset + 0, 33, 84, 18);
            OLED_SetDrawMode(OLED_DRAW_SET);
        }

        /* 操作提示 */
        Font_DrawMixedString(x_offset + 2, 52,
                             radar_edit ? "[OK]Save [Hold]Back" : "[OK]Edit [Hold]Back");
        break;
    }

    default:
        break;
    }
}

/* ========== 公共接口 ========== */

UI_Status App_UI_Init(void)
{
    OLED_Status oled_status;
    UI_Status ui_status;

    /* 初始化 OLED 硬件 */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        return UI_DISPLAY_ERROR;
    }

    /* 初始化 BASH_UI */
    ui_status = UI_Init(&app_desc);
    return ui_status;
}

void App_UI_Process(void)
{
    static uint32_t recover_last_tick = 0;
    UI_Status status;

    /* 检查解锁待处理标志 */
    if (unlock_pending)
    {
        unlock_pending = 0;
        app_desc.root_page = APP_PAGE_HOME;
        UI_Init(&app_desc);
        return;
    }

    UI_Input input = Input_Read();
    status = UI_Update(HAL_GetTick(), input);

    /*
     * BASH_UI 在显示通路出错（或一帧超过 UI_DISPLAY_TIMEOUT_MS）时会把
     * display_fault 锁存住并永久停止重绘，而框架自己不会解锁。
     * 这里必须主动恢复，否则一次偶发错误就等于整块屏幕"卡死"。
     * 限速 500ms 一次，避免真的坏掉时反复冲击总线。
     */
    if (status == UI_DISPLAY_ERROR || status == UI_DISPLAY_TIMEOUT)
    {
        uint32_t now = HAL_GetTick();
        if (now - recover_last_tick >= 500U)
        {
            recover_last_tick = now;
            UI_RecoverDisplay();
        }
    }
}

void App_UI_HandleEvents(void)
{
    /* 预留：处理其他业务事件 */
}
