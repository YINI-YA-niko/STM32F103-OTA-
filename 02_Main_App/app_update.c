#include "app_update.h"

uint8_t uart_rec_buff[BOOTLOADER_UART_REC_BUFF_LEN] = {0};

Update_State_t update_state = UPDATE_IDLE;

// 声明一个能够容纳整个程序的静态缓存  BSS断里面  SRAM空间
uint8_t app_data_buff[APP_DATA_MAX_LEN] = {0};

// can接收消息的缓冲区
CAN_Rec_MSG can_rec_msg[3] = {0};
// 单次接收消息的条数
uint8_t can_rec_msg_cnt = 0;
// 接收程序的长度
uint16_t can_rec_msg_len = 0;
// 记录当前一次接收的时间
uint32_t can_rec_time = 0;

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    // 串口接收到了数据 -> 如果是cmd
    if ((huart->Instance == USART1) && (update_state == UPDATE_IDLE))
    {
        // 校验数据 => cmd  => 让开发板给网关发送更新指令  使用CAN发送
        if (strstr((char *)uart_rec_buff, "cmd"))
        {
            update_state = UPDATE_RECV_SEND_CMD;
            // 添加回滚逻辑 => 如果校验失败  还能重新接收cmd
            //  清空掉初始化串口使用之前的所有问题
            __HAL_UART_CLEAR_OREFLAG(&huart1);
            __HAL_UART_CLEAR_IDLEFLAG(&huart1);
            // 带有中断的串口接收函数
            // 少一个参数 => 超时时间  因为IT带中断的函数方法是异步执行的
            HAL_UARTEx_ReceiveToIdle_IT(&huart1, uart_rec_buff, BOOTLOADER_UART_REC_BUFF_LEN);
        }
    }
}

//闪灯表示正在运行
void App_run(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_Delay(500);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
}

/**
 * @brief 接收串口数据  =>  收到更新标记 => 发送CAN的更新指令
 *
 */
void App_update_init(void)
{
    // 启动串口的接收程序
    //  清空掉初始化串口使用之前的所有问题
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    // 带有中断的串口接收函数
    // 少一个参数 => 超时时间  因为IT带中断的函数方法是异步执行的
    HAL_UARTEx_ReceiveToIdle_IT(&huart1, uart_rec_buff, BOOTLOADER_UART_REC_BUFF_LEN);
    Int_CAN_init();
    printf("app go gO GO");
    printf("if need update");
    printf("please send cmd");
}

/**
 * @brief 使用CAN发送更新指令
 *
 */
void App_update_send_update_cmd(void)
{
    Int_CAN_send(CAN_UPDATE_CMD_ID, APP_UPDATE_CMD, APP_UPDATE_CMD_LEN);
    // 更新命令已经发送  修改状态外接收程序
    update_state = UPDATE_RECV_DATA;
}

/**
 * @brief CAN接收程序数据  保存到W25Q32
 *
 */
void App_update_receive_app_data(void)
{
    Int_CAN_receive_msg(can_rec_msg, &can_rec_msg_cnt);
    for (uint8_t i = 0; i < can_rec_msg_cnt; i++)
    {
        can_rec_time = HAL_GetTick();
        // 将数据缓存到app_data_buff中
        memcpy(app_data_buff + can_rec_msg_len, can_rec_msg[i].data, can_rec_msg[i].txHeader.DLC);
        // 记录接收程序的长度
        can_rec_msg_len += can_rec_msg[i].txHeader.DLC;
    }
    can_rec_msg_cnt = 0;
    // 判断接收完成
    if (can_rec_time != 0 && (can_rec_time + 2000 < HAL_GetTick()))
    {
        // 已经断开发送数据2s
        // 打印接收长度
        printf("can_rec_msg_len:%d\n", can_rec_msg_len);
        update_state = UPDATE_RECV_CHECK_DATA;
    }
}

//crc校验
static uint32_t App_crc_cal(uint8_t *data, uint16_t len)
{
    uint32_t *p_data = (uint32_t *)data;
    uint32_t word_count = (len + 3) / 4;

    // 复位crc
    __HAL_CRC_DR_RESET(&hcrc);

    uint32_t crc_val = HAL_CRC_Calculate(&hcrc, p_data, word_count);
    return crc_val;
}

/**
 * @brief 添加校验逻辑
 *
 */
void App_update_check_data(void)
{
    Int_CAN_receive_msg(can_rec_msg, &can_rec_msg_cnt);
    for (uint8_t i = 0; i < can_rec_msg_cnt; i++)
    {
        // 读取发送过来的crc值  发送crc值 四字节 低位在前
        uint32_t rec_crc_val = can_rec_msg[i].data[0] | (can_rec_msg[i].data[1] << 8) | (can_rec_msg[i].data[2] << 16) | (can_rec_msg[i].data[3] << 24);
        uint32_t cru_crc_val = App_crc_cal(app_data_buff, can_rec_msg_len);
        if (rec_crc_val == cru_crc_val)
        {
            // 校验通过
            printf("crc check pass\r\n");
            update_state = UPDATE_RECV_BOOT_UPDATE;
        }
        else
        {
            // 校验没通过  => 回滚到idle状态
            printf("crc check fail\r\n");
            // 清空缓存和状态
            memset(app_data_buff, 0, APP_DATA_MAX_LEN);
            can_rec_msg_len = 0;
            can_rec_time = 0;
            // 回滚到发送更新指令
            update_state = UPDATE_IDLE;
        }
    }
}
//写入W25QXX的元数据
uint8_t w25q32_write_buff[8] = {0};
/**
  * @brief  在FLASH(0X0800F000)中的更新标志位（分页版：FLAG 一页，CHECK 一页）
  * 
  */
void App_update_change_boot_mode(void)
{
    // 1. 将程序写入到外置flash  w25q32
    // 1.1 擦除正确的区域和足够的空间
    uint16_t sector_erase_count = (can_rec_msg_len / 4096) + 2;

    for (uint8_t i = 0; i < sector_erase_count; i++)
    {
        Int_W25Qxx_erase_sector(0, i);
    }

    // 1.2 写入元数据
    w25q32_write_buff[0] = (FLASH_APP_ADDR & 0xff);
    w25q32_write_buff[1] = ((FLASH_APP_ADDR >> 8) & 0xff);
    w25q32_write_buff[2] = ((FLASH_APP_ADDR >> 16) & 0xff);
    w25q32_write_buff[3] = ((FLASH_APP_ADDR >> 24) & 0xff);
    w25q32_write_buff[4] = ((can_rec_msg_len) & 0xff);
    w25q32_write_buff[5] = ((can_rec_msg_len >> 8) & 0xff);
    w25q32_write_buff[6] = ((can_rec_msg_len >> 16) & 0xff);
    w25q32_write_buff[7] = ((can_rec_msg_len >> 24) & 0xff);
    Int_W25Qxx_write_data_32(FLASH_META_ADDR, w25q32_write_buff, 8);
    uint16_t write_len = 0;
    uint16_t write_tmp_len = 0;
    // 1.3 按照页 将程序写入到w25q32中
    while (write_len < can_rec_msg_len)
    {
        // 剩下的长度是否超过1页
        if (can_rec_msg_len - write_len > 256)
        {
            write_tmp_len = 256;
        }
        else
        {
            write_tmp_len = can_rec_msg_len - write_len;
        }
        Int_W25Qxx_write_data_32(FLASH_APP_ADDR + write_len, app_data_buff + write_len, write_tmp_len);
        write_len += write_tmp_len;
    }

    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;
    HAL_FLASH_Unlock();
    /* 擦除 FLAG 页（只擦这一页，CHECK 页不动） */
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = UPGRADE_FLAG_ADDR;   /* FLAG 页起始 */
    EraseInitStruct.NbPages     = 1;

    HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);
    /* 写 FLAG 值 */
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                               UPGRADE_FLAG_ADDR,
                               UPGRADE_FLAG_VALUE);

    HAL_FLASH_Lock();
    // 修改状态
    update_state = UPDATE_END;
}

/**
 * @brief 循环调用 执行状态机逻辑
 *
 */
void App_update_work(void)
{
    switch (update_state)
    {
    case UPDATE_IDLE:
        // 只有不需要进行更新程序的时候  才会运行程序之前的功能
        App_run();
        break;
    case UPDATE_RECV_SEND_CMD:
        printf("recv cmd\r\n");
        printf("send cmd\r\n");
        App_update_send_update_cmd();
        // 在接受数据之前  清空缓冲区
        memset(app_data_buff, 0, APP_DATA_MAX_LEN);
        can_rec_msg_len = 0;
        break;
    case UPDATE_RECV_DATA:
        App_update_receive_app_data();
        break;
    case UPDATE_RECV_CHECK_DATA:
        App_update_check_data();
        break;
    case UPDATE_RECV_BOOT_UPDATE:
        App_update_change_boot_mode();
        break;
    case UPDATE_END:
        // 延时 => 重启
        HAL_Delay(1000);
        HAL_NVIC_SystemReset();
        break;
    default:
        break;
    }
}
