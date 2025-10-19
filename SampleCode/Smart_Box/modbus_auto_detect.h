/**
 * @file modbus_auto_detect.h
 * @brief Modbus RTU Auto-Detection Module
 * @date 2025-10-19
 *
 * Automatically scans for Modbus RTU devices by testing different
 * baudrates and device IDs to find connected devices.
 */

#ifndef MODBUS_AUTO_DETECT_H
#define MODBUS_AUTO_DETECT_H

#include <stdint.h>
#include <stdbool.h>
#include "modbus_rtu_client.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Detection result structure
     */
    typedef struct
    {
        bool device_found;         ///< True if a device was found
        uint8_t slave_address;     ///< Detected slave address (1-255)
        uint32_t baudrate;         ///< Detected baudrate
        uint32_t scan_duration_ms; ///< Total scan duration in milliseconds
    } modbus_detect_result_t;

    /**
     * @brief Detection configuration structure
     */
    typedef struct
    {
        bool quick_scan;                ///< Quick scan mode (ID 1-10 only)
        uint32_t per_device_timeout_ms; ///< Timeout per device test (recommended: 80-100ms)
        uint16_t test_register_address; ///< Test register address (default: 0x0000)
        uint8_t max_device_id;          ///< Maximum device ID to scan (1-255, quick_scan overrides this)
    } modbus_detect_config_t;

    /**
     * @brief Progress callback type
     * @param baudrate Current baudrate being tested
     * @param device_id Current device ID being tested
     * @param context User context pointer
     */
    typedef void (*modbus_detect_progress_callback_t)(uint32_t baudrate, uint8_t device_id, void *context);

    /**
     * @brief Auto-detect Modbus RTU device
     *
     * Scans through configured baudrates and device IDs to find a connected device.
     * Baudrate scan order: 9600 -> 38400 -> 4800 -> 57600 -> 115200
     *
     * @param result Pointer to result structure (output)
     * @param config Pointer to configuration structure
     * @param get_time_ms Function pointer to get current time in milliseconds
     * @param progress_callback Optional progress callback (can be NULL)
     * @param callback_context User context for progress callback
     * @return true if scan completed successfully, false on error
     *
     * @note This function temporarily configures UART0 and will restore it after scan.
     *       The scan can take several seconds depending on configuration.
     *       Quick scan (ID 1-10, 5 baudrates) takes approximately 4 seconds.
     */
    bool modbus_auto_detect_scan(
        modbus_detect_result_t *result,
        const modbus_detect_config_t *config,
        modbus_rtu_client_timestamp_cb_t get_time_ms,
        modbus_detect_progress_callback_t progress_callback,
        void *callback_context);

    /**
     * @brief Get default detection configuration
     *
     * Returns a configuration structure with sensible defaults:
     * - Quick scan enabled (ID 1-10)
     * - 80ms timeout per device
     * - Test register address: 0x0000
     *
     * @param config Pointer to configuration structure to initialize
     */
    void modbus_auto_detect_get_default_config(modbus_detect_config_t *config);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_AUTO_DETECT_H
