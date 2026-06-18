#ifndef BLE_HID_H
#define BLE_HID_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize BLE HID Gamepad
 */
void ble_hid_init(void);

/**
 * @brief Deinitialize BLE HID
 */
void ble_hid_deinit(void);

/**
 * @brief Send Gamepad HID report
 * 
 * @param throttle 1000-2000
 * @param yaw      1000-2000
 * @param pitch    1000-2000
 * @param roll     1000-2000
 * @param p1       1000-2000 (Potentiometer 1)
 * @param p2       1000-2000 (Potentiometer 2)
 * @param buttons  Bitmask of 8 buttons
 */
void ble_hid_send_report(uint16_t throttle, uint16_t yaw, uint16_t pitch, uint16_t roll, uint16_t p1, uint16_t p2, uint8_t buttons);

/**
 * @brief Check if BLE is connected to a host
 */
bool ble_hid_is_connected(void);

#endif // BLE_HID_H
