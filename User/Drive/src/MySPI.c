#include "MySPI.h"

static void MySPIRCCInit();
static void MySPIGPIOInit();

#ifdef DMA_MODEL
static uint8_t MySPIDMAInit();
#endif

void MySPIInit(void)
{
    MySPIRCCInit();
    MySPIGPIOInit();
#ifdef DMA_MODE
    MySPIDMAInit();
#endif
    spi_parameter_struct spi_init_struct;

    /* configure SPI1 parameter */
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_2;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(SPI4, &spi_init_struct);
    spi_enable(SPI4);
}

static void MySPIRCCInit()
{
    rcu_periph_clock_enable(RCU_GPIOF);
#ifdef DMA_MODEL
    rcu_periph_clock_enable(RCU_DMA0);
#endif
    rcu_periph_clock_enable(RCU_SPI4);
}

static void MySPIGPIOInit()
{
    /* configure SPI4 GPIO */
    gpio_af_set(GPIOF, GPIO_AF_5, GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
    gpio_mode_set(GPIOF, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
    gpio_output_options_set(GPIOF, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
    /*
        //set SPI4_NSS as GPIO
        gpio_mode_set(GPIOF, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6);
        gpio_output_options_set(GPIOF, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
    */
}

#ifdef DMA_MODEL
static uint8_t MySPIDMAInit()
{
    dma_single_data_parameter_struct dma_init_struct;

    /* configure SPI4 transmit dma */
    dma_deinit(DMA1, DMA_CH4);
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_LOW;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;

    dma_init_struct.memory0_addr        = (uint32_t)0;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.number              = 0;

    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI4);
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;


    dma_single_data_mode_init(DMA1, DMA_CH4, &dma_init_struct);
    dma_channel_subperipheral_select(DMA1, DMA_CH4, DMA_SUBPERI3);
}
#endif
