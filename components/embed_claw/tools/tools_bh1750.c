/**
 * @file ec_tool_bh1750.c
 * @author your_name (your_email@example.com)
 * @brief BH1750 light sensor tool implementation
 * @version 0.1
 * @date 2026-03-21
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "core/ec_tools.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c.h"


/* ==================== [Defines] =========================================== */

#define TAG "ec_tool_bh1750"

/* 定义 BH1750 的 I2C 地址与指令 */
#define BH1750_I2C_ADDR             0x23
#define BH1750_CMD_PWR_ON           0x01  /* 打开模块等待测量指令 */
#define BH1750_CMD_ONE_TIME_H_RES   0x20  /* 一次高分辨率模式，测量后自动转入 PowerDown */
#define BH1750_MEASURE_TIME_MS      180   /* 高分辨率模式典型转换时间 */
#define BH1750_DIVISOR              1.2f  /* 光照强度计算系数 */

/* I2C 主机参数 */
#define I2C_MASTER_SCL_IO           8      /*!< 根据实际接线的 SCL 引脚修改 */
#define I2C_MASTER_SDA_IO           9      /*!< 根据实际接线的 SDA 引脚修改 */
#define I2C_MASTER_NUM              0      /*!< I2C 端口号 */
#define I2C_MASTER_FREQ_HZ          100000 /*!< I2C 时钟频率 100KHz */
#define I2C_MASTER_TIMEOUT_MS       1000

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static esp_err_t i2c_master_init(void);
static esp_err_t ec_tool_bh1750_execute(const char *input_json, char *output, size_t output_size);

/* ==================== [Static Variables] ================================== */

static const ec_tools_t s_bh1750_tool = {
    .name = "read_light_intensity",
    .description = "Read the current ambient light intensity in lux from the BH1750 sensor.",
    .input_schema_json = "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
    .execute = ec_tool_bh1750_execute,
};

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t ec_tools_bh1750(void)
{
    esp_err_t err = i2c_master_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BH1750 I2C init failed: %s", esp_err_to_name(err));
        return err;
    }

    ec_tools_register(&s_bh1750_tool);
    return ESP_OK;
}

static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;

    ESP_LOGI(TAG, "Configuring I2C master: port=%d, SDA=%d, SCL=%d, freq=%dHz",
             i2c_master_port, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(i2c_master_port, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C master initialized successfully");

    return ESP_OK;
}

/**
 * @brief 底层 BH1750 读取硬件接口
 */
esp_err_t s_bh1750_read_lux(float *lux)
{
    esp_err_t err;
    uint8_t cmd;
    uint8_t data[2] = {0};
    uint16_t level = 0;

    if (lux == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 1. 发送 Power On 启动命令
    cmd = BH1750_CMD_PWR_ON;
    err = i2c_master_write_to_device(I2C_MASTER_NUM, BH1750_I2C_ADDR, &cmd, 1, 
                                     I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BH1750 i2c write PWR_ON failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGD(TAG, "BH1750: sent PWR_ON");

    // 2. 发送 One-Time 高分辨率模式指令 (测量一次后自动断电)
    cmd = BH1750_CMD_ONE_TIME_H_RES;
    err = i2c_master_write_to_device(I2C_MASTER_NUM, BH1750_I2C_ADDR, &cmd, 1, 
                                     I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BH1750 i2c write ONE_TIME_H_RES failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGD(TAG, "BH1750: sent ONE_TIME_H_RES");

    // 3. 等待传感器完成转换测量 (手册要求高分辨率下至少等 120ms，这里给 180ms 留出裕量)
    vTaskDelay(pdMS_TO_TICKS(BH1750_MEASURE_TIME_MS));

    // 4. 读取 2 字节的测量数据
    err = i2c_master_read_from_device(I2C_MASTER_NUM, BH1750_I2C_ADDR, data, 2,
                                      I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BH1750 i2c read failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(TAG, "BH1750 raw bytes: 0x%02x 0x%02x", data[0], data[1]);

    // 5. 数据解析与计算
    level = (data[0] << 8) | data[1];
    *lux = (float)level / BH1750_DIVISOR;

    ESP_LOGI(TAG, "BH1750 level=%u => %.2f lux", level, *lux);

    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */

static esp_err_t ec_tool_bh1750_execute(const char *input_json, char *output, size_t output_size)
{
    float lux = 0.0f;
    esp_err_t err;

    if (output == NULL || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    err = s_bh1750_read_lux(&lux);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BH1750 read failed: %s", esp_err_to_name(err));
        snprintf(output, output_size, "{\"error\": \"Failed to read BH1750 sensor, err: %d\"}", err);
        return err;
    }

    snprintf(output, output_size, "{\"lux\": %.2f}", lux);
    
    return ESP_OK;
}

