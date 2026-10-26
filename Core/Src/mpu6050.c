#include <math.h>
#include <stddef.h>
#include "mpu6050.h"

#define RAD_TO_DEG 57.29578f

#define WHO_AM_I_REG 0x75
#define PWR_MGMT_1_REG 0x6B
#define PWR_MGMT_2_REG 0x6C
#define SIGNAL_PATH_RESET_REG 0x68
#define SMPLRT_DIV_REG 0x19
#define ACCEL_CONFIG_REG 0x1C
#define ACCEL_XOUT_H_REG 0x3B
#define TEMP_OUT_H_REG 0x41
#define GYRO_CONFIG_REG 0x1B
#define GYRO_XOUT_H_REG 0x43

// Setup MPU6050
#define MPU6050_ADDR 0xD0
const uint16_t i2c_timeout = 100;
/*
 * ±2g 量程下 1g 对应 16384 LSB。早期这里写的是 14418（约 1.14g），
 * 会让 Z 轴静止时恒偏大 13.6%，已改回理论值。
 * 如果确实要给 Z 轴做单点校准，改这个常数即可。
 */
const float Accel_Z_corrector = 16384.0f;

static uint32_t timer;

/* 最近一次读到的 WHO_AM_I，供诊断页面显示 */
uint8_t mpu6050_who_am_i = 0;

/* 初始化时回读到的 PWR_MGMT_1，用于判断芯片是否真的醒了 */
uint8_t mpu6050_pwr_mgmt = 0xFF;
uint8_t mpu6050_pwr_mgmt2 = 0xFF;
uint8_t mpu6050_accel_config = 0xFF;
uint8_t mpu6050_gyro_config = 0xFF;

/* 常见 MPU6050 及其兼容型号的 WHO_AM_I 回读值 */
static uint8_t MPU6050_IsSupportedId(uint8_t id)
{
    switch (id) {
    case 0x68: /* MPU6050 / MPU6000 */
    case 0x70: /* MPU6500 */
    case 0x71: /* MPU9250 */
    case 0x72: /* MPU9255 */
    case 0x73: /* MPU6505 */
    case 0x98: /* MPU6500 部分批次 */
        return 1;
    default:
        return 0;
    }
}

Kalman_t KalmanX = {
    .Q_angle = 0.001f,
    .Q_bias = 0.003f,
    .R_measure = 0.03f};

Kalman_t KalmanY = {
    .Q_angle = 0.001f,
    .Q_bias = 0.003f,
    .R_measure = 0.03f,
};

uint8_t MPU6050_Init(I2C_HandleTypeDef *I2Cx)
{
    uint8_t check = 0;
    uint8_t Data;

    mpu6050_who_am_i = 0;
    mpu6050_pwr_mgmt = 0xFF;

    /* ---- 1. 确认器件在线并识别型号 ---- */
    if (HAL_I2C_Mem_Read(I2Cx, MPU6050_ADDR, WHO_AM_I_REG, 1, &check, 1,
                         i2c_timeout) != HAL_OK) {
        return MPU6050_ERR_I2C;
    }

    mpu6050_who_am_i = check;

    /* 0x68 是正品 MPU6050；0x70/0x71/0x98 等是常见的 MPU6500/9250 兼容片，
     * 寄存器映射一致，可以正常用，不再一律判死。 */
    if (!MPU6050_IsSupportedId(check)) {
        return MPU6050_ERR_ID;
    }

    /* ---- 2. 软复位，清掉上一次运行残留的休眠/循环模式 ---- */
    Data = 0x80; /* DEVICE_RESET */
    if (HAL_I2C_Mem_Write(I2Cx, MPU6050_ADDR, PWR_MGMT_1_REG, 1,
                          &Data, 1, i2c_timeout) != HAL_OK) {
        return MPU6050_ERR_CONFIG;
    }
    HAL_Delay(100);

    /* ---- 3. 唤醒 ----
     * 原来写 0x00，走内部 8MHz RC 振荡器。部分兼容片这个振荡器起不来，
     * 表现就是：WHO_AM_I 正常、I2C 读写都成功，但加速度/陀螺/温度寄存器
     * 恒为 0（温度恰好 36.53）。改用 X 轴陀螺 PLL 作时钟源(CLKSEL=1) 更稳。 */
    Data = 0x01;
    if (HAL_I2C_Mem_Write(I2Cx, MPU6050_ADDR, PWR_MGMT_1_REG, 1,
                          &Data, 1, i2c_timeout) != HAL_OK) {
        return MPU6050_ERR_CONFIG;
    }
    HAL_Delay(50);

    /* ---- 4. 回读确认真的醒了 ----
     * 写寄存器时 HAL 只保证字节发出去了，芯片有没有接受它并不知道。 */
    if (HAL_I2C_Mem_Read(I2Cx, MPU6050_ADDR, PWR_MGMT_1_REG, 1, &Data, 1,
                         i2c_timeout) != HAL_OK) {
        return MPU6050_ERR_I2C;
    }
    mpu6050_pwr_mgmt = Data;
    if ((Data & 0x40U) != 0U) { /* SLEEP 位没清掉 */
        return MPU6050_ERR_SLEEP;
    }

    /* ---- 5. 复位模拟信号通路，并把所有轴从 standby 里放出来 ----
     * 这两步是"数据寄存器恒为 0"的标准恢复手段：只要模拟通路卡住或
     * PWR_MGMT_2 的 STBY 位被置上，芯片就会一直输出 0。 */
    Data = 0x07; /* SIGNAL_PATH_RESET: gyro | accel | temp */
    if (HAL_I2C_Mem_Write(I2Cx, MPU6050_ADDR, SIGNAL_PATH_RESET_REG, 1,
                          &Data, 1, i2c_timeout) != HAL_OK) {
        return MPU6050_ERR_CONFIG;
    }
    HAL_Delay(100);

    Data = 0x00; /* PWR_MGMT_2: 全部轴使能 */
    if (HAL_I2C_Mem_Write(I2Cx, MPU6050_ADDR, PWR_MGMT_2_REG, 1,
                          &Data, 1, i2c_timeout) != HAL_OK) {
        return MPU6050_ERR_CONFIG;
    }

    /* ---- 6. 采样率 / 量程 ---- */
    // Set DATA RATE of 1KHz by writing SMPLRT_DIV register
    Data = 0x07;
    if (HAL_I2C_Mem_Write(I2Cx, MPU6050_ADDR, SMPLRT_DIV_REG, 1,
                          &Data, 1, i2c_timeout) != HAL_OK) {
        return MPU6050_ERR_CONFIG;
    }

    // Set accelerometer configuration in ACCEL_CONFIG Register
    // XA_ST=0,YA_ST=0,ZA_ST=0, FS_SEL=0 -> ±2g
    Data = 0x00;
    if (HAL_I2C_Mem_Write(I2Cx, MPU6050_ADDR, ACCEL_CONFIG_REG, 1,
                          &Data, 1, i2c_timeout) != HAL_OK) {
        return MPU6050_ERR_CONFIG;
    }

    // Set Gyroscopic configuration in GYRO_CONFIG Register
    // XG_ST=0,YG_ST=0,ZG_ST=0, FS_SEL=0 -> ±250 °/s
    Data = 0x00;
    if (HAL_I2C_Mem_Write(I2Cx, MPU6050_ADDR, GYRO_CONFIG_REG, 1,
                          &Data, 1, i2c_timeout) != HAL_OK) {
        return MPU6050_ERR_CONFIG;
    }

    /* ---- 7. 回读配置寄存器，诊断页面要用 ---- */
    if (HAL_I2C_Mem_Read(I2Cx, MPU6050_ADDR, PWR_MGMT_2_REG, 1,
                         &mpu6050_pwr_mgmt2, 1, i2c_timeout) != HAL_OK ||
        HAL_I2C_Mem_Read(I2Cx, MPU6050_ADDR, ACCEL_CONFIG_REG, 1,
                         &mpu6050_accel_config, 1, i2c_timeout) != HAL_OK ||
        HAL_I2C_Mem_Read(I2Cx, MPU6050_ADDR, GYRO_CONFIG_REG, 1,
                         &mpu6050_gyro_config, 1, i2c_timeout) != HAL_OK) {
        return MPU6050_ERR_I2C;
    }

    timer = HAL_GetTick();
    return MPU6050_OK;
}

void MPU6050_Read_Accel(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct)
{
    uint8_t Rec_Data[6];

    // Read 6 BYTES of data starting from ACCEL_XOUT_H register

    HAL_I2C_Mem_Read(I2Cx, MPU6050_ADDR, ACCEL_XOUT_H_REG, 1, Rec_Data, 6, i2c_timeout);

    DataStruct->Accel_X_RAW = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->Accel_Y_RAW = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
    DataStruct->Accel_Z_RAW = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);

    /*** convert the RAW values into acceleration in 'g'
         we have to divide according to the Full scale value set in FS_SEL
         I have configured FS_SEL = 0. So I am dividing by 16384.0
         for more details check ACCEL_CONFIG Register              ****/

    DataStruct->Ax = DataStruct->Accel_X_RAW / 16384.0f;
    DataStruct->Ay = DataStruct->Accel_Y_RAW / 16384.0f;
    DataStruct->Az = DataStruct->Accel_Z_RAW / Accel_Z_corrector;
}

void MPU6050_Read_Gyro(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct)
{
    uint8_t Rec_Data[6];

    // Read 6 BYTES of data starting from GYRO_XOUT_H register

    HAL_I2C_Mem_Read(I2Cx, MPU6050_ADDR, GYRO_XOUT_H_REG, 1, Rec_Data, 6, i2c_timeout);

    DataStruct->Gyro_X_RAW = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->Gyro_Y_RAW = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
    DataStruct->Gyro_Z_RAW = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);

    /*** convert the RAW values into dps (�/s)
         we have to divide according to the Full scale value set in FS_SEL
         I have configured FS_SEL = 0. So I am dividing by 131.0
         for more details check GYRO_CONFIG Register              ****/

    DataStruct->Gx = DataStruct->Gyro_X_RAW / 131.0f;
    DataStruct->Gy = DataStruct->Gyro_Y_RAW / 131.0f;
    DataStruct->Gz = DataStruct->Gyro_Z_RAW / 131.0f;
}

void MPU6050_Read_Temp(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct)
{
    uint8_t Rec_Data[2];
    int16_t temp;

    // Read 2 BYTES of data starting from TEMP_OUT_H_REG register

    HAL_I2C_Mem_Read(I2Cx, MPU6050_ADDR, TEMP_OUT_H_REG, 1, Rec_Data, 2, i2c_timeout);

    temp = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->Temperature = (float)((int16_t)temp / (float)340.0 + (float)36.53);
}

uint8_t MPU6050_Read_All(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct)
{
    uint8_t Rec_Data[14];
    int16_t temp;
    uint8_t i;

    if (I2Cx == NULL || DataStruct == NULL) {
        return 1;
    }

    // Read 14 BYTES of data starting from ACCEL_XOUT_H register

    if (HAL_I2C_Mem_Read(I2Cx, MPU6050_ADDR, ACCEL_XOUT_H_REG, 1,
                         Rec_Data, 14, i2c_timeout) != HAL_OK) {
        return 1;
    }

    /*
     * 14 字节连读全为 0，而单字节读（WHO_AM_I / PWR_MGMT_1 / 逐字节数据）
     * 一直是好的 —— 说明问题出在这条连续读通路上（起始寄存器地址没生效，
     * 读到了 0x00 附近一片全空的寄存器）。
     *
     * 这里逐字节重读一遍。14 次事务约 2.2ms，按 100ms 周期算总线占用 2% 左右，
     * 完全可以每次采样都做，不需要限流。
     */
    for (i = 0U; i < 14U; ++i) {
        if (Rec_Data[i] != 0U) {
            break;
        }
    }
    if (i == 14U) {
        for (i = 0U; i < 14U; ++i) {
            if (HAL_I2C_Mem_Read(I2Cx, MPU6050_ADDR,
                                 (uint16_t)(ACCEL_XOUT_H_REG + i), 1,
                                 &Rec_Data[i], 1, i2c_timeout) != HAL_OK) {
                return 1;
            }
        }
    }

    DataStruct->Accel_X_RAW = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->Accel_Y_RAW = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
    DataStruct->Accel_Z_RAW = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);
    temp = (int16_t)(Rec_Data[6] << 8 | Rec_Data[7]);
    DataStruct->Gyro_X_RAW = (int16_t)(Rec_Data[8] << 8 | Rec_Data[9]);
    DataStruct->Gyro_Y_RAW = (int16_t)(Rec_Data[10] << 8 | Rec_Data[11]);
    DataStruct->Gyro_Z_RAW = (int16_t)(Rec_Data[12] << 8 | Rec_Data[13]);

    DataStruct->Ax = DataStruct->Accel_X_RAW / 16384.0f;
    DataStruct->Ay = DataStruct->Accel_Y_RAW / 16384.0f;
    DataStruct->Az = DataStruct->Accel_Z_RAW / Accel_Z_corrector;
    DataStruct->Temperature = (float)temp / 340.0f + 36.53f;
    DataStruct->Gx = DataStruct->Gyro_X_RAW / 131.0f;
    DataStruct->Gy = DataStruct->Gyro_Y_RAW / 131.0f;
    DataStruct->Gz = DataStruct->Gyro_Z_RAW / 131.0f;

    // Kalman angle solve
    uint32_t now = HAL_GetTick();
    float dt = (float)(now - timer) / 1000.0f;
    timer = now;
    float roll;
    float roll_sqrt = sqrtf(
        (float)(DataStruct->Accel_X_RAW * DataStruct->Accel_X_RAW + DataStruct->Accel_Z_RAW * DataStruct->Accel_Z_RAW));
    if (roll_sqrt != 0.0f)
    {
        roll = atanf((float)DataStruct->Accel_Y_RAW / roll_sqrt) * RAD_TO_DEG;
    }
    else
    {
        roll = 0.0f;
    }
    float pitch = atan2f((float)-DataStruct->Accel_X_RAW, (float)DataStruct->Accel_Z_RAW) * RAD_TO_DEG;
    if ((pitch < -90 && DataStruct->KalmanAngleY > 90) || (pitch > 90 && DataStruct->KalmanAngleY < -90))
    {
        KalmanY.angle = pitch;
        DataStruct->KalmanAngleY = pitch;
    }
    else
    {
        DataStruct->KalmanAngleY = Kalman_getAngle(&KalmanY, pitch, DataStruct->Gy, dt);
    }
    /* 俯仰超过 ±90° 时角速度方向翻转，只影响送进卡尔曼的速率，
     * 不能去改 Gx —— Gx 是页面和蓝牙要显示的原始角速度。 */
    {
        float rate_x = (fabsf(DataStruct->KalmanAngleY) > 90.0f)
                           ? -DataStruct->Gx
                           : DataStruct->Gx;
        DataStruct->KalmanAngleX = Kalman_getAngle(&KalmanX, roll, rate_x, dt);
    }
    return 0;
}

float Kalman_getAngle(Kalman_t *Kalman, float newAngle, float newRate, float dt)
{
    float rate = newRate - Kalman->bias;
    Kalman->angle += dt * rate;

    Kalman->P[0][0] += dt * (dt * Kalman->P[1][1] - Kalman->P[0][1] - Kalman->P[1][0] + Kalman->Q_angle);
    Kalman->P[0][1] -= dt * Kalman->P[1][1];
    Kalman->P[1][0] -= dt * Kalman->P[1][1];
    Kalman->P[1][1] += Kalman->Q_bias * dt;

    float S = Kalman->P[0][0] + Kalman->R_measure;
    float K[2];
    K[0] = Kalman->P[0][0] / S;
    K[1] = Kalman->P[1][0] / S;

    float y = newAngle - Kalman->angle;
    Kalman->angle += K[0] * y;
    Kalman->bias += K[1] * y;

    float P00_temp = Kalman->P[0][0];
    float P01_temp = Kalman->P[0][1];

    Kalman->P[0][0] -= K[0] * P00_temp;
    Kalman->P[0][1] -= K[0] * P01_temp;
    Kalman->P[1][0] -= K[1] * P00_temp;
    Kalman->P[1][1] -= K[1] * P01_temp;

    return Kalman->angle;
};
