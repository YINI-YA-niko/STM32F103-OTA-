#include "Int_bootloader.h"

/* ============================================================
 * 汇编跳转函数：把 APP 向量表的前两个字加载到 SP 和 PC
 * AC5 下 __asm 函数参数通过 r0-r3 传递，appAddr 在 r0
 * ============================================================ */
__asm void JumpToApplication(uint32_t appAddr)
{
    LDR     SP, [R0]           ; //取 [appAddr] → MSP
    LDR     PC, [R0, #4]       ; //取 [appAddr+4] → PC，跳转
}

/**
 * @brief 跳转到A程序
 * uint8_t: 0:成功 1:失败
 */
uint8_t Int_bootloader_jump_to_app(uint32_t app_start_addr)
{

    typedef void (*pFunc)(void);
    // 1. 校验
    // 栈顶地址的值
    uint32_t app_stack_ptr = *(volatile uint32_t *)(app_start_addr);
    uint32_t app_reset_handle = *(volatile uint32_t *)(app_start_addr + 4);

    // 1.1 校验栈顶地址
    if ((app_stack_ptr & 0xFFFF0000) != STACK_ADDR)
    {
        printf("stack addr error\n");
        return 1;
    }

    // 1.2 校验复位中断地址
    if (app_reset_handle < app_start_addr || app_reset_handle > (uint32_t)(app_start_addr+APP_SIZE))
    {
        printf("reset handle error\n");
        return 1;
    }

    // 2. 注销boot loader程序
    /*2.1关闭中断,关闭 SysTick,清除所有 NVIC 中断使能和挂起标志 */
    __disable_irq();
  
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    for (uint8_t i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
    HAL_RCC_DeInit();   /* 时钟树回默认 HSI */
    HAL_DeInit();       /* 复位所有外设 */
    // 2.2 设置堆栈指针
    // __set_MSP(app_stack_ptr);

    // 2.4 跳转到A程序复位中断
    // pFunc jump_to_app = (pFunc)app_reset_handle;

    // // 跳转代码之后的内容是执行不到的
    // //把存在 Flash 里的一个地址，当成函数来调用，实现“跳转到 APP”。
    // jump_to_app();

    /* --- 4. 汇编函数内完成 MSP + PC 切换 --- */
    JumpToApplication(app_start_addr);
    return 0;
}
