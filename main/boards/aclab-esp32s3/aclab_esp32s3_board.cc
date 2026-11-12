#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "servo_controller.h"
#include "dht11.h"
#include "env_display_manager.h"
#include "led/single_led.h"
#include "power_save_timer.h"
#include "settings.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include "esp_lcd_ili9341.h"
#include <driver/spi_common.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "AclabEsp32S3Board"

// ST7735 Hardware initialization commands (required for ST7735 internal charge pump & VCOM)
static const uint8_t st7735_cmd_b1[] = {0x05, 0x3A, 0x3A};
static const uint8_t st7735_cmd_b2[] = {0x05, 0x3A, 0x3A};
static const uint8_t st7735_cmd_b3[] = {0x05, 0x3A, 0x3A, 0x05, 0x3A, 0x3A};
static const uint8_t st7735_cmd_b4[] = {0x03};
static const uint8_t st7735_cmd_c0[] = {0x44, 0x04, 0x04};
static const uint8_t st7735_cmd_c1[] = {0xC0};
static const uint8_t st7735_cmd_c2[] = {0x0D, 0x00};
static const uint8_t st7735_cmd_c3[] = {0x8D, 0x6A};
static const uint8_t st7735_cmd_c4[] = {0x8D, 0xEE};
static const uint8_t st7735_cmd_c5[] = {0x08};
static const uint8_t st7735_cmd_e0[] = {0x0F, 0x10, 0x03, 0x03, 0x07, 0x02, 0x00, 0x02,
                                        0x07, 0x0C, 0x13, 0x38, 0x0A, 0x0E, 0x03, 0x10};
static const uint8_t st7735_cmd_e1[] = {0x10, 0x0B, 0x04, 0x04, 0x10, 0x03, 0x00, 0x03,
                                        0x03, 0x09, 0x17, 0x33, 0x0B, 0x0C, 0x06, 0x10};
static const uint8_t st7735_cmd_35[] = {0x00};
static const uint8_t st7735_cmd_3a[] = {0x05};
static const uint8_t st7735_cmd_36[] = {0xC8};

static const ili9341_lcd_init_cmd_t st7735_vendor_init[] = {
    {0x11, NULL, 0, 120},  // Sleep out, Delay 120ms
    {0xB1, st7735_cmd_b1, sizeof(st7735_cmd_b1), 0},
    {0xB2, st7735_cmd_b2, sizeof(st7735_cmd_b2), 0},
    {0xB3, st7735_cmd_b3, sizeof(st7735_cmd_b3), 0},
    {0xB4, st7735_cmd_b4, sizeof(st7735_cmd_b4), 0},
    {0xC0, st7735_cmd_c0, sizeof(st7735_cmd_c0), 0},
    {0xC1, st7735_cmd_c1, sizeof(st7735_cmd_c1), 0},
    {0xC2, st7735_cmd_c2, sizeof(st7735_cmd_c2), 0},
    {0xC3, st7735_cmd_c3, sizeof(st7735_cmd_c3), 0},
    {0xC4, st7735_cmd_c4, sizeof(st7735_cmd_c4), 0},
    {0xC5, st7735_cmd_c5, sizeof(st7735_cmd_c5), 0},
    {0xE0, st7735_cmd_e0, sizeof(st7735_cmd_e0), 0},
    {0xE1, st7735_cmd_e1, sizeof(st7735_cmd_e1), 0},
    {0x35, st7735_cmd_35, sizeof(st7735_cmd_35), 0},
    {0x3A, st7735_cmd_3a, sizeof(st7735_cmd_3a), 0},
    {0x36, st7735_cmd_36, sizeof(st7735_cmd_36), 0},
    {0x29, NULL, 0, 0},  // Display on
    {0x2C, NULL, 0, 0},  // Memory write
};

class AclabEsp32S3Board : public WifiBoard {
private:
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    Button touch_button_;
    Button mode_button_1_;
    Button mode_button_2_;
    Dht11 dht11_;
    EnvDisplayManager* env_display_ = nullptr;
    LcdDisplay* display_ = nullptr;
    PowerSaveTimer* power_save_timer_ = nullptr;

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 10 * 1000 * 1000; // ST7735 gioi han phan cung toi da 15MHz, dat 10MHz de dam bao tin hieu on dinh tren day cam breadboard
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        ESP_LOGD(TAG, "Install ST7735 LCD driver with vendor init");
        static const ili9341_vendor_config_t vendor_config = {
            .init_cmds = st7735_vendor_init,
            .init_cmds_size = sizeof(st7735_vendor_init) / sizeof(ili9341_lcd_init_cmd_t),
        };

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = (void*)&vendor_config;
        
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
        
        esp_lcd_panel_reset(panel);
        vTaskDelay(pdMS_TO_TICKS(120));
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);

        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeButtons() {
        // Nut BOOT (GPIO 0): Nhan de vao che do cau hinh Wi-Fi hoac chuyen trang thai hoi thoai
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        // Nut Tang Am Luong (Vol+ GPIO 1): Tang 10%, hien thi OSD, luu NVS
        volume_up_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) volume = 100;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume) + "%");
        });

        // Nut Giam Am Luong (Vol- GPIO 2): Giam 10%, hien thi OSD, luu NVS
        volume_down_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) volume = 0;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume) + "%");
        });

        // Nhan giu Nut Vol-: Mute
        volume_down_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });

        // Cam bien cham TTP223 (GPIO 3): Danh thuc tuc thi (< 0.2s) va bat dau lang nghe
        touch_button_.OnPressDown([this]() {
            power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            app.StartListening();
        });

        touch_button_.OnPressUp([this]() {
            auto& app = Application::GetInstance();
            app.StopListening();
        });

        // Nut 1 (GPIO 17): Chuyen vong tron qua cac mode 1 -> 2 -> 3 -> 4 -> 5 -> 1
        // Phan hoi tuc thi ngay khi an xuong (OnPressDown)
        mode_button_1_.OnPressDown([this]() {
            static uint32_t last_press = 0;
            uint32_t now = esp_log_timestamp();
            if (now - last_press < 250) return;
            last_press = now;

            ESP_LOGI(TAG, "Mode Button 1 Pressed (GPIO 17) -> Cycle Mode");
            power_save_timer_->WakeUp();
            if (env_display_) {
                env_display_->CycleModeAll();
            }
        });

        // Nut 2 (GPIO 21): Chuyen nhanh sang Mode 4 (Moi truong) hoac Mode 5 (Music) hoac ve Mode 1
        // Phan hoi tuc thi ngay khi an xuong (OnPressDown)
        mode_button_2_.OnPressDown([this]() {
            static uint32_t last_press = 0;
            uint32_t now = esp_log_timestamp();
            if (now - last_press < 250) return;
            last_press = now;

            ESP_LOGI(TAG, "Mode Button 2 Pressed (GPIO 21) -> Switch Mode 4/5");
            power_save_timer_->WakeUp();
            if (env_display_) {
                env_display_->SwitchToMode4Or5();
            }
        });
    }

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, POWER_SAVE_SLEEP_SECONDS, -1);
        
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Entering Wi-Fi Modem-Sleep mode");
            GetDisplay()->SetPowerSaveMode(true);
            if (GetBacklight()) {
                GetBacklight()->SetBrightness(20); // Giam do sang xuong 20% thay vi tat toi den
            }
        });

        power_save_timer_->OnExitSleepMode([this]() {
            ESP_LOGI(TAG, "Exiting Sleep mode");
            GetDisplay()->SetPowerSaveMode(false);
            if (GetBacklight()) {
                GetBacklight()->RestoreBrightness(); // Bat lai do sang goc
            }
        });

        power_save_timer_->SetEnabled(true);
    }

    void PlayMusicAfterSpeaking(const std::string& audio_url, const std::string& title, const std::string& artist, const std::string& platform, int duration_sec) {
        struct PlaybackJob {
            AclabEsp32S3Board* board;
            std::string audio_url;
            std::string title;
            std::string artist;
            std::string platform;
            int duration_sec;
        };

        auto job = new PlaybackJob{this, audio_url, title, artist, platform, duration_sec};

        xTaskCreate([](void* arg) {
            auto j = static_cast<PlaybackJob*>(arg);
            ESP_LOGI(TAG, "PlaybackJob: Waiting for AI speech to finish before starting music...");

            auto& app = Application::GetInstance();
            // Cho thoi gian 400ms de neu AI chuan bi phat TTS thi state kip chuyen sang Speaking
            vTaskDelay(pdMS_TO_TICKS(400));

            // Cho toi da 15 giay cho den khi AI noi xong hoan toan va audio buffer xa sach
            int waited_ms = 0;
            while (waited_ms < 15000) {
                DeviceState state = app.GetDeviceState();
                bool audio_idle = app.GetAudioService().IsPlaybackIdle();
                if (state != kDeviceStateSpeaking && audio_idle) {
                    ESP_LOGI(TAG, "PlaybackJob: AI speech done (state=%d), starting music now!", (int)state);
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(100));
                waited_ms += 100;
            }

            vTaskDelay(pdMS_TO_TICKS(200));

            // Cap nhat giao dien Mode 5 voi thong tin bai hat va thoi luong chuan
            if (j->board->env_display_) {
                j->board->env_display_->SetMusicTrack(j->title, j->artist, j->platform, j->duration_sec);
            }

            // Phat luong audio stream neu co link
            if (!j->audio_url.empty()) {
                ESP_LOGI(TAG, "PlaybackJob: Starting direct audio stream: %s", j->audio_url.c_str());
                app.Schedule([url = j->audio_url]() {
                    Application::GetInstance().StartNotification(url, {});
                });
            } else {
                app.PlaySound(Lang::Sounds::OGG_POPUP);
            }

            delete j;
            vTaskDelete(NULL);
        }, "music_delay_task", 4096, job, 2, NULL);
    }

    void InitializeTools() {
        // Ngoai vi LED tren GPIO 38
        static LampController lamp(LAMP_GPIO);
        // Ngoai vi Servo tren GPIO 18
        static ServoController servo(SERVO_GPIO);

        auto& mcp_server = McpServer::GetInstance();

        // MCP Tool 1: Doc gia tri nhiet do
        mcp_server.AddTool("self.sensor.get_temperature",
            "Doc gia tri nhiet do moi truong phong hien tai (do C) tu cam bien DHT11.",
            PropertyList(), [this](const PropertyList& properties) {
                float t = dht11_.GetTemperature();
                char buf[32];
                snprintf(buf, sizeof(buf), "%.1f", t);
                return std::string(buf);
            });

        // MCP Tool 2: Doc gia tri do am
        mcp_server.AddTool("self.sensor.get_humidity",
            "Doc gia tri do am khong khi phong hien tai (%) tu cam bien DHT11.",
            PropertyList(), [this](const PropertyList& properties) {
                float h = dht11_.GetHumidity();
                char buf[32];
                snprintf(buf, sizeof(buf), "%.1f", h);
                return std::string(buf);
            });

        // MCP Tool 3: Doc tong hop ca hai thong so
        mcp_server.AddTool("self.sensor.get_environment",
            "Doc ca hai thong so nhiet do phong (do C) va do am (%) tu cam bien DHT11.",
            PropertyList(), [this](const PropertyList& properties) {
                float t, h;
                dht11_.Read(t, h);
                char buf[64];
                snprintf(buf, sizeof(buf), "{\"temperature\": %.1f, \"humidity\": %.1f}", t, h);
                return std::string(buf);
            });

        // MCP Tool 4: Chuyen doi che do hien thi man hinh qua giong noi hoac server
        mcp_server.AddTool("self.screen.set_display_mode",
            "Chuyen che do hien thi TFT: 1 (AI Xiaozhi), 2 (Do thi Nhiet do), 3 (Do thi Do am), 4 (Tong hop Nhiet do & Do am), 5 (Trinh phat nhac Music Player).",
            PropertyList({
                Property("mode", kPropertyTypeInteger, 1, 5)
            }), [this](const PropertyList& properties) {
                int mode = properties["mode"].value<int>();
                if (env_display_ && mode >= 1 && mode <= 5) {
                    env_display_->SetMode((DisplayMode)mode);
                    return true;
                }
                return false;
            });

        // MCP Tool 5: Tim kiem va phat nhac tren Spotify, YouTube Music, MP3
        mcp_server.AddTool("self.music.search_and_play",
            "Tim kiem va phat bai hat tren Spotify, YouTube Music hoac MP3. Thiet bi se tu dong cho AI noi xong cau phan hoi roi moi bat dau phat nhac de tranh xung dot am thanh.\n"
            "Tham so:\n"
            "  `query`: Ten bai hat hoac tu khoa tim kiem (VD: 'See Tinh', 'Waiting For You', 'Son Tung M-TP')\n"
            "  `platform`: Nen tang phat ('spotify', 'youtube', 'mp3'). Mac dinh la 'spotify'.\n"
            "  `duration_sec`: (Tuy chon) Thoi luong bai hat tinh bang giay (VD: 245 cho 4 phut 5 giay). Neu khong truyen se tu dong uoc tinh.\n"
            "  `audio_url`: (Tuy chon) URL luong am thanh truc tiep (OGG Opus / stream) de loa phat ngay.",
            PropertyList({
                Property("query", kPropertyTypeString),
                Property("platform", kPropertyTypeString, std::string("spotify")),
                Property("duration_sec", kPropertyTypeInteger, 0),
                Property("audio_url", kPropertyTypeString, std::string(""))
            }), [this](const PropertyList& properties) -> ReturnValue {
                std::string query = properties["query"].value<std::string>();
                std::string platform = properties["platform"].value<std::string>();
                int duration = properties["duration_sec"].value<int>();
                std::string audio_url = properties["audio_url"].value<std::string>();

                // Chuan hoa ten nen tang
                std::string norm_platform = "Spotify";
                std::string p_lower = platform;
                std::transform(p_lower.begin(), p_lower.end(), p_lower.begin(), ::tolower);
                if (p_lower.find("you") != std::string::npos || p_lower.find("yt") != std::string::npos) {
                    norm_platform = "YouTube Music";
                } else if (p_lower.find("mp3") != std::string::npos || p_lower.find("zing") != std::string::npos) {
                    norm_platform = "MP3 Online";
                }

                // Tach nghe si va ten bai hat tu query (neu co)
                std::string title = query;
                std::string artist = norm_platform + " Top Hits";
                size_t dash = query.find(" - ");
                if (dash != std::string::npos) {
                    title = query.substr(0, dash);
                    artist = query.substr(dash + 3);
                } else {
                    size_t cua = query.find(" cua ");
                    if (cua != std::string::npos) {
                        title = query.substr(0, cua);
                        artist = query.substr(cua + 5);
                    }
                }

                if (duration <= 0) {
                    duration = 215; // Mac dinh 3 phut 35 giay
                }

                // Cho AI noi xong roi moi kich hoat giao dien Mode 5 va phat nhac
                PlayMusicAfterSpeaking(audio_url, title, artist, norm_platform, duration);

                char ret_buf[512];
                snprintf(ret_buf, sizeof(ret_buf),
                    "{\"status\":\"scheduled\",\"platform\":\"%s\",\"title\":\"%s\",\"artist\":\"%s\",\"duration_sec\":%d,"
                    "\"direct_stream\":%s,"
                    "\"message\":\"Da len lich phat bai %s tren %s. Thiet bi se tu dong phat ngay sau khi ban noi xong.\"}",
                    norm_platform.c_str(), title.c_str(), artist.c_str(), duration,
                    audio_url.empty() ? "false" : "true",
                    title.c_str(), norm_platform.c_str());
                return std::string(ret_buf);
            });

        // MCP Tool 6: Dieu khien trinh phat nhac
        mcp_server.AddTool("self.music.control",
            "Dieu khien trinh phat nhac. Cac hanh dong: 'play' (tiep tuc), 'pause' (tam dung), 'stop' (dung phat), 'next' (bai tiep), 'prev' (bai truoc).",
            PropertyList({
                Property("action", kPropertyTypeString)
            }), [this](const PropertyList& properties) -> ReturnValue {
                std::string action = properties["action"].value<std::string>();
                std::transform(action.begin(), action.end(), action.begin(), ::tolower);

                if (action == "stop" || action == "pause") {
                    Application::GetInstance().Schedule([]() {
                        Application::GetInstance().StopNotification();
                    });
                }

                if (env_display_) {
                    if (action == "pause" || action == "stop") {
                        env_display_->SetMusicPlayState(false);
                        return std::string("{\"status\":\"paused\",\"message\":\"Da tam dung phat nhac\"}");
                    } else if (action == "play" || action == "resume") {
                        env_display_->SetMusicPlayState(true);
                        return std::string("{\"status\":\"playing\",\"message\":\"Da tiep tuc phat nhac\"}");
                    } else if (action == "next") {
                        env_display_->SetMusicTrack("Bai hat ke tiep", "Top Trending", env_display_->GetMusicPlatform(), 195);
                        return std::string("{\"status\":\"playing\",\"message\":\"Da chuyen sang bai hat ke tiep\"}");
                    } else if (action == "prev") {
                        env_display_->SetMusicTrack("Bai hat truoc do", "History", env_display_->GetMusicPlatform(), 210);
                        return std::string("{\"status\":\"playing\",\"message\":\"Da quay lai bai hat truoc do\"}");
                    }
                }
                return std::string("{\"status\":\"error\",\"message\":\"Khong hop le\"}");
            });

        // MCP Tool 7: Lay thong tin bai hat dang phat
        mcp_server.AddTool("self.music.get_status",
            "Lay thong tin bai hat va trang thai cua trinh phat nhac tren thiet bi.",
            PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                if (!env_display_) {
                    return std::string("{\"status\":\"idle\"}");
                }
                char buf[256];
                snprintf(buf, sizeof(buf),
                    "{\"playing\":%s,\"platform\":\"%s\",\"title\":\"%s\",\"artist\":\"%s\",\"current_sec\":%d,\"total_sec\":%d}",
                    env_display_->IsMusicPlaying() ? "true" : "false",
                    env_display_->GetMusicPlatform().c_str(),
                    env_display_->GetMusicTitle().c_str(),
                    env_display_->GetMusicArtist().c_str(),
                    env_display_->GetMusicCurSec(),
                    env_display_->GetMusicTotalSec());
                return std::string(buf);
            });

        // MCP Tool 8: Thiet lap thong tin bai hat dang phat
        mcp_server.AddTool("self.music.set_track",
            "Thiet lap thong tin bai hat hien thi len man hinh LCD.\n"
            "Tham so:\n"
            "  `title`: Ten bai hat\n"
            "  `artist`: Ca si / Nhac si\n"
            "  `platform`: Nen tang ('Spotify', 'YouTube Music', 'MP3 Online')\n"
            "  `duration_sec`: Thoi luong bai hat tinh bang giay\n"
            "  `audio_url`: (Tuy chon) URL luong am thanh truc tiep de loa phat ngay.",
            PropertyList({
                Property("title", kPropertyTypeString),
                Property("artist", kPropertyTypeString),
                Property("platform", kPropertyTypeString, std::string("Spotify")),
                Property("duration_sec", kPropertyTypeInteger, 210),
                Property("audio_url", kPropertyTypeString, std::string(""))
            }), [this](const PropertyList& properties) -> ReturnValue {
                std::string title = properties["title"].value<std::string>();
                std::string artist = properties["artist"].value<std::string>();
                std::string platform = properties["platform"].value<std::string>();
                int duration = properties["duration_sec"].value<int>();
                std::string audio_url = properties["audio_url"].value<std::string>();

                PlayMusicAfterSpeaking(audio_url, title, artist, platform, duration);

                return std::string("{\"status\":\"success\"}");
            });

        // MCP Tool 9: Cap nhat thoi luong bai hat va vi tri phat
        mcp_server.AddTool("self.music.set_duration",
            "Cap nhat thoi luong bai hat va vi tri hien tai len thanh tien trinh LCD (Mode 5).\n"
            "Tham so:\n"
            "  `total_sec`: Tong thoi luong bai hat tinh bang giay (VD: 240 cho 4 phut)\n"
            "  `current_sec`: (Tuy chon) Vi tri thoi gian phat hien tai tinh bang giay.",
            PropertyList({
                Property("total_sec", kPropertyTypeInteger),
                Property("current_sec", kPropertyTypeInteger, -1)
            }), [this](const PropertyList& properties) -> ReturnValue {
                int total_sec = properties["total_sec"].value<int>();
                int current_sec = properties["current_sec"].value<int>();
                if (env_display_) {
                    env_display_->SetMusicDuration(total_sec, current_sec);
                    return std::string("{\"status\":\"success\"}");
                }
                return std::string("{\"status\":\"error\"}");
            });
    }

public:
    AclabEsp32S3Board() :
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO),
        touch_button_(TOUCH_BUTTON_GPIO),
        mode_button_1_(MODE_BUTTON_1_GPIO),
        mode_button_2_(MODE_BUTTON_2_GPIO),
        dht11_(DHT11_DATA_GPIO) {
        
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeTools();
        InitializePowerSaveTimer();

        // Khoi dong trinh quan ly 4 che do hien thi moi truong (DHT11 + Charts)
        env_display_ = new EnvDisplayManager(display_, &dht11_);
        env_display_->Start();

        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, 
            AUDIO_I2S_SPK_GPIO_LRCK, 
            AUDIO_I2S_SPK_GPIO_DOUT, 
            AUDIO_I2S_MIC_GPIO_SCK, 
            AUDIO_I2S_MIC_GPIO_WS, 
            AUDIO_I2S_MIC_GPIO_DIN
        );
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }
};

DECLARE_BOARD(AclabEsp32S3Board);
