#include "nrf24.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "rom/ets_sys.h"
#include <string.h>

// static const char *TAG = "NRF24"; // [v265] Unused in current config

// --- HSPI Pins (V2.0 FINAL) ---
#define PIN_MOSI    13
#define PIN_MISO    25
#define PIN_SCK     14
#define PIN_CSN     21
#define PIN_CE      16

// --- NRF24L01 Registers ---
#define REG_CONFIG      0x00
#define REG_EN_AA       0x01
#define REG_EN_RXADDR   0x02
#define REG_SETUP_AW    0x03
#define REG_SETUP_RETR  0x04
#define REG_RF_CH       0x05
#define REG_RF_SETUP    0x06
#define REG_STATUS      0x07
#define REG_TX_ADDR     0x10
#define REG_RX_ADDR_P0  0x0A
#define REG_RX_PW_P0    0x11

static spi_device_handle_t spi_dev;
static uint8_t *nrf_tx_buf = NULL;
static uint8_t *nrf_rx_buf = NULL;
#include "freertos/semphr.h"
static SemaphoreHandle_t nrf_mutex = NULL;
static bool nrf_hw_present = false;

void nrf_write_reg(uint8_t reg, uint8_t val);
void nrf_write_reg_buf(uint8_t reg, const uint8_t *buf, uint8_t len);
static uint8_t nrf_read_reg(uint8_t reg);
static void NRF24_FlushTX(void);
static void NRF24_FlushRx(void);

static inline void CSN_LOW(void)  { gpio_set_level(PIN_CSN, 0); }
static inline void CSN_HIGH(void) { gpio_set_level(PIN_CSN, 1); }
static inline void CE_LOW(void)   { gpio_set_level(PIN_CE,  0); }
static inline void CE_HIGH(void)  { gpio_set_level(PIN_CE,  1); }

void nrf_write_reg(uint8_t reg, uint8_t val) {
    if (xSemaphoreTakeRecursive(nrf_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
    nrf_tx_buf[0] = CMD_W_REGISTER | reg;
    nrf_tx_buf[1] = val;
    spi_transaction_t t = { .length = 16, .tx_buffer = nrf_tx_buf };
    CSN_LOW(); spi_device_transmit(spi_dev, &t); CSN_HIGH();
    xSemaphoreGiveRecursive(nrf_mutex);
}

void nrf_write_reg_buf(uint8_t reg, const uint8_t *buf, uint8_t len) {
    if (len > 31) return;
    if (xSemaphoreTake(nrf_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
    nrf_tx_buf[0] = CMD_W_REGISTER | reg;
    memcpy(nrf_tx_buf + 1, buf, len);
    spi_transaction_t t = { .length = (len + 1) * 8, .tx_buffer = nrf_tx_buf };
    CSN_LOW(); spi_device_transmit(spi_dev, &t); CSN_HIGH();
    xSemaphoreGive(nrf_mutex);
}

bool NRF24_Init(void) {
    gpio_config_t io_conf = { .pin_bit_mask = (1ULL << PIN_CSN) | (1ULL << PIN_CE), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io_conf);
    CSN_HIGH(); CE_LOW();

    if (nrf_tx_buf == NULL) nrf_tx_buf = heap_caps_malloc(32, MALLOC_CAP_DMA);
    if (nrf_rx_buf == NULL) nrf_rx_buf = heap_caps_malloc(32, MALLOC_CAP_DMA);

    if (spi_dev == NULL) {
        spi_bus_config_t buscfg = { .miso_io_num = PIN_MISO, .mosi_io_num = PIN_MOSI, .sclk_io_num = PIN_SCK };
        // FIX v2.0: Tăng SPI lên 8MHz để giảm thời gian chiếm bus SPI
        // nRF24L01+ hỗ trợ tối đa 10MHz, 8MHz là mức an toàn.
        spi_device_interface_config_t devcfg = { .clock_speed_hz = 8 * 1000 * 1000, .mode = 0, .spics_io_num = -1, .queue_size = 7 };
        spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
        spi_bus_add_device(HSPI_HOST, &devcfg, &spi_dev);
    }

    if (nrf_mutex == NULL) nrf_mutex = xSemaphoreCreateRecursiveMutex(); // [v276] Recursive Mutex

    if (xSemaphoreTake(nrf_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ets_delay_us(5000);
        nrf_write_reg(REG_SETUP_AW, 0x03);
        if (nrf_read_reg(REG_SETUP_AW) != 0x03) { 
            ESP_LOGE("NRF24", "Hardware check failed (REG_SETUP_AW != 0x03)");
            nrf_hw_present = false; 
            xSemaphoreGive(nrf_mutex); 
            return false; 
        }
        nrf_hw_present = true;

        uint8_t bind_addr[] = NRF_BIND_ADDR;
        nrf_write_reg(REG_CONFIG,      0x0E); // TX, CRC 2-byte, PWR_UP
        nrf_write_reg(REG_EN_AA,       0x01);
        nrf_write_reg(REG_EN_RXADDR,   0x01);
        nrf_write_reg(REG_SETUP_AW,    0x03);

        // [v269] ARD=1500µs (0x5x), ARC=15 (0xF) - Khớp với cấu hình PX4
        // Tại 250kbps, ARD phải >= 1500µs để đủ thời gian nhận ACK Payload.
        nrf_write_reg(REG_SETUP_RETR,  0x5F);
        nrf_write_reg(REG_RF_CH,       NRF_CHANNEL);
        nrf_write_reg(REG_RF_SETUP,    0x26); // [v264] 250kbps, 0dBm

        nrf_write_reg(0x1C, 0x01); // DYNPD = 1
        nrf_write_reg(0x1D, 0x06); // FEATURE (EN_DPL, EN_ACK_PAY)

        nrf_write_reg_buf(REG_TX_ADDR,    bind_addr, 5);
        nrf_write_reg_buf(REG_RX_ADDR_P0, bind_addr, 5);
        /* [fix-dpl] TX dùng Dynamic Payload Length (DYNPD=0x01, FEATURE=0x06).
         * Khi DPL bật, NRF24 tự thiết lập độ dài payload động.
         * RX_PW_P0 TUYỆT ĐỐI KHÔNG SET khi dùng DPL (nếu set = 15 sẽ xảy ra
         * xất cấu hình conflict, FC bão hiệu FIFO size sai → lỗi đọc payload). */
        // nrf_write_reg(0x11, 15);  // ← ĐÃ XÓA: conflict với Dynamic Payload Length

        NRF24_FlushTX(); NRF24_FlushRx();
        nrf_write_reg(REG_STATUS, 0x70);
        xSemaphoreGive(nrf_mutex);
        return true;
    }
    return false;
}

bool NRF24_Transmit(void *payload) {
    if (!nrf_hw_present) return false;
    if (xSemaphoreTakeRecursive(nrf_mutex, pdMS_TO_TICKS(5)) != pdTRUE) return false;

    /* 1. Clear status, switch TX mode, flush TX FIFO */
    nrf_write_reg(REG_STATUS, 0x70);
    nrf_write_reg(REG_CONFIG, 0x0E); // PWR_UP, CRC 2-byte, TX mode
    NRF24_FlushTX();

    /* 2. Nạp payload (15 bytes, bit-reverse theo giao thức Bayang) */
    nrf_tx_buf[0] = CMD_W_TX_PAYLOAD;
    uint8_t *src = (uint8_t*)payload;
    for(int i=0; i < 15; i++) nrf_tx_buf[i+1] = bit_reverse(src[i]);

    spi_transaction_t t = { .length = 16 * 8, .tx_buffer = nrf_tx_buf };
    CSN_LOW(); spi_device_transmit(spi_dev, &t); CSN_HIGH();

    /* 3. CE pulse >= 10us để kích TX, sau đó kéo xuống ngay.
     * NRF24L01+ tự xử lý hardware retry (ARD/ARC) hoàn toàn trong silicon.
     * CPU KHÔNG CẦN giữ CE HIGH trong suốt quá trình retry.
     * Chỉ cần đợi TX_DS hoặc MAX_RT interrupt flag. */
    CE_HIGH();
    ets_delay_us(15); // Bare minimum: 10us theo datasheet — dùng ets OK vì chỉ 15us
    CE_LOW();

    /* 4. Poll TX_DS / MAX_RT với vTaskDelay(1) thay vì busy-wait.
     * [TWDT FIX] ets_delay_us(100) × 300 lần = 30ms CPU bị chiếm liên tục
     *            → IDLE1 không chạy được → TWDT trigger.
     * vTaskDelay(1) = yield FreeRTOS scheduler → IDLE1 sống → TWDT OK.
     * Trade-off: polling interval 1ms thay vì 100us, nhưng NRF24 hardware
     * retry tự chạy không cần CPU → poll chậm hơn 10x là hoàn toàn ổn. */
    bool success = false;
    const TickType_t timeout_ticks = pdMS_TO_TICKS(30); // ARD×ARC max = 22.5ms
    TickType_t start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < timeout_ticks) {
        uint8_t st = nrf_read_reg(REG_STATUS);
        if (st & 0x20) { success = true;  break; } // TX_DS: ACK nhận được
        if (st & 0x10) { success = false; break; } // MAX_RT: hết retry
        vTaskDelay(1); // Yield 1ms — IDLE1 chạy, TWDT được feed
    }

    /* 5. Dọn dẹp */
    nrf_write_reg(REG_STATUS, 0x70);
    if (!success) { NRF24_FlushTX(); }

    xSemaphoreGiveRecursive(nrf_mutex);
    return success;
}

static uint8_t nrf_read_reg(uint8_t reg) {
    nrf_tx_buf[0] = reg & 0x1F; nrf_tx_buf[1] = 0xFF;
    spi_transaction_t t = { .length = 16, .tx_buffer = nrf_tx_buf, .rx_buffer = nrf_rx_buf };
    CSN_LOW(); spi_device_transmit(spi_dev, &t); CSN_HIGH();
    return nrf_rx_buf[1];
}

uint8_t nrf_read_reg_direct(uint8_t reg) {
    return nrf_read_reg(reg);
}

bool NRF24_ReadAckPayload(Telemetry_Payload_t *telemetry) {
    if (xSemaphoreTake(nrf_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
    uint8_t st = nrf_read_reg(0x17);
    if (st & 0x01) { xSemaphoreGive(nrf_mutex); return false; }

    nrf_tx_buf[0] = 0x61; // R_RX_PAYLOAD
    spi_transaction_t t = { .length = 16 * 8, .tx_buffer = nrf_tx_buf, .rx_buffer = nrf_rx_buf };
    CSN_LOW(); spi_device_transmit(spi_dev, &t); CSN_HIGH();
    nrf_write_reg(0x07, 0x40);

    uint8_t *dst = (uint8_t*)telemetry;
    for(int i=0; i<15; i++) dst[i] = ( ( (nrf_rx_buf[i+1] & 0x80) >> 7 ) | ( (nrf_rx_buf[i+1] & 0x40) >> 5 ) | ( (nrf_rx_buf[i+1] & 0x20) >> 3 ) | ( (nrf_rx_buf[i+1] & 0x10) >> 1 ) | ( (nrf_rx_buf[i+1] & 0x08) << 1 ) | ( (nrf_rx_buf[i+1] & 0x04) << 3 ) | ( (nrf_rx_buf[i+1] & 0x02) << 5 ) | ( (nrf_rx_buf[i+1] & 0x01) << 7 ) );

    xSemaphoreGive(nrf_mutex);
    return true;
}

bool NRF24_IsReady(void) { return nrf_hw_present; }
void NRF24_Deinit(void) { if(nrf_hw_present) nrf_write_reg(0x00, 0x08); }
static void NRF24_FlushTX(void) { uint8_t c = 0xE1; spi_transaction_t t = {.length=8, .tx_buffer=&c}; CSN_LOW(); spi_device_transmit(spi_dev, &t); CSN_HIGH(); }
static void NRF24_FlushRx(void) { uint8_t c = 0xE2; spi_transaction_t t = {.length=8, .tx_buffer=&c}; CSN_LOW(); spi_device_transmit(spi_dev, &t); CSN_HIGH(); }
