#include "app_bootloader.h"

static volatile uint8_t app_upgrade_statu = no_upgrade;
static uint32_t to_address = 0;

/**
  * @brief  只擦除升级标志所在页，保留 CHECK 页
  * @retval HAL_OK: 成功  其他: 失败
  */
void Clear_Upgrade_Flag_Only(void)
{
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;

    HAL_FLASH_Unlock();

    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = UPGRADE_FLAG_ADDR;
    EraseInitStruct.NbPages     = 1;
    HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);

    HAL_FLASH_Lock();
    return;
}

/**
  * @brief  检查是否存在有效的升级标志（分页版）
  *         FLAG 与 CHECK 分属两页，互不影响
  */
void Check_Upgrade_Flag(void)
{
    printf("bootloader start\n");
    printf("check update\n");

    uint32_t flag  = *(__IO uint32_t*)UPGRADE_FLAG_ADDR;
    uint32_t check = *(__IO uint32_t*)UPGRADE_CHECK_ADDR;

    /* check 值无效：说明 CHECK 页未初始化或被破坏，重建 check 值 */
    if (check != UPGRADE_CHECK_VALUE)
    {
        FLASH_EraseInitTypeDef EraseInitStruct;
        uint32_t PageError = 0;

        HAL_FLASH_Unlock();

        EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
        EraseInitStruct.PageAddress = UPGRADE_CHECK_ADDR;   /* CHECK 页起始 */
        EraseInitStruct.NbPages     = 1;

        if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK)
        {
            HAL_FLASH_Lock();
            app_upgrade_statu = no_upgrade;
            return;
        }

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                              UPGRADE_CHECK_ADDR,
                              UPGRADE_CHECK_VALUE) != HAL_OK)
        {
            HAL_FLASH_Lock();
            app_upgrade_statu = no_upgrade;
            return;
        }

        HAL_FLASH_Lock();
        app_upgrade_statu = no_upgrade;
        return;   /* check 刚刚重建，本次不升级 */
    }

    /* check 有效，再看 flag */
    if (flag == UPGRADE_FLAG_VALUE)
    {
        app_upgrade_statu = _upgrade;
    }
    else
    {
        app_upgrade_statu = no_upgrade;
    }
}

/**
 * @brief 检查是否需要恢复出厂设置
 * 
 */
void App_bootloader_check_default(void)
{
    HAL_Delay(3000);
}
//key1按键回调函数（3s考虑是否回归出厂设置）
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == Key1_Pin)
    {
        app_upgrade_statu = default_set;
        // printf("default_set\n");
    }
}

/*
********************************
将w25qxx中的数据解析并写入到flash中
********************************
*/
uint8_t meta_app_buff[9] = {0};
// 程序在W25Qxx中保存的位置
uint32_t app_start_addr = 0;
// 需要写入到flash的程序大小
uint32_t app_size = 0;
// 一次能够写入1页flash的缓冲区
uint8_t flash_data_buff[2049] = {0};
//读取元数据信息
static uint8_t App_bootloader_check_meta_data(void)
{
    // 前4个字节是程序的起始地址  后4个字节是程序的大小    低位在前
    Int_W25Qxx_read_data(META_APP_ADDR_BLOCK, META_APP_ADDR_SECTOR,
         META_APP_ADDR_PAGE, META_APP_ADDR_ADDR, meta_app_buff, 8);
    app_start_addr = meta_app_buff[0] | meta_app_buff[1] << 8 
    | meta_app_buff[2] << 16 | meta_app_buff[3] << 24;
    app_size = meta_app_buff[4] | meta_app_buff[5] << 8 | meta_app_buff[6] << 16 
    | meta_app_buff[7] << 24;

    // 假设程序存储的地址不能在第一扇中  0x00 1 000
    if (app_start_addr < APP_START_ADDR_MIN)
    {
        printf("app start addr error\n");
        return 1;
    }
    if (app_size < APP_SIZE_MIN || app_size > APP_SIZE_MAX)
    {
        printf("app size error\n");
        return 1;
    }

    // 读取程序  判断头两个32位数据
    Int_W25Qxx_read_data_with_32addr(app_start_addr, meta_app_buff, 8);

    uint32_t app_stack_ptr = meta_app_buff[0] | meta_app_buff[1] << 8 | meta_app_buff[2] << 16 | meta_app_buff[3] << 24;
    uint32_t app_reset_handle = meta_app_buff[4] | meta_app_buff[5] << 8 | meta_app_buff[6] << 16 | meta_app_buff[7] << 24;

    // 1.1 校验栈顶地址
    if ((app_stack_ptr & 0xFFFF0000) != STACK_ADDR)
    {
        printf("stack addr error\n");
        return 1;
    }

    // 1.2 校验复位中断地址
    if (app_reset_handle < APPLICATION_START_ADDRESS || app_reset_handle > APPLICATION_END_ADDRESS)
    {
        printf("app_reset_handle:%x\n", app_reset_handle);
        printf("reset handle error\n");
        return 1;
    }

    return 0;
}
//直接擦除足够多的页数
static void App_flash_erase(uint8_t pages)
{
    // 直接擦除足够的页大小
    FLASH_EraseInitTypeDef EraseInitStruct;
    // 擦除单独页
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    // 擦除第1个bank的页
    EraseInitStruct.Banks = FLASH_BANK_1;//双Bank芯(如STM32H7,F4大容量型号)Flash分为Bank1/Bank2
    EraseInitStruct.PageAddress = APPLICATION_START_ADDRESS;
    // 擦除几页
    EraseInitStruct.NbPages = pages;
    uint32_t page_error = 0;
    // flash擦除比较耗费性能
    HAL_FLASHEx_Erase(&EraseInitStruct, &page_error);
}
//程序写入到flash中
static void App_bootloader_write_app_flash(void)
{
    // 1. 读取元数据信息  =>  描述后续的程序
    // 2. 校验程序
    if(App_bootloader_check_meta_data() == 1)
    {
        return;
    }
    // 3. 写入程序
    // 解锁flash
    HAL_FLASH_Unlock();
    // 3.1 擦除足够的flash区域
    App_flash_erase((app_size / FLASH_PAGE_SIZE) + 1);
    // 3.2 读出1页的内容
    // 3.3 写入到flash中
    // 剩余程序的大小
    uint32_t app_size_left = app_size;
    uint16_t data_tmp = 0;
    uint32_t write_data_size;

    while (app_size_left >= FLASH_PAGE_SIZE)
    {
        // 已经写入的数据大小
        write_data_size = app_size - app_size_left;
        // 程序剩下的大小大于1页  => 调用的地址是W25Q32的地址
        Int_W25Qxx_read_data_with_32addr(app_start_addr + write_data_size, flash_data_buff, FLASH_PAGE_SIZE);
        app_size_left -= FLASH_PAGE_SIZE;
        // 3.4 写入1页数据到flash中
        for (uint16_t i = 0; i < FLASH_PAGE_SIZE; i += 2)
        {
            if (i + 1 < FLASH_PAGE_SIZE)
            {
                data_tmp = flash_data_buff[i] | flash_data_buff[i + 1] << 8;
                // 写入到flash的地址
                HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, APPLICATION_START_ADDRESS
                     + write_data_size + i, data_tmp);
            }
        }
    }
    // 写入最后一页
    if (app_size_left > 0)
    {
        write_data_size = app_size - app_size_left;
        // 3.5 读出W25Q32中剩下的程序
        Int_W25Qxx_read_data_with_32addr(app_start_addr + write_data_size, flash_data_buff, app_size_left);
        // 3.6 将剩余的程序写入到最后一页flash中
        for (uint16_t i = 0; i < app_size_left; i += 2)
        {
            if (i + 1 < app_size_left)
            {
                data_tmp = flash_data_buff[i] | flash_data_buff[i + 1] << 8;
                // 写入到flash的地址
                HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, APPLICATION_START_ADDRESS
                     + write_data_size + i, data_tmp);
            }
        }
    }
    printf("write app flash success:%d,%d\n",app_size,app_size_left);

    // 2. 上锁flash
    HAL_FLASH_Lock();
}

/**
 * @brief 执行更新操作
 *
 */
void App_bootloader_update(void)
{
    if(app_upgrade_statu == no_upgrade)
    {
        printf("no_upgrade\r\n");
        printf("jump_to_app\r\n");
        to_address = APPLICATION_START_ADDRESS;
        return;
    }
    if(app_upgrade_statu == _upgrade)
    {
        // 擦除更新标志位
        Clear_Upgrade_Flag_Only();
        // 将W25Q32中的程序写入到flash中
        printf("update\n");
        printf("jump_to_app\r\n");
        App_bootloader_write_app_flash();
        to_address = APPLICATION_START_ADDRESS;
    }
    if(app_upgrade_statu == default_set)
    {
        printf("default_set\n");
        printf("jump_to_default\r\n");
        to_address = DefaultApp_START_ADDRESS;
    }
}

/**
 * @brief 执行跳转操作
 *
 */
void App_bootloader_jump_app(void)
{
    if(Int_bootloader_jump_to_app(to_address)==0)
    {
        printf("jump success\n");
    }
    else
    {
        printf("jump error\n");
    }
}
