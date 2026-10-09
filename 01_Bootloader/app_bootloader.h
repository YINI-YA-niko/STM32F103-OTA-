#ifndef APP_BOOTLOADER_H
#define APP_BOOTLOADER_H

#include "int_bootloader.h"
#include "W25Qxx.h"
/* 升级标志相关定义 */
#define UPGRADE_FLAG_ADDR      0x0800F000   // 第 1 页
#define UPGRADE_CHECK_ADDR     0x0800F400   // 第 2 页，跨页
#define UPGRADE_FLAG_VALUE     0x5A5A5A5A
#define UPGRADE_CHECK_VALUE    0x1A2B3C4D

// 元数据信息的地址
#define META_APP_ADDR_BLOCK 0X00
#define META_APP_ADDR_SECTOR 0X00
#define META_APP_ADDR_PAGE 0X00
#define META_APP_ADDR_ADDR 0X00

// 程序存储的判断条件
#define APP_START_ADDR_MIN 0X001000
#define APP_SIZE_MIN 500
#define APP_SIZE_MAX (22*1024)

#define no_upgrade 0
#define default_set 2
#define _upgrade   1
/*
 * 判断当前是否需要进行更新
 */
void Check_Upgrade_Flag(void);

/**
 * @brief 检查是否需要恢复出厂设置
 * 
 */
void App_bootloader_check_default(void);

/**
 * @brief 执行更新操作
 *
 */
void App_bootloader_update(void);

/**
 * @brief 执行跳转操作
 *
 */
void App_bootloader_jump_app(void);
#endif 
