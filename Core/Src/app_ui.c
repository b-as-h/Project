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
#include <string.h>
#include <stdio.h>

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
static uint32_t servo_saved_count = 0; /* 进入舵机页时保存的编码器值 */
static uint32_t servo_last_count = 0;  /* 锁定时保存的编码器值 */

/* ========== 自定义页定时器 ========== */
static uint32_t lock_last_tick = 0;
static uint32_t sensor_last_tick = 0;
static uint32_t servo_last_tick = 0;
static uint32_t mpu_last_tick = 0;

/* Sensor values are sampled on the page tick, never from the draw callback. */
static uint8_t sensor_light_state = 0;
static uint16_t sensor_light_percent = 0;
static float sensor_vdda = 0.0f;
static int8_t sensor_chip_temp = 0;

static void App_UI_UpdateSensorData(void)
{
    uint32_t light_adc = Read_ADC_Channel(ADC_CHANNEL_9);
    uint32_t temp_adc = Read_ADC_Channel(ADC_CHANNEL_TEMPSENSOR);
    uint32_t vref_adc = Read_ADC_Channel(ADC_CHANNEL_VREFINT);

    sensor_light_state = (HAL_GPIO_ReadPin(Light_do_GPIO_Port, Light_do_Pin) !=
                          GPIO_PIN_RESET)
                             ? 1U
                             : 0U;
    sensor_light_percent = (uint16_t)((4095U - light_adc) * 100U / 4095U);

    if (vref_adc != 0U)
    {
        sensor_vdda = 1.20f * 4095.0f / (float)vref_adc;
        {
            float vsense = (float)temp_adc * sensor_vdda / 4095.0f;
            sensor_chip_temp = (int8_t)((vsense - 1.43f) / 0.0043f + 25.0f);
        }
    }
}

static void App_UI_UpdateMpuData(void)
{
    if (mpu6050_initialized)
    {
        MPU6050_Read_All(&hi2c1, &mpu6050_data);
    }
}

/* ========== 首页 ========== */
static const UI_HomeItem home_items[] = {
    {"Sensor", icon_sensor, APP_PAGE_SENSOR},
    {"Servo", icon_servo, APP_PAGE_SERVO},
    {"MPU6050", icon_mpu, APP_PAGE_MPU6050},
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
};

/* ========== 文案 ========== */
static const UI_Texts texts = {
    .return_text = "<Back",
    .cancel_text = "Cancel",
    .confirm_text = "OK",
    .on_text = "ON",
    .off_text = "OFF",
    .message_title = "Info",
};

/* ========== 应用描述（非 const，以便动态修改 root_page） ========== */
static UI_App app_desc = {
    .root_page = APP_PAGE_LOCK, /* 上电默认锁屏 */
    .routes = routes,
    .route_count = sizeof(routes) / sizeof(routes[0]),
    .home_pages = home_pages,
    .home_page_count = sizeof(home_pages) / sizeof(home_pages[0]),
    .menu_pages = NULL,
    .menu_page_count = 0,
    .info_pages = NULL,
    .info_page_count = 0,
    .custom_page_count = 4, /* 锁屏、传感器、舵机、MPU6050 */
    .int_bindings = NULL,
    .int_binding_count = 0,
    .bool_bindings = NULL,
    .bool_binding_count = 0,
    .confirm_descs = NULL,
    .confirm_desc_count = 0,
    .fonts = {
        .home_font = font_6x10,
        .title_font = font_6x10,
        .body_font = font_6x10,
    },
    .texts = texts,
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

#if UI_ENABLE_CUSTOM
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
        /* 保存当前编码器值，设置为舵机角度 */
        servo_saved_count = __HAL_TIM_GET_COUNTER(&htim2);
        /* 从 Servo_lock 恢复或设置默认值 */
        if (Servo_lock)
        {
            __HAL_TIM_SET_COUNTER(&htim2, servo_last_count);
        }
        servo_last_tick = 0;
        break;

    case APP_PAGE_SENSOR:
        sensor_last_tick = 0;
        break;

    case APP_PAGE_MPU6050:
        mpu_last_tick = 0;
        break;

    default:
        break;
    }
}

void UI_CustomOnLeave(UI_PageId page)
{
    switch (page)
    {
    case APP_PAGE_SERVO:
        /* 恢复编码器值 */
        __HAL_TIM_SET_COUNTER(&htim2, servo_saved_count);
        break;

    default:
        break;
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
            /* 切换舵机锁定状态 */
            if (Servo_lock)
            {
                /* 解锁：恢复之前的位置 */
                Servo_lock = 0;
                __HAL_TIM_SET_COUNTER(&htim2, servo_last_count);
            }
            else
            {
                /* 锁定：保存当前位置 */
                servo_last_count = __HAL_TIM_GET_COUNTER(&htim2);
                Servo_lock = 1;
            }
        }

        /* 编码器控制舵机角度 */
        if (event.source == UI_INPUT_ENCODER && !Servo_lock)
        {
            int32_t count = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
            int16_t delta = (event.action == UI_INPUT_DOWN) ? (int16_t)event.steps : -(int16_t)event.steps;
            count += delta;

            /* 限制范围 0~20 */
            if (count < 0)
                count = 0;
            if (count > 20)
                count = 20;
            __HAL_TIM_SET_COUNTER(&htim2, (uint32_t)count);

            /* 更新舵机 PWM */
            Servo();
        }
        break;

    case APP_PAGE_SENSOR:
    case APP_PAGE_MPU6050:
        /* OK键返回首页 */
        if (event.action == UI_INPUT_OK && event.source == UI_INPUT_PRESS)
        {
            UI_CustomRequestClose();
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
        /* 传感器页每 200ms 刷新 */
        if (now_ms - sensor_last_tick >= 200)
        {
            sensor_last_tick = now_ms;
            App_UI_UpdateSensorData();
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
            App_UI_UpdateMpuData();
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

    char buf[32];

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
            sprintf(buf, "Err:%d/%d", error_count, MAX_ERROR_COUNT);
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
            sprintf(buf, "Num:%d", select_num);
            Font_DrawMixedString(x_offset + 2, 32, buf);
            Font_DrawMixedString(x_offset + 2, 48, "[OK]Confirm");
        }
        else if (lock_state == LOCK_ALARM)
        {
            OLED_SetDrawMode(OLED_DRAW_XOR);
            OLED_DrawBox(x_offset + 2, 16, 80, 16);
            OLED_SetDrawMode(OLED_DRAW_SET);
            Font_DrawMixedString(x_offset + 4, 18, "ERROR!");
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
        sprintf(buf, "L:%d%%", sensor_light_percent);
        Font_DrawMixedString(x_offset + 2, 18, buf);

        /* DS18B20 温度 + DHT11 湿度 */
        if (ds18b20_valid && dht11_data.valid)
        {
            int16_t t = ds18b20_temp;
            sprintf(buf, "T:%d.%02dC H:%d%%",
                    t / 100, (t > 0 ? t : -t) % 100,
                    dht11_data.humi);
        }
        else if (ds18b20_valid)
        {
            int16_t t = ds18b20_temp;
            sprintf(buf, "T:%d.%02dC H:--%%",
                    t / 100, (t > 0 ? t : -t) % 100);
        }
        else if (dht11_data.valid)
        {
            sprintf(buf, "T:--.--C H:%d%%", dht11_data.humi);
        }
        else
        {
            sprintf(buf, "T:--.--C H:--%%");
        }
        Font_DrawMixedString(x_offset + 2, 28, buf);

        /* 片内温度 + VDDA 电压 */
        sprintf(buf, "C:%dC V:%.2fV", sensor_chip_temp, (double)sensor_vdda);
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

        /* 当前角度 */
        uint32_t count = __HAL_TIM_GET_COUNTER(&htim2);
        if (count > 20)
            count = 20;
        int angle = (int)(count * 180 / 20);
        sprintf(buf, "Angle: %d", angle);
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

        /* 标题 */
        Font_DrawMixedString(x_offset + 2, 0, "MPU6050");

        if (!mpu6050_initialized)
        {
            Font_DrawMixedString(x_offset + 2, 18, "初始失败");
            Font_DrawMixedString(x_offset + 2, 38, "[OK]Back");
            break;
        }

        /* 加速度 */
        sprintf(buf, "A:%.1f,%.1f,%.1f", mpu6050_data.Ax, mpu6050_data.Ay, mpu6050_data.Az);
        Font_DrawMixedString(x_offset + 2, 14, buf);

        /* 陀螺仪 */
        sprintf(buf, "G:%.1f,%.1f,%.1f", mpu6050_data.Gx, mpu6050_data.Gy, mpu6050_data.Gz);
        Font_DrawMixedString(x_offset + 2, 28, buf);

        /* 温度 */
        sprintf(buf, "T:%.1fC", mpu6050_data.Temperature);
        Font_DrawMixedString(x_offset + 2, 42, buf);

        /* 返回提示 */
        Font_DrawMixedString(x_offset + 2, 54, "[OK]Back");
        break;

    default:
        break;
    }
}
#endif

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
    /* 检查解锁待处理标志 */
    if (unlock_pending)
    {
        unlock_pending = 0;
        app_desc.root_page = APP_PAGE_HOME;
        UI_Init(&app_desc);
        return;
    }

    UI_Input input = Input_Read();
    UI_Update(HAL_GetTick(), input);
}

void App_UI_HandleEvents(void)
{
    /* 预留：处理其他业务事件 */
}
