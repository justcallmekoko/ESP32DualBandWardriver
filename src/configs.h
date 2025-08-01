#ifndef configs_h
#define configs_h

// Pins used:

/*
2
4
5
6
7
8
9
10
13
14
15
23 TFT
24 TFT
27
28
*/

#define JCMK_HOST_BOARD

//// Firmware info stuff
#define FIRMWARE_VERSION "v1.0.0"
#define DEVICE_NAME      "JCMK C5 Wardriver"


//// BLE stuff
#define BLE_SCAN_DURATION   1 * 1000 // 1 second


//// LED stuff
#define LED_PIN 28


//// Display stuff
#define ON  HIGH
#define OFF LOW

#define TFT_HEIGHT 80
#define TFT_WIDTH  160

#define TFT_SPI_SPEED 27000000

#define TFT_CS   23
#define TFT_DC   24
#define TFT_RST  -1
#define TOUCH_CS -1
#define TFT_MOSI 7
#define TFT_SCLK 6
#define TFT_BL   27


//// UI Stuff
#define UI_UPDATE_TIME 1 * 1000 // 1 second

#define U_BTN 9
#define D_BTN 8
#define C_BTN 1

#define C_PULL false
#define U_PULL false
#define D_PULL false

#define WEB_PAGE_TIMEOUT 60 * 1000 // 60 seconds
#define TIMER_UPDATE 1 * 1000 // 1 second
#define STATION_CONNECT_TIMEOUT 5 * 1000 // 5 seconds
#define WIFI_CONFIG "/wifi_config.json"

#define SMALL_CHAR_HEIGHT 8


//// Buffer stuff
#define BUF_SIZE 2 * 1024
#define SNAP_LEN 2324


//// Battery stuff
#define HAS_BATTERY
#define I2C_SCL 4
#define I2C_SDA 5


//// GPS stuff
#define GPS_SERIAL_INDEX 1
#define TX_TO_GPS 13
#define RX_TO_GPS 14


//// SD stuff
#define SPI_SCK  6
#define SPI_MISO 2
#define SPI_MOSI 7 
#define SD_CS    10

#define UPDATE_KEY "UpdateFile"


//// Switch stuff



//// Device stuff
#define HAS_PSRAM
#define HAS_GPS
#define HAS_SD


////WiFi stuff
#define mac_history_len 500
#define CHANNEL_TIMER 80

#endif