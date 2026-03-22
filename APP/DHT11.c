#include "DHT11.h"

// 位操作宏
#define DHT11_DQ_HIGH HAL_GPIO_WritePin(DHT11_DQ_GPIO_Port, DHT11_DQ_Pin, GPIO_PIN_SET)
#define DHT11_DQ_LOW  HAL_GPIO_WritePin(DHT11_DQ_GPIO_Port, DHT11_DQ_Pin, GPIO_PIN_RESET)
#define DHT11_DQ_READ HAL_GPIO_ReadPin(DHT11_DQ_GPIO_Port, DHT11_DQ_Pin)

// 微秒延时函数
void DHT11_DelayUs(uint32_t us)
{
    uint32_t i;
    for(i = 0; i < us * 8; i++)
    {
        __NOP();
    }
}

// 毫秒延时函数
void DHT11_DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}

// 设置DQ引脚为输出模式
void DHT11_DQ_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_DQ_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_DQ_GPIO_Port, &GPIO_InitStruct);
}

// 设置DQ引脚为输入模式
void DHT11_DQ_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_DQ_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DHT11_DQ_GPIO_Port, &GPIO_InitStruct);
}

/**
  * @brief  初始化DHT11
  * @retval None
  */
void DHT11_Init(void)
{
    // 初始化DQ引脚
    DHT11_DQ_SetOutput();
    DHT11_DQ_HIGH;
    DHT11_DelayMs(100);
}

/**
  * @brief  从DHT11读取一个字节
  * @retval 读取的字节
  */
uint8_t DHT11_ReadByte(void)
{
    uint8_t i, data = 0;
    
    for(i = 0; i < 8; i++)
    {
        // 等待低电平结束
        while(DHT11_DQ_READ == 0);
        
        // 延时40us，判断高电平持续时间
        DHT11_DelayUs(40);
        
        data <<= 1;
        if(DHT11_DQ_READ == 1)
        {
            data |= 0x01;
            // 等待高电平结束
            while(DHT11_DQ_READ == 1);
        }
    }
    
    return data;
}

/**
  * @brief  从DHT11读取数据
  * @param  data: 数据结构体指针
  * @retval 0: 成功，1: 失败
  */
uint8_t DHT11_ReadData(DHT11_Data *data)
{
    uint8_t buf[5];
    uint8_t i;
    
    // 发送开始信号
    DHT11_DQ_SetOutput();
    DHT11_DQ_LOW;
    DHT11_DelayMs(20);
    DHT11_DQ_HIGH;
    DHT11_DelayUs(30);
    
    // 切换到输入模式
    DHT11_DQ_SetInput();
    
    // 等待DHT11响应
    if(DHT11_DQ_READ == 0)
    {
        // 等待低电平结束
        while(DHT11_DQ_READ == 0);
        // 等待高电平结束
        while(DHT11_DQ_READ == 1);
        
        // 读取40位数据
        for(i = 0; i < 5; i++)
        {
            buf[i] = DHT11_ReadByte();
        }
        
        // 切换回输出模式
        DHT11_DQ_SetOutput();
        DHT11_DQ_HIGH;
        
        // 校验
        if(buf[0] + buf[1] + buf[2] + buf[3] == buf[4])
        {
            data->humidity = buf[0];
            data->humidity_dec = buf[1];
            data->temperature = buf[2];
            data->temperature_dec = buf[3];
            data->checksum = buf[4];
            return 0; // 成功
        }
    }
    
    // 切换回输出模式
    DHT11_DQ_SetOutput();
    DHT11_DQ_HIGH;
    
    return 1; // 失败
}