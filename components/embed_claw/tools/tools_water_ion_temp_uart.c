/**
 * @file tools_water_ion_temp_uart.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-03-29
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "core/ec_tools.h"
#include "ec_config_internal.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"

/* ==================== [Defines] =========================================== */

#define WATER_ION_UART_PORT                UART_NUM_1
#define WATER_ION_UART_BAUD_RATE           9600
#define WATER_ION_UART_TX_PIN              14
#define WATER_ION_UART_RX_PIN              13
#define WATER_ION_UART_TIMEOUT_MS          1200

#define WATER_ION_UART_RX_BUF_SIZE 256
#define WATER_ION_UART_TX_BUF_SIZE 256
#define WATER_ION_PROTO_FRAME_SIZE  6
#define WATER_ION_PROTO_CMD_READ    0xA0
#define WATER_ION_PROTO_RESP_OK     0xAA
#define WATER_ION_PROTO_RESP_ERR    0xAC

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static esp_err_t ec_tool_water_ion_temp_uart_execute(const char *input_json, char *output, size_t output_size);
static esp_err_t uart_init_for_sensor(uart_port_t uart_port);
static esp_err_t read_sensor_frame_uart(uint8_t frame_out[WATER_ION_PROTO_FRAME_SIZE]);
static uint8_t proto_checksum(const uint8_t frame[WATER_ION_PROTO_FRAME_SIZE]);
static void format_hex_frame(const uint8_t frame[WATER_ION_PROTO_FRAME_SIZE], char *out, size_t out_size);
static const char *sensor_error_desc(uint8_t err_code);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "tools_water_ion_temp";

static const ec_tools_t s_water_ion_temp_uart = {
    .name = "water_ion_temp_uart",
    .description = "Read water ion concentration and temperature from a UART sensor and return structured JSON.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{},"
        "\"required\":[]}",
    .execute = ec_tool_water_ion_temp_uart_execute,
};

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t ec_tools_water_ion_temp_uart(void)
{
    ec_tools_register(&s_water_ion_temp_uart);
    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */

static esp_err_t ec_tool_water_ion_temp_uart_execute(const char *input_json, char *output, size_t output_size)
{
    (void)input_json;

    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t frame[WATER_ION_PROTO_FRAME_SIZE] = {0};
    char frame_hex[32] = {0};
    bool checksum_ok = false;

    esp_err_t err = read_sensor_frame_uart(frame);
    if (err != ESP_OK) {
        snprintf(output, output_size, "Error: UART read failed (%s)", esp_err_to_name(err));
        return err;
    }

    format_hex_frame(frame, frame_hex, sizeof(frame_hex));
    checksum_ok = (frame[5] == proto_checksum(frame));

    if (frame[0] == WATER_ION_PROTO_RESP_OK) {
        uint16_t ion_raw = ((uint16_t)frame[1] << 8) | frame[2];
        uint16_t temp_raw = ((uint16_t)frame[3] << 8) | frame[4];
        float ion_mg_l = (float)ion_raw;
        float temp_c = (float)temp_raw / 100.0f;

        snprintf(output,
                 output_size,
                 "{\"ion_mg_l\":%.2f,\"temperature_c\":%.2f,\"checksum_ok\":%s,\"raw_hex\":\"%s\"}",
                 ion_mg_l,
                 temp_c,
                 checksum_ok ? "true" : "false",
                 frame_hex);

        ESP_LOGI(TAG,
                 "UART sensor parsed: ion=%.2f mg/L, temp=%.2f C, frame=%s, checksum_ok=%s",
                 ion_mg_l,
                 temp_c,
                 frame_hex,
                 checksum_ok ? "true" : "false");
        return ESP_OK;
    }

    if (frame[0] == WATER_ION_PROTO_RESP_ERR) {
        uint8_t err_code = frame[1];
        snprintf(output,
                 output_size,
                 "Error: Sensor returned error code 0x%02X (%s), raw=%s",
                 err_code,
                 sensor_error_desc(err_code),
                 frame_hex);
        return ESP_FAIL;
    }

    snprintf(output,
             output_size,
             "Error: Unsupported response frame header 0x%02X, raw=%s",
             frame[0],
             frame_hex);
    return ESP_ERR_INVALID_RESPONSE;
}

static esp_err_t uart_init_for_sensor(uart_port_t uart_port)
{
    uart_config_t uart_cfg = {
        .baud_rate = WATER_ION_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(uart_port,
                                        WATER_ION_UART_RX_BUF_SIZE,
                                        WATER_ION_UART_TX_BUF_SIZE,
                                        0,
                                        NULL,
                                        0);
    if (err != ESP_OK) {
        return err;
    }

    err = uart_param_config(uart_port, &uart_cfg);
    if (err != ESP_OK) {
        uart_driver_delete(uart_port);
        return err;
    }

    err = uart_set_pin(uart_port,
                       WATER_ION_UART_TX_PIN,
                       WATER_ION_UART_RX_PIN,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        uart_driver_delete(uart_port);
        return err;
    }

    return ESP_OK;
}

static esp_err_t read_sensor_frame_uart(uint8_t frame_out[WATER_ION_PROTO_FRAME_SIZE])
{
    if (!frame_out) {
        return ESP_ERR_INVALID_ARG;
    }

    const uart_port_t uart_port = WATER_ION_UART_PORT;

    esp_err_t err = uart_init_for_sensor(uart_port);
    if (err != ESP_OK) {
        return err;
    }

    uart_flush(uart_port);

    uint8_t tx_frame[WATER_ION_PROTO_FRAME_SIZE];
    tx_frame[0] = WATER_ION_PROTO_CMD_READ;
    tx_frame[1] = 0x00;
    tx_frame[2] = 0x00;
    tx_frame[3] = 0x00;
    tx_frame[4] = 0x00;
    tx_frame[5] = proto_checksum(tx_frame);

    int wrote = uart_write_bytes(uart_port, (const char *)tx_frame, sizeof(tx_frame));
    if (wrote != sizeof(tx_frame)) {
        uart_driver_delete(uart_port);
        return ESP_FAIL;
    }

    int total = 0;
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(WATER_ION_UART_TIMEOUT_MS);
    while (total < WATER_ION_PROTO_FRAME_SIZE) {
        TickType_t now = xTaskGetTickCount();
        if (now >= deadline) {
            break;
        }

        TickType_t remain = deadline - now;
        int n = uart_read_bytes(uart_port,
                                frame_out + total,
                                WATER_ION_PROTO_FRAME_SIZE - total,
                                remain);
        if (n > 0) {
            total += n;
        }
    }

    uart_driver_delete(uart_port);

    if (total != WATER_ION_PROTO_FRAME_SIZE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

static uint8_t proto_checksum(const uint8_t frame[WATER_ION_PROTO_FRAME_SIZE])
{
    uint32_t sum = 0;
    for (int i = 0; i < WATER_ION_PROTO_FRAME_SIZE - 1; i++) {
        sum += frame[i];
    }
    return (uint8_t)(sum & 0xFF);
}

static void format_hex_frame(const uint8_t frame[WATER_ION_PROTO_FRAME_SIZE], char *out, size_t out_size)
{
    if (!frame || !out || out_size == 0) {
        return;
    }

    snprintf(out,
             out_size,
             "%02X %02X %02X %02X %02X %02X",
             frame[0],
             frame[1],
             frame[2],
             frame[3],
             frame[4],
             frame[5]);
}

static const char *sensor_error_desc(uint8_t err_code)
{
    switch (err_code) {
        case 0x01:
            return "command order error";
        case 0x02:
            return "busy";
        case 0x03:
            return "calibration failed";
        case 0x04:
            return "temperature out of range";
        default:
            return "unknown error";
    }
}
