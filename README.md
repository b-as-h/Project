基于 STM32 HAL 库的裸机小项目，把学习过的外设、算法和 UI 拼成一台能用的桌面小终端。

主控 STM32F103C8T6（64KB Flash / 20KB RAM），0.96" OLED + EC11 编码器 + 舵机 + 多路传感器，支持蓝牙串口遥控与上位机实时曲线。

## 功能

| 模块 | 内容 |
|---|---|
| 显示 | SSD1306 OLED（I2C1），自绘 UI 框架 BASH_UI：图标轮播首页、页面切换动画、多级视图 |
| 输入 | EC11 旋转编码器 + 确认键，长按/短按/连发区分 |
| 安全 | 4 位密码锁屏，错误计数与报警态 |
| 传感器 | DS18B20（单总线）、DHT11（单总线）、光敏（ADC + 数字量）、片内温度与 VREFINT 反推供电电压 |
| 姿态 | MPU6050（I2C），卡尔曼滤波解算 roll/pitch |
| 执行 | TIM3 PWM 驱动舵机，编码器调角，锁定/解锁 |
| 系统 | 复位原因、Flash/RAM 占用、栈峰值、主循环频率与最大帧耗时、I2C 在位自检 |
| 通信 | USART1 蓝牙，中断收发 + 环形缓冲 + ASCII 行协议，支持周期遥测 |

## 目录

```
Core/
├── Inc/            应用头文件（fmt / sys_info / bt / start / app_ui ...）
├── Src/            应用源码
├── bash_oled/      OLED 驱动与图形库
└── bash_ui/        UI 框架（home / menu / info / custom / dialog / anim）
tools/
└── host_monitor.py 蓝牙上位机（pyserial + tkinter）
```

## 编译

```bash
cmake --preset Release
cmake --build --preset Release     # 产物：build/Release/Project.elf / .hex
```

Debug 预设为 `-O0 -g3`，仅用于调试；烧录请用 Release（`-Os`）。

## 资源占用（Release）

| 区域 | 已用 | 总量 | 占比 |
|---|---|---|---|
| Flash | 34404 B | 64 KB | 52.5% |
| RAM | 5848 B | 20 KB | 28.6% |

> 早期版本使用 `sprintf("%.2f")`，在 newlib-nano 下 `%f` 默认不生效（需 `-u _printf_float`），
> 且会引入约 3~5KB 浮点格式化代码。现已全部改为整数/定点格式化（`Core/Src/fmt.c`），
> 并把 MPU6050 与卡尔曼滤波从 `double` 降到 `float`，固件中已不含任何 double 软浮点符号。

## 蓝牙串口协议

USART1，9600 8N1。命令以 `\n` 结尾，应答以 `\r\n` 结尾，全部为 ASCII。

| 命令 | 说明 | 示例应答 |
|---|---|---|
| `PING` | 心跳 | `+PONG` |
| `GET` | 读取一次全部数据 | `+DATA,light=85,temp=25.34,...` |
| `SERVO,<0-180>` | 设置舵机角度 | `+OK` |
| `LOCK,<0/1>` | 舵机解锁 / 锁定 | `+OK` |
| `STREAM,<0/1>[,<ms>]` | 开关周期遥测，周期 100~5000ms | `+OK,STREAM=1` |
| `BAUD,<rate>` | 切换波特率（9600/19200/38400/57600/115200），掉电恢复 9600 | `+OK,BAUD` |
| `SYS` | 复位原因、运行时间、内存、负载 | `+SYS,rst=POR,up=123,...` |
| `VER` | 固件版本 | `+VER,BASH-STM32,1.4.0` |
| `RESET` | 软复位 | `+BYE` |
| 其他 | 未知命令 | `-ERR,UNKNOWN` |

周期遥测帧：

```
$D,<light>,<temp×100>,<humi>,<ax_mg>,<ay_mg>,<az_mg>
```

## 上位机

上位机已独立到 **`E:\上位机`** 目录，双击其中的 `run_host.bat` 或桌面快捷方式「STM32 上位机」即可，
详细说明见 `E:\上位机\README.md`。

```bash
pip install pyserial
python host_monitor.py
```

脚本会自动寻找带 tkinter 的 Python，缺少 pyserial 时自动安装。

界面功能：

| 区域 | 内容 |
|---|---|
| 连接 | 串口下拉 + **波特率下拉**（9600/19200/38400/57600/115200），连接中改速率需点「同步设备波特率」 |
| 数据 | 光照、温度、湿度、片内温度、供电电压、舵机角度、三轴加速度、温度曲线 |
| 控制 | 舵机角度滑条、舵机锁定、周期遥测 |
| 发送 | 命令输入框（回车即发）、**自动追加回车**、**HEX 发送**、**定时发送**、命令历史（↑/↓） |
| 其它 | 时间戳、清空日志、快捷命令按钮（PING/GET/SYS/VER/HELP/RESET） |

**改波特率的正确顺序**：连接状态下把下拉框切到目标速率 → 点「同步设备波特率」→
上位机在旧链路上发 `BAUD,<rate>` → 设备回 `+OK,BAUD` 并切换 → 上位机自动用新速率重连。
若设备 2 秒内没确认，下拉框会自动退回原速率，不会把链路弄断。

## 说明

代码不太规范，仅供参考，持续更新中。
