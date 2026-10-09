#ifndef __APP_UPDATE__
#define __APP_UPDATE__

#include "usart.h"
#include "Int_can.h"
#include "crc.h"
#include "W25Qxx.h"

#define BOOTLOADER_UART_REC_BUFF_LEN 32

#define CAN_UPDATE_CMD_ID 0
// 更新指令
#define APP_UPDATE_CMD "qwe"
#define APP_UPDATE_CMD_LEN 3

// 存储更新状态的位置
#define CHECK_UPDATE_ADDR 0x10
// 更新状态的值
#define BOOT_UPDATE 0X01
#define BOOT_NO_UPDATE 0X02
// 添加校验的秘钥
#define CHECK_KEY_ADDR 0X11
#define CHECK_KEY 0X5A6B

/* 升级标志相关定义 */
#define UPGRADE_FLAG_ADDR      0x0800F000   // 第 1 页
#define UPGRADE_CHECK_ADDR     0x0800F400   // 第 2 页，跨页
#define UPGRADE_FLAG_VALUE     0x5A5A5A5A
#define UPGRADE_CHECK_VALUE    0x1A2B3C4D

// 10k缓冲区 接收整个程序
#define APP_DATA_MAX_LEN 10240

// w25q32 存放元数据的地址  存放到第0扇
#define FLASH_META_ADDR 0x000000

// w25q32 存放程序的地址  存放到第一扇
#define FLASH_APP_ADDR 0x001000

// 程序状态机
typedef enum
{
    UPDATE_IDLE = 0,
    UPDATE_RECV_SEND_CMD,
    UPDATE_RECV_DATA,
    UPDATE_RECV_CHECK_DATA,
    UPDATE_RECV_BOOT_UPDATE,
    UPDATE_END
} Update_State_t;

/**
 * @brief 接收串口数据  =>  收到更新标记 => 发送CAN的更新指令
 *
 */
void App_update_init(void);

/**
 * @brief 循环调用 执行状态机逻辑
 *
 */
void App_update_work(void);

#endif // __APP_UPDATE__
