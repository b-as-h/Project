/**
 * @file start.c
 * @brief 业务逻辑：传感器驱动、舵机控制、MPU6050 初始化。
 *
 * 包含 DHT11、DS18B20 驱动，ADC 读取，舵机 PWM 控制。
 * 显示和输入处理已移至 app_ui.c 和 input.c。
 */

#include "start.h"
#include "mpu6050.h"
#include "i2c.h"

/* ========== 全局变量定义 ========== */

/* MPU6050 */
MPU6050_t mpu6050_data;
uint8_t mpu6050_initialized = 0;

/* 舵机 */
uint8_t Servo_lock = 0;

/* DHT11 */
DHT11_Data dht11_data = {0, 0, 0};
static uint32_t dht11_lasttime = 0;

/* DS18B20 */
int16_t ds18b20_temp = 0;
uint8_t ds18b20_valid = 0;
static uint32_t ds18b20_lasttime = 0;

/* 编码器计数（舵机控制用） */
static uint32_t Count = 0;
#define count_MAX 20

/* ========== DWT 微秒延时 ========== */
static uint8_t dwt_inited = 0;

static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    dwt_inited = 1;
}

static void delay_us(uint32_t us)
{
    if (!dwt_inited) DWT_Init();
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < ticks);
}

static int wait_pin_level_ex(GPIO_TypeDef *port, uint16_t pin, uint8_t level, uint32_t timeout_us)
{
    if (!dwt_inited) DWT_Init();
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = timeout_us * (SystemCoreClock / 1000000);
    while (HAL_GPIO_ReadPin(port, pin) != level) {
        if ((DWT->CYCCNT - start) > ticks) return -1;
    }
    return 0;
}

static int wait_pin_level(uint8_t level, uint32_t timeout_us)
{
    return wait_pin_level_ex(Temperature_Humidity_GPIO_Port, Temperature_Humidity_Pin, level, timeout_us);
}

/* ========== DHT11 驱动 ========== */
static DHT11_Data Read_DHT11(void)
{
    DHT11_Data data = {0, 0, 0};
    uint8_t bits[5] = {0};
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (!dwt_inited) DWT_Init();

    /* 重配 PA7 为推挽输出 */
    GPIO_InitStruct.Pin = Temperature_Humidity_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(Temperature_Humidity_GPIO_Port, &GPIO_InitStruct);

    /* 发送起始信号：拉低 ≥18ms */
    HAL_GPIO_WritePin(Temperature_Humidity_GPIO_Port, Temperature_Humidity_Pin, GPIO_PIN_RESET);
    HAL_Delay(20);

    /* 拉高 20-40μs */
    HAL_GPIO_WritePin(Temperature_Humidity_GPIO_Port, Temperature_Humidity_Pin, GPIO_PIN_SET);
    delay_us(30);

    /* 切换为输入 */
    GPIO_InitStruct.Pin = Temperature_Humidity_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(Temperature_Humidity_GPIO_Port, &GPIO_InitStruct);

    /* 等待 DHT11 响应 */
    if (wait_pin_level(GPIO_PIN_RESET, 200) != 0) return data;
    if (wait_pin_level(GPIO_PIN_SET, 200) != 0) return data;
    if (wait_pin_level(GPIO_PIN_RESET, 200) != 0) return data;

    /* 读取 40 位数据 */
    for (int i = 0; i < 40; i++) {
        if (wait_pin_level(GPIO_PIN_SET, 200) != 0) return data;
        uint32_t start = DWT->CYCCNT;
        if (wait_pin_level(GPIO_PIN_RESET, 200) != 0) return data;
        uint32_t elapsed = (DWT->CYCCNT - start) / (SystemCoreClock / 1000000);

        if (elapsed > 40) {
            bits[i / 8] |= (1 << (7 - (i % 8)));
        }
    }

    /* 校验和 */
    if ((uint8_t)(bits[0] + bits[1] + bits[2] + bits[3]) == bits[4]) {
        data.humi = bits[0];
        data.temp = bits[2];
        data.valid = 1;
    }

    return data;
}

/* ========== DS18B20 驱动 ========== */
static void DS18B20_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = T_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(T_GPIO_Port, &GPIO_InitStruct);
}

static void DS18B20_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = T_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(T_GPIO_Port, &GPIO_InitStruct);
}

static uint8_t DS18B20_Reset(void)
{
    DS18B20_SetOutput();
    HAL_GPIO_WritePin(T_GPIO_Port, T_Pin, GPIO_PIN_RESET);
    delay_us(480);
    DS18B20_SetInput();
    delay_us(80);
    uint8_t presence = (HAL_GPIO_ReadPin(T_GPIO_Port, T_Pin) == GPIO_PIN_RESET) ? 1 : 0;
    delay_us(400);
    return presence;
}

static void DS18B20_WriteBit(uint8_t bit)
{
    DS18B20_SetOutput();
    HAL_GPIO_WritePin(T_GPIO_Port, T_Pin, GPIO_PIN_RESET);
    delay_us(5);
    if (bit) HAL_GPIO_WritePin(T_GPIO_Port, T_Pin, GPIO_PIN_SET);
    delay_us(60);
    DS18B20_SetInput();
    delay_us(2);
}

static uint8_t DS18B20_ReadBit(void)
{
    uint8_t bit;
    DS18B20_SetOutput();
    HAL_GPIO_WritePin(T_GPIO_Port, T_Pin, GPIO_PIN_RESET);
    delay_us(2);
    DS18B20_SetInput();
    delay_us(10);
    bit = (HAL_GPIO_ReadPin(T_GPIO_Port, T_Pin) == GPIO_PIN_SET) ? 1 : 0;
    delay_us(50);
    return bit;
}

static void DS18B20_WriteByte(uint8_t data)
{
    for (int i = 0; i < 8; i++) {
        DS18B20_WriteBit(data & 1);
        data >>= 1;
    }
}

static uint8_t DS18B20_ReadByte(void)
{
    uint8_t data = 0;
    for (int i = 0; i < 8; i++) {
        data >>= 1;
        if (DS18B20_ReadBit()) data |= 0x80;
    }
    return data;
}

static int16_t Read_DS18B20(void)
{
    uint8_t presence;
    uint8_t temp_l, temp_h;
    int16_t raw;

    presence = DS18B20_Reset();
    if (!presence) return -32768;

    DS18B20_WriteByte(0xCC); /* 跳过 ROM */
    DS18B20_WriteByte(0x44); /* 启动温度转换 */

    /* 等待转换完成 */
    DS18B20_SetInput();
    uint32_t wait_start = HAL_GetTick();
    while (HAL_GPIO_ReadPin(T_GPIO_Port, T_Pin) == GPIO_PIN_RESET) {
        if (HAL_GetTick() - wait_start > 1000) return -32768;
    }

    presence = DS18B20_Reset();
    if (!presence) return -32768;

    DS18B20_WriteByte(0xCC); /* 跳过 ROM */
    DS18B20_WriteByte(0xBE); /* 读暂存器 */

    temp_l = DS18B20_ReadByte();
    temp_h = DS18B20_ReadByte();

    raw = (int16_t)((temp_h << 8) | temp_l);
    return raw * 100 / 16;
}

/* ========== ADC 读取 ========== */
uint32_t Read_ADC_Channel(uint32_t channel)
{
    uint32_t value = 0;

    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    /* 临时改为单通道单次转换 */
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.NbrOfConversion = 1;
    HAL_ADC_Init(&hadc1);

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK) {
        value = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    return value;
}

/* ========== MPU6050 初始化 ========== */
void MPU6050_System_Init(void)
{
    if (MPU6050_Init(&hi2c1) == 0) {
        mpu6050_initialized = 1;
    }
}

/* ========== 传感器定时读取 ========== */
void Sensor_Process(void)
{
    /* DHT11 每2秒读一次（仅用湿度） */
    if (HAL_GetTick() - dht11_lasttime >= 2000) {
        dht11_data = Read_DHT11();
        dht11_lasttime = HAL_GetTick();
    }

    /* DS18B20 每2秒读一次 */
    if (HAL_GetTick() - ds18b20_lasttime >= 2000) {
        int16_t temp = Read_DS18B20();
        if (temp > -500) {
            ds18b20_temp = temp;
            ds18b20_valid = 1;
        } else {
            ds18b20_valid = 0;
        }
        ds18b20_lasttime = HAL_GetTick();
    }
}

/* ========== 舵机控制 ========== */
void Servo(void)
{
    Count = __HAL_TIM_GET_COUNTER(&htim2);

    if (Count > count_MAX) {
        if (Count > 60000) {
            Count = 0;
            __HAL_TIM_SET_COUNTER(&htim2, Count);
        } else {
            Count = count_MAX;
            __HAL_TIM_SET_COUNTER(&htim2, count_MAX);
        }
    }

    uint8_t duty = (10 * Count / (float)count_MAX + 2.5) / 100 * 2000;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, duty);
}
