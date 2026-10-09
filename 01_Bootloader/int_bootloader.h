#ifndef INT_BOOTLOADER_H
#define INT_BOOTLOADER_H

#include "main.h"
#include "usart.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define BOOTLOADER_UART_REC_BUFF_LEN 512
//出厂设置地址& 结束地址(22k)
#define DefaultApp_START_ADDRESS 0x08004000
#define DefaultApp_END_ADDRESS   0x080097FF
//程序起始地址& 结束地址(22k)
#define APPLICATION_START_ADDRESS 0x08009800
#define APPLICATION_END_ADDRESS   0x0800EFFF
//程序存储空间
#define APP_SIZE (1024*22)
//栈起始地址
#define STACK_ADDR 0X20000000

/**
 * @brief 跳转到A程序
 * uint8_t: 0:成功 1:失败
 */
uint8_t Int_bootloader_jump_to_app(uint32_t app_start_addr);

#endif
