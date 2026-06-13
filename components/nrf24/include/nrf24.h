#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static inline uint8_t nrf_calc_checksum(uint8_t *data, uint8_t len) {
    uint8_t crc = 0x5A; // Seed để tránh dữ liệu toàn 0 hoặc toàn 0xFF pass checksum
    for (uint8_t i = 0; i < len; i++) {
        crc = (crc + data[i]) ^ 0xA5; // Kết hợp cộng và XOR để chống nhiễu lặp byte
    }
    return crc;
}

// =====================================================
// NRF24L01 Driver - ESP32 HSPI (TX01 Ground Station)
// HSPI: MOSI=13, MISO=19, SCK=14, CSN=21, CE=16
// =====================================================
#define BAYANG_RF_BIND_CHANNEL 0

/* CRC-8 Lookup Table (Poly: 0x07) */
static const uint8_t crc8_table[256] = {
    0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, 0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d,
    0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65, 0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d,
    0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5, 0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
    0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85, 0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd,
    0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2, 0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea,
    0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2, 0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
    0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32, 0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
    0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42, 0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a,
    0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c, 0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
    0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec, 0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4,
    0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c, 0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44,
    0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c, 0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
    0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b, 0x76, 0x71, 0x78, 0x7f, 0x6a, 0x6d, 0x64, 0x63,
    0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b, 0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13,
    0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb, 0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8d, 0x84, 0x83,
    0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb, 0xf6, 0xf1, 0xf8, 0xff, 0xea, 0xed, 0xe4, 0xe3
};

static inline uint8_t nRF24_ComputeCRC8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc = crc8_table[crc ^ data[i]];
    }
    return crc;
}

#define BAYANG_PACKET_SIZE      15
#define BAYANG_HEADER_BIND      0xA4
#define BAYANG_HEADER_DATA      0xA5

// --- Cấu trúc gói tin Bayang 15 bytes ---
typedef struct __attribute__((packed)) {
    uint8_t  header;     // 0xA4 hoặc 0xA5
    uint8_t  address[5]; // Unique TX ID
    uint8_t  extra[8];   // Channels or Logic
    uint8_t  checksum;   // Sum of all bytes
} Bayang_Packet_t;

// Porting bit reversal from Multiprotocol
static inline uint8_t bit_reverse(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

#define NRF_PAYLOAD_SIZE      18   // Extended for P1, P2
#define NRF_ACK_PAYLOAD_SIZE  18   // Extended for Telemetry consistency

void nrf_write_reg(uint8_t reg, uint8_t val); // Khai báo sớm cho toàn bộ project

#define MAGIC_CONTROL         0x99
#define MAGIC_CONFIG          0x98
#define MAGIC_TELEMETRY       0x88

#define REG_DYNPD       0x1C
#define REG_FEATURE     0x1D

#define CMD_R_REGISTER    0x00
#define CMD_W_REGISTER    0x20
#define CMD_R_RX_PAYLOAD  0x61
#define CMD_W_TX_PAYLOAD  0xA0
#define CMD_FLUSH_TX      0xE1
#define CMD_FLUSH_RX      0xE2
#define CMD_W_ACK_PAYLOAD 0xA8

// --- Địa chỉ Pipe mặc định phần cứng ---
#define NRF_BIND_ADDR       {0x66, 0x88, 0x68, 0x68, 0x68} // [v264] Standard Bayang Bind Address
#define NRF_PIPE_ADDR       {0xE7, 0xE7, 0xE7, 0xE7, 0xE7} // Default for DATA
#define NRF_CHANNEL         110  // [v264] Fixed channel 110 to match RX

// --- Cấu trúc gói tin 14 bytes ---
typedef struct __attribute__((packed)) {
    uint8_t  magic;      // 1: Magic Word = 0x99
    uint16_t roll;       // 2: 1000-2000
    uint16_t pitch;      // 2: 1000-2000
    uint16_t throttle;   // 2: 1000-2000
    uint16_t yaw;        // 2: 1000-2000
    uint8_t  switchA;    // 1: ARM (0/1)
    uint8_t  switchB;    // 1: Flight Mode (0/1/2)
    uint8_t  switchC;    // 1: Speed (0/1/2)
    uint8_t  switchD;    // 1: Beeper/LED (0/1)
    uint16_t p1;         // 2: AUX 1 (1000-2000)
    uint16_t p2;         // 2: AUX 2 (1000-2000)
    uint8_t  checksum;   // 1: XOR checksum
} RF_Payload_t;

// --- Gói tin Cấu hình 14 bytes (TX -> FC) ---
typedef struct __attribute__((packed)) {
    uint8_t  magic;      // 0x98
    uint8_t  cmd;        // Lệnh (CALIB_ACC, CALIB_GYRO, SET_PID_P...)
    uint8_t  sub_cmd;    // Tham số bổ sung (Trục Roll/Pitch/Yaw)
    float    value;      // Giá trị mới (float 4 bytes)
    uint8_t  reserve[10]; // Padding to 18 bytes
    uint8_t  checksum;
} Config_Payload_t;

typedef struct __attribute__((packed)) {
    uint8_t  magic;      // 0: 0x88
    uint16_t vbat;       // 1-2: Voltage in 10mV units (Big Endian from PX4)
    uint8_t  rssi;       // 3: RSSI (PX4 might fill this later)
    uint8_t  padding[11]; // Total to 15 bytes
} Telemetry_PX4_t;

// Giữ nguyên union cũ nhưng thêm struct PX4 cho dễ dùng
typedef struct __attribute__((packed)) {
    uint8_t  magic;      // 0x88
    uint8_t  type;
    union {
        struct {
            uint16_t vbat;
            uint8_t  mode;
            uint8_t  armed;
            uint8_t  rssi;
            uint8_t  reserve[2];
        } tel;
        struct {
            float    p;
            float    i;
            float    d;
        } pid;
    } data;
    uint8_t  padding[3];
    uint8_t  checksum;
} Telemetry_Payload_t;

// --- API ---
bool NRF24_Init(void);
bool NRF24_Transmit(void *payload);
bool NRF24_ReadAckPayload(Telemetry_Payload_t *telemetry);
bool NRF24_IsReady(void);
void NRF24_Deinit(void);
void nrf_write_reg(uint8_t reg, uint8_t val);
void nrf_write_reg_buf(uint8_t reg, const uint8_t *buf, uint8_t len); 
uint8_t nrf_read_reg_direct(uint8_t reg); // [v274] For external diagnostics
