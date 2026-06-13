#include "crsf.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <string.h>

#define CRSF_UART_PORT      UART_NUM_2
#define CRSF_BAUDRATE       420000
#define CRSF_TX_PIN         17
#define CRSF_RX_PIN         16
#define CRSF_POWER_EN_PIN   21

/**
 * Initialize UART2 for CRSF communication with ELRS module.
 */
void crsf_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = CRSF_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    
    uart_driver_install(CRSF_UART_PORT, 512, 512, 0, NULL, 0);
    uart_param_config(CRSF_UART_PORT, &uart_config);
    uart_set_pin(CRSF_UART_PORT, CRSF_TX_PIN, CRSF_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Initialize Power EN pin
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CRSF_POWER_EN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io_conf);
    crsf_set_power(true); // Default to ON
}

/**
 * Control ELRS module power via MOSFET (GPIO 21).
 */
void crsf_set_power(bool enable) {
    gpio_set_level(CRSF_POWER_EN_PIN, enable ? 1 : 0);
}

/**
 * Compute CRSF CRC8 using polynomial 0xD5.
 */
uint8_t crsf_compute_crc(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0xD5;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * Map standard 1000-2000us PWM values to CRSF 11-bit values.
 */
uint16_t us_to_crsf(uint16_t us) {
    if (us < 1000) us = 1000;
    if (us > 2000) us = 2000;
    // (us - 1000) * (1792 - 191) / (2000 - 1000) + 191
    return (uint16_t)((float)(us - 1000) * 1.601f + 191.0f);
}

/**
 * Pack and send 16 channels over UART using CRSF protocol.
 */
void crsf_send_channels(const uint16_t channels[16]) {
    uint8_t packet[CRSF_PACKET_SIZE];
    
    // Header
    packet[0] = CRSF_SYNC_BYTE;
    packet[1] = 24; // Length (Type + Payload + CRC)
    packet[2] = CRSF_FRAMETYPE_RC_CHANNELS;
    
    // Payload (Packing 16 channels x 11 bits = 176 bits = 22 bytes)
    uint8_t *payload = &packet[3];
    uint32_t read_value = 0;
    uint32_t read_bit = 0;
    uint32_t bits_merged = 0;
    
    for (int i = 0; i < 16; i++) {
        uint32_t value = us_to_crsf(channels[i]);
        // Mask to 11 bits just in case
        value &= 0x07FF; 
        
        read_value |= (value << read_bit);
        read_bit += 11;
        while (read_bit >= 8) {
            payload[bits_merged++] = (uint8_t)(read_value & 0xFF);
            read_value >>= 8;
            read_bit -= 8;
        }
    }
    
    // CRC (Calculated from Type and Payload, which is 23 bytes: packet[2] to packet[24])
    packet[25] = crsf_compute_crc(&packet[2], 23);
    
    // Write to UART
    uart_write_bytes(CRSF_UART_PORT, (const char *)packet, CRSF_PACKET_SIZE);
}
