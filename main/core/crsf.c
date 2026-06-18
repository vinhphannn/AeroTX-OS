#include "crsf.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "core/sys_state.h"
#include "esp_timer.h"
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

/**
 * Read from UART and parse Telemetry frames
 */
void crsf_receive_telemetry(TelemetryData_t *telemetry) {
    static uint8_t rx_buf[256];
    static int rx_idx = 0;
    
    // Đọc tất cả dữ liệu có sẵn vào buffer tĩnh
    int len = uart_read_bytes(CRSF_UART_PORT, &rx_buf[rx_idx], sizeof(rx_buf) - rx_idx, 0);
    if (len > 0) {
        rx_idx += len;
    }
    
    if (rx_idx < 4) return;
    
    int i = 0;
    while (i < rx_idx - 2) { // Cần ít nhất 3 byte (Sync, Len, Type)
        // Tìm Sync Byte hợp lệ (0xEA = Telemetry, 0xC8, 0xEE)
        if (rx_buf[i] == 0xEA || rx_buf[i] == 0xC8 || rx_buf[i] == 0xEE || rx_buf[i] == CRSF_ADDRESS_RADIO_TRANSMITTER) {
            uint8_t frame_len = rx_buf[i+1];
            
            // Frame quá dài hoặc quá ngắn -> rác, bỏ qua Sync này
            if (frame_len > 64 || frame_len < 2) {
                i++;
                continue;
            }
            
            // Nếu chưa nhận đủ nguyên một frame -> Dừng lại chờ luồng tiếp theo đọc thêm
            if (i + 2 + frame_len > rx_idx) {
                break;
            }
            
            uint8_t type = rx_buf[i+2];
            uint8_t crc = crsf_compute_crc(&rx_buf[i+2], frame_len - 1);
            
            if (crc == rx_buf[i + 2 + frame_len - 1]) { // CRC khớp!
                telemetry->link_active = true;
                telemetry->last_recv_ms = (uint32_t)(esp_timer_get_time() / 1000);
                
                uint8_t *payload = &rx_buf[i+3];
                if (type == CRSF_FRAMETYPE_BATTERY_SENSOR) {
                    telemetry->vbat_drone = ((payload[0] << 8) | payload[1]) / 10.0f;
                    telemetry->current_drone = ((payload[2] << 8) | payload[3]) / 10.0f;
                    telemetry->batt_remaining = payload[7];
                } else if (type == CRSF_FRAMETYPE_LINK_STATISTICS) {
                    telemetry->rssi = -1 * payload[7]; // Downlink RSSI
                    telemetry->link_quality = payload[8]; // Downlink LQ
                    telemetry->snr = (int8_t)payload[9]; // Downlink SNR
                } else if (type == CRSF_FRAMETYPE_GPS) {
                    telemetry->gps_lat = (payload[0]<<24) | (payload[1]<<16) | (payload[2]<<8) | payload[3];
                    telemetry->gps_lon = (payload[4]<<24) | (payload[5]<<16) | (payload[6]<<8) | payload[7];
                    telemetry->gps_alt = (((payload[12]<<8) | payload[13]) - 1000);
                    telemetry->gps_sats = payload[14];
                } else if (type == CRSF_FRAMETYPE_ATTITUDE) {
                    int16_t pt = (payload[0]<<8) | payload[1];
                    int16_t ro = (payload[2]<<8) | payload[3];
                    int16_t ya = (payload[4]<<8) | payload[5];
                    telemetry->pitch = pt / 10000.0f;
                    telemetry->roll = ro / 10000.0f;
                    telemetry->yaw = ya / 10000.0f;
                } else if (type == CRSF_FRAMETYPE_FLIGHT_MODE) {
                    int slen = frame_len - 2; // Type + Payload
                    if (slen > 15) slen = 15;
                    memcpy(telemetry->flight_mode, payload, slen);
                    telemetry->flight_mode[slen] = 0;
                }
                
                // Nhảy qua frame này
                i += (frame_len + 2);
                continue;
            }
        }
        // Nêu không phải sync byte hoặc sai CRC, dò byte tiếp theo
        i++;
    }
    
    // Dời các byte chưa xử lý xong về đầu buffer
    if (i > 0 && i < rx_idx) {
        memmove(rx_buf, &rx_buf[i], rx_idx - i);
        rx_idx -= i;
    } else if (i >= rx_idx) {
        rx_idx = 0;
    }
}

