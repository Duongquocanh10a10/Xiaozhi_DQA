#ifndef __SERVO_CONTROLLER_H__
#define __SERVO_CONTROLLER_H__

#include "mcp_server.h"
#include <driver/ledc.h>
#include <esp_log.h>

static const char* SERVO_TAG = "ServoController";

class ServoController {
private:
    gpio_num_t gpio_num_;
    int current_angle_ = 90;
    bool initialized_ = false;

    void SetAngleHardware(int angle) {
        if (!initialized_) return;
        if (angle < 0) angle = 0;
        if (angle > 180) angle = 180;
        current_angle_ = angle;

        // 50Hz chu ky 20ms (20000us). Timer 14-bit (16384 steps)
        // 0.5ms (0 do) -> 410 duty
        // 2.5ms (180 do) -> 2048 duty
        uint32_t duty = 410 + (angle * (2048 - 410)) / 180;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        ESP_LOGI(SERVO_TAG, "Servo angle set to: %d (duty: %lu)", angle, (unsigned long)duty);
    }

public:
    ServoController(gpio_num_t gpio_num) : gpio_num_(gpio_num) {
        if (gpio_num_ == GPIO_NUM_NC) {
            return;
        }

        ledc_timer_config_t ledc_timer = {
            .speed_mode       = LEDC_LOW_SPEED_MODE,
            .duty_resolution  = LEDC_TIMER_14_BIT,
            .timer_num        = LEDC_TIMER_0,
            .freq_hz          = 50, // 50Hz cho dong co servo
            .clk_cfg          = LEDC_AUTO_CLK
        };
        ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

        ledc_channel_config_t ledc_channel = {
            .gpio_num       = gpio_num_,
            .speed_mode     = LEDC_LOW_SPEED_MODE,
            .channel        = LEDC_CHANNEL_0,
            .intr_type      = LEDC_INTR_DISABLE,
            .timer_sel      = LEDC_TIMER_0,
            .duty           = 0,
            .hpoint         = 0
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
        initialized_ = true;

        // Dat goc ban dau 90 do
        SetAngleHardware(90);

        // Dang ky MCP Tools cho Large Language Model goi qua giong noi
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool("self.servo.get_angle", "Get the current angle of the servo motor (0 to 180 degrees)", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            return "{\"angle\": " + std::to_string(current_angle_) + "}";
        });

        mcp_server.AddTool("self.servo.set_angle", "Rotate the servo motor to a specific angle (0 to 180 degrees).", 
            PropertyList({Property("angle", kPropertyTypeInteger, 0, 180)}), 
            [this](const PropertyList& properties) -> ReturnValue {
                int angle = properties["angle"].value<int>();
                SetAngleHardware(angle);
                return "{\"success\": true, \"angle\": " + std::to_string(angle) + "}";
            });
    }

    int GetAngle() const {
        return current_angle_;
    }

    void SetAngle(int angle) {
        SetAngleHardware(angle);
    }
};

#endif // __SERVO_CONTROLLER_H__
