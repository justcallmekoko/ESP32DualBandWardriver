#ifndef configs_h
#define configs_h

//// Firmware info stuff
#define FIRMWARE_VERSION "v0.1.0"
#define DEVICE_NAME      "JCMK C5 Wardriver"



//// Buffer stuff
#define BUF_SIZE 2 * 1024
#define SNAP_LEN 2324


//// Battery stuff
#define I2C_SDA 23
#define I2C_SCL 24


//// GPS stuff
#define GPS_SERIAL_INDEX 1
#define TX_TO_GPS 13
#define RX_TO_GPS 14


//// SD stuff
#define SPI_SCK  6
#define SPI_MISO 2
#define SPI_MOSI 7 
#define SD_CS    10


//// Switch stuff



//// Device stuff
#define HAS_PSRAM
#define HAS_GPS
#define HAS_SD



////WiFi stuff
#define mac_history_len 500

#endif