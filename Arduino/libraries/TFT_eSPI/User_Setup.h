// User_Setup.h for ESP32-S3 and 1.8" ST7735 TFT
#define ST7789_DRIVER     
#define TFT_WIDTH  128
#define TFT_HEIGHT 160
#define ST7735_BLACKTAB
#define TFT_RGB_ORDER TFT_RGB 

// ESP32-S3 Pin definitions
#define TFT_MISO -1
#define TFT_MOSI 17
#define TFT_SCLK 18
#define TFT_CS   14
#define TFT_DC   16
#define TFT_RST  15
#define TFT_BL   21  

#define LOAD_GLCD   
#define LOAD_FONT2  
#define LOAD_FONT4  
#define LOAD_FONT6  
#define LOAD_FONT7  
#define LOAD_FONT8  
#define LOAD_GFXFF  
#define SMOOTH_FONT
#define SPI_FREQUENCY  27000000