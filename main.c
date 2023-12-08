#include <stdint.h>
#include <ch32v00x.h>

// IS31FL3731_COMPATIBLE:
//     this mode uses 8bit reg address, allows pages(but ignores them)
//     and is compatible with IS31FL3731 register of LED colors.

// convert one 8bit to 32bits.
// 0 code 0.33us/H, 1us/L, 0x08/0b1000
// 1 code 0.66us/H, 0.66us/L, 0x0c/0b1100
// reset 50us/L, 160bits, 50 bytes of SPI around 120us.
#define SPI_RESET_COUNT     50

#ifdef IS31FL3731_COMPATIBLE
#define I2C_ADDRESS         0x74
#define WS2812_MAX_LEDS     72
volatile static uint8_t i2c_page;
#else
#define I2C_ADDRESS         0x74
#define WS2812_MAX_LEDS     512
#endif

volatile static uint8_t cid = SPI_RESET_COUNT;
volatile static uint16_t pid;
volatile static uint8_t pixel[WS2812_MAX_LEDS * 3];
volatile static uint16_t i2c_flag, i2c_reg;
const uint8_t pixel_map[4] = {0x88, 0x8c, 0xc8, 0xcc};

INTERRUPT void SPI1_IRQHandler(void)
{
    if (SPI_I2S_GetITStatus(SPI1, SPI_I2S_IT_TXE)) {
        // color id range [0:3]: we send color by bit.
        // color id range [4:84]: we send zero only as reset.
        if (cid < 4) {
            SPI1->DATAR = pixel_map[(pixel[pid] >> (cid << 1)) & 3];

            // one color has send to end, move to next color.
            if (cid == 0) {
                // if exceed the array size, turn back to begin of the pixels.
                if (++pid >= sizeof(pixel)) {
                    pid = 0;
                    // we need to send reset to leds to show colors.
                    cid = SPI_RESET_COUNT;
                } else {
                    // rearm the color id to send next color.
                    cid = 4;
                }
            }
        } else {
            // reset mode, we send two 0 bits only.
            SPI1->DATAR = 0;
        }

        cid--;
    }
}

INTERRUPT void I2C1_EV_IRQHandler(void)
{
    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR)) {
        (volatile void)(I2C1->STAR2); // read to clear flag.
        // get address, new transfer begin.
        i2c_reg = i2c_flag = 0;
    } else if (I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE)) {
#ifdef IS31FL3731_COMPATIBLE
        if (i2c_flag == 0) {
            i2c_reg = I2C_ReceiveData(I2C1);
            i2c_flag++;
        } else if(i2c_reg == 0xfd) {
            i2c_page = I2C_ReceiveData(I2C1);
        } else if (i2c_page == 0) {
            // WS2812 is GRB, but IS31 is RGB, need to convert.
            uint8_t reg;
            switch (i2c_reg % 3) {
            case 0: reg = i2c_reg + 1; break;
            case 1: reg = i2c_reg; break;
            case 2: reg = i2c_reg + 2; break;
            }
            // offset with IS31 start address.
            if (reg >= 0x24)
                pixel[reg - 0x24] = I2C_ReceiveData(I2C1);
            i2c_reg++;
        }
#else
        switch (i2c_flag) {
        case 0:   // receive register address low byte.
        case 1:   // receive register address high byte.
            i2c_reg |= (uint16_t)I2C_ReceiveData(I2C1) << (8 * i2c_flag++);
            break;
        default:
            if (i2c_reg < sizeof(pixel))
                pixel[i2c_reg++] = I2C_ReceiveData(I2C1);
            break;
        }
#endif

    } else if (I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE)) {
        I2C_SendData(I2C1, i2c_reg < sizeof(pixel) ? pixel[i2c_reg++] : 0x00);
    } else if (I2C_GetFlagStatus(I2C1, I2C_FLAG_STOPF)) {
        I2C1->CTLR1 &= I2C1->CTLR1;
    }
}

void spi_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef SPI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

    // WS2812B => PC6
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // SPI output data speed = 48M / 16 = 3M
    SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = SPI1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    SPI_I2S_ITConfig(SPI1, SPI_I2S_IT_TXE, ENABLE);
    SPI_Cmd(SPI1, ENABLE);
}

void i2c_init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    I2C_InitTypeDef   I2C_InitTSturcture;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    // I2CDAT => PC1, I2CCLK => PC2
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    I2C_InitTSturcture.I2C_ClockSpeed = 400000;
    I2C_InitTSturcture.I2C_Mode = I2C_Mode_I2C;
    I2C_InitTSturcture.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitTSturcture.I2C_Ack = I2C_Ack_Enable;
    I2C_InitTSturcture.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitTSturcture.I2C_OwnAddress1 = I2C_ADDRESS << 1;
    I2C_Init(I2C1, &I2C_InitTSturcture);

    NVIC_InitStructure.NVIC_IRQChannel = I2C1_EV_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    // subpriority set to 8, for group 0 to enable interrupt preemption.
    // enable this or WS2812B data will have wrong timing caused flicker.
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 8;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    I2C_ITConfig(I2C1, I2C_IT_BUF | I2C_IT_EVT, ENABLE);
    I2C_Cmd(I2C1, ENABLE);
}

int main(void)
{
    SystemCoreClockUpdate();
    Delay_Init();

    spi_init();
    i2c_init();

#ifndef UNITTEST_LED_BREATH
    uint8_t count = 0, dir = 0, color = 0;
    while (1) {
        for(int i = color; i < sizeof(pixel); i += 3)
            pixel[i] = count;
        Delay_Ms(10);

        if (dir) {
            if (++count == 0) {
                dir = 0;
                count = 254;
            }
        } else {
            if (--count == 0) {
                dir = 1;
                if (++color >= 3)
                    color = 0;
            }
        }
    }
#else
    while (1);
#endif
}

