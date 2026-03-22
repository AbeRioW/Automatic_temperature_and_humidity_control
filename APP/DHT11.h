#ifndef __DHT11_H
#define __DHT11_H

#include "main.h"

// DHT11数据结构体
typedef struct {
    uint8_t humidity;    // 湿度整数部分
    uint8_t humidity_dec; // 湿度小数部分
    uint8_t temperature;  // 温度整数部分
    uint8_t temperature_dec; // 温度小数部分
    uint8_t checksum;     // 校验和
} DHT11_Data;

// 函数原型
void DHT11_Init(void);
uint8_t DHT11_ReadData(DHT11_Data *data);

#endif