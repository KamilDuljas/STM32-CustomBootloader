#include <stdbool.h>
#include "boot.h"
#include "usart.h"
#include "crc.h"

extern uint32_t APPLICATION_ADDRESS;
char const *const BOOT_MSG_OK = "OK!\n";
char const *const BOOT_MSG_ERR = "ERR\n";
uint8_t bootladerBuffer[BOOT_BUFFER_SIZE];

BootCommand get_command(UART_HandleTypeDef *const uart,
		uint32_t const timeout) {
	BootCommand cmd = { .opcode = BOOT_CMD_INVALID };
	uint8_t buffer[5] = { };

	HAL_StatusTypeDef status = HAL_UART_Receive(uart, buffer, 5, timeout);
	if (status == HAL_OK) {
		cmd.opcode = buffer[0];
		cmd.data = (((uint32_t) buffer[1]) << 24)
				| (((uint32_t) buffer[2]) << 16) | (((uint32_t) buffer[3]) << 8)
				| buffer[4];
	}

	return cmd;
}

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

void respond_ok(UART_HandleTypeDef *const uart) {
	HAL_UART_Transmit(uart, (uint8_t*) BOOT_MSG_OK, 5, HAL_MAX_DELAY);
}

void respond_err(UART_HandleTypeDef *const uart) {
	HAL_UART_Transmit(uart, (uint8_t*) BOOT_MSG_ERR, 5, HAL_MAX_DELAY);
}

bool erase_application() {
	FLASH_EraseInitTypeDef eraseConfig;
	eraseConfig.TypeErase = FLASH_TYPEERASE_PAGES;
	eraseConfig.Banks = FLASH_BANK_1;
	eraseConfig.Page = 16;
	eraseConfig.NbPages = 240;

	uint32_t pageError = 0;
	printf("Erasing bank 1...\n");
	if (HAL_FLASHEx_Erase(&eraseConfig, &pageError) != HAL_OK) {
		return false;
	}

	if (pageError != 0xFFFFFFFFU) {
		return false;
	}
	//printf("OK\n");
	eraseConfig.TypeErase = FLASH_TYPEERASE_PAGES;
	eraseConfig.Banks = FLASH_BANK_2;
	eraseConfig.Page = 256;
	eraseConfig.NbPages = 256;

	pageError = 0;
	printf("Erasing bank 2...\n");
	if (HAL_FLASHEx_Erase(&eraseConfig, &pageError) != HAL_OK) {
		return false;
	}
	//printf("OK\n");
	return pageError == 0xFFFFFFFFU;
}

bool receive_and_flash_firmware(UART_HandleTypeDef *const uart,
		uint32_t const firmwareSize) {
	if (firmwareSize == 0) {
		return false;
	}

	if (HAL_FLASH_Unlock() != HAL_OK) {
		return false;
	}

	if (!erase_application()) {
		HAL_FLASH_Lock();
		return false;
	}

	respond_ok(uart);
	uint32_t bytesProgrammed = 0;

	while (bytesProgrammed < firmwareSize) {
		uint32_t const bytesLeft = firmwareSize - bytesProgrammed;
		uint32_t const bytesToReceive = (
				bytesLeft > BOOT_BUFFER_SIZE ?
						BOOT_BUFFER_SIZE : bytesLeft);

		if (HAL_UART_Receive(uart, bootladerBuffer, bytesToReceive, 5000)
				!= HAL_OK) {
			HAL_FLASH_Lock();
			printf("Error during flashing!!!!\n");
			respond_err(uart);
			return false;
		}

		if (!flash_and_verify(bootladerBuffer, bytesToReceive,
				bytesProgrammed)) {
			HAL_FLASH_Lock();
			return false;
		}
		bytesProgrammed += bytesToReceive;
		respond_ok(uart);
	}

	return true;
}

bool flash_and_verify(uint8_t const *const bytes, size_t const amount, uint32_t const offset) {
	if (amount == 0 || bytes == NULL || amount % 4 != 0) {
		return false;
	}
	for (uint32_t bytesCounter = 0; bytesCounter < amount; bytesCounter += 8) {
		uint64_t const programmingData = *(uint64_t*) (&bytes[bytesCounter]);
		uint32_t const programmingAddress = APPLICATION_ADDRESS + offset
				+ bytesCounter;
		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, programmingAddress,
				programmingData) != HAL_OK) {
			return false;
		}
		uint64_t const verificationData = *(uint64_t*) programmingAddress;
		if (verificationData != programmingData) {
			return false;
		}
	}
	return true;
}

bool verify_firmware(uint32_t firmwareSize, uint32_t expectedChecksum) {
	if (firmwareSize == 0) {
		return false;
	}
	printf("BOOT: firmwareSize: 0x%lX\n", firmwareSize);
	printf("BOOT: expected checksum: 0x%lX\n", expectedChecksum);
	uint32_t const calculatedChecksum = HAL_CRC_Calculate(&hcrc,
			(uint32_t*) APPLICATION_ADDRESS, firmwareSize / sizeof(uint32_t));
	//uint32_t const calculatedChecksum = HAL_CRC_Calculate(&hcrc, (uint32_t*)dummyProgram, firmwareSize / sizeof(uint32_t));
	printf("Checksum is 0x%lX\n", (uint32_t) calculatedChecksum);
	return (calculatedChecksum == expectedChecksum);

}
