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
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "ds1302.h"
#include "DHT11.h"
#include <stdio.h>
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
#define DEFAULT_TEMP_MAX 30  // 默认温度上限
#define DEFAULT_TEMP_MIN 20  // 默认温度下限
#define DEFAULT_HUMID_MAX 70 // 默认湿度上限
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN EV */
// 外部变量声明，用于中断处理
extern uint8_t keyPressed;
/* USER CODE END EV */

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

// 按键状态
uint8_t keyPressed = 0; // 记录按键状态

// 系统启动状态
uint8_t systemReady = 0; // 系统就绪标志
uint32_t startupCounter = 0; // 启动计数器
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
                // 切换到设备界面
                currentInterface = INTERFACE_DEVICE;
            }
            else if(key == KEY2_PRESSED)
            {
                // 切换到主界面
                currentInterface = INTERFACE_MAIN;
            }
            else if(key == KEY3_PRESSED)
            {
                // 减少阈值
                if(tempMax > 20) tempMax--;
                else if(tempMin > 10) tempMin--;
                else if(humidMax > 30) humidMax--;
            }
            else if(key == KEY4_PRESSED)
            {
                // 增加阈值
                if(tempMax < 50) tempMax++;
                else if(tempMin < tempMax - 5) tempMin++;
                else if(humidMax < 99) humidMax++;
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
  /* USER CODE BEGIN 2 */
	 HAL_GPIO_WritePin(GPIOB, LAY_COLD_Pin|LAY_FAN_Pin|LAY_WUHUA_Pin, GPIO_PIN_RESET);
	 HAL_GPIO_WritePin(LAY_HOT_GPIO_Port, LAY_HOT_Pin, GPIO_PIN_RESET);
	 
  OLED_Init();
  DS1302_Init();
  DHT11_Init();
  
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
  
  // 初始状态：关闭所有设备
  ControlDevice(1, DEVICE_OFF);
  ControlDevice(2, DEVICE_OFF);
  ControlDevice(3, DEVICE_OFF);
  ControlDevice(4, DEVICE_OFF);
  
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
    
    // 启动计数器，系统运行10秒后标记为就绪
    startupCounter++;
    if(startupCounter > 100) // 100次循环 * 100ms = 10秒
    {
        systemReady = 1;
    }
    
    // 处理按键（由中断触发）
    if(keyPressed != 0)
    {
        ProcessKeyPress(keyPressed);
        keyPressed = 0; // 清除按键状态
    }
    
    // 自动控制逻辑
    AutoControlLogic();
    
    // 根据当前界面绘制显示
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
    // 延时消抖
    for(volatile uint32_t i = 0; i < 100000; i++);
    
    // 检查按键状态并等待释放
    if(GPIO_Pin == KEY1_Pin && HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)
    {
        keyPressed = KEY1_PRESSED;
        // 等待按键释放
        while(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET);
    }
    else if(GPIO_Pin == KEY2_Pin && HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET)
    {
        keyPressed = KEY2_PRESSED;
        // 等待按键释放
        while(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET);
    }
    else if(GPIO_Pin == KEY3_Pin && HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == GPIO_PIN_RESET)
    {
        keyPressed = KEY3_PRESSED;
        // 等待按键释放
        while(HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == GPIO_PIN_RESET);
    }
    else if(GPIO_Pin == KEY4_Pin && HAL_GPIO_ReadPin(KEY4_GPIO_Port, KEY4_Pin) == GPIO_PIN_RESET)
    {
        keyPressed = KEY4_PRESSED;
        // 等待按键释放
        while(HAL_GPIO_ReadPin(KEY4_GPIO_Port, KEY4_Pin) == GPIO_PIN_RESET);
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
