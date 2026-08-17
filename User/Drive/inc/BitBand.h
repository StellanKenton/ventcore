#ifndef GD32F470_FREERTOS_BITBAND_H
#define GD32F470_FREERTOS_BITBAND_H

#include "gd32f4xx.h"
#include "gd32f4xx_gpio.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

//IO口操作宏定义
#define BITBAND(addr, bitnum) ((addr & 0xF0000000)+0x2000000+((addr &0xFFFFF)<<5)+(bitnum<<2))
#define MEM_ADDR(addr)  *((volatile unsigned long  *)(addr))
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum))
//IO口地址映射
#define GPIOA_OCTL_Addr    (GPIOA+0x14) //0x40020014
#define GPIOB_OCTL_Addr    (GPIOB+0x14) //0x40020414
#define GPIOC_OCTL_Addr    (GPIOC+0x14) //0x40020814
#define GPIOD_OCTL_Addr    (GPIOD+0x14) //0x40020C14
#define GPIOE_OCTL_Addr    (GPIOE+0x14) //0x40021014
#define GPIOF_OCTL_Addr    (GPIOF+0x14) //0x40021414
#define GPIOG_OCTL_Addr    (GPIOG+0x14) //0x40021814
#define GPIOH_OCTL_Addr    (GPIOH+0x14) //0x40021C14
#define GPIOI_OCTL_Addr    (GPIOI+0x14) //0x40022014

#define GPIOA_ISTAT_Addr    (GPIOA+0x10) //0x40020010
#define GPIOB_ISTAT_Addr    (GPIOB+0x10) //0x40020410
#define GPIOC_ISTAT_Addr    (GPIOC+0x10) //0x40020810
#define GPIOD_ISTAT_Addr    (GPIOD+0x10) //0x40020C10
#define GPIOE_ISTAT_Addr    (GPIOE+0x10) //0x40021010
#define GPIOF_ISTAT_Addr    (GPIOF+0x10) //0x40021410
#define GPIOG_ISTAT_Addr    (GPIOG+0x10) //0x40021810
#define GPIOH_ISTAT_Addr    (GPIOH+0x10) //0x40021C10
#define GPIOI_ISTAT_Addr    (GPIOI+0x10) //0x40022010

//IO口操作,只对单一的IO口!
//确保n的值小于16!
#define PAout(n)   BIT_ADDR(GPIOA_OCTL_Addr,n)  //输出
#define PAin(n)    BIT_ADDR(GPIOA_ISTAT_Addr,n)  //输入

#define PBout(n)   BIT_ADDR(GPIOB_OCTL_Addr,n)  //输出
#define PBin(n)    BIT_ADDR(GPIOB_ISTAT_Addr,n)  //输入

#define PCout(n)   BIT_ADDR(GPIOC_OCTL_Addr,n)  //输出
#define PCin(n)    BIT_ADDR(GPIOC_ISTAT_Addr,n)  //输入

#define PDout(n)   BIT_ADDR(GPIOD_OCTL_Addr,n)  //输出
#define PDin(n)    BIT_ADDR(GPIOD_ISTAT_Addr,n)  //输入

#define PEout(n)   BIT_ADDR(GPIOE_OCTL_Addr,n)  //输出
#define PEin(n)    BIT_ADDR(GPIOE_ISTAT_Addr,n)  //输入

#define PFout(n)   BIT_ADDR(GPIOF_OCTL_Addr,n)  //输出
#define PFin(n)    BIT_ADDR(GPIOF_ISTAT_Addr,n)  //输入

#define PGout(n)   BIT_ADDR(GPIOG_OCTL_Addr,n)  //输出
#define PGin(n)    BIT_ADDR(GPIOG_ISTAT_Addr,n)  //输入

#define PHout(n)   BIT_ADDR(GPIOH_OCTL_Addr,n)  //输出
#define PHin(n)    BIT_ADDR(GPIOH_ISTAT_Addr,n)  //输入

#define PIout(n)   BIT_ADDR(GPIOI_OCTL_Addr,n)  //输出
#define PIin(n)    BIT_ADDR(GPIOI_ISTAT_Addr,n)  //输入

#endif  // GD32F470_FREERTOS_BITBAND_H
