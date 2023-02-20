/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
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
#include "crc.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <string.h>
#include "ioputchar.h"
#include "boot.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// Address for main program to be executed after jump from bootloader
uint32_t const APPLICATION_ADDRESS = 0x08008000UL;
uint32_t applicationSize = 0;

UART_HandleTypeDef *uart[2] = { &huart1, &huart2 };
bool g_waitForUsart = true;
bool g_jumpToApp = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart == uart[0]) {
		mainUart = uart[0];
	}
	if (huart == uart[1]) {
		mainUart = uart[1];
	}
	g_waitForUsart = false;
}

void HAL_GPIO_EXTI_Callback(uint16_t pin) {
	if (pin == USER_BUTTON_Pin)
		g_jumpToApp = true;
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
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_CRC_Init();
  /* USER CODE BEGIN 2 */
	// Startup test for CRC
	uint32_t echo = 0x12345678;
	uint32_t calculatedChecksum = HAL_CRC_Calculate(&hcrc, &echo, 1);
	if (calculatedChecksum != 0xDF8A8A2B)
		Error_Handler();

	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
	printf("Bootloader start...\n");
	printf("Press user button to jump application (start: 0x08000000)\n");
	printf("or send ECHO command (0x1 0x0 0x0 0x0 0x0) to choose and set UART\n");
	uint8_t pattern[5] = { 1, 0, 0, 0, 0 };
	uint8_t buffer[5] = { 0 };
	HAL_UART_Receive_IT(&huart1, buffer, 5);
	HAL_UART_Receive_IT(&huart2, buffer, 5);
	while (g_waitForUsart == true || memcmp(pattern, buffer, 5))
	{
		if (g_jumpToApp == true)
			break;
	}
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	if (mainUart != NULL)
	{
		if(mainUart == uart[0])
		{
			printf("Set UART1\n");
			printf("DeInit UART2\n");
			HAL_UART_DeInit(uart[1]);
		}
		else
		{
			printf("Set UART2\n");
			printf("DeInit UART1\n");
			HAL_UART_DeInit(uart[0]);
		}
	}
	while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		if(g_jumpToApp == true)
		{
			printf("Jump to application\n");
			jump_to_application(APPLICATION_ADDRESS);
		}
		if (mainUart != NULL) {
			for (int i = 1; i < 2; i++) {
				BootCommand cmd = get_command(mainUart, 100);
				switch (cmd.opcode) {
				case BOOT_CMD_ECHO: {
					// Simple echo request, respond with OK to tell that
					// the BOOT is running.
					respond_ok(mainUart);
					break;
				}
				case BOOT_CMD_SETSIZE: {
					// Set the application size and respond with OK
					applicationSize = cmd.data;
					respond_ok(mainUart);
					break;
				}
				case BOOT_CMD_UPDATE: {
					// Update firmware
					if (receive_and_flash_firmware(mainUart, applicationSize)) {
						respond_ok(mainUart);
					} else {
						respond_err(mainUart);
					}
					break;
				}
				case BOOT_CMD_CHECK: {
					// Check if flashed firmware has valid
					if (verify_firmware(applicationSize, cmd.data)) {
						respond_ok(mainUart);
					} else {
						respond_err(mainUart);
					}
					break;
				}
				case BOOT_CMD_JUMP: {
					// Jump directly to the application.
					respond_ok(mainUart);
					jump_to_application(APPLICATION_ADDRESS);
					break;
				}
				case BOOT_CMD_INVALID: {
					// No command received. We have to handle this case, because
					// otherwise the BOOT would indefinitely spam ERR
					// while nothing is happening.
					break;
				}
				default: {
					// Invalid opcode, respond with error.
					respond_err(mainUart);
					break;
				}
				}
			}
		}
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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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
	while (1) {
	}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
