#ifndef _W25QXX_H_
#define _W25QXX_H_

#include "spi.h"

#define W25QXX_READ_ID 0x9F
#define W25Qxx_WRITE_ENABLE 0x06
#define W25Qxx_READ_STATUS_REG 0x05
#define W25Qxx_READ_DATA 0x03
#define W25Qxx_PAGE_PROGRAM 0x02
#define W25Qxx_SECTOR_ERASE 0x20

//拉低cs选择w25qxx

#define W25QXX_CS_LOW() HAL_GPIO_WritePin(W_SPI_cs_Pin_GPIO_Port, W_SPI_cs_Pin_Pin, GPIO_PIN_RESET)
//拉高cs取消w25qxx
#define W25QXX_CS_HIGH() HAL_GPIO_WritePin(W_SPI_cs_Pin_GPIO_Port, W_SPI_cs_Pin_Pin, GPIO_PIN_SET)
/*
* 读取W25Qxx数据
*/
uint8_t Int_W25Qxx_read_byte(void);
/*
* 写入W25Qxx数据
*/
void Int_W25Qxx_write_byte(uint8_t data);
/*
* 读取W25QxxID
*/
void W25Qxx_ReadID(uint8_t *mf, uint16_t *id);

/**
 * @brief 读取数据
 *
 * addr: 一共是22位  0x000000 -> 0x3FF   FFF  一次擦除4096字节  一次写入是256字节
 */
// void Int_W25Qxx_read_data(uint32_t addr, uint8_t *data, uint16_t len);
void Int_W25Qxx_read_data(uint8_t block,uint8_t sector,uint8_t page,uint8_t addr, uint8_t *data, uint16_t len);

/**
 * @brief 读取数据 使用32位地址
 * 
 * @param addr 
 * @param data 
 * @param len 
 */
void Int_W25Qxx_read_data_with_32addr(uint32_t addr, uint8_t *data, uint16_t len);

/**
 * @brief 写入数据
 * 
 */
void Int_W25Qxx_write_data(uint8_t block,uint8_t sector,uint8_t page,uint8_t addr, uint8_t *data, uint16_t len);
void Int_W25Qxx_write_data_32(uint32_t addr_24, uint8_t *data, uint16_t len);
/**
 * @brief 擦除1扇区域
 *  
 */
void Int_W25Qxx_erase_sector(uint8_t block,uint8_t sector);

// 读取状态寄存器
uint8_t W25Qxx_ReadSR(void);

#endif
