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
 * @brief 设置舵机角度（立即到位）
 * @param a10 0~1800，单位 0.1°，越界自动钳位
 * @note  会取消正在进行的平滑运动或扫描
 */
void Servo_SetAngleX10(int16_t a10);

/**
 * @brief 读取当前角度
 * @return 0~1800，单位 0.1°
 */
int16_t Servo_GetAngleX10(void);

/**
 * @brief 按整度设置舵机（立即到位）
 * @param angle 0~180°
 */
void Servo_SetAngle(uint8_t angle);

/**
 * @brief 读取当前角度（四舍五入到整度）
 * @return 0~180°
 */
uint8_t Servo_GetAngle(void);

/**
 * @brief 在当前角度上按整度增减（编码器页用）
 * @param deg 增量，可正可负；越界自动钳位，越界量不丢失
 * @note  会取消正在进行的扫描
 */
void Servo_Step(int16_t deg);

/**
 * @brief 平滑运动到目标角度
 * @param target_a10 目标角度 ×10（0~1800）
 * @param speed_dps  角速度，单位 °/s；传 0 用默认 60°/s
 * @note  非阻塞，必须由主循环持续调用 Servo_Update()
 */
void Servo_MoveTo(int16_t target_a10, uint16_t speed_dps);

/**
 * @brief 启动 0°↔180° 来回扫描（雷达云台用）
 * @param speed_dps 角速度，单位 °/s；传 0 用默认 60°/s
 */
void Servo_SweepStart(uint16_t speed_dps);

/**
 * @brief 停止扫描/平滑运动，原地保持
 */
void Servo_SweepStop(void);

/**
 * @brief 是否正在运动
 * @return 1=运动中
 */
uint8_t Servo_IsMoving(void);

/**
 * @brief 平滑运动推进，主循环每轮必须调用一次
 * @param now_ms 传入 HAL_GetTick()
 * @return 1=仍在运动
 * @note  内部按真实经过毫秒数插补，主循环被阻塞也不会变速
 */
uint8_t Servo_Update(uint32_t now_ms);

/* ========== 雷达 ========== */
#define RADAR_ALARM_DEFAULT_CM 30U

extern uint16_t Radar_AlarmCm; /* 警报距离（cm），报警逻辑后续接入 */

/**
 * @brief 最近一次测距结果
 * @return 距离 cm；-1 表示无回波，或超声波模块尚未接入
 */
int16_t Radar_GetDistanceCm(void);

/**
 * @brief 读取 ADC 通道值
 * @param channel ADC 通道号
 * @return ADC 值 (0~4095)
 */
uint32_t Read_ADC_Channel(uint32_t channel);

#endif /* __START_H */
