#define USER_SETUP_INFO "ESP32-S3 Waveshare ST7789V2 240x280"

#define ST7789_DRIVER

#define TFT_WIDTH  170  
#define TFT_HEIGHT 320

// Waveshare vertical offset
#define TFT_X_OFFSET 0
#define TFT_Y_OFFSET 20

// SPI pins
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS   10
#define TFT_DC   9
#define TFT_RST  8

// SPI speed
#define SPI_FREQUENCY  27000000

// IMPORTANT: Waveshare ST7789V2 is BGR
#define TFT_RGB_ORDER TFT_BGR

// Fonts
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
