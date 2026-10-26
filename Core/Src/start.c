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
uint8_t mpu6050_init_status = MPU6050_ERR_I2C;
uint32_t mpu6050_read_errors = 0;

/* 舵机 */
uint8_t Servo_lock = 0;

/* DHT11 */
DHT11_Data dht11_data = {0, 0, 0};
static uint32_t dht11_lasttime = 0;

/* DS18B20 */
int16_t ds18b20_temp = 0;
uint8_t ds18b20_valid = 0;
static uint32_t ds18b20_lasttime = 0;
static uint32_t ds18b20_conversion_started = 0;
static uint8_t ds18b20_conversion_pending = 0;

/* MPU6050 */
static uint32_t mpu6050_next_sample = 0;
static uint32_t mpu6050_retry_lasttime = 0;
static uint8_t mpu6050_retry_count = 0;
static uint8_t mpu6050_fail_count = 0;

/* 光照与片内温度/电压 */
uint16_t sensor_light_percent = 0;
uint8_t sensor_light_state = 0;
int16_t sensor_chip_temp_x10 = 0;
uint16_t sensor_vdd_mv = 0;
static uint32_t env_lasttime = 0;

#define SENSOR_READ_INTERVAL_MS 2000U
#define ENV_SAMPLE_INTERVAL_MS 200U
#define DS18B20_CONVERSION_TIME_MS 750U
#define MPU6050_SAMPLE_INTERVAL_MS 100U
#define MPU6050_RETRY_INTERVAL_MS 2000U  /* 初始化失败后的快速重试周期 */
#define MPU6050_RETRY_SLOW_MS 30000U     /* 连续失败多次后改成慢速重试 */
#define MPU6050_RETRY_MS 10U             /* 单次采样失败后的最小重试间隔 */

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

static uint8_t DS18B20_StartConversion(void)
{
    if (!DS18B20_Reset()) return 0;

    DS18B20_WriteByte(0xCC); /* 跳过 ROM */
    DS18B20_WriteByte(0x44); /* 启动温度转换 */
    return 1;
}

static uint8_t DS18B20_ReadTemperature(int16_t *temperature)
{
    uint8_t temp_l, temp_h;
    int16_t raw;

    if (temperature == NULL || !DS18B20_Reset()) return 0;

    DS18B20_WriteByte(0xCC); /* 跳过 ROM */
    DS18B20_WriteByte(0xBE); /* 读取暂存器 */

    temp_l = DS18B20_ReadByte();
    temp_h = DS18B20_ReadByte();

    raw = (int16_t)((temp_h << 8) | temp_l);
    *temperature = raw * 100 / 16;
    return 1;
}

/* ========== ADC 读取 ========== */
uint32_t Read_ADC_Channel(uint32_t channel)
{
    uint32_t value = 0;

    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    /* 内部温度和参考电压通道要求至少 17.1 us 的采样时间。 */
    sConfig.SamplingTime = (channel == ADC_CHANNEL_TEMPSENSOR ||
                            channel == ADC_CHANNEL_VREFINT)
                               ? ADC_SAMPLETIME_239CYCLES_5
                               : ADC_SAMPLETIME_71CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return 0;
    }

    if (HAL_ADC_Start(&hadc1) == HAL_OK) {
        if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK) {
            value = HAL_ADC_GetValue(&hadc1);
        }
        HAL_ADC_Stop(&hadc1);
    }

    return value;
}

/* ========== MPU6050 初始化 ========== */
void MPU6050_System_Init(void)
{
    uint8_t status = MPU6050_Init(&hi2c1);

    mpu6050_init_status = status;
    mpu6050_initialized = (status == MPU6050_OK) ? 1U : 0U;

    if (mpu6050_initialized) {
        mpu6050_read_errors = 0;
        mpu6050_retry_count = 0;
        mpu6050_fail_count = 0;
        mpu6050_next_sample = HAL_GetTick() + MPU6050_SAMPLE_INTERVAL_MS;
    }
}

/* ========== 光照 / 片内温度 / 供电电压 ========== */

/*
 * 全部使用整数运算：
 *   VDD(mV) = 1.20V * 4095 / VREFINT_ADC
 *   片内温度(×10) = ((Vsense(mV) - 1430) * 100) / 43 + 250
 * 这样整个工程不需要链接浮点 printf，节省约 3KB Flash。
 */
static void Sensor_UpdateEnv(void)
{
    uint32_t light_adc = Read_ADC_Channel(ADC_CHANNEL_9);
    uint32_t temp_adc = Read_ADC_Channel(ADC_CHANNEL_TEMPSENSOR);
    uint32_t vref_adc = Read_ADC_Channel(ADC_CHANNEL_VREFINT);

    sensor_light_state = (HAL_GPIO_ReadPin(Light_do_GPIO_Port, Light_do_Pin) !=
                          GPIO_PIN_RESET)
                             ? 1U
                             : 0U;
    sensor_light_percent = (uint16_t)((4095U - light_adc) * 100U / 4095U);

    if (vref_adc != 0U) {
        uint32_t vdd_mv = 1200U * 4095U / vref_adc;
        int32_t vsense_mv = (int32_t)(temp_adc * vdd_mv / 4095U);

        sensor_vdd_mv = (uint16_t)vdd_mv;
        sensor_chip_temp_x10 = (int16_t)(((vsense_mv - 1430) * 100) / 43 + 250);
    }
}

/* ========== 传感器定时读取 ========== */
void Sensor_Process(void)
{
    uint32_t now = HAL_GetTick();

    /* 光照 / 片内温度每 200ms 采样一次 */
    if (now - env_lasttime >= ENV_SAMPLE_INTERVAL_MS) {
        env_lasttime = now;
        Sensor_UpdateEnv();
    }

    /* DHT11 每2秒读一次（仅用湿度） */
    if (now - dht11_lasttime >= SENSOR_READ_INTERVAL_MS) {
        dht11_data = Read_DHT11();
        dht11_lasttime = HAL_GetTick();
    }

    /* 将单总线温度转换启动和后续读取分开处理。 */
    now = HAL_GetTick();
    if (!ds18b20_conversion_pending &&
        now - ds18b20_lasttime >= SENSOR_READ_INTERVAL_MS) {
        ds18b20_lasttime = now;
        if (DS18B20_StartConversion()) {
            ds18b20_conversion_started = HAL_GetTick();
            ds18b20_conversion_pending = 1;
        } else {
            ds18b20_valid = 0;
        }
    }

    now = HAL_GetTick();
    if (ds18b20_conversion_pending &&
        now - ds18b20_conversion_started >= DS18B20_CONVERSION_TIME_MS) {
        int16_t temperature;
        ds18b20_conversion_pending = 0;
        if (DS18B20_ReadTemperature(&temperature)) {
            ds18b20_temp = temperature;
            ds18b20_valid = 1;
        } else {
            ds18b20_valid = 0;
        }
    }

    now = HAL_GetTick();
    if (!mpu6050_initialized) {
        /* 上电时序、接触不良都可能让初始化失败。前 5 次每 2s 重试一次，
         * 之后降到 30s —— 芯片要是真坏了，不能每 2s 拿 250ms 的延时去拖慢主循环。 */
        uint32_t interval = (mpu6050_retry_count < 5U) ? MPU6050_RETRY_INTERVAL_MS
                                                       : MPU6050_RETRY_SLOW_MS;
        if (now - mpu6050_retry_lasttime >= interval) {
            mpu6050_retry_lasttime = now;
            if (mpu6050_retry_count < 0xFFU) {
                ++mpu6050_retry_count;
            }
            MPU6050_System_Init();
        }
    } else if ((int32_t)(now - mpu6050_next_sample) >= 0) {
        if (hi2c1.State != HAL_I2C_STATE_READY) {
            /* OLED 的中断/DMA 刷新正占着 I2C1。这时候发起阻塞读一定会拿到
             * HAL_BUSY，既读不到数据又会把错误计数刷爆，所以先让一让。 */
            mpu6050_next_sample = now + MPU6050_RETRY_MS;
        } else if (MPU6050_Read_All(&hi2c1, &mpu6050_data) == 0U) {
            mpu6050_fail_count = 0;
            mpu6050_next_sample = now + MPU6050_SAMPLE_INTERVAL_MS;
        } else {
            /* 连续失败要指数退避。固定 10ms 重试会把 I2C 总线打满，
             * 主循环拿不到时间片，界面看起来就是卡死。 */
            ++mpu6050_read_errors;
            if (mpu6050_fail_count < 6U) {
                ++mpu6050_fail_count;
            }
            mpu6050_next_sample = now + ((uint32_t)MPU6050_RETRY_MS << mpu6050_fail_count);
        }
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

    /* 整数映射：0.5ms~2.5ms 对应 0°~180°，CCR = 50 + 200 * Count / 20 */
    uint16_t duty = (uint16_t)(50U + 200U * Count / count_MAX);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, duty);
}
