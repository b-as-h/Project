#include "start.h"

// 构思:
// 蓝牙发送格式:0xAA + 0x设备名 + 0x命令 + 0x校验和后二位;--------------暂未实现
// 回传:😜(成功) 😢(失败)
// 设备名:
// 一屏:欢迎页,门禁锁,婴儿防误食
// 二屏:雷达页面,雷达锁
// 三屏:ADC环境监测
// 四屏:电机风扇

#define tick_Max 20

/* OLED相关变量定义 */
unsigned int oled_lasttime = 0;            // OLED刷新时间戳
unsigned int oled_uart_lasttime = 0;       // UART显示时间戳
char oled_buf[20];                         // 显示缓冲区(倒计时)
char oled_buf_name[20];                    // 显示缓冲区(名称)
char oled_buf_num[20];                     // 显示缓冲区(密码)
char oled_uart_buf[20] = "UART1:OFF   ED"; // 显示缓冲区(UART状态)
char oled_switch_buf[4] = "OFF";           // 显示缓冲区(蓝牙开关)
char oled_encoder_buf[20] = "";            // 显示缓冲区(编码器角度)
char oled_ui_buf[20] = "";                 // 显示缓冲区(页面标识)
char oled_servo_lock[10] = "";
char oled_lock[10] = "";
char oled_light[20] = "";
uint8_t oled_uart_flag = 0; // UART发送中标志
uint8_t oled_ui = 1;        // 当前页面编号
uint8_t tick = tick_Max;    // 倒计时,为0时候上锁
uint8_t Lock = 0;           // 1为上锁
uint8_t Servo_lock = 0;
uint32_t servo_last_count = 0; // 保存上锁时的编码器值
uint8_t Light_do = 0;

/* 按键相关变量定义 */
uint32_t key_lasttime_0 = 0;  // PB11蓝牙按键消抖时间
uint32_t key_lasttime_1 = 0;  // PB12切屏按键消抖时间
uint32_t key_lasttime_2 = 0;  // PB10功能按键消抖时间
uint8_t key_last_state_0 = 1; // PB11上拉，未按下=1
uint8_t key_last_state_1 = 0; // PB12下拉，未按下=0
uint8_t key_last_state_2 = 0; // PB10下拉，未按下=0

uint32_t Count = 0; // 编码器计数值
uint32_t Count_2;   // 切屏时保存编码器值

#define count_MAX 20 // 编码器最大值
uint8_t duty = 0;    // 舵机PWM占空比

/* 密码相关变量定义 */
LockState lock_state = LOCK_IDLE;
uint8_t password[PASSWORD_LEN] = {1, 2, 3, 4}; // 正确密码
uint8_t input[PASSWORD_LEN] = {0};             // 用户输入
uint8_t input_index = 0;                       // 当前输入第几位
uint8_t select_num = 0;                        // 当前选择的数字(0-9)
uint8_t error_count = 0;                       // 错误次数

/* ========== DHT11 温湿度 ========== */
typedef struct
{
    int8_t temp;   // 温度(℃)
    uint8_t humi;  // 湿度(%)
    uint8_t valid; // 1=数据有效
} DHT11_Data;
static DHT11_Data dht11_data = {0, 0, 0};
static uint32_t dht11_lasttime = 0;

// DWT 初始化 (用于微秒级延时)
static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

// 微秒延时 (基于DWT, 不受编译器优化影响)
static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < ticks)
        ;
}

// 等待引脚到指定电平, 超时返回-1 (通用版本)
static int wait_pin_level_ex(GPIO_TypeDef *port, uint16_t pin, uint8_t level, uint32_t timeout_us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = timeout_us * (SystemCoreClock / 1000000);
    while (HAL_GPIO_ReadPin(port, pin) != level)
    {
        if ((DWT->CYCCNT - start) > ticks)
            return -1;
    }
    return 0;
}

// 等待引脚到指定电平, 超时返回-1 (DHT11专用, PA7)
static int wait_pin_level(uint8_t level, uint32_t timeout_us)
{
    return wait_pin_level_ex(Temperature_Humidity_GPIO_Port, Temperature_Humidity_Pin, level, timeout_us);
}

// 读取DHT11 (单总线协议)
static DHT11_Data Read_DHT11(void)
{
    DHT11_Data data = {0, 0, 0};
    uint8_t bits[5] = {0};
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    static uint8_t dwt_inited = 0;

    if (!dwt_inited)
    {
        DWT_Init();
        dwt_inited = 1;
    }

    // 1. 重配PA7为推挽输出 (CubeMX配成了ADC, 需要覆盖)
    GPIO_InitStruct.Pin = Temperature_Humidity_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(Temperature_Humidity_GPIO_Port, &GPIO_InitStruct);

    // 2. 发送起始信号: 拉低 ≥18ms
    HAL_GPIO_WritePin(Temperature_Humidity_GPIO_Port, Temperature_Humidity_Pin, GPIO_PIN_RESET);
    HAL_Delay(20);

    // 3. 拉高 20-40μs (上拉电阻拉高电平)
    HAL_GPIO_WritePin(Temperature_Humidity_GPIO_Port, Temperature_Humidity_Pin, GPIO_PIN_SET);
    delay_us(30);

    // 4. 切换为输入 (内部上拉+外部上拉)
    GPIO_InitStruct.Pin = Temperature_Humidity_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(Temperature_Humidity_GPIO_Port, &GPIO_InitStruct);

    // 5. 等待DHT11响应: 拉低80μs → 拉高80μs
    if (wait_pin_level(GPIO_PIN_RESET, 200) != 0)
        return data; // 超时
    if (wait_pin_level(GPIO_PIN_SET, 200) != 0)
        return data;
    if (wait_pin_level(GPIO_PIN_RESET, 200) != 0)
        return data;

    // 6. 读取40位数据
    for (int i = 0; i < 40; i++)
    {
        // 等待低电平开始 (50μs)
        if (wait_pin_level(GPIO_PIN_SET, 200) != 0)
            return data;
        // 测量高电平持续时间
        uint32_t start = DWT->CYCCNT;
        if (wait_pin_level(GPIO_PIN_RESET, 200) != 0)
            return data;
        uint32_t elapsed = (DWT->CYCCNT - start) / (SystemCoreClock / 1000000);

        // 高电平 > 40μs = 1, 否则 = 0
        if (elapsed > 40)
        {
            bits[i / 8] |= (1 << (7 - (i % 8)));
        }
    }

    // 7. 校验和
    if ((uint8_t)(bits[0] + bits[1] + bits[2] + bits[3]) == bits[4])
    {
        data.humi = bits[0];
        data.temp = bits[2];
        data.valid = 1;
    }

    return data;
}

/* ========== DS18B20 ========== */
// DS18B20 温度数据 (0.01°C精度)
static int16_t ds18b20_temp = 0; // 温度×100, 正数, 无效时 = -32768
static uint8_t ds18b20_valid = 0;
static uint32_t ds18b20_lasttime = 0;
static uint8_t ds18b20_converting = 0; // 1=正在转换

// 配置DS18B20引脚为推挽输出
static void DS18B20_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = T_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(T_GPIO_Port, &GPIO_InitStruct);
}

// 配置DS18B20引脚为输入
static void DS18B20_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = T_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(T_GPIO_Port, &GPIO_InitStruct);
}

// DS18B20 复位 + 检测存在
static uint8_t DS18B20_Reset(void)
{
    DS18B20_SetOutput();
    HAL_GPIO_WritePin(T_GPIO_Port, T_Pin, GPIO_PIN_RESET);
    delay_us(480); // 拉低 480μs
    DS18B20_SetInput();
    delay_us(80); // 等待 80μs 后采样
    uint8_t presence = (HAL_GPIO_ReadPin(T_GPIO_Port, T_Pin) == GPIO_PIN_RESET) ? 1 : 0;
    delay_us(400);   // 等待剩余时间
    return presence; // 1=存在, 0=无设备
}

// 写1位
static void DS18B20_WriteBit(uint8_t bit)
{
    DS18B20_SetOutput();
    HAL_GPIO_WritePin(T_GPIO_Port, T_Pin, GPIO_PIN_RESET);
    delay_us(5);
    if (bit)
        HAL_GPIO_WritePin(T_GPIO_Port, T_Pin, GPIO_PIN_SET);
    delay_us(60);
    DS18B20_SetInput();
    delay_us(2);
}

// 读1位
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

// 写1字节 (LSB first)
static void DS18B20_WriteByte(uint8_t data)
{
    for (int i = 0; i < 8; i++)
    {
        DS18B20_WriteBit(data & 1);
        data >>= 1;
    }
}

// 读1字节 (LSB first)
static uint8_t DS18B20_ReadByte(void)
{
    uint8_t data = 0;
    for (int i = 0; i < 8; i++)
    {
        data >>= 1;
        if (DS18B20_ReadBit())
            data |= 0x80;
    }
    return data;
}

// 读取DS18B20温度 (返回 温度×100)
// 成功返回 > -500, 失败返回 -32768
static int16_t Read_DS18B20(void)
{
    uint8_t presence;
    uint8_t temp_l, temp_h;
    int16_t raw;

    // 复位 + 检测存在
    presence = DS18B20_Reset();
    if (!presence)
        return -32768;

    // 跳过ROM
    DS18B20_WriteByte(0xCC);
    // 启动温度转换
    DS18B20_WriteByte(0x44);

    // 等待转换完成 (DS18B20在转换中拉低总线, 完成后释放)
    DS18B20_SetInput();
    uint32_t wait_start = HAL_GetTick();
    while (HAL_GPIO_ReadPin(T_GPIO_Port, T_Pin) == GPIO_PIN_RESET)
    {
        if (HAL_GetTick() - wait_start > 1000)
            return -32768; // 超时
    }

    // 复位 + 检测存在
    presence = DS18B20_Reset();
    if (!presence)
        return -32768;

    // 跳过ROM
    DS18B20_WriteByte(0xCC);
    // 读暂存器
    DS18B20_WriteByte(0xBE);

    // 读温度低字节 + 高字节
    temp_l = DS18B20_ReadByte();
    temp_h = DS18B20_ReadByte();

    // 合并为12位有符号值
    raw = (int16_t)((temp_h << 8) | temp_l);

    // 转换为 0.01°C: raw × 100 / 16
    return raw * 100 / 16;
}

/* ========== ADC 通用读取 ========== */
// 读取任意ADC通道, 返回 0~4095
static uint32_t Read_ADC_Channel(uint32_t channel)
{
    uint32_t value = 0;

    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    // 临时改为单通道单次转换
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.NbrOfConversion = 1;
    HAL_ADC_Init(&hadc1);

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
    {
        value = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    return value;
}

/**
 * @brief OLED显示刷新
 */
void OLED_Display(void)
{
    if (HAL_GetTick() - oled_lasttime >= 100)
    {
        if (Lock == 1) // 上锁
        {
            OLED_NewFrame();
            sprintf(oled_lock, "已上锁");
            OLED_PrintString(0, 0, oled_lock, &font16x16, OLED_COLOR_NORMAL);

            // 显示密码输入状态
            if (lock_state == LOCK_IDLE)
            {
                sprintf(oled_buf_num, "PassWord:");
                OLED_PrintString(0, 15, oled_buf_num, &font16x16, OLED_COLOR_NORMAL);
                sprintf(oled_buf, "Err:%d/%d", error_count, MAX_ERROR_COUNT);
                OLED_PrintString(0, 30, oled_buf, &font16x16, OLED_COLOR_NORMAL);
            }
            else if (lock_state == LOCK_INPUT)
            {
                sprintf(oled_buf_num, "PassWord:");
                OLED_PrintString(0, 15, oled_buf_num, &font16x16, OLED_COLOR_NORMAL);
                // 显示已输入的星号
                char pwd_display[10] = "";
                for (int i = 0; i < input_index; i++)
                {
                    pwd_display[i] = '*';
                }
                pwd_display[input_index] = '\0';
                OLED_PrintString(90, 15, pwd_display, &font16x16, OLED_COLOR_NORMAL);
                // 显示当前选择的数字
                sprintf(oled_buf, "Num:%d", select_num);
                OLED_PrintString(0, 30, oled_buf, &font16x16, OLED_COLOR_NORMAL);
            }
            else if (lock_state == LOCK_ALARM)
            {
                sprintf(oled_buf_num, "ERROR!");
                OLED_PrintString(0, 15, oled_buf_num, &font16x16, OLED_COLOR_REVERSED);
                sprintf(oled_buf, "Try again");
                OLED_PrintString(0, 30, oled_buf, &font16x16, OLED_COLOR_NORMAL);
            }

            OLED_PrintString(90, 0, oled_buf_name, &font16x16, OLED_COLOR_NORMAL);
            OLED_ShowFrame();
            return;
        }
        if (oled_ui == 1)
        {
            tick--;
            if (tick == 0)
            {
                Lock = 1;
                tick = tick_Max;
            }
            OLED_NewFrame();
            sprintf(oled_buf, "OLED_Time:%d", tick);
            sprintf(oled_ui_buf, "       ①");
            sprintf(oled_buf_name, "BASH");

            sprintf(oled_lock, "已解锁");
            OLED_PrintString(80, 0, oled_lock, &font16x16, OLED_COLOR_REVERSED);
            OLED_PrintString(0, 0, oled_ui_buf, &font16x16, OLED_COLOR_NORMAL);
            OLED_PrintString(0, 15, oled_buf, &font16x16, OLED_COLOR_NORMAL);
            OLED_PrintString(0, 0, oled_buf_name, &font16x16, OLED_COLOR_REVERSED);

            OLED_ShowFrame();
            oled_lasttime = HAL_GetTick();
        }
        if (oled_ui == 2)
        {
            OLED_NewFrame();
            if (oled_uart_flag == 1 && HAL_GetTick() - oled_uart_lasttime >= 100) // 正在发送
            {
                sprintf(oled_uart_buf, "UART1:%s   ING", oled_switch_buf);
                oled_uart_lasttime = HAL_GetTick();
                oled_uart_flag = 0;
            }
            else if (oled_uart_flag == 0 && HAL_GetTick() - oled_uart_lasttime >= 1000)
            {
                sprintf(oled_uart_buf, "UART1:%s   ED", oled_switch_buf);
            }
            if (Servo_lock == 1)
            {
                sprintf(oled_servo_lock, "已上锁");
            }
            if (Servo_lock == 0)
            {
                sprintf(oled_servo_lock, "已解锁");
            }
            sprintf(oled_ui_buf, "       ②");
            sprintf(oled_encoder_buf, "Angle: %dº", Count * 180 / count_MAX);
            OLED_PrintString(0, 0, oled_ui_buf, &font16x16, OLED_COLOR_NORMAL);
            OLED_PrintString(0, 15, oled_uart_buf, &font16x16, OLED_COLOR_NORMAL);
            OLED_PrintString(0, 30, oled_encoder_buf, &font16x16, OLED_COLOR_NORMAL);
            OLED_PrintString(80, 0, oled_servo_lock, &font16x16, OLED_COLOR_REVERSED);
            OLED_ShowFrame();
            oled_lasttime = HAL_GetTick();
        }
        if (oled_ui == 3)
        {
            OLED_NewFrame();

            sprintf(oled_ui_buf, "       ③");
            Light_do = HAL_GPIO_ReadPin(Light_do_GPIO_Port, Light_do_Pin);
            if (Light_do)
                sprintf(oled_light, "☪");
            else
                sprintf(oled_light, "☀");

            // 读取各ADC通道
            uint32_t light_adc = Read_ADC_Channel(ADC_CHANNEL_9);         // PB1 光敏
            uint32_t temp_adc = Read_ADC_Channel(ADC_CHANNEL_TEMPSENSOR); // 片内温度
            uint32_t vref_adc = Read_ADC_Channel(ADC_CHANNEL_VREFINT);    // 内部参考电压

            // 计算光强百分比 (反算: 越暗ADC值越大)
            uint16_t light_percent = (4095 - light_adc) * 100 / 4095;

            // 计算片内温度: 用实际VDDA代替固定3.3V, 更准确
            // T = (VSENSE - V25) / Avg_Slope + 25
            // V25=1.43V, Avg_Slope=4.3mV/°C
            // 计算实际VDDA电压: VDDA = 1.20V * 4095 / VREFINT_ADC
            float vdda = 1.20f * 4095.0f / (float)vref_adc;
            float vsense = (float)temp_adc * vdda / 4095.0f;
            int8_t chip_temp = (int8_t)((vsense - 1.43f) / 0.0043f + 25.0f);

            // 显示
            char oled_light_str[10], oled_line2[20], oled_line3[20];
            sprintf(oled_light_str, "L:%d%%", light_percent);

            // 第3行: DS18B20温度(保留两位小数) + DHT11湿度
            if (ds18b20_valid && dht11_data.valid)
            {
                int16_t t = ds18b20_temp;
                sprintf(oled_line2, "T:%d.%02dC H:%d%%",
                        t / 100, (t > 0 ? t : -t) % 100,
                        dht11_data.humi);
            }
            else if (ds18b20_valid)
            {
                int16_t t = ds18b20_temp;
                sprintf(oled_line2, "T:%d.%02dC H:--%%",
                        t / 100, (t > 0 ? t : -t) % 100);
            }
            else if (dht11_data.valid)
            {
                sprintf(oled_line2, "T:--.--C H:%d%%", dht11_data.humi);
            }
            else
            {
                sprintf(oled_line2, "T:--.--C H:--%%");
            }
            sprintf(oled_line3, "C: %dC   V:%.2fV", chip_temp, (double)vdda);

            // 第1行: 页码 + 晴/暗图标
            OLED_PrintString(0, 0, oled_ui_buf, &font16x16, OLED_COLOR_NORMAL);
            OLED_PrintString(32, 0, oled_light, &font16x16, OLED_COLOR_NORMAL);
            // 第2行: 亮度百分比
            OLED_PrintString(0, 16, oled_light_str, &font16x16, OLED_COLOR_NORMAL);
            // 第3行: 外部温湿度
            OLED_PrintString(0, 32, oled_line2, &font16x16, OLED_COLOR_NORMAL);
            // 第4行: 片内温度 + 电压
            OLED_PrintString(0, 48, oled_line3, &font16x16, OLED_COLOR_NORMAL);
            OLED_ShowFrame();
            oled_lasttime = HAL_GetTick();
        }
        if (oled_ui == 4)
        {
            OLED_NewFrame();
            sprintf(oled_ui_buf, "       ④");

            OLED_PrintString(0, 0, oled_ui_buf, &font16x16, OLED_COLOR_NORMAL);
            OLED_ShowFrame();
            oled_lasttime = HAL_GetTick();
        }
    }
}

/**
 * @brief 按键检测与蓝牙开关控制
 */
void Key_Process(void)
{
    uint8_t key_current_0 = HAL_GPIO_ReadPin(Bule_switch_GPIO_Port, Bule_switch_Pin);
    uint8_t key_current_1 = HAL_GPIO_ReadPin(oled_ui_GPIO_Port, oled_ui_Pin);
    uint8_t key_current_2 = HAL_GPIO_ReadPin(confirm_key_GPIO_Port, confirm_key_Pin);

    // 检测下降沿（从未按下变为按下）+ 消抖延时500ms
    if (key_current_0 == GPIO_PIN_RESET && key_last_state_0 == GPIO_PIN_SET && HAL_GetTick() - key_lasttime_0 >= 500)
    {
        blue_switch = !blue_switch; // 0不接受蓝牙数据
        if (blue_switch)
        {
            uart_flag = 0;                                                   // 清除之前缓存的标志，避免开启蓝牙时误触发
            HAL_GPIO_WritePin(Blue_en_GPIO_Port, Blue_en_Pin, GPIO_PIN_SET); // 写1在蓝牙EN引脚让蓝牙引脚工作
            sprintf(oled_switch_buf, "ON");
        }
        else
        {
            HAL_GPIO_WritePin(Blue_en_GPIO_Port, Blue_en_Pin, GPIO_PIN_RESET);
            sprintf(oled_switch_buf, "OFF");
        }
        key_lasttime_0 = HAL_GetTick();
    }
    key_last_state_0 = key_current_0;

    // PB12 切屏/提交密码 按键检测
    if (key_current_1 == GPIO_PIN_SET && key_last_state_1 == GPIO_PIN_RESET && HAL_GetTick() - key_lasttime_1 >= 500)
    {
        if (Lock == 1 && lock_state == LOCK_INPUT)
        {
            // 提交密码进行验证
            uint8_t correct = 1;
            for (int i = 0; i < PASSWORD_LEN; i++)
            {
                if (input[i] != password[i])
                {
                    correct = 0;
                    break;
                }
            }
            if (correct)
            {
                // 密码正确，解锁
                Lock = 0;
                lock_state = LOCK_IDLE;
                error_count = 0;
                input_index = 0;
                tick = tick_Max;
            }
            else
            {
                // 密码错误
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
        else if (Lock == 0)
        {
            // 解锁状态下切换页面
            oled_ui++;
            if (oled_ui > 4)
            {
                oled_ui = 1;
                tick = 20;
            }
            if (oled_ui == 2)
            {
                Count = Count_2;
                __HAL_TIM_SET_COUNTER(&htim2, Count);
            }
        }
        key_lasttime_1 = HAL_GetTick();
    }
    key_last_state_1 = key_current_1;

    // PB10 confirm_key (EC11按下) 按键检测
    if (key_current_2 == GPIO_PIN_SET && key_last_state_2 == GPIO_PIN_RESET && HAL_GetTick() - key_lasttime_2 >= 200)
    {
        if (Lock == 1)
        {
            // 密码输入状态
            if (lock_state == LOCK_IDLE || lock_state == LOCK_ALARM)
            {
                // 进入输入状态
                lock_state = LOCK_INPUT;
                input_index = 0;
                select_num = 0;
            }
            else if (lock_state == LOCK_INPUT)
            {
                // 确认当前位数字
                input[input_index] = select_num;
                input_index++;
                select_num = 0;
                if (input_index >= PASSWORD_LEN)
                {
                    // 输入完成，等待PB12提交
                    // 可以在这里自动提交，或者等待PB12
                }
            }
        }
        else
        {
            // 解锁状态下，切换舵机锁
            if (oled_ui == 2)
            {
                if (Servo_lock == 1) // 从上锁变为解锁，恢复之前的位置
                {
                    Servo_lock = 0;
                    __HAL_TIM_SET_COUNTER(&htim2, servo_last_count);
                    Count = servo_last_count;
                }
                else // 从解锁变为上锁，保存当前位置
                {
                    servo_last_count = __HAL_TIM_GET_COUNTER(&htim2);
                    Servo_lock = 1;
                }
            }
        }
        key_lasttime_2 = HAL_GetTick();
    }
    key_last_state_2 = key_current_2;
}

/**
 * @brief 串口数据处理（回传）
 */
void UART_Process(void)
{
    if (uart_flag == 1 && blue_switch == 1 && oled_ui == 2)
    {
        uart_flag = 0;
        HAL_UART_Transmit_IT(&huart1, (uint8_t *)uart_re, 2);
        oled_uart_flag = 1;
    }
}

void Encoder(void) // 旋转编码器
{
    Count = __HAL_TIM_GET_COUNTER(&htim2);

    // 密码输入状态下，将编码器值转换为数字0-9
    if (Lock == 1 && lock_state == LOCK_INPUT)
    {
        select_num = Count % 10;
    }
}

void Servo(void) // 舵机控制   2.5%~12.5%占空比
//                              0~180度
{
    if (Count > count_MAX)
    {
        if (Count > 60000)
        {
            Count = 0;
            __HAL_TIM_SET_COUNTER(&htim2, Count);
        }
        else
        {
            Count = count_MAX;
            __HAL_TIM_SET_COUNTER(&htim2, count_MAX);
        }
    }
    Count_2 = Count;
    duty = (10 * Count / (float)count_MAX + 2.5) / 100 * 2000;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, duty);
}

/**
 * @brief 传感器定时读取
 * @note  DHT11每2秒读一次(仅取湿度), DS18B20每2秒读一次(温度更准)
 * @note  在main.c的主循环中调用
 */
void Sensor_Process(void)
{
    // DHT11 每2秒读一次 (仅用湿度)
    if (HAL_GetTick() - dht11_lasttime >= 2000)
    {
        dht11_data = Read_DHT11();
        dht11_lasttime = HAL_GetTick();
    }

    // DS18B20 每2秒读一次
    if (HAL_GetTick() - ds18b20_lasttime >= 2000)
    {
        int16_t temp = Read_DS18B20();
        if (temp > -500)
        {
            ds18b20_temp = temp;
            ds18b20_valid = 1;
        }
        else
        {
            ds18b20_valid = 0;
        }
        ds18b20_lasttime = HAL_GetTick();
    }
}
