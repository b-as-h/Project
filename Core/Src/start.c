#include "start.h"

// 构思:
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
uint8_t oled_uart_flag = 0; // UART发送中标志
uint8_t oled_ui = 1;        // 当前页面编号
uint8_t tick = tick_Max;    // 倒计时,为0时候上锁
uint8_t Lock = 0;           // 1为上锁
uint8_t Servo_lock = 0;
uint32_t servo_last_count = 0; // 保存上锁时的编码器值

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
            if (oled_ui > 2)
            {
                oled_ui = 1;
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
