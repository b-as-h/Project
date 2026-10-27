/**
 * @file start.h
 * @brief 业务逻辑接口：传感器、舵机、MPU6050。
 *
 * 提供传感器数据访问、舵机控制、MPU6050 初始化等功能。
 * 显示和输入处理已移至 app_ui.c 和 input.c。
 */

#ifndef __START_H
#define __START_H

#include "main.h"
#include "adc.h"
#include "tim.h"
#include <string.h>
#include <stdio.h>

/* ========== DHT11 数据结构 ========== */
typedef struct {
    int8_t temp;   /* 温度(℃) */
    uint8_t humi;  /* 湿度(%) */
    uint8_t valid; /* 1=数据有效 */
} DHT11_Data;

/* ========== 传感器数据（供 app_ui.c / bt.c 访问） ========== */
extern DHT11_Data dht11_data;      /* DHT11 温湿度数据 */
extern int16_t ds18b20_temp;       /* DS18B20 温度×100 */
extern uint8_t ds18b20_valid;      /* DS18B20 数据有效标志 */

/* 光照与片内传感器：由 Sensor_Process() 周期采样，与页面是否打开无关 */
extern uint16_t sensor_light_percent; /* 光照强度 0~100 */
extern uint8_t sensor_light_state;    /* 1=明亮环境，0=黑暗环境 */
extern int16_t sensor_chip_temp_x10;  /* 片内温度 ×10（298 表示 29.8℃） */
extern uint16_t sensor_vdd_mv;        /* VDDA 电压，单位 mV */

/* ========== MPU6050 数据 ========== */
#include "mpu6050.h"
extern MPU6050_t mpu6050_data;
extern uint8_t mpu6050_initialized;    /* 1=初始化成功，可以采样 */
extern uint8_t mpu6050_init_status;    /* 最近一次初始化结果：MPU6050_OK / MPU6050_ERR_* */
extern uint32_t mpu6050_read_errors;   /* 采样失败累计次数（多为 I2C 被 OLED 异步刷新占用） */

/* ========== 舵机控制变量 ========== */
extern uint8_t Servo_lock;         /* 舵机锁定状态：1=锁定，编码器不再改变角度 */

/* ========== 函数声明 ========== */

/**
 * @brief MPU6050 初始化
 */
void MPU6050_System_Init(void);

/**
 * @brief 传感器定时读取（主循环调用）
 * @note  DHT11 和 DS18B20 每2秒更新一次；MPU6050 在后台按100ms周期采样
 */
void Sensor_Process(void);

/**
 * @brief 设置舵机档位
 * @param pos 0~20，对应 0°~180°，越界自动钳位
 * @note  只改软件位置并刷新 CCR，不触碰 TIM2 编码器计数器
 */
void Servo_SetPos(int16_t pos);

/**
 * @brief 在当前档位上增减
 * @param delta 增量，可正可负
 * @note  越界自动钳位；越界量不会丢失，往回拧立即生效
 */
void Servo_Step(int16_t delta);

/**
 * @brief 读取当前档位
 * @return 0~20
 */
int16_t Servo_GetPos(void);

/**
 * @brief 按角度设置舵机
 * @param angle 0~180°
 */
void Servo_SetAngle(uint8_t angle);

/**
 * @brief 读取当前角度
 * @return 0~180°
 */
uint8_t Servo_GetAngle(void);

/**
 * @brief 读取 ADC 通道值
 * @param channel ADC 通道号
 * @return ADC 值 (0~4095)
 */
uint32_t Read_ADC_Channel(uint32_t channel);

#endif /* __START_H */
