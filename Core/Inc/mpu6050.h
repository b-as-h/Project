#ifndef INC_GY521_H_
#define INC_GY521_H_

#include <stdint.h>
#include "i2c.h"

// MPU6050 structure
typedef struct
{

    int16_t Accel_X_RAW;
    int16_t Accel_Y_RAW;
    int16_t Accel_Z_RAW;
    float Ax;
    float Ay;
    float Az;

    int16_t Gyro_X_RAW;
    int16_t Gyro_Y_RAW;
    int16_t Gyro_Z_RAW;
    float Gx;
    float Gy;
    float Gz;

    float Temperature;

    float KalmanAngleX;
    float KalmanAngleY;
} MPU6050_t;

// Kalman structure
typedef struct
{
    float Q_angle;
    float Q_bias;
    float R_measure;
    float angle;
    float bias;
    float P[2][2];
} Kalman_t;

/* ========== 初始化结果码 ========== */
#define MPU6050_OK          0U  /* 初始化成功 */
#define MPU6050_ERR_I2C     1U  /* 器件无应答（接线 / 供电 / AD0 / 坏片） */
#define MPU6050_ERR_ID      2U  /* 有应答但 WHO_AM_I 不是已知型号 */
#define MPU6050_ERR_CONFIG  3U  /* 唤醒或量程寄存器写入失败 */
#define MPU6050_ERR_SLEEP   4U  /* 写进去了但回读仍是 SLEEP，芯片没真正醒来 */

/**
 * @brief 最近一次读到的 WHO_AM_I（0 表示从未读到）。
 * @note  诊断页面用它区分"器件不 ACK"和"型号不对"。
 */
extern uint8_t mpu6050_who_am_i;

/**
 * @brief 初始化时回读到的 PWR_MGMT_1（0xFF 表示没读到）。
 * @note  正常应为 0x01；若 bit6(SLEEP) 仍为 1，说明唤醒没生效。
 */
extern uint8_t mpu6050_pwr_mgmt;

/**
 * @brief 初始化后回读的配置寄存器，供诊断页面显示。
 * @note  PWR_MGMT_2 全 0 表示所有轴都在工作；ACCEL/GYRO_CONFIG 为量程设置。
 */
extern uint8_t mpu6050_pwr_mgmt2;
extern uint8_t mpu6050_accel_config;
extern uint8_t mpu6050_gyro_config;

uint8_t MPU6050_Init(I2C_HandleTypeDef *I2Cx);

void MPU6050_Read_Accel(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct);

void MPU6050_Read_Gyro(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct);

void MPU6050_Read_Temp(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct);

/* Returns 0 on success; failed reads leave the previous data unchanged. */
uint8_t MPU6050_Read_All(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct);

float Kalman_getAngle(Kalman_t *Kalman, float newAngle, float newRate, float dt);

#endif /* INC_GY521_H_ */
