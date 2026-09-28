/**
 * @file app_ui.h
 * @brief BASH_UI 应用层接口。
 *
 * 定义页面 ID、事件 ID 和公共函数。
 * 密码锁屏 → 首页（图标轮播）→ 传感器/舵机/MPU6050 功能页
 */

#ifndef APP_UI_H
#define APP_UI_H

#include "bash_ui.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化 BASH_OLED + BASH_UI，建立菜单、页面和交互。
 * 在 MX_I2C1_Init() 和 Input_Init() 之后调用。
 * @return UI_OK 表示成功。
 */
UI_Status App_UI_Init(void);

/**
 * 主循环中持续调用，处理输入和 UI 刷新。
 * 编码器增量由 Input_Read() 提供。
 */
void App_UI_Process(void);

/**
 * 处理 BASH_UI 事件队列中的业务事件。
 * 在 App_UI_Process() 之后调用。
 */
void App_UI_HandleEvents(void);

/* ========== 事件 ID 定义 ========== */
enum {
    APP_EVENT_UNLOCKED = 1,     /* 密码正确，解锁 */
    APP_EVENT_LOCK,             /* 锁定系统 */
};

/* ========== 自定义页 ID 定义（路由表索引） ========== */
enum {
    APP_PAGE_LOCK     = 1,      /* 自定义：密码锁屏 */
    APP_PAGE_HOME     = 2,      /* 首页：图标轮播 */
    APP_PAGE_SENSOR   = 3,      /* 自定义：传感器显示 */
    APP_PAGE_SERVO    = 4,      /* 自定义：舵机控制 */
    APP_PAGE_MPU6050  = 5,      /* 自定义：MPU6050 显示 */
};

#ifdef __cplusplus
}
#endif

#endif /* APP_UI_H */
