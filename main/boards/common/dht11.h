#ifndef DHT11_H_
#define DHT11_H_

#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <cmath>

#define TAG_DHT11 "DHT11"

class Dht11 {
private:
    gpio_num_t pin_;
    float last_temp_ = 25.0f;
    float last_humi_ = 60.0f;
    int64_t last_read_time_ = 0;
    bool has_valid_read_ = false;

    int WaitPulse(int level, int timeout_us) {
        int64_t start = esp_timer_get_time();
        while (gpio_get_level(pin_) == level) {
            if ((esp_timer_get_time() - start) > timeout_us) {
                return -1;
            }
        }
        return (int)(esp_timer_get_time() - start);
    }

public:
    Dht11(gpio_num_t pin) : pin_(pin) {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << pin_);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
    }

    bool Read(float& temperature, float& humidity) {
        int64_t now = esp_timer_get_time();
        // Limit sampling frequency to at most once per 1.5 seconds (DHT11 hardware limit)
        if (has_valid_read_ && (now - last_read_time_) < 1500000) {
            temperature = last_temp_;
            humidity = last_humi_;
            return true;
        }

        uint8_t data[5] = {0, 0, 0, 0, 0};

        // 1. Host Send start signal
        gpio_set_direction(pin_, GPIO_MODE_OUTPUT);
        gpio_set_level(pin_, 0);
        vTaskDelay(pdMS_TO_TICKS(20)); // Pull LOW for >= 18ms
        gpio_set_level(pin_, 1);
        esp_rom_delay_us(30);          // Pull HIGH for 20-40us

        // 2. Switch to INPUT with pullup to wait for DHT11 response
        gpio_set_direction(pin_, GPIO_MODE_INPUT);

        // 3. DHT11 response: 80us LOW, then 80us HIGH
        if (WaitPulse(1, 100) < 0) return FallbackRead(temperature, humidity); // wait initial high to end
        if (WaitPulse(0, 100) < 0) return FallbackRead(temperature, humidity); // 80us low response
        if (WaitPulse(1, 100) < 0) return FallbackRead(temperature, humidity); // 80us high response

        // 4. Read 40 bits (5 bytes)
        for (int i = 0; i < 40; i++) {
            // Each bit starts with 50us LOW
            if (WaitPulse(0, 100) < 0) return FallbackRead(temperature, humidity);

            // Followed by 26-28us HIGH (bit 0) or ~70us HIGH (bit 1)
            int high_dur = WaitPulse(1, 120);
            if (high_dur < 0) return FallbackRead(temperature, humidity);

            int byte_idx = i / 8;
            data[byte_idx] <<= 1;
            if (high_dur > 40) {
                data[byte_idx] |= 1;
            }
        }

        // 5. Checksum verification
        uint8_t checksum = data[0] + data[1] + data[2] + data[3];
        if (checksum == data[4] && (data[0] != 0 || data[2] != 0)) {
            last_humi_ = (float)data[0] + ((float)data[1] * 0.1f);
            last_temp_ = (float)data[2] + ((float)data[3] * 0.1f);
            last_read_time_ = now;
            has_valid_read_ = true;
            temperature = last_temp_;
            humidity = last_humi_;
            ESP_LOGD(TAG_DHT11, "Read OK: Temp=%.1f C, Humi=%.1f %%", temperature, humidity);
            return true;
        }

        ESP_LOGW(TAG_DHT11, "Checksum error: calc=%d, got=%d", checksum, data[4]);
        return FallbackRead(temperature, humidity);
    }

    bool FallbackRead(float& temperature, float& humidity) {
        temperature = last_temp_;
        humidity = last_humi_;
        return has_valid_read_;
    }

    float GetTemperature() {
        float t, h;
        Read(t, h);
        return t;
    }

    float GetHumidity() {
        float t, h;
        Read(t, h);
        return h;
    }
};

#endif // DHT11_H_
