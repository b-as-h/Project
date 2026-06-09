/**
 * @file    oled.c
 * @brief   SSD1306 OLED驱动 (I2C接口, 128x64分辨率)
 * @anchor  波特律动(keysking 博哥在学习)
 * @version 1.1 (优化版)
 * @date    2023-08-19
 * @license MIT License
 *
 * @attention
 * 本驱动库针对波特律动·keysking的STM32教程学习套件进行开发
 * 在其他平台或驱动芯片上使用可能需要进行移植
 *
 * @note 硬件连接:
 *   - SCL (时钟线) -> PB6 (I2C1_SCL)
 *   - SDA (数据线) -> PB7 (I2C1_SDA)
 *   - VCC -> 3.3V
 *   - GND -> GND
 *
 * @note 使用流程:
 *   1. STM32初始化I2C完成后调用 OLED_Init() 初始化OLED
 *      (STM32启动比OLED上电快, 建议等待20ms再初始化)
 *   2. 调用 OLED_NewFrame() 开始绘制新的一帧 (清空显存)
 *   3. 调用 OLED_DrawXXX() 绘制图形, 或 OLED_PrintXXX() 绘制文字
 *   4. 调用 OLED_ShowFrame() 将显存内容刷新到OLED屏幕
 *
 * @note 为保证中文显示正常, 请将编译器的字符集设置为UTF-8
 *
 * @note 移植说明:
 *   如需移植到其他平台, 只需修改以下函数:
 *   - OLED_Send()      : 底层I2C发送函数
 *   - OLED_SendCmd()   : 发送命令函数 (通常只需修改调用的I2C函数)
 *   - OLED_Init()      : 初始化命令序列 (根据驱动芯片型号调整)
 *   - OLED_ShowFrame() : 显存刷新函数 (根据驱动芯片通信协议调整)
 */

#include "oled.h"
#include "i2c.h"
#include <math.h>
#include <stdlib.h>

// ========================== 硬件配置参数 ==========================

/**
 * @brief OLED器件I2C地址
 * @note  SSD1306的7位地址为0x3C, 左移1位后为0x78
 *        如果屏幕不亮, 尝试改为0x7A (地址为0x3D的情况)
 */
#define OLED_ADDRESS 0x78

/**
 * @brief OLED屏幕物理参数
 * @note  以下参数对应128x64分辨率的OLED, 如果是128x32的屏幕
 *        需要将OLED_PAGE改为4
 */
#define OLED_PAGE 8              ///< OLED页数 (64像素 / 8像素每页 = 8页)
#define OLED_ROW (8 * OLED_PAGE) ///< OLED总行数 = 64行
#define OLED_COLUMN 128          ///< OLED总列数 = 128列

/**
 * @brief 显存缓冲区 (128x64像素 = 1024字节)
 * @note  显存按"页"组织: 8页 x 128列, 每页8行像素
 *        OLED_GRAM[页号][列号] 的每一bit对应一个像素
 *        bit0 = 该页该列的第0行, bit7 = 第7行
 */
uint8_t OLED_GRAM[OLED_PAGE][OLED_COLUMN];

// ========================== 底层通信函数 ==========================

/**
 * @brief  向OLED发送数据的底层函数 (移植时需修改此函数)
 * @param  data 要发送的数据缓冲区指针
 * @param  len  要发送的数据长度(字节)
 * @note   当前使用I2C1 (hi2c1), 如需使用其他I2C或SPI需修改此函数
 */
void OLED_Send(uint8_t *data, uint8_t len)
{
  HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDRESS, data, len, HAL_MAX_DELAY);
}

/**
 * @brief  向OLED发送一条指令
 * @param  cmd  SSD1306指令字节
 * @note   SSD1306的I2C协议: 第1字节为控制字节(0x00=命令, 0x40=数据)
 *         本函数自动添加0x00控制字节前缀
 */
void OLED_SendCmd(uint8_t cmd)
{
  static uint8_t sendBuffer[2] = {0}; // sendBuffer[0]=0x00 表示后续为命令
  sendBuffer[1] = cmd;
  OLED_Send(sendBuffer, 2);
}

// ========================== OLED驱动函数 ==========================

/**
 * @brief  初始化OLED (SSD1306)
 * @note   发送一系列配置命令来初始化SSD1306驱动芯片
 * @note   移植时如使用不同的驱动芯片, 需要根据数据手册修改此函数
 */
void OLED_Init(void)
{
  // ---- 关闭显示 ----
  OLED_SendCmd(0xAE); // AE = Display OFF (关闭显示, 便于后续配置)

  // ---- 设置内存寻址模式 ----
  OLED_SendCmd(0x20); // 设置内存寻址模式
  OLED_SendCmd(0x10); // 00=水平寻址, 01=垂直寻址, 10=页寻址(默认)
                      // 此处使用页寻址模式, 每写满128列自动换到下一页

  // ---- 设置页起始地址 ----
  OLED_SendCmd(0xB0); // B0~B7 = 设置页起始地址 (页0~页7)
                      // 页寻址模式下, 每页包含8行像素

  // ---- 设置COM输出扫描方向 ----
  OLED_SendCmd(0xC8); // C0=正常方向(从COM0到COM63), C8=反转(从COM63到COM0)
                      // C8适合屏幕底部朝上的安装方式

  // ---- 设置列起始地址 ----
  OLED_SendCmd(0x00); // 设置列起始地址低4位 (0x00 = 列0)
  OLED_SendCmd(0x10); // 设置列起始地址高4位 (0x10 = 列0)

  // ---- 设置显示起始行 ----
  OLED_SendCmd(0x40); // 40~7F = 设置显示起始行 (行0~行63)
                      // 0x40表示从第0行开始显示

  // ---- 设置对比度 ----
  OLED_SendCmd(0x81); // 设置对比度命令
  OLED_SendCmd(0xDF); // 对比度值: 00~FF, 数值越大越亮
                      // 0xDF = 223, 较高的对比度, 适合大多数场景

  // ---- 设置段重映射 ----
  OLED_SendCmd(0xA1); // A0=正常(列0在左侧), A1=反转(列0在右侧)
                      // A1适合屏幕镜像安装的情况

  // ---- 设置显示模式 ----
  OLED_SendCmd(0xA6); // A6=正常显示(1=亮), A7=反色显示(1=灭)

  // ---- 设置多路复用率 ----
  OLED_SendCmd(0xA8); // 设置多路复用率命令
  OLED_SendCmd(0x3F); // 多路复用率: 16~64, 0x3F=63 (即64行)
                      // 128x32的屏幕应设为0x1F (32行)

  // ---- 设置显示内容 ----
  OLED_SendCmd(0xA4); // A4=显示显存内容, A5=全亮(忽略显存)

  // ---- 设置显示偏移 ----
  OLED_SendCmd(0xD3); // 设置显示偏移命令
  OLED_SendCmd(0x00); // 偏移量: 0~63, 0x00=无偏移

  // ---- 设置显示时钟分频 ----
  OLED_SendCmd(0xD5); // 设置时钟分频命令
  OLED_SendCmd(0xF0); // 高4位=分频因子, 低4位=振荡频率
                      // 0xF0 = 分频因子15, 振荡频率0 (最大刷新率)

  // ---- 设置预充电周期 ----
  OLED_SendCmd(0xD9); // 设置预充电周期命令
  OLED_SendCmd(0x22); // 高4位=放电周期, 低4位=充电周期
                      // 0x22 = 充放电均为2个时钟周期

  // ---- 设置COM引脚硬件配置 ----
  OLED_SendCmd(0xDA); // 设置COM引脚配置命令
  OLED_SendCmd(0x12); // 0x12 = 顺序COM引脚配置, 禁用左右反置
                      // 128x64屏幕使用0x12, 128x32屏幕使用0x02

  // ---- 设置VCOMH取消选择电平 ----
  OLED_SendCmd(0xDB); // 设置VCOMH电压命令
  OLED_SendCmd(0x20); // 0x20 = VCOMH = 0.77xVCC (常用值)
                      // 可选: 0x00(0.65xVCC), 0x20(0.77xVCC), 0x30(0.83xVCC)

  // ---- 使能电荷泵 ----
  OLED_SendCmd(0x8D); // 设置电荷泵命令
  OLED_SendCmd(0x14); // 0x14=使能电荷泵, 0x10=禁用电荷泵
                      // 电荷泵必须开启才能正常显示

  // ---- 清屏并开启显示 ----
  OLED_NewFrame();  // 清空显存 (全黑)
  OLED_ShowFrame(); // 刷新到屏幕

  OLED_SendCmd(0xAF); // AF = Display ON (开启显示)
}

/**
 * @brief  开启OLED显示
 * @note   使能电荷泵并点亮屏幕, 从睡眠模式唤醒
 */
void OLED_DisPlay_On(void)
{
  OLED_SendCmd(0x8D); // 电荷泵使能命令
  OLED_SendCmd(0x14); // 0x14 = 开启电荷泵
  OLED_SendCmd(0xAF); // 0xAF = 开启显示
}

/**
 * @brief  关闭OLED显示
 * @note   关闭电荷泵并关闭屏幕, 进入低功耗模式
 * @note   屏幕关闭后显存内容仍保留, 重新开启后可恢复显示
 */
void OLED_DisPlay_Off(void)
{
  OLED_SendCmd(0x8D); // 电荷泵使能命令
  OLED_SendCmd(0x10); // 0x10 = 关闭电荷泵
  OLED_SendCmd(0xAE); // 0xAE = 关闭显示
}

/**
 * @brief  设置颜色模式 (正常/反色)
 * @param  mode 颜色模式
 *              - OLED_COLOR_NORMAL:  正常显示 (黑底白字)
 *              - OLED_COLOR_REVERSED: 反色显示 (白底黑字)
 * @note   此函数直接修改屏幕的显示模式, 不需要刷新显存
 */
void OLED_SetColorMode(OLED_ColorMode mode)
{
  if (mode == OLED_COLOR_NORMAL)
  {
    OLED_SendCmd(0xA6); // A6 = 正常显示 (数据1=亮, 0=灭)
  }
  if (mode == OLED_COLOR_REVERSED)
  {
    OLED_SendCmd(0xA7); // A7 = 反色显示 (数据1=灭, 0=亮)
  }
}

// ========================== 显存操作函数 ==========================

/**
 * @brief  清空显存, 开始绘制新的一帧
 * @note   将整个OLED_GRAM数组清零, 屏幕变为全黑
 * @note   此函数只操作内存, 不会刷新到屏幕
 */
void OLED_NewFrame(void)
{
  memset(OLED_GRAM, 0, sizeof(OLED_GRAM));
}

/**
 * @brief  将显存内容刷新到OLED屏幕
 * @note   按页发送: 先设置页地址和列地址, 再发送该页的128字节数据
 * @note   共发送 8页 x 128列 = 1024字节
 * @note   移植时需根据驱动芯片的通信协议修改此函数
 */
void OLED_ShowFrame(void)
{
  static uint8_t sendBuffer[OLED_COLUMN + 1]; // +1字节用于I2C控制字节
  sendBuffer[0] = 0x40;                       // 0x40 = 后续数据为显示数据 (区别于0x00命令)

  for (uint8_t i = 0; i < OLED_PAGE; i++)
  {
    OLED_SendCmd(0xB0 + i);                            // 设置页地址 (B0~B7 对应页0~页7)
    OLED_SendCmd(0x00);                                // 设置列地址低4位 (起始列=0)
    OLED_SendCmd(0x10);                                // 设置列地址高4位 (起始列=0)
    memcpy(sendBuffer + 1, OLED_GRAM[i], OLED_COLUMN); // 复制该页数据
    OLED_Send(sendBuffer, OLED_COLUMN + 1);            // 发送128字节数据
  }
}

/**
 * @brief  设置单个像素点
 * @param  x     横坐标 (列), 取值范围: 0 ~ 127
 * @param  y     纵坐标 (行), 取值范围: 0 ~ 63
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  点亮像素 (白色)
 *               - OLED_COLOR_REVERSED: 熄灭像素 (黑色)
 * @note   坐标原点在左上角, x向右增大, y向下增大
 * @note   超出屏幕范围的坐标会被自动忽略
 *
 * @note   显存组织方式说明:
 *         OLED_GRAM[y/8][x] 的第 (y%8) 位对应像素 (x, y)
 *         例如像素(10, 13): page=13/8=1, bit=13%8=5
 *         对应 OLED_GRAM[1][10] 的第5位
 */
void OLED_SetPixel(uint8_t x, uint8_t y, OLED_ColorMode color)
{
  if (x >= OLED_COLUMN || y >= OLED_ROW)
    return; // 超出屏幕范围, 忽略

  if (!color) // OLED_COLOR_NORMAL = 0, 点亮像素
  {
    OLED_GRAM[y / 8][x] |= 1 << (y % 8); // 将对应位置1
  }
  else // OLED_COLOR_REVERSED = 1, 熄灭像素
  {
    OLED_GRAM[y / 8][x] &= ~(1 << (y % 8)); // 将对应位置0
  }
}

/**
 * @brief  设置显存中一字节数据的指定位段 (内部函数)
 * @param  page   页地址, 取值范围: 0 ~ 7
 * @param  column 列地址, 取值范围: 0 ~ 127
 * @param  data   要写入的数据 (只使用其中的 start~end 位)
 * @param  start  起始位 (从0开始), 取值范围: 0 ~ 7
 * @param  end    结束位 (从0开始), 取值范围: start ~ 7
 * @param  color  颜色模式
 * @note   将显存 OLED_GRAM[page][column] 的第 start 位到第 end 位
 *         设置为与 data 对应位相同, 其余位保持不变
 *
 * @note   工作原理:
 *         1. 构造掩码, 将不需要修改的位保留
 *         2. 清除需要修改的位
 *         3. 将data对应位写入
 *
 * @example OLED_SetByte_Fine(0, 10, 0x0F, 2, 5, OLED_COLOR_NORMAL)
 *          将第0页第10列字节的第2~5位设为 0x0F 的第2~5位
 */
void OLED_SetByte_Fine(uint8_t page, uint8_t column, uint8_t data, uint8_t start, uint8_t end, OLED_ColorMode color)
{
  static uint8_t temp;
  if (page >= OLED_PAGE || column >= OLED_COLUMN)
    return; // 越界保护

  if (color) // 反色模式下取反数据
    data = ~data;

  // 保留不需要修改的位, 清除 start~end 位
  temp = data | (0xff << (end + 1)) | (0xff >> (8 - start));
  OLED_GRAM[page][column] &= temp;

  // 写入 start~end 位
  temp = data & ~(0xff << (end + 1)) & ~(0xff >> (8 - start));
  OLED_GRAM[page][column] |= temp;
}

/**
 * @brief  设置显存中的一字节数据 (整字节写入)
 * @param  page   页地址, 取值范围: 0 ~ 7
 * @param  column 列地址, 取值范围: 0 ~ 127
 * @param  data   要写入的8位数据
 * @param  color  颜色模式
 * @note   直接覆盖整个字节, 比 OLED_SetByte_Fine 更高效
 */
void OLED_SetByte(uint8_t page, uint8_t column, uint8_t data, OLED_ColorMode color)
{
  if (page >= OLED_PAGE || column >= OLED_COLUMN)
    return; // 越界保护

  if (color) // 反色模式下取反数据
    data = ~data;

  OLED_GRAM[page][column] = data;
}

/**
 * @brief  以像素坐标设置多行数据 (可跨页, 内部函数)
 * @param  x    横坐标, 取值范围: 0 ~ 127
 * @param  y    纵坐标, 取值范围: 0 ~ 63
 * @param  data 要写入的数据 (仅使用低 len 位)
 * @param  len  位数, 取值范围: 1 ~ 8
 * @param  color 颜色模式
 * @note   从像素(x,y)开始, 向下写入 len 个像素
 * @note   如果跨越页边界, 会自动分两次写入不同页
 * @note   data 的 bit0 对应最上面的像素, bit(len-1) 对应最下面
 *
 * @note   跨页处理示例:
 *         假设 y=6, len=4, 则 y%8=6, 6+4=10>8, 需要跨页
 *         第1页: 写入第6~7位 (2位)
 *         第2页: 写入第0~1位 (2位)
 */
void OLED_SetBits_Fine(uint8_t x, uint8_t y, uint8_t data, uint8_t len, OLED_ColorMode color)
{
  uint8_t page = y / 8; // 起始页
  uint8_t bit = y % 8;  // 页内起始位

  if (bit + len > 8) // 需要跨页
  {
    // 当前页: 写入 bit~7 位
    OLED_SetByte_Fine(page, x, data << bit, bit, 7, color);
    // 下一页: 写入剩余位
    OLED_SetByte_Fine(page + 1, x, data >> (8 - bit), 0, len + bit - 1 - 8, color);
  }
  else // 不跨页
  {
    OLED_SetByte_Fine(page, x, data << bit, bit, bit + len - 1, color);
  }
}

/**
 * @brief  以像素坐标设置一字节数据 (8位, 可跨页, 内部函数)
 * @param  x     横坐标, 取值范围: 0 ~ 127
 * @param  y     纵坐标 (必须是8的倍数), 取值范围: 0 ~ 63
 * @param  data  要写入的8位数据
 * @param  color 颜色模式
 * @note   从像素(x,y)开始, 向下写入8个像素
 * @note   如果y不是8的倍数, 会自动跨页处理
 */
void OLED_SetBits(uint8_t x, uint8_t y, uint8_t data, OLED_ColorMode color)
{
  uint8_t page = y / 8;
  uint8_t bit = y % 8;

  // 写入当前页 (从bit位开始到页尾)
  OLED_SetByte_Fine(page, x, data << bit, bit, 7, color);

  if (bit) // 如果有跨页
  {
    // 写入下一页 (从页头开始)
    OLED_SetByte_Fine(page + 1, x, data >> (8 - bit), 0, bit - 1, color);
  }
}

/**
 * @brief  设置一块矩形区域的显存数据 (内部函数, 用于图片和文字渲染)
 * @param  x     起始横坐标, 取值范围: 0 ~ 127
 * @param  y     起始纵坐标, 取值范围: 0 ~ 63
 * @param  data  数据指针, 数据采用列行式排列
 *               - 排列方式: 先列后行, 每列从上到下按字节排列
 *               - 例如 8x16 图片: 第1列的2字节, 第2列的2字节, ...
 * @param  w     宽度(列数), 取值范围: 1 ~ 128
 * @param  h     高度(行数), 取值范围: 1 ~ 64
 * @param  color 颜色模式
 * @note   此函数是图片和文字渲染的核心函数
 * @note   数据格式: 每列由 ceil(h/8) 个字节组成
 *         第1个字节包含第0~7行, 第2个字节包含第8~15行, 以此类推
 */
void OLED_SetBlock(uint8_t x, uint8_t y, const uint8_t *data, uint8_t w, uint8_t h, OLED_ColorMode color)
{
  uint8_t fullRow = h / 8; // 完整的字节行数 (每行8像素)
  uint8_t partBit = h % 8; // 最后一行的有效位数 (不足8像素的部分)

  // 处理完整的字节行
  for (uint8_t i = 0; i < w; i++) // 遍历每一列
  {
    for (uint8_t j = 0; j < fullRow; j++) // 遍历每个完整字节行
    {
      OLED_SetBits(x + i, y + j * 8, data[i + j * w], color);
    }
  }

  // 处理最后不完整的字节行 (如果高度不是8的倍数)
  if (partBit)
  {
    uint16_t fullNum = w * fullRow; // 完整字节的总数, 也是最后行数据的起始偏移
    for (uint8_t i = 0; i < w; i++)
    {
      OLED_SetBits_Fine(x + i, y + (fullRow * 8), data[fullNum + i], partBit, color);
    }
  }
}

// ========================== 图形绘制函数 ==========================

/**
 * @brief  绘制直线段 (Bresenham算法)
 * @param  x1    起点横坐标, 取值范围: 0 ~ 127
 * @param  y1    起点纵坐标, 取值范围: 0 ~ 63
 * @param  x2    终点横坐标, 取值范围: 0 ~ 127
 * @param  y2    终点纵坐标, 取值范围: 0 ~ 63
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  画白线
 *               - OLED_COLOR_REVERSED: 画黑线(擦除)
 *
 * @note   Bresenham直线算法原理:
 *         通过整数运算逐步逼近直线, 避免浮点运算
 *         利用误差累积判断下一个像素的位置
 *         对于水平线和垂直线做了特殊优化 (直接循环绘制)
 */
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, OLED_ColorMode color)
{
  uint8_t temp;

  if (x1 == x2) // 垂直线 (x坐标相同)
  {
    // 确保 y1 <= y2
    if (y1 > y2)
    {
      temp = y1;
      y1 = y2;
      y2 = temp;
    }
    // 从上到下逐点绘制
    for (uint8_t y = y1; y <= y2; y++)
    {
      OLED_SetPixel(x1, y, color);
    }
  }
  else if (y1 == y2) // 水平线 (y坐标相同)
  {
    // 确保 x1 <= x2
    if (x1 > x2)
    {
      temp = x1;
      x1 = x2;
      x2 = temp;
    }
    // 从左到右逐点绘制
    for (uint8_t x = x1; x <= x2; x++)
    {
      OLED_SetPixel(x, y1, color);
    }
  }
  else // 斜线, 使用Bresenham算法
  {
    int16_t dx = x2 - x1;             // x方向增量
    int16_t dy = y2 - y1;             // y方向增量
    int16_t ux = ((dx > 0) << 1) - 1; // x方向步进: +1或-1
    int16_t uy = ((dy > 0) << 1) - 1; // y方向步进: +1或-1
    int16_t x = x1, y = y1;
    int16_t eps = 0; // 误差累积器

    dx = abs(dx);
    dy = abs(dy);

    if (dx > dy) // x方向跨度更大, 以x为主轴
    {
      for (x = x1; x != x2; x += ux)
      {
        OLED_SetPixel(x, y, color);
        eps += dy;
        if ((eps << 1) >= dx) // 误差超过半步, y方向前进
        {
          y += uy;
          eps -= dx;
        }
      }
    }
    else // y方向跨度更大, 以y为主轴
    {
      for (y = y1; y != y2; y += uy)
      {
        OLED_SetPixel(x, y, color);
        eps += dx;
        if ((eps << 1) >= dy)
        {
          x += ux;
          eps -= dy;
        }
      }
    }
  }
}

/**
 * @brief  绘制矩形边框 (空心)
 * @param  x     左上角横坐标, 取值范围: 0 ~ 127
 * @param  y     左上角纵坐标, 取值范围: 0 ~ 63
 * @param  w     矩形宽度(像素), 取值范围: 1 ~ 128
 * @param  h     矩形高度(像素), 取值范围: 1 ~ 64
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  白色边框
 *               - OLED_COLOR_REVERSED: 黑色边框(擦除)
 * @note   绘制4条边组成矩形
 */
void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, OLED_ColorMode color)
{
  OLED_DrawLine(x, y, x + w, y, color);         // 上边
  OLED_DrawLine(x, y + h, x + w, y + h, color); // 下边
  OLED_DrawLine(x, y, x, y + h, color);         // 左边
  OLED_DrawLine(x + w, y, x + w, y + h, color); // 右边
}

/**
 * @brief  绘制填充矩形 (实心)
 * @param  x     左上角横坐标, 取值范围: 0 ~ 127
 * @param  y     左上角纵坐标, 取值范围: 0 ~ 63
 * @param  w     矩形宽度(像素), 取值范围: 1 ~ 128
 * @param  h     矩形高度(像素), 取值范围: 1 ~ 64
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  白色填充
 *               - OLED_COLOR_REVERSED: 黑色填充(擦除)
 * @note   通过逐行绘制水平线实现填充
 */
void OLED_DrawFilledRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, OLED_ColorMode color)
{
  for (uint8_t i = 0; i < h; i++)
  {
    OLED_DrawLine(x, y + i, x + w, y + i, color);
  }
}

/**
 * @brief  绘制三角形边框 (空心)
 * @param  x1    第一个顶点横坐标, 取值范围: 0 ~ 127
 * @param  y1    第一个顶点纵坐标, 取值范围: 0 ~ 63
 * @param  x2    第二个顶点横坐标, 取值范围: 0 ~ 127
 * @param  y2    第二个顶点纵坐标, 取值范围: 0 ~ 63
 * @param  x3    第三个顶点横坐标, 取值范围: 0 ~ 127
 * @param  y3    第三个顶点纵坐标, 取值范围: 0 ~ 63
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  白色边框
 *               - OLED_COLOR_REVERSED: 黑色边框(擦除)
 */
void OLED_DrawTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3, OLED_ColorMode color)
{
  OLED_DrawLine(x1, y1, x2, y2, color); // 边1-2
  OLED_DrawLine(x2, y2, x3, y3, color); // 边2-3
  OLED_DrawLine(x3, y3, x1, y1, color); // 边3-1
}

/**
 * @brief  绘制填充三角形 (实心)
 * @param  x1    第一个顶点横坐标, 取值范围: 0 ~ 127
 * @param  y1    第一个顶点纵坐标, 取值范围: 0 ~ 63
 * @param  x2    第二个顶点横坐标, 取值范围: 0 ~ 127
 * @param  y2    第二个顶点纵坐标, 取值范围: 0 ~ 63
 * @param  x3    第三个顶点横坐标, 取值范围: 0 ~ 127
 * @param  y3    第三个顶点纵坐标, 取值范围: 0 ~ 63
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  白色填充
 *               - OLED_COLOR_REVERSED: 黑色填充(擦除)
 *
 * @note   算法原理:
 *         将三角形沿y方向扫描, 对每一行计算左右边界并填充
 *         以y3为分界线, 上半部分和下半部分分别处理
 *
 * @warning 任意两个顶点的y坐标不能相同, 否则会导致除零错误!
 *          例如: y1==y2 或 y1==y3 或 y2==y3 的情况
 */
void OLED_DrawFilledTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3, OLED_ColorMode color)
{
  uint8_t a, b, y, last;

  // 找出y1和y2中的较小值和较大值
  if (y1 > y2)
  {
    a = y2; // 较小的y
    b = y1; // 较大的y
  }
  else
  {
    a = y1;
    b = y2;
  }

  y = a; // 从最小的y开始扫描

  // ---- 上半部分: 从 y=a 扫描到 y=y3 ----
  for (; y <= b; y++)
  {
    if (y <= y3)
    {
      // 计算当前行的左右边界x坐标 (线性插值)
      // 左边界: 边1-2上的点
      // 右边界: 边1-3上的点
      uint8_t leftX = x1 + (y - y1) * (x2 - x1) / (y2 - y1);
      uint8_t rightX = x1 + (y - y1) * (x3 - x1) / (y3 - y1);
      OLED_DrawLine(leftX, y, rightX, y, color);
    }
    else
    {
      last = y - 1; // 记录上半部分的最后一个y值
      break;
    }
  }

  // ---- 下半部分: 从 y=y3+1 扫描到 y=b ----
  for (; y <= b; y++)
  {
    // 左边界: 边2-3上的点
    // 右边界: 边1-3上的点 (使用last作为基准避免重复计算)
    uint8_t leftX = x2 + (y - y2) * (x3 - x2) / (y3 - y2);
    uint8_t rightX = x1 + (y - last) * (x3 - x1) / (y3 - last);
    OLED_DrawLine(leftX, y, rightX, y, color);
  }
}

/**
 * @brief  绘制圆形边框 (空心, Bresenham圆算法)
 * @param  x     圆心横坐标, 取值范围: 0 ~ 127
 * @param  y     圆心纵坐标, 取值范围: 0 ~ 63
 * @param  r     圆的半径(像素), 建议范围: 1 ~ 32
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  白色圆
 *               - OLED_COLOR_REVERSED: 黑色圆(擦除)
 *
 * @note   Bresenham圆算法 (中点圆算法) 原理:
 *         利用圆的8重对称性, 只需计算1/8圆弧
 *         每次迭代计算下一个像素位置, 只需整数运算
 *         对于每个点(a,b), 同时绘制8个对称点
 */
void OLED_DrawCircle(uint8_t x, uint8_t y, uint8_t r, OLED_ColorMode color)
{
  int16_t a = 0, b = r;      // a=当前x偏移, b=当前y偏移
  int16_t di = 3 - (r << 1); // 决策参数初始值: 3 - 2*r

  while (a <= b)
  {
    // 利用圆的8重对称性绘制8个点
    OLED_SetPixel(x + a, y + b, color); // 第1象限下
    OLED_SetPixel(x + b, y + a, color); // 第1象限右
    OLED_SetPixel(x + b, y - a, color); // 第4象限右
    OLED_SetPixel(x + a, y - b, color); // 第4象限上
    OLED_SetPixel(x - a, y - b, color); // 第3象限上
    OLED_SetPixel(x - b, y - a, color); // 第3象限左
    OLED_SetPixel(x - b, y + a, color); // 第2象限左
    OLED_SetPixel(x - a, y + b, color); // 第2象限下

    a++; // x方向前进

    if (di < 0) // 选择右方的点
    {
      di += 4 * a + 6;
    }
    else // 选择右下方的点
    {
      di += 10 + 4 * (a - b);
      b--; // y方向后退
    }
  }
}

/**
 * @brief  绘制填充圆 (实心, Bresenham圆算法)
 * @param  x     圆心横坐标, 取值范围: 0 ~ 127
 * @param  y     圆心纵坐标, 取值范围: 0 ~ 63
 * @param  r     圆的半径(像素), 建议范围: 1 ~ 32
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  白色实心圆
 *               - OLED_COLOR_REVERSED: 黑色实心圆(擦除)
 *
 * @note   原理与空心圆类似, 但不是绘制单点而是绘制水平线段
 *         对于每个y偏移, 从左侧到右侧逐点填充
 */
void OLED_DrawFilledCircle(uint8_t x, uint8_t y, uint8_t r, OLED_ColorMode color)
{
  int16_t a = 0, b = r;
  int16_t di = 3 - (r << 1);

  while (a <= b)
  {
    // 填充第1、4象限的水平线 (左右对称)
    for (int16_t i = x - b; i <= x + b; i++)
    {
      OLED_SetPixel(i, y + a, color); // 下半部分
      OLED_SetPixel(i, y - a, color); // 上半部分
    }
    // 填充第2、3象限的水平线 (左右对称)
    for (int16_t i = x - a; i <= x + a; i++)
    {
      OLED_SetPixel(i, y + b, color); // 下半部分
      OLED_SetPixel(i, y - b, color); // 上半部分
    }

    a++;
    if (di < 0)
    {
      di += 4 * a + 6;
    }
    else
    {
      di += 10 + 4 * (a - b);
      b--;
    }
  }
}

/**
 * @brief  绘制椭圆边框 (中点椭圆算法)
 * @param  x     椭圆中心横坐标, 取值范围: 0 ~ 127
 * @param  y     椭圆中心纵坐标, 取值范围: 0 ~ 63
 * @param  a     椭圆X轴半径(水平方向), 建议范围: 1 ~ 64
 * @param  b     椭圆Y轴半径(垂直方向), 建议范围: 1 ~ 32
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  白色椭圆
 *               - OLED_COLOR_REVERSED: 黑色椭圆(擦除)
 *
 * @note   中点椭圆算法原理:
 *         将椭圆分为两个区域, 分别用不同的决策参数
 *         利用椭圆的4重对称性, 每次迭代绘制4个对称点
 */
void OLED_DrawEllipse(uint8_t x, uint8_t y, uint8_t a, uint8_t b, OLED_ColorMode color)
{
  int xpos = 0, ypos = b;
  int a2 = a * a, b2 = b * b;
  int d = b2 + a2 * (0.25 - b); // 区域1的决策参数

  // ---- 区域1: 斜率绝对值 < 1 (x变化快) ----
  while (a2 * ypos > b2 * xpos)
  {
    // 绘制4个对称点
    OLED_SetPixel(x + xpos, y + ypos, color);
    OLED_SetPixel(x - xpos, y + ypos, color);
    OLED_SetPixel(x + xpos, y - ypos, color);
    OLED_SetPixel(x - xpos, y - ypos, color);

    if (d < 0) // 选择东侧的点
    {
      d = d + b2 * ((xpos << 1) + 3);
      xpos += 1;
    }
    else // 选择东南侧的点
    {
      d = d + b2 * ((xpos << 1) + 3) + a2 * (-(ypos << 1) + 2);
      xpos += 1;
      ypos -= 1;
    }
  }

  // ---- 区域2: 斜率绝对值 >= 1 (y变化快) ----
  d = b2 * (xpos + 0.5) * (xpos + 0.5) + a2 * (ypos - 1) * (ypos - 1) - a2 * b2;

  while (ypos > 0)
  {
    OLED_SetPixel(x + xpos, y + ypos, color);
    OLED_SetPixel(x - xpos, y + ypos, color);
    OLED_SetPixel(x + xpos, y - ypos, color);
    OLED_SetPixel(x - xpos, y - ypos, color);

    if (d < 0) // 选择东南侧的点
    {
      d = d + b2 * ((xpos << 1) + 2) + a2 * (-(ypos << 1) + 3);
      xpos += 1;
      ypos -= 1;
    }
    else // 选择南侧的点
    {
      d = d + a2 * (-(ypos << 1) + 3);
      ypos -= 1;
    }
  }
}

/**
 * @brief  绘制图片
 * @param  x     起始点横坐标(左上角), 取值范围: 0 ~ 127
 * @param  y     起始点纵坐标(左上角), 取值范围: 0 ~ 63
 * @param  img   指向Image结构体的指针
 *               - img->w: 图片宽度(像素)
 *               - img->h: 图片高度(像素)
 *               - img->data: 图片数据指针 (列行式排列)
 *               - 使用波特律动LED取模工具生成: https://led.baud-dance.com
 * @param  color 颜色模式
 *               - OLED_COLOR_NORMAL:  正常显示
 *               - OLED_COLOR_REVERSED: 反色显示
 */
void OLED_DrawImage(uint8_t x, uint8_t y, const Image *img, OLED_ColorMode color)
{
  OLED_SetBlock(x, y, img->data, img->w, img->h, color);
}

// ================================ 文字绘制 ================================

/**
 * @brief  绘制单个ASCII字符
 * @param  x     起始点横坐标(左上角), 取值范围: 0 ~ 127
 * @param  y     起始点纵坐标(左上角), 取值范围: 0 ~ 63
 * @param  ch    要显示的ASCII字符, 取值范围: ' '(0x20) ~ '~'(0x7E)
 * @param  font  指向ASCIIFont结构体的指针
 *               - font->w: 字符宽度(像素)
 *               - font->h: 字符高度(像素)
 *               - font->chars: 字模数据数组
 * @param  color 颜色模式
 *
 * @note   字模数据排列方式:
 *         每个字符占用 ((h+7)/8) * w 个字节
 *         按列排列, 每列从上到下按字节存储
 *         字符在字模数组中的偏移 = (ch - ' ') * 每字符字节数
 */
void OLED_PrintASCIIChar(uint8_t x, uint8_t y, char ch, const ASCIIFont *font, OLED_ColorMode color)
{
  // 计算每个字符占多少字节: ceil(h/8) * w
  uint8_t bytesPerChar = ((font->h + 7) / 8) * font->w;
  // 计算当前字符在字模数组中的起始地址
  const uint8_t *charData = font->chars + (ch - ' ') * bytesPerChar;
  // 绘制字符区域
  OLED_SetBlock(x, y, charData, font->w, font->h, color);
}

/**
 * @brief  绘制ASCII字符串
 * @param  x     起始点横坐标(左上角), 取值范围: 0 ~ 127
 * @param  y     起始点纵坐标(左上角), 取值范围: 0 ~ 63
 * @param  str   要显示的ASCII字符串, 以'\0'结尾
 *               - 支持的字符范围: ' '(0x20) ~ '~'(0x7E)
 * @param  font  指向ASCIIFont结构体的指针
 *               - 可选: &afont8x6, &afont12x6, &afont16x8, &afont24x12
 * @param  color 颜色模式
 * @note   字符从左到右依次绘制, 按字体宽度自动推进光标
 * @note   不会自动换行, 超出屏幕的部分会被截断
 */
void OLED_PrintASCIIString(uint8_t x, uint8_t y, char *str, const ASCIIFont *font, OLED_ColorMode color)
{
  uint8_t x0 = x;
  while (*str)
  {
    OLED_PrintASCIIChar(x0, y, *str, font, color);
    x0 += font->w; // 光标右移一个字符宽度
    str++;
  }
}

/**
 * @brief  获取UTF-8编码字符的字节长度 (内部函数)
 * @param  string 指向UTF-8编码字符串的指针
 * @return UTF-8字符的字节长度: 1~4, 0表示编码错误
 *
 * @note   UTF-8编码规则:
 *         0xxxxxxx -> 1字节 (ASCII字符)
 *         110xxxxx 10xxxxxx -> 2字节
 *         1110xxxx 10xxxxxx 10xxxxxx -> 3字节 (常用中文)
 *         11110xxx 10xxxxxx 10xxxxxx 10xxxxxx -> 4字节
 */
uint8_t _OLED_GetUTF8Len(char *string)
{
  if ((string[0] & 0x80) == 0x00) // 0xxxxxxx -> 1字节
    return 1;
  else if ((string[0] & 0xE0) == 0xC0) // 110xxxxx -> 2字节
    return 2;
  else if ((string[0] & 0xF0) == 0xE0) // 1110xxxx -> 3字节
    return 3;
  else if ((string[0] & 0xF8) == 0xF0) // 11110xxx -> 4字节
    return 4;

  return 0; // 无效的UTF-8编码
}

/**
 * @brief  绘制字符串 (支持中英文混合)
 * @param  x     起始点横坐标(左上角), 取值范围: 0 ~ 127
 * @param  y     起始点纵坐标(左上角), 取值范围: 0 ~ 63
 * @param  str   要显示的字符串, 以'\0'结尾, 支持UTF-8编码
 *               - 中文字符: 使用Font结构体中的字库查找
 *               - ASCII字符: 优先在字库中查找, 找不到则使用缺省ASCII字体
 * @param  font  指向Font结构体的指针
 *               - font->w: 中文字符宽度
 *               - font->h: 中文字符高度
 *               - font->chars: 中文字库数据
 *               - font->len: 字库中字符数量
 *               - font->ascii: 缺省ASCII字体指针
 *               - 可选: &font16x16
 * @param  color 颜色模式
 *
 * @note   工作流程:
 *         1. 解析UTF-8编码, 确定当前字符的字节长度
 *         2. 在中文字库中查找匹配的字模 (前4字节为UTF-8编码)
 *         3. 找到则使用中文字模绘制, 未找到则:
 *            - ASCII字符: 使用缺省ASCII字体绘制
 *            - 中文字符: 用空格代替
 *         4. 光标右移, 继续处理下一个字符
 *
 * @note   为保证中文正常显示:
 *         1. 编译器字符集必须设置为UTF-8
 *         2. 中文字模需使用波特律动LED取模工具生成: https://led.baud-dance.com
 *
 * @note   字库数据格式:
 *         每个字符条目 = 4字节UTF-8编码 + ceil(h/8)*w 字节字模数据
 *         字符串匹配时只比较前 utf8Len 字节
 */
void OLED_PrintString(uint8_t x, uint8_t y, char *str, const Font *font, OLED_ColorMode color)
{
  uint16_t i = 0;                                       // 当前字符串索引
  uint8_t oneLen = (((font->h + 7) / 8) * font->w) + 4; // 每个字模条目的字节数 (4字节编码 + 字模数据)
  uint8_t found;                                        // 是否在字库中找到当前字符
  uint8_t utf8Len;                                      // 当前UTF-8字符的字节长度
  uint8_t *head;                                        // 字库中当前条目的指针

  while (str[i])
  {
    found = 0;
    utf8Len = _OLED_GetUTF8Len(str + i); // 获取当前字符的UTF-8长度

    if (utf8Len == 0)
      break; // 无效的UTF-8编码, 终止

    // 在字库中查找当前字符 (顺序查找)
    // TODO: 优化查找算法, 可使用二分查找或hash表
    for (uint8_t j = 0; j < font->len; j++)
    {
      head = (uint8_t *)(font->chars) + (j * oneLen); // 计算第j个条目的地址

      if (memcmp(str + i, head, utf8Len) == 0) // 比较UTF-8编码
      {
        OLED_SetBlock(x, y, head + 4, font->w, font->h, color); // 跳过4字节编码, 绘制字模
        x += font->w;                                           // 光标右移一个中文字宽
        i += utf8Len;                                           // 字符串索引前进UTF-8字节数
        found = 1;
        break;
      }
    }

    // 未在字库中找到, 使用缺省处理
    if (found == 0)
    {
      if (utf8Len == 1) // ASCII字符, 使用缺省ASCII字体显示
      {
        OLED_PrintASCIIChar(x, y, str[i], font->ascii, color);
        x += font->ascii->w; // 光标右移一个ASCII字符宽度
      }
      else // 中文字符未找到字模, 用空格代替
      {
        OLED_PrintASCIIChar(x, y, ' ', font->ascii, color);
        x += font->ascii->w;
      }
      i += utf8Len; // 无论是否找到, 都要前进UTF-8字节数
    }
  }
}
