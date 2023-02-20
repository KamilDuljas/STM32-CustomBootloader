/*
 * ioputchar.h
 *
 *  Created on: Feb 16, 2023
 *      Author: kduljas
 */

#ifndef INC_IOPUTCHAR_H_
#define INC_IOPUTCHAR_H_
#include "usart.h"
UART_HandleTypeDef *mainUart = NULL;
int __io_putchar(int ch)
{
  if (ch == '\n') {
    __io_putchar('\r');
  }

  if(mainUart == NULL)
  {
	  HAL_UART_Transmit(&huart2, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
  }
  else
  {
	  HAL_UART_Transmit(mainUart, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
  }

  return 1;
}

#endif /* INC_IOPUTCHAR_H_ */
