/**
 * @file bt.c
 * @brief 蓝牙串口通信实现：中断收发 + 环形缓冲 + 行协议解析。
 */

#include "bt.h"
#include "fmt.h"
#include "main.h"
#include "start.h"
#include "tim.h"
#include "usart.h"
#include "sys_info.h"

#define BT_RX_SIZE 256U
#define BT_TX_SIZE 256U
#define BT_LINE_SIZE 48U
#define BT_ARG_MAX 4U

#define BT_ONLINE_TIMEOUT_MS 5000U
#define BT_DEFAULT_PERIOD_MS 500U
#define BT_FW_VERSION "1.4.0"

/* ========== 环形缓冲 ========== */

static uint8_t rx_buf[BT_RX_SIZE];
static volatile uint16_t rx_head; /* 仅中断写入 */
static volatile uint16_t rx_tail; /* 仅主循环写入 */
static uint8_t rx_byte;

static uint8_t tx_buf[BT_TX_SIZE];
static volatile uint16_t tx_head; /* 仅主循环写入 */
static volatile uint16_t tx_tail; /* 仅中断写入 */
static volatile uint8_t tx_busy;
static uint8_t tx_byte;

/* ========== 协议状态 ========== */

static char line_buf[BT_LINE_SIZE];
static uint8_t line_len;
static volatile uint32_t last_rx_tick;
static volatile uint8_t rx_seen;

static uint8_t stream_on;
static uint16_t stream_period = BT_DEFAULT_PERIOD_MS;
static uint32_t stream_last_tick;

/* 当前串口波特率；上电默认与 MX_USART1_UART_Init() 一致，掉电不保存 */
static uint32_t current_baud = 9600U;
static uint32_t pending_baud; /* 非 0 表示等待发送缓冲排空后切换 */

/* ========== 发送 ========== */

static void bt_tx_kick(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if (tx_busy == 0U && tx_head != tx_tail) {
        tx_byte = tx_buf[tx_tail];
        if (HAL_UART_Transmit_IT(&huart1, &tx_byte, 1U) == HAL_OK) {
            tx_busy = 1U;
        }
    }
    __set_PRIMASK(primask);
}

static void bt_push(const char *text)
{
    while (*text != '\0') {
        uint16_t next = (uint16_t)((tx_head + 1U) % BT_TX_SIZE);
        if (next == tx_tail) {
            break; /* 缓冲满，丢弃剩余内容 */
        }
        tx_buf[tx_head] = (uint8_t)(*text++);
        tx_head = next;
    }
    bt_tx_kick();
}

void BT_SendString(const char *text)
{
    bt_push(text);
    bt_push("\r\n");
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        tx_tail = (uint16_t)((tx_tail + 1U) % BT_TX_SIZE);
        tx_busy = 0U;
        bt_tx_kick();
    }
}

/* ========== 接收 ========== */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        uint16_t next = (uint16_t)((rx_head + 1U) % BT_RX_SIZE);
        if (next != rx_tail) {
            rx_buf[rx_head] = rx_byte;
            rx_head = next;
        }
        last_rx_tick = HAL_GetTick();
        rx_seen = 1U;
        (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        /* 溢出/噪声错误后必须重新挂接收，否则会永久收不到数据。 */
        __HAL_UART_CLEAR_OREFLAG(huart);
        (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);
    }
}

/* ========== 字符串工具（避免引入 string.h / stdlib.h） ========== */

static uint8_t bt_ieq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        char ca = *a++;
        char cb = *b++;
        if (ca >= 'a' && ca <= 'z') {
            ca = (char)(ca - 32);
        }
        if (cb >= 'a' && cb <= 'z') {
            cb = (char)(cb - 32);
        }
        if (ca != cb) {
            return 0U;
        }
    }
    return (uint8_t)(*a == '\0' && *b == '\0');
}

static int32_t bt_atoi(const char *s)
{
    int32_t value = 0;
    uint8_t negative = 0U;

    while (*s == ' ') {
        ++s;
    }
    if (*s == '-') {
        negative = 1U;
        ++s;
    }
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        ++s;
    }
    return negative ? -value : value;
}

static uint8_t bt_split(char *text, char *argv[], uint8_t max_args)
{
    uint8_t argc = 0U;

    if (text == NULL || *text == '\0') {
        return 0U;
    }
    argv[argc++] = text;
    while (*text != '\0') {
        if (*text == ',') {
            *text = '\0';
            if (argc >= max_args) {
                break;
            }
            argv[argc++] = text + 1;
        }
        ++text;
    }
    return argc;
}

/* ========== 波特率 ========== */

static uint8_t bt_baud_valid(int32_t rate)
{
    switch (rate) {
    case 9600:
    case 19200:
    case 38400:
    case 57600:
    case 115200:
        return 1U;
    default:
        return 0U;
    }
}

/*
 * 运行时改波特率：先停接收，改 Init.BaudRate 后重新初始化 USART，
 * 再重新挂中断接收。发送侧此时已排空，不会把半截数据发错速率。
 */
static void bt_apply_baud(uint32_t rate)
{
    (void)HAL_UART_AbortReceive_IT(&huart1);

    huart1.Init.BaudRate = rate;
    if (HAL_UART_Init(&huart1) == HAL_OK) {
        current_baud = rate;
    }

    rx_head = 0U;
    rx_tail = 0U;
    line_len = 0U;
    (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);
}

uint32_t BT_GetBaud(void)
{
    return current_baud;
}

/* ========== 舵机 ========== */
static uint16_t bt_servo_angle(void)
{
    return (uint16_t)Servo_GetAngle();
}

static void bt_servo_set(int32_t angle)
{
    if (angle < 0) {
        angle = 0;
    }
    if (angle > 180) {
        angle = 180;
    }
    /* 走 Servo_SetAngle：只改软件位置并刷新 CCR，不触碰 TIM2 编码器计数器 */
    Servo_SetAngle((uint8_t)angle);
}

/* ========== 应答 ========== */

static int32_t mpu_mg(float value)
{
    return (int32_t)(value * 1000.0f);
}

static void bt_reply_data(void)
{
    char buf[200];
    char *p = buf;

    p = Fmt_Str(p, "+DATA,light=");
    p = Fmt_U32(p, sensor_light_percent);
    p = Fmt_Str(p, ",temp=");
    p = Fmt_Fixed(p, ds18b20_temp, 2);
    p = Fmt_Str(p, ",humi=");
    p = Fmt_U32(p, dht11_data.humi);
    p = Fmt_Str(p, ",chip=");
    p = Fmt_Fixed(p, sensor_chip_temp_x10, 1);
    p = Fmt_Str(p, ",vdd=");
    p = Fmt_U32(p, sensor_vdd_mv);
    p = Fmt_Str(p, ",ax=");
    p = Fmt_I32(p, mpu_mg(mpu6050_data.Ax));
    p = Fmt_Str(p, ",ay=");
    p = Fmt_I32(p, mpu_mg(mpu6050_data.Ay));
    p = Fmt_Str(p, ",az=");
    p = Fmt_I32(p, mpu_mg(mpu6050_data.Az));
    p = Fmt_Str(p, ",servo=");
    p = Fmt_U32(p, bt_servo_angle());
    p = Fmt_Str(p, ",lock=");
    p = Fmt_U32(p, Servo_lock ? 1U : 0U);
    /* 陀螺仪角速度（°/s，取整）与卡尔曼姿态角（×10，一位小数） */
    p = Fmt_Str(p, ",gx=");
    p = Fmt_I32(p, (int32_t)mpu6050_data.Gx);
    p = Fmt_Str(p, ",gy=");
    p = Fmt_I32(p, (int32_t)mpu6050_data.Gy);
    p = Fmt_Str(p, ",gz=");
    p = Fmt_I32(p, (int32_t)mpu6050_data.Gz);
    p = Fmt_Str(p, ",roll=");
    p = Fmt_Fixed(p, (int32_t)(mpu6050_data.KalmanAngleX * 10.0f), 1);
    p = Fmt_Str(p, ",pitch=");
    p = Fmt_Fixed(p, (int32_t)(mpu6050_data.KalmanAngleY * 10.0f), 1);
    BT_SendString(buf);
}

static void bt_send_telemetry(void)
{
    char buf[128];
    char *p = buf;

    p = Fmt_Str(p, "$D,");
    p = Fmt_U32(p, sensor_light_percent);
    *p++ = ',';
    p = Fmt_I32(p, ds18b20_temp);
    *p++ = ',';
    p = Fmt_U32(p, dht11_data.humi);
    *p++ = ',';
    p = Fmt_I32(p, mpu_mg(mpu6050_data.Ax));
    *p++ = ',';
    p = Fmt_I32(p, mpu_mg(mpu6050_data.Ay));
    *p++ = ',';
    p = Fmt_I32(p, mpu_mg(mpu6050_data.Az));
    /* 追加陀螺仪与姿态，旧版上位机只取前 6 个字段，不受影响 */
    *p++ = ',';
    p = Fmt_I32(p, (int32_t)mpu6050_data.Gx);
    *p++ = ',';
    p = Fmt_I32(p, (int32_t)mpu6050_data.Gy);
    *p++ = ',';
    p = Fmt_I32(p, (int32_t)mpu6050_data.Gz);
    *p++ = ',';
    p = Fmt_I32(p, (int32_t)(mpu6050_data.KalmanAngleX * 10.0f));
    *p++ = ',';
    p = Fmt_I32(p, (int32_t)(mpu6050_data.KalmanAngleY * 10.0f));
    BT_SendString(buf);
}

static void bt_reply_sys(void)
{
    char buf[128];
    char *p = buf;

    p = Fmt_Str(p, "+SYS,rst=");
    p = Fmt_Str(p, SysInfo_ResetCauseText());
    p = Fmt_Str(p, ",up=");
    p = Fmt_U32(p, SysInfo_UptimeSeconds());
    p = Fmt_Str(p, ",flash=");
    p = Fmt_U32(p, sys_info.flash_used);
    p = Fmt_Str(p, ",ram=");
    p = Fmt_U32(p, sys_info.ram_static);
    p = Fmt_Str(p, ",stack=");
    p = Fmt_U32(p, sys_info.stack_peak);
    p = Fmt_Str(p, ",loop=");
    p = Fmt_U32(p, sys_info.loop_hz);
    p = Fmt_Str(p, ",lmax=");
    p = Fmt_U32(p, sys_info.loop_max_ms);
    p = Fmt_Str(p, ",baud=");
    p = Fmt_U32(p, current_baud);
    BT_SendString(buf);
}

/* ========== 命令解析 ========== */

static void bt_exec(char *cmd)
{
    char *argv[BT_ARG_MAX];
    uint8_t argc = bt_split(cmd, argv, BT_ARG_MAX);

    if (argc == 0U) {
        return;
    }

    if (bt_ieq(argv[0], "PING")) {
        BT_SendString("+PONG");
    } else if (bt_ieq(argv[0], "GET")) {
        bt_reply_data();
    } else if (bt_ieq(argv[0], "SERVO")) {
        if (argc < 2U) {
            BT_SendString("-ERR,ARG");
            return;
        }
        /* 与按键一致：锁定状态下拒绝远程改角，避免误动作 */
        if (Servo_lock != 0U) {
            BT_SendString("-ERR,LOCKED");
            return;
        }
        bt_servo_set(bt_atoi(argv[1]));
        BT_SendString("+OK");
    } else if (bt_ieq(argv[0], "LOCK")) {
        if (argc < 2U) {
            BT_SendString("-ERR,ARG");
            return;
        }
        Servo_lock = (bt_atoi(argv[1]) != 0) ? 1U : 0U;
        BT_SendString("+OK");
    } else if (bt_ieq(argv[0], "STREAM")) {
        stream_on = (uint8_t)((argc >= 2U && bt_atoi(argv[1]) != 0) ? 1U : 0U);
        if (argc >= 3U) {
            int32_t period = bt_atoi(argv[2]);
            if (period < 100) {
                period = 100;
            }
            if (period > 5000) {
                period = 5000;
            }
            stream_period = (uint16_t)period;
        }
        stream_last_tick = HAL_GetTick();
        BT_SendString(stream_on ? "+OK,STREAM=1" : "+OK,STREAM=0");
    } else if (bt_ieq(argv[0], "BAUD")) {
        if (argc < 2U) {
            BT_SendString("-ERR,ARG");
            return;
        }
        {
            int32_t rate = bt_atoi(argv[1]);
            if (!bt_baud_valid(rate)) {
                BT_SendString("-ERR,BAUD");
                return;
            }
            /* 先按旧波特率回复，等发送缓冲排空后由 BT_Process 真正切换 */
            BT_SendString("+OK,BAUD");
            pending_baud = (uint32_t)rate;
        }
    } else if (bt_ieq(argv[0], "SYS")) {
        bt_reply_sys();
    } else if (bt_ieq(argv[0], "VER")) {
        BT_SendString("+VER,BASH-STM32," BT_FW_VERSION);
    } else if (bt_ieq(argv[0], "HELP")) {
        BT_SendString("+HELP,PING|GET|SERVO,deg|LOCK,0/1|STREAM,0/1,ms|BAUD,rate|SYS|VER|RESET");
    } else if (bt_ieq(argv[0], "RESET")) {
        BT_SendString("+BYE");
        HAL_Delay(60); /* 等待发送缓冲排空 */
        NVIC_SystemReset();
    } else {
        BT_SendString("-ERR,UNKNOWN");
    }
}

static void bt_feed(uint8_t ch)
{
    if (ch == '\r' || ch == '\n') {
        if (line_len > 0U) {
            line_buf[line_len] = '\0';
            bt_exec(line_buf);
            line_len = 0U;
        }
        return;
    }
    if (ch == 0x08U || ch == 0x7FU) { /* 退格 */
        if (line_len > 0U) {
            --line_len;
        }
        return;
    }
    if (line_len < (BT_LINE_SIZE - 1U)) {
        line_buf[line_len++] = (char)ch;
    } else {
        line_len = 0U; /* 超长行丢弃 */
    }
}

/* ========== 公共接口 ========== */

void BT_Init(void)
{
    rx_head = 0U;
    rx_tail = 0U;
    tx_head = 0U;
    tx_tail = 0U;
    tx_busy = 0U;
    line_len = 0U;
    stream_on = 0U;
    rx_seen = 0U;

    (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);
}

void BT_Process(void)
{
    uint32_t now = HAL_GetTick();

    while (rx_tail != rx_head) {
        uint8_t ch = rx_buf[rx_tail];
        rx_tail = (uint16_t)((rx_tail + 1U) % BT_RX_SIZE);
        bt_feed(ch);
    }

    if (stream_on != 0U && (now - stream_last_tick) >= stream_period) {
        stream_last_tick = now;
        bt_send_telemetry();
    }

    /* 待切换波特率：等发送缓冲彻底排空再改，避免最后几个字节按新速率发出 */
    if (pending_baud != 0U && tx_busy == 0U && tx_head == tx_tail) {
        uint32_t rate = pending_baud;
        pending_baud = 0U;
        bt_apply_baud(rate);
    }
}

uint8_t BT_IsOnline(void)
{
    if (rx_seen == 0U) {
        return 0U;
    }
    return (uint8_t)((HAL_GetTick() - last_rx_tick) < BT_ONLINE_TIMEOUT_MS);
}
