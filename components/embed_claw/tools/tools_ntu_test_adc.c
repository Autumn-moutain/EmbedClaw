/**
 * @file tools_ntu_test_adc.c
 * @author embedclaw_developer
 * @brief ADC initialization and read tool for NTU test sensor
 * @version 0.4
 * @date 2026-04-10
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#include "core/ec_tools.h"
#include <stdbool.h>
#include <stdio.h>
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"

#define TAG "tools_ntu_test_adc"

#define NTU_ADC_UNIT               ADC_UNIT_1
#define NTU_ADC_CHANNEL            ADC_CHANNEL_2
#define NTU_ADC_ATTEN              ADC_ATTEN_DB_12
#define NTU_ADC_BITWIDTH           ADC_BITWIDTH_DEFAULT

#define NTU_TEMP_DATA_C            25.0f
#define NTU_K_VALUE                3985.59f
#define NTU_VOLTAGE_DIVIDER_RATIO  2.0f

static esp_err_t ec_tool_ntu_test_adc_execute(const char *input_json, char *output, size_t output_size);
static esp_err_t ntu_adc_init(void);
static esp_err_t ntu_adc_calibration_init(void);
static esp_err_t ntu_adc_read_raw(int *raw_value);
static float ntu_compute_value(float tu_calibration);

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static bool s_adc_inited = false;
static adc_cali_handle_t s_cali_handle = NULL;
static bool s_cali_inited = false;
static bool s_cali_available = false;

static const ec_tools_t s_ntu_test_adc_tool = {
    .name = "ntu_test_adc",
    .description = "Initialize ADC, apply ESP-IDF calibration, and read the NTU test analog value. No input is required.",
    .input_schema_json = "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
    .execute = ec_tool_ntu_test_adc_execute,
};

esp_err_t ec_tools_ntu_test_adc(void)
{
    ec_tools_register(&s_ntu_test_adc_tool);
    return ESP_OK;
}

static esp_err_t ec_tool_ntu_test_adc_execute(const char *input_json, char *output, size_t output_size)
{
    int raw = 0;
    int voltage_mv = 0;
    float tu_voltage = 0.0f;
    float tu_calibration = 0.0f;
    float tu_value = 0.0f;
    esp_err_t err;

    (void)input_json;

    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    err = ntu_adc_init();
    if (err != ESP_OK) {
        snprintf(output, output_size, "{\"error\":\"adc init failed\",\"err\":%d}", err);
        return err;
    }

    err = ntu_adc_calibration_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration init failed: %s", esp_err_to_name(err));
    }

    err = ntu_adc_read_raw(&raw);
    if (err != ESP_OK) {
        snprintf(output, output_size, "{\"error\":\"adc read failed\",\"err\":%d}", err);
        return err;
    }

    if (s_cali_inited && s_cali_available && s_cali_handle) {
        esp_err_t cali_err = adc_cali_raw_to_voltage(s_cali_handle, raw, &voltage_mv);
        if (cali_err == ESP_OK) {
            tu_voltage = ((float)voltage_mv / 1000.0f) * NTU_VOLTAGE_DIVIDER_RATIO;
            tu_calibration = -0.0192f * (NTU_TEMP_DATA_C - 25.0f) + tu_voltage;
            tu_value = ntu_compute_value(tu_calibration);

            snprintf(output,
                     output_size,
                     "{\"status\":\"success\",\"raw\":%d,\"voltage_mv\":%d,\"tu_voltage\":%.4f,\"tu_calibration\":%.4f,\"ntu_value\":%.2f,\"calibrated\":true,\"unit\":%d,\"channel\":%d}",
                     raw,
                     voltage_mv,
                     tu_voltage,
                     tu_calibration,
                     tu_value,
                     (int)NTU_ADC_UNIT,
                     (int)NTU_ADC_CHANNEL);
            ESP_LOGI(TAG, "ADC read OK: raw=%d, voltage_mv=%d, tu_voltage=%.4f, tu_cal=%.4f, ntu=%.2f",
                     raw, voltage_mv, tu_voltage, tu_calibration, tu_value);
            return ESP_OK;
        }
        ESP_LOGW(TAG, "adc_cali_raw_to_voltage failed: %s", esp_err_to_name(cali_err));
    }

    snprintf(output,
             output_size,
             "{\"status\":\"success\",\"raw\":%d,\"calibrated\":false,\"unit\":%d,\"channel\":%d}",
             raw,
             (int)NTU_ADC_UNIT,
             (int)NTU_ADC_CHANNEL);
    ESP_LOGI(TAG, "ADC read OK: raw=%d", raw);
    return ESP_OK;
}

static esp_err_t ntu_adc_init(void)
{
    if (s_adc_inited) {
        return ESP_OK;
    }

    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = NTU_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    esp_err_t err = adc_oneshot_new_unit(&init_cfg, &s_adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit failed: %s", esp_err_to_name(err));
        return err;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = NTU_ADC_ATTEN,
        .bitwidth = NTU_ADC_BITWIDTH,
    };

    err = adc_oneshot_config_channel(s_adc_handle, NTU_ADC_CHANNEL, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel failed: %s", esp_err_to_name(err));
        adc_oneshot_del_unit(s_adc_handle);
        s_adc_handle = NULL;
        return err;
    }

    s_adc_inited = true;
    ESP_LOGI(TAG, "ADC initialized: unit=%d channel=%d", (int)NTU_ADC_UNIT, (int)NTU_ADC_CHANNEL);
    return ESP_OK;
}

static esp_err_t ntu_adc_calibration_init(void)
{
    if (s_cali_inited) {
        return ESP_OK;
    }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg_curve = {
        .unit_id = NTU_ADC_UNIT,
        .atten = NTU_ADC_ATTEN,
        .bitwidth = NTU_ADC_BITWIDTH,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg_curve, &s_cali_handle) == ESP_OK) {
        s_cali_available = true;
        s_cali_inited = true;
        ESP_LOGI(TAG, "ADC calibration initialized with curve fitting");
        return ESP_OK;
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_cfg_line = {
        .unit_id = NTU_ADC_UNIT,
        .atten = NTU_ADC_ATTEN,
        .bitwidth = NTU_ADC_BITWIDTH,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_cfg_line, &s_cali_handle) == ESP_OK) {
        s_cali_available = true;
        s_cali_inited = true;
        ESP_LOGI(TAG, "ADC calibration initialized with line fitting");
        return ESP_OK;
    }
#endif

    s_cali_inited = true;
    s_cali_available = false;
    ESP_LOGW(TAG, "ADC calibration is not available on this target/configuration");
    return ESP_OK;
}

static esp_err_t ntu_adc_read_raw(int *raw_value)
{
    if (!raw_value) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ntu_adc_init();
    if (err != ESP_OK) {
        return err;
    }

    err = adc_oneshot_read(s_adc_handle, NTU_ADC_CHANNEL, raw_value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_read failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

static float ntu_compute_value(float tu_calibration)
{
    float tu_value = -865.68f * tu_calibration + NTU_K_VALUE;

    if (tu_value <= 0.0f) {
        tu_value = 0.0f;
    } else if (tu_value >= 3000.0f) {
        tu_value = 3000.0f;
    }

    return tu_value;
}