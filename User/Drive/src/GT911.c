//
// Created by lanchanghai on 2024/3/10.
//

#include <stdio.h>
#include "GT911.h"
#include "BitBand.h"
#include "delay.h"

char touch_flag=0;

static void GT911_GPIOInit();
static void GT911_EXTI_Init();
static void GT911_IIC_Init(void);
static void gt911_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len);
static void gt911_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len);

// PB6 LCD_SCL
// PB7 LCD_SDA
// PD11 LCD_INT
// PD12 LCD_RST

void GT911_Init(void)
{
    // GT911初始化
    GT911_GPIOInit();
    GT911_EXTI_Init(); // GT911中断初始化 中断模式
    GT911_IIC_Init();  // GT911 IIC初始化

    uint8_t buf[4];
    gt911_rd_reg(GT911_PID_REG, buf, 3);
    buf[3]='\0';
    printf("GT911 ID:%s\n",buf);

    gt911_rd_reg(0x804D, buf, 1);
    printf("0x%X\n",buf[0]);

    buf[0] = 0X02;
    gt911_wr_reg(GT911_CTRL_REG, buf, 1);    /* 软复位GT911 */
    delay_ms(10);
    buf[0] = 0X00;
    gt911_wr_reg(GT911_CTRL_REG, buf, 1);    /* 结束复位, 进入读坐标状态 */
    buf[0] = 0x00;
    gt911_wr_reg(GT911_GSTID_REG, buf, 1); /* 清标志 */
}

static void GT911_IIC_Init(void)
{
    // GT911 IIC初始化
    /* enable I2C clock */
    rcu_periph_clock_enable(RCU_I2C0);
    /* configure I2C clock */
    i2c_clock_config(I2C0, 100000, I2C_DTCY_2);
    /* configure I2C address */
    i2c_mode_addr_config(I2C0, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, GT911_ADDR);
    /* enable I2CX */
    i2c_enable(I2C0);
    /* enable acknowledge */
    i2c_ack_config(I2C0, I2C_ACK_ENABLE);

}

static void GT911_EXTI_Init(void)
{
    // GT911中断初始化
    // ...
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_SYSCFG);

    gpio_mode_set(GPIOD, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_11);  // GT911中断线
  /*  nvic_irq_enable(EXTI10_15_IRQn, 0, 3);
    syscfg_exti_line_config(EXTI_SOURCE_GPIOD, EXTI_SOURCE_PIN11);
    exti_init(EXTI_11, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_enable(EXTI_11);
    exti_interrupt_flag_clear(EXTI_11);
    touch_flag=0;*/
}

static void GT911_GPIOInit(void)
{
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOD);

    gpio_af_set(GPIOB, GPIO_AF_4, GPIO_PIN_6|GPIO_PIN_7);
    gpio_mode_set(GPIOB,GPIO_MODE_AF,GPIO_PUPD_PULLUP,GPIO_PIN_6|GPIO_PIN_7);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_6|GPIO_PIN_7);

    gpio_mode_set(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_11|GPIO_PIN_12);
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_11|GPIO_PIN_12);
    PDout(11)=0;
    PDout(12)=1;
    delay_ms(10);
    PDout(12)=0;    // GT911复位
    delay_ms(1);
#ifdef IICADDRSETUP
    PDout(11)=1;
    delay_ms(1);  // 满足时序大于100us
    PDout(12)=1;
#else
    PDout(11)=0;
    delay_ms(1);  // 满足时序大于100us
    PDout(12)=1;
#endif
    delay_ms(10);  // 满足时序大于5ms
}

/**
 * @brief       从gt911读出一次数据
 * @param       reg : 起始寄存器地址
 * @param       buf : 数据缓缓存区
 * @param       len : 读数据长度
 * @retval      无
 */
static void gt911_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    /* 等待I2C总线空闲*/
    while(i2c_flag_get(I2C0, I2C_FLAG_I2CBSY));

    /* 向 I2C 总线发送启动条件 */
    i2c_start_on_bus(I2C0);
    /* 等待直到 SBSEND 位被置位 */
    while(!i2c_flag_get(I2C0, I2C_FLAG_SBSEND));
    /* 发送从机地址到I2C总线 */
    i2c_master_addressing(I2C0, GT911_ADDR, I2C_TRANSMITTER);
   /* 等待直到 ADDSEND 位被置位 */
    while(!i2c_flag_get(I2C0, I2C_FLAG_ADDSEND));
    /* 清除 ADDSEND 位 */
    i2c_flag_clear(I2C0, I2C_FLAG_ADDSEND);
    /* 等待发送数据缓冲区为空 */
    while(!i2c_flag_get(I2C0, I2C_FLAG_TBE));

    i2c_data_transmit(I2C0, reg >> 8); // 发送高8位地址
    /* 等待直到 TBE 位被置位 */
    while(!i2c_flag_get(I2C0, I2C_FLAG_TBE));
    i2c_data_transmit(I2C0, reg & 0xFF); // 发送低8位地址
    /* 等待直到 TBE 位被置位 */
    while(!i2c_flag_get(I2C0, I2C_FLAG_TBE));

    /* 向 I2C 总线发送启动条件 */
    i2c_start_on_bus(I2C0);
    /* 等待直到 SBSEND 位被置位 */
    while(!i2c_flag_get(I2C0, I2C_FLAG_SBSEND));

    /* 发送从机地址到I2C总线 */
    i2c_master_addressing(I2C0, GT911_ADDR, I2C_RECEIVER);

    /* 等待直到 ADDSEND 位被置位 */
    while(!i2c_flag_get(I2C0, I2C_FLAG_ADDSEND));

    if(len == 1)
        i2c_ack_config(I2C0,I2C_ACK_DISABLE);

    /* 清除 ADDSEND 位 */
    i2c_flag_clear(I2C0, I2C_FLAG_ADDSEND);

    while(len > 0)
    {
        if(i2c_flag_get(I2C0, I2C_FLAG_RBNE))
        {
            if(len == 2)
                i2c_ack_config(I2C0,I2C_ACK_DISABLE);
            *buf = i2c_data_receive(I2C0);
            buf++;
            len--;
        }
    }
    /* 向 I2C 总线发送停止条件 */
    i2c_stop_on_bus(I2C0);
   /* 等待直到停止条件产生 */
    while(I2C_CTL0(I2C0) & I2C_CTL0_STOP);
    /* 启用ACK */
    i2c_ack_config(I2C0, I2C_ACK_ENABLE);
}

/**
 * @brief       向gt911写入一次数据
 * @param       reg : 起始寄存器地址
 * @param       buf : 数据缓冲区
 * @param       len : 写数据长度
 * @retval      无
 */

static void gt911_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    /* 等待I2C总线空闲*/
    while(i2c_flag_get(I2C0, I2C_FLAG_I2CBSY));
    /* 向 I2C 总线发送启动条件 */
    i2c_start_on_bus(I2C0);
    /* 等待直到 SBSEND 位被置位 */
    while(!i2c_flag_get(I2C0, I2C_FLAG_SBSEND));
    /* 发送从机地址到I2C总线 */
    i2c_master_addressing(I2C0, GT911_ADDR, I2C_TRANSMITTER);
    /* 等待直到 ADDSEND 位被置位 */
    while(!i2c_flag_get(I2C0, I2C_FLAG_ADDSEND));
    /* 清除 ADDSEND 位 */
    i2c_flag_clear(I2C0, I2C_FLAG_ADDSEND);
    /* 等待发送数据缓冲区为空 */
    while(!i2c_flag_get(I2C0, I2C_FLAG_TBE));
    /* 发送高8位地址 */
    i2c_data_transmit(I2C0, reg >> 8);
    /* 等待直到 TBE 位被置位 */
    while(!i2c_flag_get(I2C0, I2C_FLAG_TBE));
    /* 发送低8位地址 */
    i2c_data_transmit(I2C0,reg & 0xff);
    /* 等待直到 TBE 位被置位 */
    while(!i2c_flag_get(I2C0, I2C_FLAG_TBE));
    for(int i = 0; i < len; i++) {
        /* 数据传输 */
        i2c_data_transmit(I2C0, buf[i]);
        /* 等待直到 TBE 位被置位 */
        while(!i2c_flag_get(I2C0, I2C_FLAG_TBE));
    }
    /* 向 I2C 总线发送停止条件 */
    i2c_stop_on_bus(I2C0);
    while(I2C_CTL0(I2C0) & I2C_CTL0_STOP);
}

void Gt911_Scan(Touch_Struct *touch_info)
{
    uint8_t temp;
//    if (1 == (int)touch_flag)
//    {   /* 有触摸中断 */
        touch_flag = 0;
        gt911_rd_reg(GT911_GSTID_REG, &temp, 1);   /* 读取触摸点的状态 */
        touch_info->Touch_State = (uint8_t)((temp&0x80)>>7);
        touch_info->Touch_Number = (uint8_t)(temp&0x0F);
        temp=0;
        if(touch_info->Touch_State)
        {
            for(uint8_t i=0; i<touch_info->Touch_Number; i++)
            {
                uint16_t regAddr = GT911_TP1_REG+8*i;

                gt911_rd_reg(regAddr+0,&temp,1);
                touch_info->gt911_touch[i].x = temp;
                gt911_rd_reg(regAddr+1,&temp,1);
                touch_info->gt911_touch[i].x |= (temp<<8);                  //读取x坐标

                gt911_rd_reg(regAddr+2,&temp,1);
                touch_info->gt911_touch[i].y = temp;
                gt911_rd_reg(regAddr+3,&temp,1);
                touch_info->gt911_touch[i].y |= (temp<<8);                  //读取y坐标

                gt911_rd_reg(regAddr+4,&temp,1);
                touch_info->gt911_touch[i].s = temp;
                gt911_rd_reg(regAddr+5,&temp,1);
                touch_info->gt911_touch[i].s |= (temp<<8);                  //读取触摸面积
            }
            temp = 0;
            gt911_wr_reg(GT911_GSTID_REG, &temp, 1); /* 清标志 */
        }
//    }
//    else
//        return;
}