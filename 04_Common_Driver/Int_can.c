#include "Int_can.h"

/**
 * @brief 配置白名单过滤器  手动开启can
 *
 */
void Int_CAN_init(void)
{
    // 1. 配置过滤器
    CAN_FilterTypeDef filterConfig = {0};
    filterConfig.FilterBank = 0;                      // 选择过滤器编号  0-13 一共14个过滤器
    filterConfig.FilterMode = CAN_FILTERMODE_IDMASK;  // 选择使用掩码模式 模糊匹配
    filterConfig.FilterScale = CAN_FILTERSCALE_32BIT; // 选择使用的位数  32位
    // 填写寄存器  => 只要id为1的数据
    filterConfig.FilterIdHigh = 0x0020;
    filterConfig.FilterIdLow = 0x0000;
    // 只要掩码都是0  表示ID不需要匹配上   掩码哪一位是1  表示哪一位需要匹配上
    filterConfig.FilterMaskIdHigh = 0xffe0;
    filterConfig.FilterMaskIdLow = 0x0000;
    // 使用哪一个接收队列
    filterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    // 使能过滤器
    filterConfig.FilterActivation = ENABLE;

    HAL_CAN_ConfigFilter(&hcan, &filterConfig);

    // 2. 手动开启CAN
    HAL_CAN_Start(&hcan);
}

/**
 * @brief 发送消息
 * id : 消息ID
 * data : 消息数据
 * len : 消息长度
 *
 */
void Int_CAN_send(uint16_t id, uint8_t *data, uint8_t len)
{
    // 等待发送邮箱空闲
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0)
        ;

    // 将发送的消息添加到发送邮箱
    CAN_TxHeaderTypeDef txHeader = {0};
    txHeader.StdId = id;         // 标准消息ID
    txHeader.IDE = CAN_ID_STD;   // 标准格式
    txHeader.RTR = CAN_RTR_DATA; // 数据帧
    txHeader.DLC = len;          // 数据长度
    uint32_t mailbox = 0;
    // 1. 句柄 CAN编号 2. CAN消息的头信息 3. 数据 4. 邮箱编号
    HAL_CAN_AddTxMessage(&hcan, &txHeader, data, &mailbox);
}

/**
 * @brief 接收消息
 *
 * @param rec_msg
 * @param msg_count
 */
void Int_CAN_receive_msg(CAN_Rec_MSG *rec_msg, uint8_t *msg_count)
{
    *msg_count = HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0);
    for (uint8_t i = 0; i < *msg_count; i++)
    {
        // 指向当前使用的缓存
        CAN_Rec_MSG *rec_msg_temp = &rec_msg[i];
        // 清空对应消息缓存
        memset(rec_msg_temp,0,sizeof(CAN_Rec_MSG));
        // 读取一条消息
        HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0,&(rec_msg_temp->txHeader),rec_msg_temp->data);
    }
}
