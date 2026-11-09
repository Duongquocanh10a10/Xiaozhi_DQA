#ifndef ENV_DISPLAY_MANAGER_H_
#define ENV_DISPLAY_MANAGER_H_

#include "display/lcd_display.h"
#include "dht11.h"
#include <lvgl.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>

#define TAG_ENV_UI "EnvDisplayManager"

enum DisplayMode {
    kModeXiaozhi = 1,
    kModeTempChart = 2,
    kModeHumiChart = 3,
    kModeDual = 4
};

class EnvDisplayManager {
private:
    LcdDisplay* display_ = nullptr;
    Dht11* dht_ = nullptr;
    DisplayMode current_mode_ = kModeXiaozhi;

    // LVGL widgets
    lv_obj_t* overlay_container_ = nullptr;
    
    // View 2: Temperature Chart
    lv_obj_t* temp_view_ = nullptr;
    lv_obj_t* temp_val_label_ = nullptr;
    lv_obj_t* temp_chart_ = nullptr;
    lv_chart_series_t* temp_series_ = nullptr;

    // View 3: Humidity Chart
    lv_obj_t* humi_view_ = nullptr;
    lv_obj_t* humi_val_label_ = nullptr;
    lv_obj_t* humi_chart_ = nullptr;
    lv_chart_series_t* humi_series_ = nullptr;

    // View 4: Dual Display
    lv_obj_t* dual_view_ = nullptr;
    lv_obj_t* dual_temp_label_ = nullptr;
    lv_obj_t* dual_humi_label_ = nullptr;
    lv_obj_t* dual_temp_bar_ = nullptr;
    lv_obj_t* dual_humi_bar_ = nullptr;

    TaskHandle_t update_task_handle_ = nullptr;

    void CreateViews() {
        if (!display_) return;
        DisplayLockGuard lock(display_);

        auto screen = lv_screen_active();
        if (!screen) return;

        // Overlay container covers the full 128x160 screen with dark background
        overlay_container_ = lv_obj_create(screen);
        lv_obj_set_size(overlay_container_, 128, 160);
        lv_obj_align(overlay_container_, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_bg_color(overlay_container_, lv_color_hex(0x121212), 0);
        lv_obj_set_style_bg_opa(overlay_container_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(overlay_container_, 0, 0);
        lv_obj_set_style_pad_all(overlay_container_, 2, 0);
        lv_obj_set_style_radius(overlay_container_, 0, 0);

        // ==========================================
        // VIEW 2: TEMPERATURE & CHART
        // ==========================================
        temp_view_ = lv_obj_create(overlay_container_);
        lv_obj_set_size(temp_view_, 124, 156);
        lv_obj_align(temp_view_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(temp_view_, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_border_color(temp_view_, lv_color_hex(0xFF5722), 0);
        lv_obj_set_style_border_width(temp_view_, 1, 0);
        lv_obj_set_style_pad_all(temp_view_, 3, 0);

        lv_obj_t* temp_title = lv_label_create(temp_view_);
        lv_label_set_text(temp_title, "NHIET DO");
        lv_obj_set_style_text_color(temp_title, lv_color_hex(0xFF8A65), 0);
        lv_obj_align(temp_title, LV_ALIGN_TOP_MID, 0, 2);

        temp_val_label_ = lv_label_create(temp_view_);
        lv_label_set_text(temp_val_label_, "--.- °C");
        lv_obj_set_style_text_color(temp_val_label_, lv_color_hex(0xFF5722), 0);
        lv_obj_align(temp_val_label_, LV_ALIGN_TOP_MID, 0, 20);

        temp_chart_ = lv_chart_create(temp_view_);
        lv_obj_set_size(temp_chart_, 114, 96);
        lv_obj_align(temp_chart_, LV_ALIGN_BOTTOM_MID, 0, -2);
        lv_chart_set_type(temp_chart_, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(temp_chart_, 16);
        lv_chart_set_range(temp_chart_, LV_CHART_AXIS_PRIMARY_Y, 15, 45);
        lv_obj_set_style_bg_color(temp_chart_, lv_color_hex(0x262626), 0);
        lv_obj_set_style_border_width(temp_chart_, 0, 0);
        lv_obj_set_style_pad_all(temp_chart_, 2, 0);
        temp_series_ = lv_chart_add_series(temp_chart_, lv_color_hex(0xFF5722), LV_CHART_AXIS_PRIMARY_Y);

        // ==========================================
        // VIEW 3: HUMIDITY & CHART
        // ==========================================
        humi_view_ = lv_obj_create(overlay_container_);
        lv_obj_set_size(humi_view_, 124, 156);
        lv_obj_align(humi_view_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(humi_view_, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_border_color(humi_view_, lv_color_hex(0x2196F3), 0);
        lv_obj_set_style_border_width(humi_view_, 1, 0);
        lv_obj_set_style_pad_all(humi_view_, 3, 0);

        lv_obj_t* humi_title = lv_label_create(humi_view_);
        lv_label_set_text(humi_title, "DO AM KHI");
        lv_obj_set_style_text_color(humi_title, lv_color_hex(0x64B5F6), 0);
        lv_obj_align(humi_title, LV_ALIGN_TOP_MID, 0, 2);

        humi_val_label_ = lv_label_create(humi_view_);
        lv_label_set_text(humi_val_label_, "--.- %");
        lv_obj_set_style_text_color(humi_val_label_, lv_color_hex(0x2196F3), 0);
        lv_obj_align(humi_val_label_, LV_ALIGN_TOP_MID, 0, 20);

        humi_chart_ = lv_chart_create(humi_view_);
        lv_obj_set_size(humi_chart_, 114, 96);
        lv_obj_align(humi_chart_, LV_ALIGN_BOTTOM_MID, 0, -2);
        lv_chart_set_type(humi_chart_, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(humi_chart_, 16);
        lv_chart_set_range(humi_chart_, LV_CHART_AXIS_PRIMARY_Y, 20, 100);
        lv_obj_set_style_bg_color(humi_chart_, lv_color_hex(0x262626), 0);
        lv_obj_set_style_border_width(humi_chart_, 0, 0);
        lv_obj_set_style_pad_all(humi_chart_, 2, 0);
        humi_series_ = lv_chart_add_series(humi_chart_, lv_color_hex(0x2196F3), LV_CHART_AXIS_PRIMARY_Y);

        // ==========================================
        // VIEW 4: DUAL SENSOR DASHBOARD
        // ==========================================
        dual_view_ = lv_obj_create(overlay_container_);
        lv_obj_set_size(dual_view_, 124, 156);
        lv_obj_align(dual_view_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(dual_view_, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_border_color(dual_view_, lv_color_hex(0x4CAF50), 0);
        lv_obj_set_style_border_width(dual_view_, 1, 0);
        lv_obj_set_style_pad_all(dual_view_, 4, 0);

        lv_obj_t* dual_title = lv_label_create(dual_view_);
        lv_label_set_text(dual_title, "MOI TRUONG");
        lv_obj_set_style_text_color(dual_title, lv_color_hex(0x81C784), 0);
        lv_obj_align(dual_title, LV_ALIGN_TOP_MID, 0, 0);

        // Card Nhiệt độ
        lv_obj_t* card_temp = lv_obj_create(dual_view_);
        lv_obj_set_size(card_temp, 114, 58);
        lv_obj_align(card_temp, LV_ALIGN_TOP_MID, 0, 18);
        lv_obj_set_style_bg_color(card_temp, lv_color_hex(0x2D1B17), 0);
        lv_obj_set_style_border_color(card_temp, lv_color_hex(0xFF5722), 0);
        lv_obj_set_style_border_width(card_temp, 1, 0);
        lv_obj_set_style_pad_all(card_temp, 2, 0);

        lv_obj_t* lbl_t = lv_label_create(card_temp);
        lv_label_set_text(lbl_t, "Nhiet do:");
        lv_obj_set_style_text_color(lbl_t, lv_color_hex(0xFFAB91), 0);
        lv_obj_align(lbl_t, LV_ALIGN_TOP_LEFT, 4, 2);

        dual_temp_label_ = lv_label_create(card_temp);
        lv_label_set_text(dual_temp_label_, "--.- °C");
        lv_obj_set_style_text_color(dual_temp_label_, lv_color_hex(0xFF5722), 0);
        lv_obj_align(dual_temp_label_, LV_ALIGN_BOTTOM_RIGHT, -4, -4);

        dual_temp_bar_ = lv_bar_create(card_temp);
        lv_obj_set_size(dual_temp_bar_, 102, 6);
        lv_obj_align(dual_temp_bar_, LV_ALIGN_BOTTOM_MID, 0, -2);
        lv_bar_set_range(dual_temp_bar_, 0, 50);
        lv_bar_set_value(dual_temp_bar_, 25, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(dual_temp_bar_, lv_color_hex(0xFF5722), LV_PART_INDICATOR);

        // Card Độ ẩm
        lv_obj_t* card_humi = lv_obj_create(dual_view_);
        lv_obj_set_size(card_humi, 114, 58);
        lv_obj_align(card_humi, LV_ALIGN_BOTTOM_MID, 0, -2);
        lv_obj_set_style_bg_color(card_humi, lv_color_hex(0x132533), 0);
        lv_obj_set_style_border_color(card_humi, lv_color_hex(0x2196F3), 0);
        lv_obj_set_style_border_width(card_humi, 1, 0);
        lv_obj_set_style_pad_all(card_humi, 2, 0);

        lv_obj_t* lbl_h = lv_label_create(card_humi);
        lv_label_set_text(lbl_h, "Do am khi:");
        lv_obj_set_style_text_color(lbl_h, lv_color_hex(0x90CAF9), 0);
        lv_obj_align(lbl_h, LV_ALIGN_TOP_LEFT, 4, 2);

        dual_humi_label_ = lv_label_create(card_humi);
        lv_label_set_text(dual_humi_label_, "--.- %");
        lv_obj_set_style_text_color(dual_humi_label_, lv_color_hex(0x2196F3), 0);
        lv_obj_align(dual_humi_label_, LV_ALIGN_BOTTOM_RIGHT, -4, -4);

        dual_humi_bar_ = lv_bar_create(card_humi);
        lv_obj_set_size(dual_humi_bar_, 102, 6);
        lv_obj_align(dual_humi_bar_, LV_ALIGN_BOTTOM_MID, 0, -2);
        lv_bar_set_range(dual_humi_bar_, 0, 100);
        lv_bar_set_value(dual_humi_bar_, 60, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(dual_humi_bar_, lv_color_hex(0x2196F3), LV_PART_INDICATOR);

        // Default to Mode 1 (Xiaozhi mode: overlay hidden)
        ApplyModeVisibility(kModeXiaozhi);
    }

    void ApplyModeVisibility(DisplayMode mode) {
        if (!overlay_container_) return;

        if (mode == kModeXiaozhi) {
            lv_obj_add_flag(overlay_container_, LV_OBJ_FLAG_HIDDEN);
            return;
        }

        lv_obj_remove_flag(overlay_container_, LV_OBJ_FLAG_HIDDEN);

        // Hide all 3 sub-views first
        lv_obj_add_flag(temp_view_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(humi_view_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(dual_view_, LV_OBJ_FLAG_HIDDEN);

        // Show requested view
        if (mode == kModeTempChart) {
            lv_obj_remove_flag(temp_view_, LV_OBJ_FLAG_HIDDEN);
        } else if (mode == kModeHumiChart) {
            lv_obj_remove_flag(humi_view_, LV_OBJ_FLAG_HIDDEN);
        } else if (mode == kModeDual) {
            lv_obj_remove_flag(dual_view_, LV_OBJ_FLAG_HIDDEN);
        }
    }

public:
    EnvDisplayManager(LcdDisplay* display, Dht11* dht)
        : display_(display), dht_(dht) {
    }

    void Start() {
        CreateViews();

        // Start background sensor sampling & UI refresh task (runs every 2 seconds)
        xTaskCreate([](void* arg) {
            auto self = static_cast<EnvDisplayManager*>(arg);
            while (true) {
                vTaskDelay(pdMS_TO_TICKS(2000));
                self->UpdateData();
            }
        }, "env_ui_task", 4096, this, 3, &update_task_handle_);
    }

    void UpdateData() {
        if (!dht_ || !display_) return;

        float t = 0.0f, h = 0.0f;
        dht_->Read(t, h);

        DisplayLockGuard lock(display_);
        if (!overlay_container_) return;

        char buf[32];

        // 1. Update Temperature elements
        snprintf(buf, sizeof(buf), "%.1f °C", t);
        if (temp_val_label_) lv_label_set_text(temp_val_label_, buf);
        if (dual_temp_label_) lv_label_set_text(dual_temp_label_, buf);
        if (dual_temp_bar_) lv_bar_set_value(dual_temp_bar_, (int)t, LV_ANIM_OFF);
        if (temp_chart_ && temp_series_) {
            lv_chart_set_next_value(temp_chart_, temp_series_, (int)t);
            lv_chart_refresh(temp_chart_);
        }

        // 2. Update Humidity elements
        snprintf(buf, sizeof(buf), "%.1f %%", h);
        if (humi_val_label_) lv_label_set_text(humi_val_label_, buf);
        if (dual_humi_label_) lv_label_set_text(dual_humi_label_, buf);
        if (dual_humi_bar_) lv_bar_set_value(dual_humi_bar_, (int)h, LV_ANIM_OFF);
        if (humi_chart_ && humi_series_) {
            lv_chart_set_next_value(humi_chart_, humi_series_, (int)h);
            lv_chart_refresh(humi_chart_);
        }
    }

    void SetMode(DisplayMode mode) {
        if (!display_) return;
        DisplayLockGuard lock(display_);
        current_mode_ = mode;
        ApplyModeVisibility(current_mode_);
        ESP_LOGI(TAG_ENV_UI, "Display switched to Mode %d", (int)current_mode_);
    }

    DisplayMode GetMode() const {
        return current_mode_;
    }

    // Button 1: Chuyen vong tron giua Mode 1, Mode 2, Mode 3
    void CycleMode123() {
        if (current_mode_ == kModeXiaozhi) {
            SetMode(kModeTempChart);
        } else if (current_mode_ == kModeTempChart) {
            SetMode(kModeHumiChart);
        } else {
            SetMode(kModeXiaozhi);
        }
    }

    // Button 2: Chuyen sang Mode 4 (Hoac toggle ve Mode 1 neu dang o Mode 4)
    void SwitchToMode4() {
        if (current_mode_ != kModeDual) {
            SetMode(kModeDual);
        } else {
            SetMode(kModeXiaozhi);
        }
    }
};

#endif // ENV_DISPLAY_MANAGER_H_
