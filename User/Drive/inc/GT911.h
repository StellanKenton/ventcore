//
// Created by lanchanghai on 2024/3/10.
//

#ifndef GD32F470_FREERTOS_GT911_H
#define GD32F470_FREERTOS_GT911_H

#include <stdbool.h>
#include "gd32f4xx.h"

extern char touch_flag;

struct GT911_Touch {
    uint16_t x;  // x坐标
    uint16_t y;  // y坐标
    uint16_t s;  // 触摸面积
};

typedef struct
{
    uint8_t Touch_State				;	//触摸状态
    uint8_t Touch_Number			;	//触摸数量
    struct GT911_Touch gt911_touch[5];
}Touch_Struct;	//触摸信息结构体

#ifdef IICADDRSETUP
#define GT911_ADDR       0X28
#else
#define GT911_ADDR        0xBA
#endif

/* GT9XXX 部分寄存器定义  */
#define GT911_CTRL_REG     0X8040      /* GT9XXX控制寄存器 */
#define GT911_PID_REG      0X8140      /* GT9XXX产品ID寄存器 */

#define GT911_GSTID_REG    0X814E      /* GT9XXX当前检测到的触摸情况 */
#define GT911_TP1_REG      0X8150      /* 第一个触摸点数据地址 */

void GT911_Init(void);
void Gt911_Scan(Touch_Struct *touch_info);


#endif  // GD32F470_FREERTOS_GT911_H
