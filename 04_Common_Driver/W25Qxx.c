#include "W25Qxx.h"

/*
* 读取W25Qxx数据
*/
uint8_t Int_W25Qxx_read_byte(void)
{
    uint8_t data;
    HAL_SPI_Receive(&hspi1, &data, 1, 1000);
    return data;
}
/*
* 写入W25Qxx数据
*/
void Int_W25Qxx_write_byte(uint8_t data)
{
    HAL_SPI_Transmit(&hspi1, &data, 1, 1000);
}
/*
* 读取W25QxxID
*/
void W25Qxx_ReadID(uint8_t *mf, uint16_t *id)
{
    //1拉低CS
    W25QXX_CS_LOW();
    //2发送读取ID指令
    Int_W25Qxx_write_byte(W25QXX_READ_ID);
    //3读取制造商ID
    *mf = Int_W25Qxx_read_byte();
    //4读取产品ID
    *id = Int_W25Qxx_read_byte();
    *id <<= 8;
    *id |= Int_W25Qxx_read_byte();
    //5拉高CS
    W25QXX_CS_HIGH();
}

// 静态方法 等待芯片忙状态
static void Int_W25Qxx_wait_busy(void)
{
    // 1. 拉低片选
    W25QXX_CS_LOW();

    // 2. 读取状态寄存器
    while (1)
    {
        Int_W25Qxx_write_byte(W25Qxx_READ_STATUS_REG);
        uint8_t status = Int_W25Qxx_read_byte();
        // 找到busy是最低位的值  为0表示不忙
        if ((status & 0x01) == 0)
        {
            break;
        }
    }

    // 3. 拉高片选
    W25QXX_CS_HIGH();
}

/**
 * @brief 读取数据
 *
 * addr: 一共是22位  0x000000 -> 0x3F  F  F  FF  一次擦除4096字节  一次写入是256字节
 */
// void Int_W25Qxx_read_data(uint32_t addr, uint8_t *data, uint16_t len);
void Int_W25Qxx_read_data(uint8_t block, uint8_t sector, uint8_t page, uint8_t addr, uint8_t *data, uint16_t len)
{
    // 1. 等待忙状态
    Int_W25Qxx_wait_busy();

    // 2. 拉低片选
    W25QXX_CS_LOW();

    // 3. 发送读取数据指令
    Int_W25Qxx_write_byte(W25Qxx_READ_DATA);
    uint32_t addr_24 = block << 16 | sector << 12 | page << 8 | addr;
    Int_W25Qxx_write_byte(addr_24 >> 16);
    Int_W25Qxx_write_byte(addr_24 >> 8);
    Int_W25Qxx_write_byte(addr_24);

    for (uint16_t i = 0; i < len; i++)
    {
        data[i] = Int_W25Qxx_read_byte();
    }
    // 4. 拉高片选
    W25QXX_CS_HIGH();
}

/**
 * @brief 读取数据 32位地址
 *
 * @param addr
 * @param data
 * @param len
 */
void Int_W25Qxx_read_data_with_32addr(uint32_t addr, uint8_t *data, uint16_t len)
{
    // 1. 等待忙状态
    Int_W25Qxx_wait_busy();

    // 2. 拉低片选
    W25QXX_CS_LOW();

    // 3. 发送读取数据指令
    Int_W25Qxx_write_byte(W25Qxx_READ_DATA);

    Int_W25Qxx_write_byte((addr >> 16) & 0xff);
    Int_W25Qxx_write_byte((addr >> 8) & 0xff);
    Int_W25Qxx_write_byte(addr & 0xff);

    for (uint16_t i = 0; i < len; i++)
    {
        data[i] = Int_W25Qxx_read_byte();
    }
    // 4. 拉高片选
    W25QXX_CS_HIGH();
}

static void Int_W25Qxx_write_enable(void)
{
    // 1. 等待忙状态
    Int_W25Qxx_wait_busy();
    // 2. 拉低片选
    W25QXX_CS_LOW();
    // 3. 发送写使能命令
    Int_W25Qxx_write_byte(W25Qxx_WRITE_ENABLE);
    // 4. 拉高片选
    W25QXX_CS_HIGH();
}

/**
 * @brief 写入数据
 * 假设地址不超出1页的范围
 */
void Int_W25Qxx_write_data(uint8_t block, uint8_t sector, uint8_t page, uint8_t addr, uint8_t *data, uint16_t len)
{
    // 1.  写使能
    Int_W25Qxx_write_enable();

    // 2. 拉低片选
    W25QXX_CS_LOW();
    uint32_t addr_24 = block << 16 | sector << 12 | page << 8 | addr;
    Int_W25Qxx_write_byte(W25Qxx_PAGE_PROGRAM);
    Int_W25Qxx_write_byte(addr_24 >> 16);
    Int_W25Qxx_write_byte(addr_24 >> 8);
    Int_W25Qxx_write_byte(addr_24);
    // 3. 写入数据
    for (uint16_t i = 0; i < len; i++)
    {
        Int_W25Qxx_write_byte(data[i]);
    }
    // 4. 拉高片选
    W25QXX_CS_HIGH();
}
void Int_W25Qxx_write_data_32(uint32_t addr_24, uint8_t *data, uint16_t len)
{
    // 1.  写使能
    Int_W25Qxx_write_enable();

    // 2. 拉低片选
    W25QXX_CS_LOW();
    Int_W25Qxx_write_byte(W25Qxx_PAGE_PROGRAM);
    Int_W25Qxx_write_byte(addr_24 >> 16);
    Int_W25Qxx_write_byte(addr_24 >> 8);
    Int_W25Qxx_write_byte(addr_24);
    // 3. 写入数据
    for (uint16_t i = 0; i < len; i++)
    {
        Int_W25Qxx_write_byte(data[i]);
    }
    // 4. 拉高片选
    W25QXX_CS_HIGH();
}


/**
 * @brief 擦除1扇区域
 *
 */
void Int_W25Qxx_erase_sector(uint8_t block, uint8_t sector)
{
    // 1. 写使能
    Int_W25Qxx_write_enable();

    // 2. 拉低片选
    W25QXX_CS_LOW();
    // 3. 发送擦除指令
    Int_W25Qxx_write_byte(W25Qxx_SECTOR_ERASE);
    // 4. 发送地址
        uint32_t addr = ((uint32_t)block << 16) | ((uint32_t)sector << 12);
        Int_W25Qxx_write_byte((addr >> 16) & 0xFF);
        Int_W25Qxx_write_byte((addr >> 8)  & 0xFF);
        Int_W25Qxx_write_byte( addr        & 0xFF);
    // 5. 拉高片选
    W25QXX_CS_HIGH();
}

// 读取状态寄存器
uint8_t W25Qxx_ReadSR(void)
{
    uint8_t sr;
    Int_W25Qxx_write_enable();
    W25QXX_CS_LOW();
    Int_W25Qxx_write_byte(W25Qxx_READ_STATUS_REG);
    sr = Int_W25Qxx_read_byte();
    W25QXX_CS_HIGH();
    return sr;
}
