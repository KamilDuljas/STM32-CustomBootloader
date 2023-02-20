/*
 * boot.c
 *
 *  Created on: Feb 20, 2023
 *      Author: kduljas
 */

#include <stdbool.h>
#include "boot.h"
#include "usart.h"

void jump_to_application(uint32_t const application_address) {

	typedef void (*jumpFunction)(void);

	/**
	 * Step: Disable RCC and other peripherals
	 */
	HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
	HAL_NVIC_DisableIRQ(USART1_IRQn);
	HAL_NVIC_DisableIRQ(USART2_IRQn);
	HAL_GPIO_DeInit(LD2_GPIO_Port, LD2_Pin);
	HAL_GPIO_DeInit(USER_BUTTON_GPIO_Port, USER_BUTTON_Pin);
	uint8_t isDeInitOk = 0;
	if ((HAL_RCC_DeInit() || HAL_UART_DeInit(&huart1)
			|| HAL_UART_DeInit(&huart2) || HAL_DeInit()) == HAL_OK) {
		isDeInitOk = 1;
	}

	/**
	 * Step: Disable systick timer and reset it to default values
	 */
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;

	/**
	 * Step: Set jump memory location to application address
	 */
	uint32_t jumpAddress = *(__IO uint32_t*) (application_address + 4);
	jumpFunction jumpToProgram = (jumpFunction) jumpAddress;

	/**
	 * Step: Set main stack pointer.
	 *       This step must be done last otherwise local variables in this function
	 *       don't have proper value since stack pointer is located on different position
	 *
	 *       Set direct address location which specifies stack pointer in SRAM location
	 */
	__set_MSP(*(__IO uint32_t*) application_address);

	/**
	 * Step: Actually call our function to jump to set location. This will start application execution.
	 * If isDeinitOk flag is not set, then call system reset.
	 */
	if (isDeInitOk) {
		jumpToProgram();
	} else {
		HAL_NVIC_SystemReset();
	}
}
