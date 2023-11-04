#include <stdint.h>
#include <ch32v00x.h>

#define WS2812_MAX_LEDS     128

// convert one 8bit to 32bits.
// 0 code 0.33us/H, 1us/L, 0x08/0b1000
// 1 code 0.66us/H, 0.66us/L, 0x0c/0b1100
// reset 50us/L, 160bits, 10bytes of SPI.
#define SPI_RESET_BYTE    (4 + 20)

// use same address as IS31FL3731
#define I2C_ADDRESS       0x74

volatile uint8_t cid = SPI_RESET_BYTE;
volatile uint16_t pid = 0;
volatile uint8_t pixel[WS2812_MAX_LEDS * 3] = {0};
volatile uint16_t i2c_flag = 0, i2c_reg = 0;
const uint8_t pixel_map[4] = {0x88, 0x8e, 0xe8, 0xee};

INTERRUPT void SPI1_IRQHandler(void)
{
    if (SPI_I2S_GetITStatus(SPI1, SPI_I2S_IT_TXE)) {
        // color id range [0:3]: we send color by bit.
        // color id range [4:84]: we send zero only as reset.
        if (cid < 4) {
            uint8_t pos = (3 - cid) << 1;
            SPI1->DATAR = pixel_map[(pixel[pid] >> pos) & 0b11];

            // one color has send to end, move to next color.
            if (cid == 0) {
                // if exceed the array size, turn back to begin of the pixels.
                if (++pid >= sizeof(pixel)) {
                    pid = 0;
                    // we need to send reset to leds to show colors.
                    cid = SPI_RESET_BYTE;
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
        (void)(I2C1->STAR2); // clear flag.
        // get address, new transfer begin.
        i2c_reg = i2c_flag = 0;
    } else if (I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE)) {
        switch (i2c_flag) {
        case 0:   // receive register address low byte.
        case 1:   // receive register address high byte.
            i2c_reg |= (uint16_t)I2C_ReceiveData(I2C1) << (8 * i2c_flag);
            i2c_flag++;
            break;

        default:
            if (i2c_reg < sizeof(pixel))
                pixel[i2c_reg++] = I2C_ReceiveData(I2C1);
            break;
        }
    } else if (I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE)) {
        if (i2c_reg < sizeof(pixel))
            I2C_SendData(I2C1, pixel[i2c_reg++]);
        else
            I2C_SendData(I2C1, 0x00);
    } else if (I2C_GetFlagStatus(I2C1, I2C_FLAG_AF)) {
        I2C_SendData(I2C1, pixel[i2c_reg++]);
        I2C1->STAR1 &= ~I2C_FLAG_AF;
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

    // C5, C6 for SPI clock and data, actually we only use C6.
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6;
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

    // SPI interrupt setup.
    NVIC_InitStructure.NVIC_IRQChannel = SPI1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
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
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    I2C_ITConfig(I2C1, I2C_IT_BUF | I2C_IT_EVT, ENABLE);
    I2C_Cmd(I2C1, ENABLE);
}

int main(void)
{
    // use internal high speed(HSI), 48MHz.
    SystemCoreClockUpdate();
    Delay_Init();

    spi_init();
    i2c_init();

    while(1);
}