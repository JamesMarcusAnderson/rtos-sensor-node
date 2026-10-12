#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define I2C_PORT i2c0
#define I2C_SDA  4
#define I2C_SCL  5
#define TMP102_ADDR 0x48
#define LED_PIN 25
#define I2C_TIMEOUT_US 25000
#define ADC_VREF 3.3f

/* One flag bit per supervised task. The watchdog task only feeds the hardware
 * watchdog when every flag is set, so a single hung task causes a reset
 * instead of being masked by an independent feeder. */
#define FLAG_SENSOR    (1u << 0)
#define FLAG_LOG       (1u << 1)
#define FLAG_HEARTBEAT (1u << 2)
#define FLAGS_ALL      (FLAG_SENSOR | FLAG_LOG | FLAG_HEARTBEAT)

static volatile uint32_t g_task_flags = 0;

/* Guards g_reading. The writer is sensor_task, the reader is log_task; without
 * this the log can print a temperature and voltage from different samples. */
static SemaphoreHandle_t g_data_mutex;

typedef struct {
    float temp_c;
    float voltage;
    uint32_t samples;
    bool sensor_ok;
} reading_t;

static reading_t g_reading = { .sensor_ok = false };

/* Returns true on success. Both I2C calls use a bounded timeout so a missing
 * or wedged sensor cannot block this task forever and starve the lower
 * priority tasks. */
static bool read_tmp102(float *out_c) {
    uint8_t reg = 0x00, buf[2];
    if (i2c_write_timeout_us(I2C_PORT, TMP102_ADDR, &reg, 1, true, I2C_TIMEOUT_US) < 0)
        return false;
    if (i2c_read_timeout_us(I2C_PORT, TMP102_ADDR, buf, 2, false, I2C_TIMEOUT_US) < 0)
        return false;
    int16_t raw = (buf[0] << 4) | (buf[1] >> 4);
    if (raw > 2047) raw -= 4096;      /* 12-bit two's complement */
    *out_c = raw * 0.0625f;
    return true;
}

static float read_vref_scaled(void) {
    /* Internal VREF measure, scaled back to the actual supply rail.
     * 1.111 V is the RP2040 internal reference; input 4 selects it. */
    adc_select_input(4);
    adc_read();                        /* discard first conversion */
    uint16_t raw = adc_read();
    if (raw == 0) return 0.0f;
    return 1.111f * 4095.0f / (float)raw;
}

static void sensor_task(void *p) {
    (void)p;
    adc_init();
    while (1) {
        float temp_c = 0.0f;
        bool ok = read_tmp102(&temp_c);
        float v = read_vref_scaled();
        adc_select_input(0);

        if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_reading.temp_c = temp_c;
            g_reading.voltage = v;
            g_reading.sensor_ok = ok;
            g_reading.samples++;
            xSemaphoreGive(g_data_mutex);
        }
        g_task_flags |= FLAG_SENSOR;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void log_task(void *p) {
    (void)p;
    reading_t r;
    while (1) {
        bool got = false;
        if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            r = g_reading;
            got = true;
            xSemaphoreGive(g_data_mutex);
        }
        if (got) {
            printf("LOG temp=%.2f voltage=%.2f samples=%lu sensor=%s\n",
                   r.temp_c, r.voltage, (unsigned long)r.samples,
                   r.sensor_ok ? "ok" : "fault");
        } else {
            printf("LOG temp=nan voltage=nan samples=0 sensor=mutex_timeout\n");
        }
        g_task_flags |= FLAG_LOG;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void heartbeat_task(void *p) {
    (void)p;
    gpio_init(LED_PIN); gpio_set_dir(LED_PIN, GPIO_OUT);
    while (1) {
        gpio_xor_mask(1u << LED_PIN);
        g_task_flags |= FLAG_HEARTBEAT;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* Highest priority. Does not unconditionally kick the watchdog: it only does so
 * once every supervised task has checked in since the last window. */
static void watchdog_task(void *p) {
    (void)p;
    watchdog_enable(4000, 1);   /* 4 s timeout, paused while a debugger is attached */
    while (1) {
        if ((g_task_flags & FLAGS_ALL) == FLAGS_ALL) {
            watchdog_update();
            g_task_flags = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void) {
    stdio_init_all();
    i2c_init(I2C_PORT, 100 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA); gpio_pull_up(I2C_SCL);

    g_data_mutex = xSemaphoreCreateMutex();

    xTaskCreate(sensor_task,    "sensor",    1024, NULL, 3, NULL);
    xTaskCreate(log_task,       "log",       2048, NULL, 2, NULL);
    xTaskCreate(heartbeat_task, "heartbeat", 512,  NULL, 1, NULL);
    xTaskCreate(watchdog_task,  "watchdog",  512,  NULL, 5, NULL);

    vTaskStartScheduler();
    while (1);
}
