#include "App_bootloader.h"

uint8_t app_rec_start_buff[64] = {0};
uint16_t app_rec_start_len = 0;

// 记录接收程序的总长度
uint32_t app_rec_total_len = 0;

// 记录上次接收的时间
extern uint32_t last_rec_time;
// 记录接收实际数据的长度
extern uint16_t uart_rec_full_len;

// 记录当前应用层的状态
Bootloader_status boot_status = BOOTLOADER_STATUS_INIT;

/**
 * @brief 初始化bootloader => 打印日志启动
 *
 */
void App_bootloader_init(void)
{
    printf("app transmission start\r\n");
    printf("wait user send data\r\n");
    printf("send 'start:len' to start\r\n");
    boot_status = BOOTLOADER_STATUS_INIT;
}

/**
 * @brief 等待用户传输确认
 * 如果发送的指令不对  不需要重启程序  重新发送start:len启动命令即可
 *
 */
void App_bootloader_run(void)
{
    // 使用非中断方式接收 => 区分接收程序
    // 挂起等待接收  一直等待接收到buff满 或者 收到idle空闲帧
    HAL_UARTEx_ReceiveToIdle(&huart1, app_rec_start_buff, 64, &app_rec_start_len, 0xffffff);
    if (app_rec_start_len > 0)
    {
        // 判断数据中是否包含start:len
        char *start_str = strstr((char *)app_rec_start_buff, "start:");
        if (start_str != NULL)
        {
            // 保存len的值
            app_rec_total_len = atoi((char *)start_str + 6);
            if (app_rec_total_len > 0)
            {
                printf("app len: %d\r\n", app_rec_total_len);
                // 修改状态到下一个阶段
                boot_status = BOOTLOADER_STATUS_RUN;
            }
            else
            {
                printf("len error\r\n");
            }
        }
        else
        {
            printf("data error\r\n");
        }
    }
}

/**
 * @brief 接收数据
 *
 */
void App_bootloader_rec_data(void)
{
    // 接收完成之后 修改状态为check_data
    //(1) 软件方式 从接收到一次程序开始算   从idle空闲帧开始算等待2s
    if (last_rec_time != 0 && (HAL_GetTick() - last_rec_time > 2000))
    {
        // 已经2s中没有接收到数据了
        boot_status = BOOTLOADER_STATUS_CHECK_DATA;
    }
}

/**
 * @brief 已经传输完成 校验数据
 *  uint8_t: 0 校验通过 1 校验失败
 */
uint8_t App_bootloader_check_data(void)
{
    if (uart_rec_full_len == app_rec_total_len)
    {
        // 说明长度一致 没问题
        printf("app rec ok\r\n");
        boot_status = BOOTLOADER_STATUS_REC_DONE;
        return 0;
    }
    else
    {
        printf("rec_len %d\r\n",uart_rec_full_len);
        printf("app rec error or timeout\r\n");
    }
    return 1;
}

/**
 * @brief 在main方法的while循环中调用
 *
 */
void App_bootloader_work(void)
{
    switch (boot_status)
    {
    case BOOTLOADER_STATUS_INIT:
        // 等待用户传输确认
        App_bootloader_run();
        break;
    case BOOTLOADER_STATUS_RUN:
        // 接收数据的准备工作
        // 确认要写入flash => 提前擦除flash页  直接先擦除20页20k
        Int_bootloader_erase_flash(APP_START_ADDR, 20);
        printf("flash erase ok\n");
        printf("ready to receive app\n");
        boot_status = BOOTLOADER_STATUS_REC_DATA;
        Int_bootloader_receive_app();
        break;
    case BOOTLOADER_STATUS_REC_DATA:
        // 等待接收完成
        App_bootloader_rec_data();
        break;
    case BOOTLOADER_STATUS_CHECK_DATA:
        // 检查数据
        App_bootloader_check_data();
        break;
    default:
        break;
    }
}
