#ifndef __INT_BOOTLOADER_H
#define __INT_BOOTLOADER_H

#include "usart.h"
#include "stdlib.h"
#include "string.h"
#define BOOTLOADER_UART_REC_BUFF_LEN 512

// 上位机也将程序存储到0x8008000的位置  后续读取flash将程序发送给开发板 进行程序更新
#define APP_START_ADDR 0x08009800
#define APP_END_ADDR 0x0800F000
#define STACK_ADDR 0X20000000

#define App_Rec_LENGTH 5028

/**
 * @brief 串口接收 => 准备接收A程序
 *
 */
void Int_bootloader_receive_app(void);

/**
 * @brief 外部可调用 提前擦除flash空间
 *
 * @param page_addr
 * @param pages
 */
void Int_bootloader_erase_flash(uint32_t page_addr, uint16_t pages);

#endif
