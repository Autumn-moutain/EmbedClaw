/**
 * @file ec_tool_fill_light.c
 * @author embedclaw_developer
 * @brief 补光灯 PWM 控制工具 (Fill Light Control Tool)
 * @version 0.1
 * @date 2026-03-27
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "core/ec_tools.h"

#include "esp_log.h"
#include "driver/ledc.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

/* ==================== [Defines] =========================================== */

#define TAG "ec_tool_fill_light"

/* 硬件引脚与外设配置宏 */
#define FILL_LIGHT_GPIO             GPIO_NUM_21
#define FILL_LIGHT_LEDC_TIMER       LEDC_TIMER_0
#define FILL_LIGHT_LEDC_MODE        LEDC_LOW_SPEED_MODE
#define FILL_LIGHT_LEDC_CHANNEL     LEDC_CHANNEL_0
#define FILL_LIGHT_LEDC_RESOLUTION  LEDC_TIMER_10_BIT
#define FILL_LIGHT_LEDC_FREQ_HZ     5000

/* 业务逻辑约束宏 */
#define FILL_LIGHT_MAX_DUTY         ((1 << 10) - 1) /* 10-bit 分辨率对应的最大占空比 1023 */
#define FILL_LIGHT_MAX_BRIGHTNESS   100
#define FILL_LIGHT_MIN_BRIGHTNESS   0

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static esp_err_t fill_light_init(void);
static esp_err_t ec_tool_fill_light_execute(const char *input_json, char *output, size_t output_size);

/* ==================== [Static Variables] ================================== */

static bool s_fill_light_inited = false;

static const ec_tools_t s_fill_light_tool = {
    .name = "control_fill_light",
    .description = "CRITICAL HARDWARE ACTUATOR. Use this to physically change LED brightness. "
                   "PROTOCOL FOR AUTO-ADJUST: "
                   "1. You MUST have current lux from 'read_light_intensity' (DO NOT GUESS). "
                   "2. Map lux to brightness: <50lx=100%, 50-200lx=80%, 200-500lx=60%, 500-1000lx=40%, >1000lx=10%. "
                   "3. CALL THIS TOOL IMMEDIATELY after reading lux. "
                   "4. DO NOT generate a final text response to the user until you see '{\"status\": \"success\"}' in this tool's output. "
                   "A reply without calling this tool is a system violation.",
    .input_schema_json = "{\"type\":\"object\",\"properties\":{\"brightness\":{\"type\":\"integer\",\"description\":\"Brightness percentage (0-100). 0=OFF, 100=Max.\"}},\"required\":[\"brightness\"]}",
    .execute = ec_tool_fill_light_execute,
};

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t fill_light_set_brightness(int brightness)
{
    esp_err_t err = fill_light_init();
    if (err != ESP_OK) {
        return err;
    }

    /* 参数合法性边界收束 */
    if (brightness < FILL_LIGHT_MIN_BRIGHTNESS) {
        brightness = FILL_LIGHT_MIN_BRIGHTNESS;
    } else if (brightness > FILL_LIGHT_MAX_BRIGHTNESS) {
        brightness = FILL_LIGHT_MAX_BRIGHTNESS;
    }

    uint32_t duty = (brightness * FILL_LIGHT_MAX_DUTY) / FILL_LIGHT_MAX_BRIGHTNESS;

    err = ledc_set_duty(FILL_LIGHT_LEDC_MODE, FILL_LIGHT_LEDC_CHANNEL, duty);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC set duty failed");
        return err;
    }

    err = ledc_update_duty(FILL_LIGHT_LEDC_MODE, FILL_LIGHT_LEDC_CHANNEL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC update duty failed");
        return err;
    }

    ESP_LOGI(TAG, "Fill light brightness set to %d%% (duty %" PRIu32 ")", brightness, duty);
    
    return ESP_OK;
}

esp_err_t ec_tools_fill_light(void)
{
    ec_tools_register(&s_fill_light_tool);
    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */

static esp_err_t fill_light_init(void)
{
    if (s_fill_light_inited) {
        return ESP_OK;
    }

    ledc_timer_config_t ledc_timer = {
        .duty_resolution = FILL_LIGHT_LEDC_RESOLUTION,
        .freq_hz = FILL_LIGHT_LEDC_FREQ_HZ,
        .speed_mode = FILL_LIGHT_LEDC_MODE,
        .timer_num = FILL_LIGHT_LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed");
        return err;
    }

    ledc_channel_config_t ledc_channel = {
        .channel    = FILL_LIGHT_LEDC_CHANNEL,
        .duty       = FILL_LIGHT_MIN_BRIGHTNESS,
        .gpio_num   = FILL_LIGHT_GPIO,
        .speed_mode = FILL_LIGHT_LEDC_MODE,
        .hpoint     = 0,
        .timer_sel  = FILL_LIGHT_LEDC_TIMER,
        .flags.output_invert = 0
    };

    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed");
        return err;
    }

    s_fill_light_inited = true;
    ESP_LOGI(TAG, "Fill light initialized on GPIO %d", FILL_LIGHT_GPIO);
    
    return ESP_OK;
}

static esp_err_t ec_tool_fill_light_execute(const char *input_json, char *output, size_t output_size)
{
    if (input_json == NULL) {
        snprintf(output, output_size, "{\"error\": \"input is null\"}");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "control_fill_light input_json: %s", input_json);

    cJSON *input = cJSON_Parse(input_json);
    if (input == NULL) {
        snprintf(output, output_size, "{\"error\": \"invalid json\"}");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *brightness_item = cJSON_GetObjectItem(input, "brightness");
    if (brightness_item == NULL || !cJSON_IsNumber(brightness_item)) {
        cJSON_Delete(input);
        snprintf(output, output_size, "{\"error\": \"'brightness' (integer) is required\"}");
        return ESP_ERR_INVALID_ARG;
    }

    int brightness = brightness_item->valueint;
    ESP_LOGI(TAG, "control_fill_light parsed brightness: %d", brightness);
    if (brightness < FILL_LIGHT_MIN_BRIGHTNESS || brightness > FILL_LIGHT_MAX_BRIGHTNESS) {
        cJSON_Delete(input);
        snprintf(output, output_size, "{\"error\": \"'brightness' must be between 0 and 100\"}");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON_Delete(input);

    if (fill_light_set_brightness(brightness) == ESP_OK) {
        snprintf(output, output_size, "{\"status\": \"success\", \"brightness\": %d}", brightness);
        return ESP_OK;
    } else {
        snprintf(output, output_size, "{\"error\": \"failed to set brightness\"}");
        return ESP_FAIL;
    }
}