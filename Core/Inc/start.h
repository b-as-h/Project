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

/* ========== 传感器数据（供 app_ui.c 访问） ========== */
extern DHT11_Data dht11_data;      /* DHT11 温湿度数据 */
extern int16_t ds18b20_temp;       /* DS18B20 温度×100 */
extern uint8_t ds18b20_valid;      /* DS18B20 数据有效标志 */

/* ========== MPU6050 数据 ========== */
#include "mpu6050.h"
extern MPU6050_t mpu6050_data;
extern uint8_t mpu6050_initialized;

/* ========== 舵机控制变量 ========== */
extern uint8_t Servo_lock;         /* 舵机锁定状态 */

/* ========== 函数声明 ========== */

/**
 * @brief MPU6050 初始化
 */
void MPU6050_System_Init(void);

/**
 * @brief 传感器定时读取（主循环调用）
 * @note  DHT11 每2秒读一次（仅取湿度），DS18B20 每2秒读一次
 */
void Sensor_Process(void);

/**
 * @brief 舵机控制函数
 * @note  编码器值 0~20 映射到舵机角度 0~180°
 */
void Servo(void);

/**
 * @brief 读取 ADC 通道值
 * @param channel ADC 通道号
 * @return ADC 值 (0~4095)
 */
uint32_t Read_ADC_Channel(uint32_t channel);

#endif /* __START_H */
