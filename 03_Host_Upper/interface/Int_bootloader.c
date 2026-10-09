#include "Int_bootloader.h"

/**
 * @brief
 * (1) 解决性能问题最简单直白的方法: 加钱  加内存
 * (2) 换一个高速稳定的协议  USART协议高速不稳定
 * (3) usart 降低波特率
 */

// 接收程序的缓冲区
uint8_t uart_rec_buff[BOOTLOADER_UART_REC_BUFF_LEN] = {0};
uint16_t uart_rec_len = 0;
uint16_t uart_rec_full_len = 0;

// 记录当前写入程序的偏移量
uint32_t flash_write_offset = 0;
// 记录当前一次接收数据的时间
uint32_t last_rec_time = 0;
// 末尾可能出现的单独字节
uint8_t last_byte_flag = 0;
uint8_t last_byte = 0;

static void Int_flash_erase(void)
{
    uint8_t is_erase = 0;
    uint32_t page_addr = 0;
    for (uint16_t i = 0; i < uart_rec_len; i++)
    {
        // 读取每一个位置的值
        uint8_t data = *(volatile uint8_t *)(APP_START_ADDR + i + flash_write_offset);
        if (data != 0xff)
        {
            // printf("erase:%d,%d,%c", i, flash_write_offset, data);
            is_erase = 1;
            // 记录当前页的起始地址
            page_addr = (APP_START_ADDR + i + flash_write_offset) - (APP_START_ADDR + i + flash_write_offset) % FLASH_PAGE_SIZE;
            break;
        }
    }
    // 2.2 如果需要擦除  则擦除当前页
    if (is_erase)
    {
        FLASH_EraseInitTypeDef erase_init;
        // 擦除单独页
        erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
        // 擦除第1个bank的页
        erase_init.Banks = FLASH_BANK_1;
        // 擦除页的起始地址
        erase_init.PageAddress = page_addr;
        // 擦除几页
        erase_init.NbPages = 1;
        uint32_t page_error = 0;
        // flash擦除比较耗费性能
        HAL_FLASHEx_Erase(&erase_init, &page_error);
    }
}

static void Int_flash_write_with_last(void)
{
    for (uint16_t i = 0; i < uart_rec_len; i += 2)
    {
        uint32_t flash_addr = APP_START_ADDR + i + flash_write_offset;
        uint16_t data16;
        if (i == 0)
        {
            // 拼接上一次的字节
            data16 = last_byte | (uart_rec_buff[i] << 8);
        }
        else
        {
            data16 = uart_rec_buff[i - 1] | (uart_rec_buff[i] << 8);
        }
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, flash_addr, data16);
    }
}

static void Int_flash_write_no_last(void)
{
    // 正好能够写入 => 不再有遗留的字节  0   6
    for (uint16_t i = 0; i < uart_rec_len; i += 2)
    {
        uint32_t flash_addr = APP_START_ADDR + i + flash_write_offset;
        uint16_t data16;
        if (i + 1 < uart_rec_len)
        {
            data16 = uart_rec_buff[i] | (uart_rec_buff[i + 1] << 8);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, flash_addr, data16);
        }
    }
}

static void Int_flash_write_halfword(void)
{
    // 本次之后没剩余
    if ((uart_rec_len + last_byte_flag) % 2 == 0)
    {
        if (last_byte_flag)
        {
            // 有剩余 数据长度是奇数 5 => 这次需要作为第一个字节写入  1  5
            Int_flash_write_with_last();
            // 2.4 记录偏移量
            flash_write_offset += uart_rec_len + 1;
        }
        else
        {
            // 无剩余  数据长度是偶数 => 不再有遗留的字节  0   6
            Int_flash_write_no_last();
            // 2.4 记录偏移量
            flash_write_offset += uart_rec_len;
        }
        last_byte_flag = 0;
    }
    // 本次之后有剩余
    else
    {
        if (last_byte_flag)
        {
            // 有剩余 数据长度是偶数
            Int_flash_write_with_last();
            // 修改最后剩下的字节
            last_byte = uart_rec_buff[uart_rec_len - 1];
            // 2.4 记录偏移量
            flash_write_offset += uart_rec_len;
        }
        else
        {
            // 上次没有遗留字节  这次会留下一个
            Int_flash_write_no_last();

            last_byte = uart_rec_buff[uart_rec_len - 1];
            // 2.4 记录偏移量
            flash_write_offset += uart_rec_len - 1;
        }
        last_byte_flag = 1;
    }
}

/**
 * @brief 串口开启中断接收之后  触发空闲帧时使用的回调函数
 * 总长度  需要接收3936字节  串口协议稳定性差 发送长文件的时候 容易丢失字节
 *  修改波特率能够提升稳定性  => 高波特率性能比较高
 * ***HAL串口代码比较繁琐  如果在中断回调函数中调用串口输出 会非常占用资源***
 * @param huart
 * @param Size
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        // 接收到数据 记录当前的STM32系统时间  单位ms
        last_rec_time = HAL_GetTick();

        // 保存实际接收的数据长度
        uart_rec_len = Size;
        uart_rec_full_len += uart_rec_len;

        // 将接收的数据写入到flash
        // 1. 解锁flash
        HAL_FLASH_Unlock();

        // 2. 判断当前写入的地址是否为新的一页 => 需要擦除
        // 2.1 遍历需要写入的地址  长度为当前接收的数据长度  如果全部内容都是0xff 则说明已经擦除过了
        // 擦除一页需要的时候 20-30ms
        Int_flash_erase();

        // 2.3 使用16位写入 => 比较贴近实际情况
        // flash写入一次16位  40us*256 = 10ms
        Int_flash_write_halfword();

        // 3. 重新加锁
        HAL_FLASH_Lock();

        // 使用完数据之后  清空 准备下一次的接收
        memset(uart_rec_buff, 0, BOOTLOADER_UART_REC_BUFF_LEN);
        // 清空掉初始化串口使用之前的所有问题
        __HAL_UART_CLEAR_OREFLAG(&huart1);
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);
        HAL_UARTEx_ReceiveToIdle_IT(&huart1, uart_rec_buff, BOOTLOADER_UART_REC_BUFF_LEN);
    }
}

/**
 * @brief 串口接收 => 准备接收A程序
 *
 */
void Int_bootloader_receive_app(void)
{
    // 清空掉初始化串口使用之前的所有问题
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    // 带有中断的串口接收函数
    // 少一个参数 => 超时时间  因为IT带中断的函数方法是异步执行的
    HAL_UARTEx_ReceiveToIdle_IT(&huart1, uart_rec_buff, BOOTLOADER_UART_REC_BUFF_LEN);
}

/**
 * @brief 外部可调用 提前擦除flash空间
 *
 * @param page_addr
 * @param pages
 */
void Int_bootloader_erase_flash(uint32_t page_addr, uint16_t pages)
{
    // 解锁flash
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase_init;
    // 擦除单独页
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    // 擦除第1个bank的页
    erase_init.Banks = FLASH_BANK_1;
    // 擦除页的起始地址
    erase_init.PageAddress = page_addr;
    // 擦除几页
    erase_init.NbPages = pages;
    uint32_t page_error = 0;
    // flash擦除比较耗费性能
    HAL_FLASHEx_Erase(&erase_init, &page_error);
    // 加锁flash
    HAL_FLASH_Lock();
}
