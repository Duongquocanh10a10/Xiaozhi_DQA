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

        // Nut 1 (GPIO 17): Chuyen vong tron qua cac mode 1 -> 2 -> 3 -> 1
        // Phan hoi tuc thi ngay khi an xuong (OnPressDown)
        mode_button_1_.OnPressDown([this]() {
            static uint32_t last_press = 0;
            uint32_t now = esp_log_timestamp();
            if (now - last_press < 250) return;
            last_press = now;

            ESP_LOGI(TAG, "Mode Button 1 Pressed (GPIO 17) -> Cycle Mode");
            power_save_timer_->WakeUp();
            if (env_display_) {
                env_display_->CycleMode123();
            }
        });

        // Nut 2 (GPIO 21): Chuyen truc tiep sang Mode 4 (Xem song song ca Nhiet do & Do am)
        // Phan hoi tuc thi ngay khi an xuong (OnPressDown)
        mode_button_2_.OnPressDown([this]() {
            static uint32_t last_press = 0;
            uint32_t now = esp_log_timestamp();
            if (now - last_press < 250) return;
            last_press = now;

            ESP_LOGI(TAG, "Mode Button 2 Pressed (GPIO 21) -> Switch Mode 4");
            power_save_timer_->WakeUp();
            if (env_display_) {
                env_display_->SwitchToMode4();
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
            "Chuyen che do hien thi TFT: 1 (AI Xiaozhi), 2 (Do thi Nhiet do), 3 (Do thi Do am), 4 (Tong hop Nhiet do & Do am).",
            PropertyList({
                Property("mode", kPropertyTypeInteger, 1, 4)
            }), [this](const PropertyList& properties) {
                int mode = properties["mode"].value<int>();
                if (env_display_ && mode >= 1 && mode <= 4) {
                    env_display_->SetMode((DisplayMode)mode);
                    return true;
                }
                return false;
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
