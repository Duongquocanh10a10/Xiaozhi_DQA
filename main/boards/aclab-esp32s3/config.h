#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// Audio sample rates
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// I2S Simplex mode (Tach biet kenh Mic INMP441 va Loa MAX98357A)
#define AUDIO_I2S_METHOD_SIMPLEX

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16

// Buttons & Touch Wakeup
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_1
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_2
#define TOUCH_BUTTON_GPIO       GPIO_NUM_3

// Ngoai vi chap hanh (LED & Servo)
#define BUILTIN_LED_GPIO        GPIO_NUM_38
#define LAMP_GPIO               GPIO_NUM_38
#define SERVO_GPIO              GPIO_NUM_18

// Man hinh ST7735 SPI 1.8" (128x160)
#define DISPLAY_BACKLIGHT_PIN   GPIO_NUM_14
#define DISPLAY_CLK_PIN         GPIO_NUM_12
#define DISPLAY_MOSI_PIN        GPIO_NUM_11
#define DISPLAY_CS_PIN          GPIO_NUM_13
#define DISPLAY_DC_PIN          GPIO_NUM_9
#define DISPLAY_RST_PIN         GPIO_NUM_10

#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH                   128
#define DISPLAY_HEIGHT                  160
#define DISPLAY_MIRROR_X                true
#define DISPLAY_MIRROR_Y                true
#define DISPLAY_SWAP_XY                 false
#define DISPLAY_INVERT_COLOR            false
#define DISPLAY_RGB_ORDER               LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X                0
#define DISPLAY_OFFSET_Y                0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE                0

// Thoi gian cho tu dong tat man hinh vao che do tiet kiem dien (giay)
#define POWER_SAVE_SLEEP_SECONDS 45

#endif // _BOARD_CONFIG_H_
