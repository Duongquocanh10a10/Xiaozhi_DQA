#ifndef ENV_DISPLAY_MANAGER_H_
#define ENV_DISPLAY_MANAGER_H_

#include "display/lcd_display.h"
#include "dht11.h"
#include <lvgl.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>
#include <algorithm>

#define TAG_ENV_UI "EnvDisplayManager"
#define HIST_SAMPLES 12

enum DisplayMode {
    kModeXiaozhi = 1,
    kModeTempChart = 2,
    kModeHumiChart = 3,
    kModeDual = 4,
    kModeMusic = 5
};

// Chuyen doi ky tu tieng Viet co dau thanh chu khong dau chuan dep cho man hinh ST7735 (LVGL Basic Font)
inline std::string SanitizeVietnameseForDisplay(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    size_t i = 0;
    while (i < input.size()) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (c < 0x80) {
            out.push_back(input[i]);
            i++;
        } else if ((c & 0xE0) == 0xC0) { // 2-byte UTF-8
            if (i + 1 < input.size()) {
                unsigned char c2 = static_cast<unsigned char>(input[i + 1]);
                uint16_t unicode = ((c & 0x1F) << 6) | (c2 & 0x3F);
                switch (unicode) {
                    // a, A
                    case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3: case 0x00E5:
                    case 0x0103: case 0x0105:
                        out.push_back('a'); break;
                    case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: case 0x00C5:
                    case 0x0102: case 0x0104:
                        out.push_back('A'); break;
                    // d, D
                    case 0x0111: case 0x00F0:
                        out.push_back('d'); break;
                    case 0x0110: case 0x00D0:
                        out.push_back('D'); break;
                    // e, E
                    case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB:
                    case 0x0113: case 0x0115: case 0x0117: case 0x0119: case 0x011B:
                        out.push_back('e'); break;
                    case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB:
                    case 0x0112: case 0x0114: case 0x0116: case 0x0118: case 0x011A:
                        out.push_back('E'); break;
                    // i, I
                    case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF:
                    case 0x0129: case 0x012B: case 0x012D: case 0x012F: case 0x0131:
                        out.push_back('i'); break;
                    case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF:
                    case 0x0128: case 0x012A: case 0x012C: case 0x012E: case 0x0130:
                        out.push_back('I'); break;
                    // o, O
                    case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5: case 0x00F6: case 0x00F8:
                    case 0x014D: case 0x014F: case 0x0151: case 0x01A1:
                        out.push_back('o'); break;
                    case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5: case 0x00D6: case 0x00D8:
                    case 0x014C: case 0x014E: case 0x0150: case 0x01A0:
                        out.push_back('O'); break;
                    // u, U
                    case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC:
                    case 0x0169: case 0x016B: case 0x016D: case 0x016F: case 0x0171: case 0x0173: case 0x01B0:
                        out.push_back('u'); break;
                    case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC:
                    case 0x0168: case 0x016A: case 0x016C: case 0x016E: case 0x0170: case 0x0172: case 0x01AF:
                        out.push_back('U'); break;
                    // y, Y
                    case 0x00FD: case 0x00FF: case 0x0177:
                        out.push_back('y'); break;
                    case 0x00DD: case 0x0176: case 0x0178:
                        out.push_back('Y'); break;
                    default:
                        if (unicode < 0x0300 || unicode > 0x036F) {
                            out.push_back(' ');
                        }
                        break;
                }
                i += 2;
            } else {
                i++;
            }
        } else if ((c & 0xF0) == 0xE0) { // 3-byte UTF-8
            if (i + 2 < input.size()) {
                unsigned char c2 = static_cast<unsigned char>(input[i + 1]);
                unsigned char c3 = static_cast<unsigned char>(input[i + 2]);
                uint32_t unicode = ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                if (unicode >= 0x1EA0 && unicode <= 0x1EB7) {
                    out.push_back((unicode % 2 == 0) ? 'A' : 'a');
                } else if (unicode >= 0x1EB8 && unicode <= 0x1EC7) {
                    out.push_back((unicode % 2 == 0) ? 'E' : 'e');
                } else if (unicode >= 0x1EC8 && unicode <= 0x1ECB) {
                    out.push_back((unicode % 2 == 0) ? 'I' : 'i');
                } else if (unicode >= 0x1ECC && unicode <= 0x1EE3) {
                    out.push_back((unicode % 2 == 0) ? 'O' : 'o');
                } else if (unicode >= 0x1EE4 && unicode <= 0x1EF1) {
                    out.push_back((unicode % 2 == 0) ? 'U' : 'u');
                } else if (unicode >= 0x1EF2 && unicode <= 0x1EF9) {
                    out.push_back((unicode % 2 == 0) ? 'Y' : 'y');
                } else if (unicode == 0x2013 || unicode == 0x2014) {
                    out.push_back('-');
                } else if (unicode == 0x201C || unicode == 0x201D) {
                    out.push_back('"');
                } else if (unicode == 0x2018 || unicode == 0x2019) {
                    out.push_back('\'');
                } else if (unicode == 0x2026) {
                    out.append("...");
                } else {
                    out.push_back(' ');
                }
                i += 3;
            } else {
                i++;
            }
        } else if ((c & 0xF8) == 0xF0) { // 4-byte UTF-8
            i += 4;
        } else {
            i++;
        }
    }
    return out;
}

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
    lv_obj_t* temp_bars_[HIST_SAMPLES] = {nullptr};
    int temp_history_[HIST_SAMPLES] = {0};

    // View 3: Humidity Chart
    lv_obj_t* humi_view_ = nullptr;
    lv_obj_t* humi_val_label_ = nullptr;
    lv_obj_t* humi_bars_[HIST_SAMPLES] = {nullptr};
    int humi_history_[HIST_SAMPLES] = {0};

    // View 4: Dual Display
    lv_obj_t* dual_view_ = nullptr;
    lv_obj_t* dual_temp_label_ = nullptr;
    lv_obj_t* dual_humi_label_ = nullptr;
    lv_obj_t* dual_temp_bar_ = nullptr;
    lv_obj_t* dual_humi_bar_ = nullptr;

    // View 5: Music Player
    lv_obj_t* music_view_ = nullptr;
    lv_obj_t* music_badge_ = nullptr;
    lv_obj_t* music_title_label_ = nullptr;
    lv_obj_t* music_artist_label_ = nullptr;
    lv_obj_t* music_time_label_ = nullptr;
    lv_obj_t* music_status_label_ = nullptr;
    lv_obj_t* music_progress_bar_ = nullptr;
    lv_obj_t* music_eq_bars_[8] = {nullptr};

    // Music playback state
    std::string music_title_ = "Chua phat nhac";
    std::string music_artist_ = "XiaoZhi Music";
    std::string music_platform_ = "Spotify"; // "Spotify", "YouTube Music", "MP3 Online"
    bool music_is_playing_ = false;
    int music_cur_sec_ = 0;
    int music_total_sec_ = 210;

    TaskHandle_t update_task_handle_ = nullptr;

    void EnsureViews() {
        if (overlay_container_ != nullptr) return;
        CreateViews();
    }

    void CreateViews() {
        if (!display_) return;

        // Use lv_layer_top() so the environmental UI sits reliably on top of
        // the Xiaozhi active screen without being obscured by container_
        auto parent_layer = lv_layer_top();
        if (!parent_layer) {
            parent_layer = lv_screen_active();
        }
        if (!parent_layer) return;

        ESP_LOGI(TAG_ENV_UI, "Creating UI overlay on lv_layer_top");

        // Overlay container covers the full 128x160 screen with dark background
        overlay_container_ = lv_obj_create(parent_layer);
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
        lv_obj_set_style_pad_all(temp_view_, 2, 0);

        lv_obj_t* temp_title = lv_label_create(temp_view_);
        lv_label_set_text(temp_title, "NHIET DO");
        lv_obj_set_style_text_color(temp_title, lv_color_hex(0xFF8A65), 0);
        lv_obj_align(temp_title, LV_ALIGN_TOP_MID, 0, 2);

        temp_val_label_ = lv_label_create(temp_view_);
        lv_label_set_text(temp_val_label_, "--.- \xc2\xb0\x43");
        lv_obj_set_style_text_color(temp_val_label_, lv_color_hex(0xFF5722), 0);
        lv_obj_align(temp_val_label_, LV_ALIGN_TOP_MID, 0, 18);

        // Chart container (for 12 history bars)
        lv_obj_t* temp_chart_box = lv_obj_create(temp_view_);
        lv_obj_set_size(temp_chart_box, 116, 92);
        lv_obj_align(temp_chart_box, LV_ALIGN_BOTTOM_MID, 0, -2);
        lv_obj_set_style_bg_color(temp_chart_box, lv_color_hex(0x222222), 0);
        lv_obj_set_style_border_color(temp_chart_box, lv_color_hex(0x333333), 0);
        lv_obj_set_style_border_width(temp_chart_box, 1, 0);
        lv_obj_set_style_pad_all(temp_chart_box, 2, 0);

        for (int i = 0; i < HIST_SAMPLES; i++) {
            temp_bars_[i] = lv_bar_create(temp_chart_box);
            lv_obj_set_size(temp_bars_[i], 6, 68);
            lv_obj_align(temp_bars_[i], LV_ALIGN_TOP_LEFT, 2 + i * 9, 2);
            lv_bar_set_range(temp_bars_[i], 10, 50);
            lv_bar_set_value(temp_bars_[i], 25, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(temp_bars_[i], lv_color_hex(0xFF5722), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(temp_bars_[i], lv_color_hex(0x2E2E2E), LV_PART_MAIN);
            lv_obj_set_style_radius(temp_bars_[i], 2, 0);
            temp_history_[i] = 25;
        }

        lv_obj_t* temp_chart_info = lv_label_create(temp_chart_box);
        lv_label_set_text(temp_chart_info, "10C       2s/m       50C");
        lv_obj_set_style_text_color(temp_chart_info, lv_color_hex(0x757575), 0);
        lv_obj_align(temp_chart_info, LV_ALIGN_BOTTOM_MID, 0, 0);

        // ==========================================
        // VIEW 3: HUMIDITY & CHART
        // ==========================================
        humi_view_ = lv_obj_create(overlay_container_);
        lv_obj_set_size(humi_view_, 124, 156);
        lv_obj_align(humi_view_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(humi_view_, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_border_color(humi_view_, lv_color_hex(0x2196F3), 0);
        lv_obj_set_style_border_width(humi_view_, 1, 0);
        lv_obj_set_style_pad_all(humi_view_, 2, 0);

        lv_obj_t* humi_title = lv_label_create(humi_view_);
        lv_label_set_text(humi_title, "DO AM KHI");
        lv_obj_set_style_text_color(humi_title, lv_color_hex(0x64B5F6), 0);
        lv_obj_align(humi_title, LV_ALIGN_TOP_MID, 0, 2);

        humi_val_label_ = lv_label_create(humi_view_);
        lv_label_set_text(humi_val_label_, "--.- %");
        lv_obj_set_style_text_color(humi_val_label_, lv_color_hex(0x2196F3), 0);
        lv_obj_align(humi_val_label_, LV_ALIGN_TOP_MID, 0, 18);

        // Chart container (for 12 history bars)
        lv_obj_t* humi_chart_box = lv_obj_create(humi_view_);
        lv_obj_set_size(humi_chart_box, 116, 92);
        lv_obj_align(humi_chart_box, LV_ALIGN_BOTTOM_MID, 0, -2);
        lv_obj_set_style_bg_color(humi_chart_box, lv_color_hex(0x222222), 0);
        lv_obj_set_style_border_color(humi_chart_box, lv_color_hex(0x333333), 0);
        lv_obj_set_style_border_width(humi_chart_box, 1, 0);
        lv_obj_set_style_pad_all(humi_chart_box, 2, 0);

        for (int i = 0; i < HIST_SAMPLES; i++) {
            humi_bars_[i] = lv_bar_create(humi_chart_box);
            lv_obj_set_size(humi_bars_[i], 6, 68);
            lv_obj_align(humi_bars_[i], LV_ALIGN_TOP_LEFT, 2 + i * 9, 2);
            lv_bar_set_range(humi_bars_[i], 10, 100);
            lv_bar_set_value(humi_bars_[i], 60, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(humi_bars_[i], lv_color_hex(0x2196F3), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(humi_bars_[i], lv_color_hex(0x2E2E2E), LV_PART_MAIN);
            lv_obj_set_style_radius(humi_bars_[i], 2, 0);
            humi_history_[i] = 60;
        }

        lv_obj_t* humi_chart_info = lv_label_create(humi_chart_box);
        lv_label_set_text(humi_chart_info, "10%       2s/m      100%");
        lv_obj_set_style_text_color(humi_chart_info, lv_color_hex(0x757575), 0);
        lv_obj_align(humi_chart_info, LV_ALIGN_BOTTOM_MID, 0, 0);

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
        lv_label_set_text(dual_temp_label_, "--.- \xc2\xb0\x43");
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

        // ==========================================
        // VIEW 5: MUSIC PLAYER (Spotify / YT Music / MP3)
        // ==========================================
        music_view_ = lv_obj_create(overlay_container_);
        lv_obj_set_size(music_view_, 124, 156);
        lv_obj_align(music_view_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(music_view_, lv_color_hex(0x121212), 0);
        lv_obj_set_style_border_color(music_view_, lv_color_hex(0x1DB954), 0);
        lv_obj_set_style_border_width(music_view_, 1, 0);
        lv_obj_set_style_pad_all(music_view_, 2, 0);

        // Platform badge
        music_badge_ = lv_label_create(music_view_);
        lv_label_set_text(music_badge_, "[ SPOTIFY ]");
        lv_obj_set_style_text_color(music_badge_, lv_color_hex(0x1DB954), 0);
        lv_obj_align(music_badge_, LV_ALIGN_TOP_MID, 0, 2);

        // Equalizer container (8 bars)
        lv_obj_t* music_eq_box = lv_obj_create(music_view_);
        lv_obj_set_size(music_eq_box, 96, 32);
        lv_obj_align(music_eq_box, LV_ALIGN_TOP_MID, 0, 20);
        lv_obj_set_style_bg_color(music_eq_box, lv_color_hex(0x1E1E1E), 0);
        lv_obj_set_style_border_width(music_eq_box, 0, 0);
        lv_obj_set_style_pad_all(music_eq_box, 2, 0);
        lv_obj_set_style_radius(music_eq_box, 4, 0);

        for (int i = 0; i < 8; i++) {
            music_eq_bars_[i] = lv_bar_create(music_eq_box);
            lv_obj_set_size(music_eq_bars_[i], 6, 26);
            lv_obj_align(music_eq_bars_[i], LV_ALIGN_TOP_LEFT, 6 + i * 10, 1);
            lv_bar_set_range(music_eq_bars_[i], 0, 100);
            lv_bar_set_value(music_eq_bars_[i], 15 + i * 8, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(music_eq_bars_[i], lv_color_hex(0x1DB954), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(music_eq_bars_[i], lv_color_hex(0x2C2C2C), LV_PART_MAIN);
            lv_obj_set_style_radius(music_eq_bars_[i], 1, 0);
        }

        // Song title (cuon chu tu dong neu ten dai)
        music_title_label_ = lv_label_create(music_view_);
        lv_label_set_text(music_title_label_, "Chua phat nhac");
        lv_label_set_long_mode(music_title_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(music_title_label_, 116);
        lv_obj_set_style_text_align(music_title_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(music_title_label_, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(music_title_label_, LV_ALIGN_TOP_MID, 0, 56);

        // Artist name (cuon chu tu dong neu ten dai)
        music_artist_label_ = lv_label_create(music_view_);
        lv_label_set_text(music_artist_label_, "XiaoZhi Music");
        lv_label_set_long_mode(music_artist_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(music_artist_label_, 116);
        lv_obj_set_style_text_align(music_artist_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(music_artist_label_, lv_color_hex(0x90A4AE), 0);
        lv_obj_align(music_artist_label_, LV_ALIGN_TOP_MID, 0, 74);

        // Progress bar
        music_progress_bar_ = lv_bar_create(music_view_);
        lv_obj_set_size(music_progress_bar_, 106, 4);
        lv_obj_align(music_progress_bar_, LV_ALIGN_TOP_MID, 0, 95);
        lv_bar_set_range(music_progress_bar_, 0, 100);
        lv_bar_set_value(music_progress_bar_, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(music_progress_bar_, lv_color_hex(0x1DB954), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(music_progress_bar_, lv_color_hex(0x2A2A2A), LV_PART_MAIN);

        // Time label
        music_time_label_ = lv_label_create(music_view_);
        lv_label_set_text(music_time_label_, "00:00 / 03:30");
        lv_obj_set_style_text_color(music_time_label_, lv_color_hex(0x78909C), 0);
        lv_obj_align(music_time_label_, LV_ALIGN_TOP_MID, 0, 103);

        // Status / control label
        music_status_label_ = lv_label_create(music_view_);
        lv_label_set_text(music_status_label_, "[ || TAM DUNG ]");
        lv_obj_set_style_text_color(music_status_label_, lv_color_hex(0xFFA726), 0);
        lv_obj_align(music_status_label_, LV_ALIGN_BOTTOM_MID, 0, -4);

        // Default to Mode 1 (Xiaozhi mode: overlay hidden)
        ApplyModeVisibility(kModeXiaozhi);
    }

    void ApplyModeVisibility(DisplayMode mode) {
        if (!overlay_container_) return;

        if (mode == kModeXiaozhi) {
            lv_obj_add_flag(overlay_container_, LV_OBJ_FLAG_HIDDEN);
            return;
        }

        // Unhide overlay container and bring to foreground on top layer
        lv_obj_remove_flag(overlay_container_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(overlay_container_);

        // Hide all 4 sub-views first
        lv_obj_add_flag(temp_view_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(humi_view_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(dual_view_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(music_view_, LV_OBJ_FLAG_HIDDEN);

        // Show requested view
        if (mode == kModeTempChart) {
            lv_obj_remove_flag(temp_view_, LV_OBJ_FLAG_HIDDEN);
        } else if (mode == kModeHumiChart) {
            lv_obj_remove_flag(humi_view_, LV_OBJ_FLAG_HIDDEN);
        } else if (mode == kModeDual) {
            lv_obj_remove_flag(dual_view_, LV_OBJ_FLAG_HIDDEN);
        } else if (mode == kModeMusic) {
            lv_obj_remove_flag(music_view_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void AnimateMusicEq() {
        if (!music_view_) return;

        if (!music_is_playing_) {
            for (int i = 0; i < 8; i++) {
                if (music_eq_bars_[i]) {
                    lv_bar_set_value(music_eq_bars_[i], 6, LV_ANIM_OFF);
                }
            }
            return;
        }

        static const uint8_t eq_pattern[6][8] = {
            {55, 90, 40, 85, 45, 95, 65, 30},
            {80, 45, 95, 35, 75, 40, 90, 60},
            {35, 75, 50, 95, 30, 85, 45, 80},
            {90, 60, 80, 40, 90, 50, 70, 35},
            {65, 85, 45, 70, 90, 60, 85, 50},
            {40, 60, 90, 50, 65, 95, 40, 75}
        };
        static int pat_idx = 0;
        pat_idx = (pat_idx + 1) % 6;

        for (int i = 0; i < 8; i++) {
            if (music_eq_bars_[i]) {
                lv_bar_set_value(music_eq_bars_[i], eq_pattern[pat_idx][i], LV_ANIM_OFF);
            }
        }
    }

    void UpdateMusicUi() {
        if (!music_view_) return;

        lv_color_t color;
        const char* badge_text = "[ SPOTIFY ]";
        if (music_platform_.find("You") != std::string::npos || music_platform_.find("yt") != std::string::npos || music_platform_.find("YT") != std::string::npos) {
            color = lv_color_hex(0xFF3333); // Red
            badge_text = "[ YT MUSIC ]";
        } else if (music_platform_.find("MP3") != std::string::npos || music_platform_.find("mp3") != std::string::npos || music_platform_.find("Zing") != std::string::npos) {
            color = lv_color_hex(0x00E5FF); // Cyan
            badge_text = "[ MP3 ONLINE ]";
        } else {
            color = lv_color_hex(0x1DB954); // Spotify Green
            badge_text = "[ SPOTIFY ]";
        }

        lv_obj_set_style_border_color(music_view_, color, 0);
        if (music_badge_) {
            lv_label_set_text(music_badge_, badge_text);
            lv_obj_set_style_text_color(music_badge_, color, 0);
        }
        if (music_title_label_) {
            std::string safe_title = SanitizeVietnameseForDisplay(music_title_);
            lv_label_set_text(music_title_label_, safe_title.c_str());
        }
        if (music_artist_label_) {
            std::string safe_artist = SanitizeVietnameseForDisplay(music_artist_);
            lv_label_set_text(music_artist_label_, safe_artist.c_str());
        }
        if (music_progress_bar_) {
            int pct = (music_total_sec_ > 0) ? (music_cur_sec_ * 100) / music_total_sec_ : 0;
            if (pct > 100) pct = 100;
            lv_bar_set_value(music_progress_bar_, pct, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(music_progress_bar_, color, LV_PART_INDICATOR);
        }
        for (int i = 0; i < 8; i++) {
            if (music_eq_bars_[i]) {
                lv_obj_set_style_bg_color(music_eq_bars_[i], color, LV_PART_INDICATOR);
            }
        }
        if (music_time_label_) {
            char time_buf[32];
            snprintf(time_buf, sizeof(time_buf), "%02d:%02d / %02d:%02d",
                     music_cur_sec_ / 60, music_cur_sec_ % 60,
                     music_total_sec_ / 60, music_total_sec_ % 60);
            lv_label_set_text(music_time_label_, time_buf);
        }
        if (music_status_label_) {
            if (music_is_playing_) {
                lv_label_set_text(music_status_label_, "[ > DANG PHAT ]");
                lv_obj_set_style_text_color(music_status_label_, lv_color_hex(0x00E676), 0);
            } else {
                lv_label_set_text(music_status_label_, "[ || TAM DUNG ]");
                lv_obj_set_style_text_color(music_status_label_, lv_color_hex(0xFFA726), 0);
            }
        }
    }

public:
    EnvDisplayManager(LcdDisplay* display, Dht11* dht)
        : display_(display), dht_(dht) {
    }

    void Start() {
        // Start background sensor sampling & UI refresh task (runs every 1 second)
        xTaskCreate([](void* arg) {
            auto self = static_cast<EnvDisplayManager*>(arg);
            // Wait 1 second after boot so display SetupUI() completes first
            vTaskDelay(pdMS_TO_TICKS(1000));
            int tick_counter = 0;
            while (true) {
                tick_counter++;
                // Read DHT11 every 2 seconds
                if (tick_counter % 2 == 0) {
                    self->UpdateSensorData();
                }
                // Update Music progress and animated equalizer every 1 second
                self->UpdateMusicProgress();
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }, "env_ui_task", 4096, this, 3, &update_task_handle_);
    }

    void UpdateSensorData() {
        if (!dht_ || !display_) return;

        float t = 0.0f, h = 0.0f;
        dht_->Read(t, h);

        DisplayLockGuard lock(display_);
        EnsureViews();
        if (!overlay_container_) return;

        char buf[32];

        // 1. Update Temperature elements
        snprintf(buf, sizeof(buf), "%.1f \xc2\xb0\x43", t);
        if (temp_val_label_) lv_label_set_text(temp_val_label_, buf);
        if (dual_temp_label_) lv_label_set_text(dual_temp_label_, buf);
        if (dual_temp_bar_) lv_bar_set_value(dual_temp_bar_, (int)t, LV_ANIM_OFF);
        
        // Shift history and update 12 temperature bars
        for (int i = 0; i < HIST_SAMPLES - 1; i++) {
            temp_history_[i] = temp_history_[i + 1];
        }
        temp_history_[HIST_SAMPLES - 1] = (int)t;
        for (int i = 0; i < HIST_SAMPLES; i++) {
            if (temp_bars_[i]) {
                lv_bar_set_value(temp_bars_[i], temp_history_[i], LV_ANIM_OFF);
            }
        }

        // 2. Update Humidity elements
        snprintf(buf, sizeof(buf), "%.1f %%", h);
        if (humi_val_label_) lv_label_set_text(humi_val_label_, buf);
        if (dual_humi_label_) lv_label_set_text(dual_humi_label_, buf);
        if (dual_humi_bar_) lv_bar_set_value(dual_humi_bar_, (int)h, LV_ANIM_OFF);

        // Shift history and update 12 humidity bars
        for (int i = 0; i < HIST_SAMPLES - 1; i++) {
            humi_history_[i] = humi_history_[i + 1];
        }
        humi_history_[HIST_SAMPLES - 1] = (int)h;
        for (int i = 0; i < HIST_SAMPLES; i++) {
            if (humi_bars_[i]) {
                lv_bar_set_value(humi_bars_[i], humi_history_[i], LV_ANIM_OFF);
            }
        }
    }

    void UpdateMusicProgress() {
        if (!display_) return;

        DisplayLockGuard lock(display_);
        EnsureViews();
        if (!overlay_container_) return;

        if (music_is_playing_) {
            music_cur_sec_++;
            if (music_cur_sec_ > music_total_sec_) {
                music_cur_sec_ = 0;
            }
            if (music_progress_bar_) {
                int pct = (music_total_sec_ > 0) ? (music_cur_sec_ * 100) / music_total_sec_ : 0;
                if (pct > 100) pct = 100;
                lv_bar_set_value(music_progress_bar_, pct, LV_ANIM_OFF);
            }
            if (music_time_label_) {
                char time_buf[32];
                snprintf(time_buf, sizeof(time_buf), "%02d:%02d / %02d:%02d",
                         music_cur_sec_ / 60, music_cur_sec_ % 60,
                         music_total_sec_ / 60, music_total_sec_ % 60);
                lv_label_set_text(music_time_label_, time_buf);
            }
        }

        AnimateMusicEq();
    }

    void SetMode(DisplayMode mode) {
        if (!display_) return;
        DisplayLockGuard lock(display_);
        EnsureViews();
        current_mode_ = mode;
        ApplyModeVisibility(current_mode_);
        ESP_LOGI(TAG_ENV_UI, "Display switched to Mode %d", (int)current_mode_);
    }

    DisplayMode GetMode() const {
        return current_mode_;
    }

    // Button 1: Chuyen vong tron qua ca 5 Mode:
    // Mode 1 (AI Xiaozhi) -> Mode 2 (Nhiet do) -> Mode 3 (Do am) -> Mode 4 (Moi truong kep) -> Mode 5 (Music) -> Mode 1
    void CycleModeAll() {
        ESP_LOGI(TAG_ENV_UI, "CycleModeAll called, current_mode=%d", (int)current_mode_);
        if (current_mode_ == kModeXiaozhi) {
            SetMode(kModeTempChart);
        } else if (current_mode_ == kModeTempChart) {
            SetMode(kModeHumiChart);
        } else if (current_mode_ == kModeHumiChart) {
            SetMode(kModeDual);
        } else if (current_mode_ == kModeDual) {
            SetMode(kModeMusic);
        } else {
            SetMode(kModeXiaozhi);
        }
    }

    // Button 2: Chuyen nhanh sang Mode 4 (Moi truong) hoac Mode 5 (Music) hoac ve Mode 1
    void SwitchToMode4Or5() {
        ESP_LOGI(TAG_ENV_UI, "SwitchToMode4Or5 called, current_mode=%d", (int)current_mode_);
        if (current_mode_ != kModeDual && current_mode_ != kModeMusic) {
            SetMode(kModeDual);
        } else if (current_mode_ == kModeDual) {
            SetMode(kModeMusic);
        } else {
            SetMode(kModeXiaozhi);
        }
    }

    // Music control methods
    void SetMusicTrack(const std::string& title, const std::string& artist, const std::string& platform, int duration_sec = 210) {
        music_title_ = SanitizeVietnameseForDisplay(title);
        music_artist_ = SanitizeVietnameseForDisplay(artist);
        music_platform_ = platform;
        music_cur_sec_ = 0;
        music_total_sec_ = duration_sec > 0 ? duration_sec : 210;
        music_is_playing_ = true;

        if (!display_) return;
        DisplayLockGuard lock(display_);
        EnsureViews();
        UpdateMusicUi();
        SetMode(kModeMusic);
    }

    void SetMusicPlayState(bool playing) {
        music_is_playing_ = playing;
        if (!display_) return;
        DisplayLockGuard lock(display_);
        EnsureViews();
        UpdateMusicUi();
    }

    std::string GetMusicTitle() const { return music_title_; }
    std::string GetMusicArtist() const { return music_artist_; }
    std::string GetMusicPlatform() const { return music_platform_; }
    bool IsMusicPlaying() const { return music_is_playing_; }
    int GetMusicCurSec() const { return music_cur_sec_; }
    int GetMusicTotalSec() const { return music_total_sec_; }
};

#endif // ENV_DISPLAY_MANAGER_H_
