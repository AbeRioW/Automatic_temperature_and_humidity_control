/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "ds1302.h"
#include "DHT11.h"
#include <stdio.h>
#include "esp8266.h"
#include "mqtt_publisher.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// 界面定义
#define INTERFACE_MAIN 1    // 主界面：显示时间、温度、湿度
#define INTERFACE_DEVICE 2  // 设备界面：显示设备状态
#define INTERFACE_SETTING 3 // 设置界面：显示阈值设置

// 工作模式
#define MODE_AUTO 0   // 自动模式
#define MODE_MANUAL 1 // 手动模式

// 设备状态
#define DEVICE_OFF 0  // 设备关闭
#define DEVICE_ON 1   // 设备开启

// 按键定义
#define KEY1_PRESSED 1
#define KEY2_PRESSED 2
#define KEY3_PRESSED 3
#define KEY4_PRESSED 4

// 阈值默认值
#define DEFAULT_TEMP_MAX 40  // 默认温度上限
#define DEFAULT_TEMP_MIN 25  // 默认温度下限
#define DEFAULT_HUMID_MAX 70 // 默认湿度上限
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
DS1302_Time currentTime;
uint8_t timeStr[32];
DHT11_Data dht11Data;

// 系统状态变量
uint8_t currentInterface = INTERFACE_MAIN; // 当前界面
uint8_t currentMode = MODE_AUTO;           // 当前工作模式

// 设备状态
uint8_t heaterStatus = DEVICE_OFF;  // 加热器状态
uint8_t coolerStatus = DEVICE_OFF;  // 制冷片状态
uint8_t fanStatus = DEVICE_OFF;     // 风扇状态
uint8_t wuhuaStatus = DEVICE_OFF;   // 雾化器状态

// 阈值设置
uint8_t tempMax = DEFAULT_TEMP_MAX;  // 温度上限
uint8_t tempMin = DEFAULT_TEMP_MIN;  // 温度下限
uint8_t humidMax = DEFAULT_HUMID_MAX; // 湿度上限

// 设置界面当前选中的项目
#define SETTING_TEMP_MAX 0  // 温度上限
#define SETTING_TEMP_MIN 1  // 温度下限
#define SETTING_HUMID_MAX 2 // 湿度上限
uint8_t currentSettingItem = SETTING_TEMP_MAX; // 当前选中的设置项

// 按键状态
uint8_t keyPressed = 0; // 记录按键状态
uint8_t keyPressFlag = 0; // 按键按下标志（用于中断）
uint32_t keyPressTime = 0; // 按键按下时间

// 系统启动状态
uint8_t systemReady = 0; // 系统就绪标志
uint32_t startupCounter = 0; // 启动计数器

// 上次数据（用于按需刷新）
DS1302_Time lastTime;
DHT11_Data lastDht11Data;
uint8_t lastInterface = INTERFACE_MAIN;
uint8_t lastMode = MODE_AUTO;
uint8_t lastHeaterStatus = DEVICE_OFF;
uint8_t lastCoolerStatus = DEVICE_OFF;
uint8_t lastFanStatus = DEVICE_OFF;
uint8_t lastWuhuaStatus = DEVICE_OFF;
uint8_t lastTempMax = DEFAULT_TEMP_MAX;
uint8_t lastTempMin = DEFAULT_TEMP_MIN;
uint8_t lastHumidMax = DEFAULT_HUMID_MAX;
uint8_t lastSettingItem = SETTING_TEMP_MAX;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
// 界面绘制函数
void DrawMainInterface(void);
void DrawDeviceInterface(void);
void DrawSettingInterface(void);

// 按键处理函数
void ProcessKeyPress(uint8_t key);

// 设备控制函数
void ControlDevice(uint8_t device, uint8_t status);

// 自动控制逻辑
void AutoControlLogic(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// 设备控制函数
void ControlDevice(uint8_t device, uint8_t status)
{
    switch(device)
    {
        case 1: // 加热器
            heaterStatus = status;
            HAL_GPIO_WritePin(LAY_HOT_GPIO_Port, LAY_HOT_Pin, status ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;
        case 2: // 制冷片
            coolerStatus = status;
            HAL_GPIO_WritePin(LAY_COLD_GPIO_Port, LAY_COLD_Pin, status ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;
        case 3: // 风扇
            fanStatus = status;
            HAL_GPIO_WritePin(LAY_FAN_GPIO_Port, LAY_FAN_Pin, status ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;
        case 4: // 雾化器
            wuhuaStatus = status;
            HAL_GPIO_WritePin(LAY_WUHUA_GPIO_Port, LAY_WUHUA_Pin, status ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;
    }
}

// 自动控制逻辑
void AutoControlLogic(void)
{
    // 检查系统是否就绪和温度数据是否有效
    if(!systemReady || dht11Data.temperature == 0 || dht11Data.temperature > 60)
    {
        // 系统未就绪或温度数据无效，关闭所有设备
        if(heaterStatus == DEVICE_ON) ControlDevice(1, DEVICE_OFF);
        if(coolerStatus == DEVICE_ON) ControlDevice(2, DEVICE_OFF);
        if(fanStatus == DEVICE_ON) ControlDevice(3, DEVICE_OFF);
        return;
    }
    
    if(currentMode == MODE_AUTO)
    {
        // 安全保护：确保加热器和制冷片不同时工作
        if(heaterStatus == DEVICE_ON)
        {
            ControlDevice(2, DEVICE_OFF); // 关闭制冷片
        }
        if(coolerStatus == DEVICE_ON)
        {
            ControlDevice(1, DEVICE_OFF); // 关闭加热器
        }
        
        // 温度控制
        if(dht11Data.temperature < tempMin)
        {
            // 温度低于下限，开启加热器
            ControlDevice(1, DEVICE_ON);
            ControlDevice(2, DEVICE_OFF);
        }
        else if(dht11Data.temperature > tempMax)
        {
            // 温度高于上限，开启制冷片
            ControlDevice(1, DEVICE_OFF);
            ControlDevice(2, DEVICE_ON);
        }
        else
        {
            // 温度在正常范围内，关闭加热和制冷
            ControlDevice(1, DEVICE_OFF);
            ControlDevice(2, DEVICE_OFF);
        }
        
        // 湿度控制
        if(dht11Data.humidity > humidMax)
        {
            // 湿度高于上限，开启风扇除湿
            ControlDevice(3, DEVICE_ON);
        }
        else
        {
            // 湿度正常，关闭风扇
            ControlDevice(3, DEVICE_OFF);
        }
    }
}

// 绘制主界面
void DrawMainInterface(void)
{
    OLED_Clear();
    
    // 显示日期
    sprintf((char*)timeStr, "20%02d-%02d-%02d", currentTime.year, currentTime.month, currentTime.date);
    OLED_ShowString(0, 0, timeStr, 8, 1);
    
    // 显示时间
    sprintf((char*)timeStr, "%02d:%02d:%02d", currentTime.hour, currentTime.minute, currentTime.second);
    OLED_ShowString(0, 16, timeStr, 8, 1);
    
    // 显示温度
    sprintf((char*)timeStr, "T: %d.%dC", dht11Data.temperature, dht11Data.temperature_dec);
    OLED_ShowString(0, 32, timeStr, 8, 1);
    
    // 显示湿度
    sprintf((char*)timeStr, "H: %d.%d%%", dht11Data.humidity, dht11Data.humidity_dec);
    OLED_ShowString(0, 48, timeStr, 8, 1);
    
    // 显示工作模式
    if(currentMode == MODE_AUTO)
    {
        OLED_ShowString(80, 0, (uint8_t*)"AUTO", 8, 1);
    }
    else
    {
        OLED_ShowString(80, 0, (uint8_t*)"MANUAL", 8, 1);
    }
    
    OLED_Refresh();
}

// 绘制设备界面
void DrawDeviceInterface(void)
{
    OLED_Clear();
    
    // 显示设备状态
    OLED_ShowString(0, 0, (uint8_t*)"Heater:", 8, 1);
    OLED_ShowString(60, 0, (heaterStatus ? (uint8_t*)"ON" : (uint8_t*)"OFF"), 8, 1);
    
    OLED_ShowString(0, 16, (uint8_t*)"Cooler:", 8, 1);
    OLED_ShowString(60, 16, (coolerStatus ? (uint8_t*)"ON" : (uint8_t*)"OFF"), 8, 1);
    
    OLED_ShowString(0, 32, (uint8_t*)"Fan:", 8, 1);
    OLED_ShowString(60, 32, (fanStatus ? (uint8_t*)"ON" : (uint8_t*)"OFF"), 8, 1);
    
    OLED_ShowString(0, 48, (uint8_t*)"Wuhua:", 8, 1);
    OLED_ShowString(60, 48, (wuhuaStatus ? (uint8_t*)"ON" : (uint8_t*)"OFF"), 8, 1);
    
    OLED_Refresh();
}

// 绘制设置界面
void DrawSettingInterface(void)
{
    OLED_Clear();
    
    // 显示阈值设置
    sprintf((char*)timeStr, "Temp Max: %dC", tempMax);
    OLED_ShowString(0, 0, timeStr, 8, 1);
    
    sprintf((char*)timeStr, "Temp Min: %dC", tempMin);
    OLED_ShowString(0, 16, timeStr, 8, 1);
    
    sprintf((char*)timeStr, "Humid Max: %d%%", humidMax);
    OLED_ShowString(0, 32, timeStr, 8, 1);
    
    // 显示当前选中的设置项
    switch(currentSettingItem)
    {
        case SETTING_TEMP_MAX:
            OLED_ShowString(100, 0, (uint8_t*)"->", 8, 1);
            break;
        case SETTING_TEMP_MIN:
            OLED_ShowString(100, 16, (uint8_t*)"->", 8, 1);
            break;
        case SETTING_HUMID_MAX:
            OLED_ShowString(100, 32, (uint8_t*)"->", 8, 1);
            break;
    }
    
    OLED_ShowString(0, 48, (uint8_t*)"KEY1:Prev KEY2:Next", 8, 1);
    OLED_ShowString(0, 56, (uint8_t*)"KEY3:Down KEY4:Up", 8, 1);
    
    OLED_Refresh();
}

// 按键处理函数
void ProcessKeyPress(uint8_t key)
{
    switch(currentInterface)
    {
        case INTERFACE_MAIN:
            if(key == KEY1_PRESSED)
            {
                // 切换工作模式
                currentMode = (currentMode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
            }
            else if(key == KEY2_PRESSED)
            {
                // 切换到设备界面
                currentInterface = INTERFACE_DEVICE;
            }
            break;
            
        case INTERFACE_DEVICE:
            if(key == KEY1_PRESSED)
            {
                // 切换到主界面
                currentInterface = INTERFACE_MAIN;
            }
            else if(key == KEY2_PRESSED)
            {
                // 切换到设置界面
                currentInterface = INTERFACE_SETTING;
                currentSettingItem = SETTING_TEMP_MAX; // 重置为第一项
            }
            else if(key == KEY3_PRESSED)
            {
                // 手动控制设备（循环关闭）
                if(heaterStatus == DEVICE_ON) ControlDevice(1, DEVICE_OFF);
                else if(coolerStatus == DEVICE_ON) ControlDevice(2, DEVICE_OFF);
                else if(fanStatus == DEVICE_ON) ControlDevice(3, DEVICE_OFF);
                else if(wuhuaStatus == DEVICE_ON) ControlDevice(4, DEVICE_OFF);
            }
            else if(key == KEY4_PRESSED)
            {
                // 手动控制设备（循环开启）
                if(heaterStatus == DEVICE_OFF) ControlDevice(1, DEVICE_ON);
                else if(coolerStatus == DEVICE_OFF) ControlDevice(2, DEVICE_ON);
                else if(fanStatus == DEVICE_OFF) ControlDevice(3, DEVICE_ON);
                else if(wuhuaStatus == DEVICE_OFF) ControlDevice(4, DEVICE_ON);
            }
            break;
            
        case INTERFACE_SETTING:
            if(key == KEY1_PRESSED)
            {
                // 切换到上一个设置项
                currentSettingItem--;
                if(currentSettingItem < SETTING_TEMP_MAX)
                {
                    // 已经到第一项，回到设备界面
                    currentInterface = INTERFACE_DEVICE;
                    currentSettingItem = SETTING_TEMP_MAX; // 重置为第一项
                }
            }
            else if(key == KEY2_PRESSED)
            {
                // 切换到下一个设置项
                currentSettingItem++;
                if(currentSettingItem > SETTING_HUMID_MAX)
                {
                    // 已经到最后一项，退出设置界面回到主界面
                    currentInterface = INTERFACE_MAIN;
                    currentSettingItem = SETTING_TEMP_MAX; // 重置为第一项
                }
            }
            else if(key == KEY3_PRESSED)
            {
                // 减少当前选中的阈值
                switch(currentSettingItem)
                {
                    case SETTING_TEMP_MAX:
                        if(tempMax > 20) tempMax--;
                        break;
                    case SETTING_TEMP_MIN:
                        if(tempMin > 10) tempMin--;
                        break;
                    case SETTING_HUMID_MAX:
                        if(humidMax > 30) humidMax--;
                        break;
                }
            }
            else if(key == KEY4_PRESSED)
            {
                // 增加当前选中的阈值
                switch(currentSettingItem)
                {
                    case SETTING_TEMP_MAX:
                        if(tempMax < 50) tempMax++;
                        break;
                    case SETTING_TEMP_MIN:
                        if(tempMin < tempMax - 5) tempMin++;
                        break;
                    case SETTING_HUMID_MAX:
                        if(humidMax < 99) humidMax++;
                        break;
                }
            }
            break;
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  uint8_t wifi_try = 0, mqtt_try = 0;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
	
  MX_DMA_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
		  HAL_GPIO_WritePin(GPIOB, LAY_COLD_Pin|LAY_FAN_Pin|LAY_WUHUA_Pin, GPIO_PIN_RESET);
		  HAL_GPIO_WritePin(GPIOA,LAY_HOT_Pin, GPIO_PIN_RESET);
	  OLED_Init();
  DS1302_Init();
  DHT11_Init();

  	ESP8266_Init();
		  //WIFI连接
  while (wifi_try < 5 && !ESP8266_ConnectWiFi())
  {
      wifi_try++;
      HAL_Delay(1000);
  }
	
	  //上云
	if(ESP8266_ConnectCloud()==false)
	{
		  while(1);
	}
	HAL_Delay(5000);
	ESP8266_Clear();
	OLED_Clear();
	
	//订阅
	if(!ESP8266_MQTT_Subscribe(MQTT_TOPIC_POST_REPLY,1))
	{
		  while(1);
	}
	
	//发布
		if(!ESP8266_MQTT_Subscribe(MQTT_TOPIC_SET,0))
	{
		  while(1);
	}
	
	
  // 初始化继电器引脚为输出
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  // 加热器
  GPIO_InitStruct.Pin = LAY_HOT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LAY_HOT_GPIO_Port, &GPIO_InitStruct);
  
  // 制冷片
  GPIO_InitStruct.Pin = LAY_COLD_Pin;
  HAL_GPIO_Init(LAY_COLD_GPIO_Port, &GPIO_InitStruct);
  
  // 风扇
  GPIO_InitStruct.Pin = LAY_FAN_Pin;
  HAL_GPIO_Init(LAY_FAN_GPIO_Port, &GPIO_InitStruct);
  
  // 雾化器
  GPIO_InitStruct.Pin = LAY_WUHUA_Pin;
  HAL_GPIO_Init(LAY_WUHUA_GPIO_Port, &GPIO_InitStruct);
  
  // 上电默认拉低，确保设备处于关闭状态
  HAL_GPIO_WritePin(LAY_HOT_GPIO_Port, LAY_HOT_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LAY_COLD_GPIO_Port, LAY_COLD_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LAY_FAN_GPIO_Port, LAY_FAN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LAY_WUHUA_GPIO_Port, LAY_WUHUA_Pin, GPIO_PIN_RESET);
  
  // 初始化设备状态变量
  heaterStatus = DEVICE_OFF;
  coolerStatus = DEVICE_OFF;
  fanStatus = DEVICE_OFF;
  wuhuaStatus = DEVICE_OFF;
  

  
//  OLED_ShowString(0,0,(uint8_t*)"hello",8,1);
//  OLED_Refresh();
//  HAL_Delay(1000);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // 读取时间和温湿度数据
    DS1302_GetTime(&currentTime);
    DHT11_ReadData(&dht11Data);
    
    // 发送温湿度数据到MQTT
    char temp_str[10];
    char humid_str[10];
    sprintf(temp_str, "%d.%d", dht11Data.temperature, dht11Data.temperature_dec);
    sprintf(humid_str, "%d.%d", dht11Data.humidity, dht11Data.humidity_dec);
    MQTT_Publish_temp(temp_str);
    MQTT_Publish_humidity(humid_str);
    
    // 启动计数器，系统运行10秒后标记为就绪
    startupCounter++;
    if(startupCounter > 100) // 100次循环 * 100ms = 10秒
    {
        systemReady = 1;
    }
    
    // 处理按键（由中断触发）
    if(keyPressFlag != 0)
    {
        // 立即更新按键时间戳（防止重复触发）
        uint32_t currentTime = HAL_GetTick();
        
        // 检查是否已经过了消抖时间（50ms）
        if(currentTime - keyPressTime > 50)
        {
            // 确认按键仍然按下
            uint8_t validKey = 0;
            if(keyPressFlag == KEY1_PRESSED && HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)
            {
                validKey = KEY1_PRESSED;
            }
            else if(keyPressFlag == KEY2_PRESSED && HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET)
            {
                validKey = KEY2_PRESSED;
            }
            else if(keyPressFlag == KEY3_PRESSED && HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == GPIO_PIN_RESET)
            {
                validKey = KEY3_PRESSED;
            }
            else if(keyPressFlag == KEY4_PRESSED && HAL_GPIO_ReadPin(KEY4_GPIO_Port, KEY4_Pin) == GPIO_PIN_RESET)
            {
                validKey = KEY4_PRESSED;
            }
            
            // 如果按键有效，处理按键事件
            if(validKey != 0)
            {
                ProcessKeyPress(validKey);
            }
            
            // 清除按键标志（无论是否有效）
            keyPressFlag = 0;
            // 更新时间戳，防止短时间内重复触发
            keyPressTime = currentTime;
        }
        // 即使消抖时间不够，也要清除标志，避免卡死
        else
        {
            keyPressFlag = 0;
        }
    }
    else
    {
        // 没有按键按下时，持续更新按键时间戳
        keyPressTime = HAL_GetTick();
    }
    
    // 自动控制逻辑
    AutoControlLogic();
    
    // 按需刷新：只在数据变化时刷新显示
    uint8_t needRefresh = 0;
    
    // 检查界面是否变化
    if(currentInterface != lastInterface)
    {
        needRefresh = 1;
        lastInterface = currentInterface;
    }
    
    // 根据当前界面检查数据变化
    switch(currentInterface)
    {
        case INTERFACE_MAIN:
            // 检查时间变化（只检查秒）
            if(currentTime.second != lastTime.second ||
               currentTime.minute != lastTime.minute ||
               currentTime.hour != lastTime.hour ||
               currentTime.date != lastTime.date ||
               currentTime.month != lastTime.month ||
               currentTime.year != lastTime.year)
            {
                needRefresh = 1;
                lastTime = currentTime;
            }
            
            // 检查温湿度变化
            if(dht11Data.temperature != lastDht11Data.temperature ||
               dht11Data.humidity != lastDht11Data.humidity)
            {
                needRefresh = 1;
                lastDht11Data = dht11Data;
            }
            
            // 检查工作模式变化
            if(currentMode != lastMode)
            {
                needRefresh = 1;
                lastMode = currentMode;
            }
            break;
            
        case INTERFACE_DEVICE:
            // 检查设备状态变化
            if(heaterStatus != lastHeaterStatus ||
               coolerStatus != lastCoolerStatus ||
               fanStatus != lastFanStatus ||
               wuhuaStatus != lastWuhuaStatus)
            {
                needRefresh = 1;
                lastHeaterStatus = heaterStatus;
                lastCoolerStatus = coolerStatus;
                lastFanStatus = fanStatus;
                lastWuhuaStatus = wuhuaStatus;
            }
            break;
            
        case INTERFACE_SETTING:
            // 检查阈值变化
            if(tempMax != lastTempMax ||
               tempMin != lastTempMin ||
               humidMax != lastHumidMax ||
               currentSettingItem != lastSettingItem)
            {
                needRefresh = 1;
                lastTempMax = tempMax;
                lastTempMin = tempMin;
                lastHumidMax = humidMax;
                lastSettingItem = currentSettingItem;
            }
            break;
    }
    
    // 只有在需要刷新时才绘制显示
    if(needRefresh)
    {
        switch(currentInterface)
        {
            case INTERFACE_MAIN:
                DrawMainInterface();
                break;
            case INTERFACE_DEVICE:
                DrawDeviceInterface();
                break;
            case INTERFACE_SETTING:
                DrawSettingInterface();
                break;
        }
    }
    
    HAL_Delay(100); // 100ms刷新一次
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
  * @brief  EXTI line detection callback.
  * @param  GPIO_Pin: Specifies the pins connected EXTI line
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    // 只设置标志位，不在中断中做任何延时或等待
    if(GPIO_Pin == KEY1_Pin)
    {
        keyPressFlag = KEY1_PRESSED;
    }
    else if(GPIO_Pin == KEY2_Pin)
    {
        keyPressFlag = KEY2_PRESSED;
    }
    else if(GPIO_Pin == KEY3_Pin)
    {
        keyPressFlag = KEY3_PRESSED;
    }
    else if(GPIO_Pin == KEY4_Pin)
    {
        keyPressFlag = KEY4_PRESSED;
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
