/*
 * boot.h
 *
 *  Created on: Feb 20, 2023
 *      Author: kduljas
 */

#ifndef INC_BOOT_H_
#define INC_BOOT_H_

#include "usart.h"
#include <stdint.h>
#include <stdio.h>

#define BOOT_BUFFER_SIZE 1024

typedef enum BootOpcode_t {
	BOOT_CMD_INVALID = 0x00,
	BOOT_CMD_ECHO = 0x01,
	BOOT_CMD_SETSIZE = 0x02,
	BOOT_CMD_UPDATE = 0x03,
	BOOT_CMD_CHECK = 0x04,
	BOOT_CMD_JUMP = 0x05,
	// TODO: Add Erease option
	// BOOT_CMD_EREASE = 0x06
} BootOpcode;

typedef struct BootCommand_t {
	uint32_t data;
	BootOpcode opcode;
} BootCommand;

BootCommand get_command(UART_HandleTypeDef* const uart, uint32_t const timeout);

void jump_to_application(uint32_t const);

bool erase_application();

bool receive_and_flash_firmware(UART_HandleTypeDef* const uart, uint32_t const firmwareSize);

bool flash_and_verify(uint8_t const* const bytes, size_t const amount, uint32_t const offset);

bool verify_firmware(uint32_t, uint32_t);

void respond_ok(UART_HandleTypeDef* const uart);

void respond_err(UART_HandleTypeDef* const uart);
#endif /* INC_BOOT_H_ */
