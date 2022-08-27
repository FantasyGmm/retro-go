#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include "rg_system.h"
#include "rg_i2c.h"

#if defined(RG_GPIO_I2C_SDA) && defined(RG_GPIO_I2C_SCL)
    #include <driver/i2c.h>
    #define USE_I2C_DRIVER
#endif

#define TRY(x) if ((err = (x)) != ESP_OK) { goto fail; }

static bool i2c_initialized = false;
static rg_i2c_gpio_t gpio_extender = RG_I2C_NONE;
static uint8_t gpio_extender_addr = 0;


bool rg_i2c_init(void)
{
    if (i2c_initialized)
        return true;
#ifdef USE_I2C_DRIVER
    const i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = RG_GPIO_I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = RG_GPIO_I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    esp_err_t err = ESP_FAIL;

    TRY(i2c_param_config(I2C_NUM_0, &i2c_config));
    TRY(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
    RG_LOGI("I2C driver ready (SDA:%d SCL:%d).\n", i2c_config.sda_io_num, i2c_config.scl_io_num);
    i2c_initialized = true;
    return true;
fail:
    RG_LOGE("Failed to initialize I2C driver. err=0x%x\n", err);
#else
    RG_LOGE("I2C driver is not available on this device.\n");
#endif
    i2c_initialized = false;
    return false;
}

bool rg_i2c_deinit(void)
{
#ifdef USE_I2C_DRIVER
    if (i2c_initialized && i2c_driver_delete(I2C_NUM_0) == ESP_OK)
        RG_LOGI("I2C driver terminated.\n");
#endif
    i2c_initialized = false;
    return true;
}

bool rg_i2c_read(uint8_t addr, int reg, void *read_data, size_t read_len)
{
    esp_err_t err = ESP_FAIL;

    if (!i2c_initialized)
        goto fail;

#ifdef USE_I2C_DRIVER
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (reg >= 0)
    {
        TRY(i2c_master_start(cmd));
        TRY(i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true));
        TRY(i2c_master_write_byte(cmd, (uint8_t)reg, true));
    }
    TRY(i2c_master_start(cmd));
    TRY(i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true));
    TRY(i2c_master_read(cmd, read_data, read_len, I2C_MASTER_LAST_NACK));
    TRY(i2c_master_stop(cmd));
    TRY(i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(500)));
    i2c_cmd_link_delete(cmd);
    return true;
#endif

fail:
    RG_LOGE("Read from 0x%02x failed. reg=%d, err=0x%x\n", addr, reg, err, i2c_initialized);
    return false;
}

bool rg_i2c_write(uint8_t addr, int reg, const void *write_data, size_t write_len)
{
    esp_err_t err = ESP_FAIL;

    if (!i2c_initialized)
        goto fail;

#ifdef USE_I2C_DRIVER
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    TRY(i2c_master_start(cmd));
    TRY(i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true));
    if (reg >= 0)
    {
        TRY(i2c_master_write_byte(cmd, (uint8_t)reg, true));
    }
    TRY(i2c_master_write(cmd, (void *)write_data, write_len, true));
    TRY(i2c_master_stop(cmd));
    TRY(i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(500)));
    i2c_cmd_link_delete(cmd);
    return true;
#endif

fail:
    RG_LOGE("Write to 0x%02x failed. reg=%d, err=0x%x, init=%d\n", addr, reg, err, i2c_initialized);
    return false;
}

bool rg_i2c_write_byte(uint8_t addr, uint8_t reg, uint8_t value)
{
    return rg_i2c_write(addr, reg, &value, 1);
}

uint8_t rg_i2c_read_byte(uint8_t addr, uint8_t reg)
{
    uint8_t value = 0;
    return rg_i2c_read(addr, reg, &value, 1) ? value : 0;
}

bool rg_i2c_gpio_init(rg_i2c_gpio_t device_type, uint8_t addr)
{
    RG_ASSERT(gpio_extender == RG_I2C_NONE, "GPIO extender already initialized!");

    if (!i2c_initialized && !rg_i2c_init())
        goto fail;

    // if (device_type == RG_I2C_AW9523)
    {
        gpio_extender = RG_I2C_AW9523;
        gpio_extender_addr = addr ?: AW9523_DEFAULT_ADDR;

        rg_i2c_write_byte(gpio_extender_addr, AW9523_REG_SOFTRESET, 0);
        vTaskDelay(pdMS_TO_TICKS(10));

        uint8_t id = rg_i2c_read_byte(gpio_extender_addr, AW9523_REG_CHIPID);
        RG_LOGI("AW9523 ID code 0x%x found\n", id);
        assert(id == 0x23);

        rg_i2c_write_byte(gpio_extender_addr, AW9523_REG_CONFIG0, 0xFF);
        rg_i2c_write_byte(gpio_extender_addr, AW9523_REG_CONFIG0+1, 0xFF);
        rg_i2c_write_byte(gpio_extender_addr, AW9523_REG_LEDMODE, 0xFF);
        rg_i2c_write_byte(gpio_extender_addr, AW9523_REG_LEDMODE+1, 0xFF);
        rg_i2c_write_byte(gpio_extender_addr, AW9523_REG_GCR, 1<<4);
        return true;
    }

fail:
    gpio_extender = RG_I2C_NONE;
    gpio_extender_addr = 0;
    return false;
}

bool rg_i2c_gpio_deinit(void)
{
    gpio_extender = RG_I2C_NONE;
    gpio_extender_addr = 0;
    return true;
}

bool rg_i2c_gpio_setup_port(int port, uint16_t config)
{
    return rg_i2c_write_byte(gpio_extender_addr, AW9523_REG_CONFIG0, config & 0xFF)
        && rg_i2c_write_byte(gpio_extender_addr, AW9523_REG_CONFIG0+1, config >> 8);
}

uint16_t rg_i2c_gpio_read_port(int port)
{
    return (rg_i2c_read_byte(gpio_extender_addr, AW9523_REG_INPUT0+1) << 8)
         | (rg_i2c_read_byte(gpio_extender_addr, AW9523_REG_INPUT0));
}

bool rg_i2c_gpio_write_port(int port, uint16_t value)
{
    return rg_i2c_write_byte(gpio_extender_addr, AW9523_REG_OUTPUT0, value & 0xFF)
        && rg_i2c_write_byte(gpio_extender_addr, AW9523_REG_OUTPUT0+1, value >> 8);
}

int rg_i2c_gpio_get_level(int pin)
{
    return (rg_i2c_gpio_read_port(0) >> pin) & 1;
}

bool rg_i2c_gpio_set_level(int pin, int level)
{
    uint16_t pins = (rg_i2c_read_byte(gpio_extender_addr, AW9523_REG_OUTPUT0+1) << 8)
                  | (rg_i2c_read_byte(gpio_extender_addr, AW9523_REG_OUTPUT0));
    pins &= ~(1UL << pin);
    pins |= (level & 1) << pin;
    return rg_i2c_write_byte(gpio_extender_addr, AW9523_REG_OUTPUT0, pins & 0xFF)
        && rg_i2c_write_byte(gpio_extender_addr, AW9523_REG_OUTPUT0+1, pins >> 8);
}
