#pragma once
// LilyGO T-Display-S3-Pro + MVSRLora back plate (LR1121).
// Sources: github.com/Xinyuan-LilyGO/T-Display-S3-Pro (examples/*/utilities.h)
//          github.com/Xinyuan-LilyGO/T-Display-S3-Pro-MVSRLora (libraries/private_library/pin_config.h)

// Shared SPI bus: display, SD card and LR1121
#define PIN_SPI_SCK     18
#define PIN_SPI_MOSI    17
#define PIN_SPI_MISO    8

// ST7796 display
#define PIN_TFT_CS      39
#define PIN_TFT_DC      9
#define PIN_TFT_RST     47
#define PIN_TFT_BL      48
#define TFT_W           222
#define TFT_H           480
#define TFT_COL_OFFSET  49

// I2C bus: CST226SE touch, SY6970 PMU, PCF85063 RTC
#define PIN_I2C_SDA     5
#define PIN_I2C_SCL     6
#define PIN_TOUCH_INT   21
#define PIN_TOUCH_RST   13
#define SY6970_ADDR     0x6A

// SD card
#define PIN_SD_CS       14

// LR1121 LoRa (sub-GHz + 2.4 GHz)
#define PIN_LR_CS       7
#define PIN_LR_IRQ      40
#define PIN_LR_RST      10
#define PIN_LR_BUSY     46

// Back-plate power (RT9080 LDO) — drive HIGH before using any back-plate peripheral
#define PIN_RT9080_EN   42

// MAX98357A speaker amp (I2S)
#define PIN_I2S_BCLK    4
#define PIN_I2S_LRCLK   15
#define PIN_I2S_DATA    11
#define PIN_AMP_SD      41

// Vibration motor
#define PIN_VIBE        45

// Buttons (active low)
#define PIN_BTN_BOOT    0   // left-bottom
#define PIN_BTN_UP      12  // right-top
#define PIN_BTN_DOWN    16  // right-bottom
