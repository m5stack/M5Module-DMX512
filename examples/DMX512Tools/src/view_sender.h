#pragma once

#include "common.h"
#include "logo_sender.h"

class view_sender_t : public view_t {
   public:
    void setup(void) override {
#if ESP_DMX_VERSION == 1
        dmx_set_mode(dmxPort, DMX_MODE_TX);
#else
        dmx_set_mode(dmxPort, DMX_MODE_WRITE);
#endif
        memset(data, 0, sizeof(data));
        dmx_write_packet(dmxPort, data[0], DMX_MAX_PACKET_SIZE);

        M5.Display.setFont(&fonts::Font0);
        M5.Display.setTextDatum(textdatum_t::top_left);
        M5.Display.setTextWrap(false);
        M5.Display.fillScreen(TFT_BLACK);

        enable_channel_count = 512;
        for (int i = 0; i < enable_channel_count; ++i) {
            int ch             = i + 1;
            enable_channels[i] = ch;
            visible[ch]        = 1;
            // data[0][ch] = rand();
            // data[1][ch] = data[0][ch];
        }
        target_channel = enable_channels[target_channel_index];

        updateDisplay(true);
        hideUIValueSet();
    }

    bool loop(void) override {
#if ESP_DMX_VERSION == 1
        if (ESP_ERR_TIMEOUT != dmx_wait_tx_done(dmxPort, 0)) {
            dmx_tx_packet(dmxPort);
        }
#else
        if (ESP_ERR_TIMEOUT != dmx_wait_send_done(dmxPort, 0)) {
            dmx_send_packet(dmxPort, DMX_MAX_PACKET_SIZE);
        }
#endif

        bool modified     = false;
        bool btnB_Clicked = M5.BtnB.wasClicked();

        auto tp = M5.Touch.getDetail();
        if (tp.state) {
            if (tp.base_y < scroll_height &&
                tp.base_x <
                    scroll_width) {  /// スクロールエリア内でのタッチ操作の処理;
                auto dy      = tp.deltaY();
                scroll_y_add = -dy;
                if (dy && ui_mode == ui_mode_t::mode_value_setting) {
                    setUIMode(ui_mode_t::mode_channel_select);
                }

                if (tp.wasClicked()) {  /// タップ操作でチャンネル選択;
                    int new_channel_index =
                        (tp.x / channel_item_width) +
                        ((tp.y + scroll_y) / channel_item_height) *
                            channel_item_cols;

                    if (new_channel_index < 0) {
                        new_channel_index = 0;
                    } else if (new_channel_index >= enable_channel_count) {
                        new_channel_index = enable_channel_count - 1;
                    } else if (target_channel_index != new_channel_index) {
                        target_channel_index = new_channel_index;
                        target_channel = enable_channels[new_channel_index];
                        target_value   = data[data_idx][target_channel];
                        modified       = true;
                        ui_mode        = ui_mode_t::mode_channel_select;
                        setUIMode(ui_mode_t::mode_value_setting);
                    } else {
                        setUIMode(ui_mode == ui_mode_t::mode_channel_select
                                      ? ui_mode_t::mode_value_setting
                                      : ui_mode_t::mode_channel_select);
                    }
                    updateDisplay(true);
                }
            } else if (tp.base_x >= scroll_width &&
                       tp.base_y < M5.Display.height()) {
                if (ui_mode == ui_mode_t::mode_value_setting) {
                    int new_value = target_value;
                    if (tp.base_y >= slider_y && tp.base_y - 128 < slider_y) {
                        new_value = 255 - ((tp.y - slider_y) << 1);
                    } else if (tp.wasPressed() || tp.isHolding()) {
                        new_value += (tp.base_y < slider_y) ? 1 : -1;
                    }

                    if (new_value < 0) {
                        new_value = 0;
                    } else if (new_value > 255) {
                        new_value = 255;
                    }
                    if (target_value != new_value) {
                        drawUIValueSet(new_value);
                    }
                }
            }
        }

        switch (ui_mode) {
            case mode_channel_select: {
                int new_channel_index = target_channel_index;
                if (M5.BtnA.wasPressed() || M5.BtnA.isHolding()) {
                    --new_channel_index;
                }
                if (M5.BtnC.wasPressed() || M5.BtnC.isHolding()) {
                    ++new_channel_index;
                }
                if (new_channel_index != target_channel_index) {
                    if (new_channel_index < 0) {
                        new_channel_index = 0;
                    } else if (new_channel_index >= enable_channel_count) {
                        new_channel_index = enable_channel_count - 1;
                    }

                    target_channel_index = new_channel_index;
                    int new_channel      = enable_channels[new_channel_index];
                    if (target_channel != new_channel) {
                        target_channel = new_channel;
                        target_value   = data[data_idx][new_channel];
                        if (!scrollToTargetItem()) {
                            updateDisplay(true);
                        }
                    }
                    modified = true;
                }
                if (btnB_Clicked) {
                    setUIMode(ui_mode_t::mode_value_setting);
                    modified = true;
                }
            } break;

            case mode_value_setting:

            {
                int new_value = target_value;
                if (M5.BtnA.wasPressed() || M5.BtnA.isHolding()) {
                    --new_value;
                }
                if (M5.BtnC.wasPressed() || M5.BtnC.isHolding()) {
                    ++new_value;
                }
                if (new_value != target_value) {
                    if (new_value <= 0) {
                        new_value = 0;
                    } else if (new_value > 255) {
                        new_value = 255;
                    }
                    drawUIValueSet(new_value);
                }
                if (btnB_Clicked) {
                    setUIMode(ui_mode_t::mode_channel_select);
                }
            }
        }

        if (target_value != data[data_idx][target_channel]) {
            data_idx                       = 1 - data_idx;
            data[data_idx][target_channel] = target_value;
            dmx_write_packet(dmxPort, data[data_idx], DMX_MAX_PACKET_SIZE);
            updateDisplay();
            modified                           = true;
            data[1 - data_idx][target_channel] = target_value;
        }

        if (scroll_y_add) {
            int new_y = scroll_y + scroll_y_add;
            scroll_y_add += (scroll_y_add < 0) ? 1 : -1;
            if (new_y < 0) {
                new_y = 0;
            }
            static constexpr int scroll_liimt =
                channel_item_height *
                    ((511 + channel_item_cols) / channel_item_cols) -
                scroll_height;
            if (new_y > scroll_liimt) {
                new_y = scroll_liimt;
            }
            if (scroll_y != new_y) {
                scroll_y = new_y;
                updateDisplay(true);
            }
        }

        if (!modified) {
            delay(1);
        }

        return true;
    }

    void close(void) override {
        for (auto& tmp : canvas) {
            tmp.deleteSprite();
        }
    }

   private:
    static constexpr int BAR_HEIGHT = 64;
    static constexpr int CONSOLE_Y  = 88;

    size_t data_idx = 0;
    M5Canvas canvas[2];
    bool canvas_flip;

    uint16_t enable_channels[DMX_MAX_PACKET_SIZE];
    uint16_t enable_channel_count;

    uint8_t visible[DMX_MAX_PACKET_SIZE];
    uint8_t data[2][DMX_MAX_PACKET_SIZE];
    uint16_t target_channel_index = 0;
    uint16_t target_channel;
    int16_t target_value                       = 0;
    int16_t scroll_y                           = 0;
    int16_t scroll_y_add                       = 0;
    static constexpr uint16_t scroll_width     = 250;
    static constexpr uint8_t scroll_height     = 240;
    static constexpr uint8_t channel_item_cols = 5;
    static constexpr uint8_t channel_item_width =
        scroll_width / channel_item_cols;
    static constexpr uint8_t channel_item_height = 54;
    static constexpr uint16_t slider_x           = scroll_width + 28;
    static constexpr uint8_t slider_w            = 16;
    static constexpr uint8_t slider_y            = 72;  // 40;
    static constexpr uint8_t slider_btn_height   = 32;

    int16_t circle_ui_x  = 160;
    int16_t circle_ui_y  = 160;
    int16_t circle_ui_r0 = 40;
    int16_t circle_ui_r1 = 72;

    enum ui_mode_t {
        mode_channel_select,
        mode_value_setting,
    };

    ui_mode_t ui_mode = mode_channel_select;

    void setUIMode(ui_mode_t new_mode) {
        if (ui_mode == new_mode) {
            return;
        }
        ui_mode = new_mode;
        switch (new_mode) {
            case ui_mode_t::mode_channel_select:
                // M5.Speaker.tone(660, 80);
                hideUIValueSet();
                break;

            case ui_mode_t::mode_value_setting:
                // M5.Speaker.tone(880, 80);
                target_value = data[data_idx][target_channel];
                drawUIValueSet(target_value, true);
                scrollToTargetItem();
                break;

            default:
                break;
        }
        updateDisplay(true);
    }

    bool scrollToTargetItem(void) {
        int new_y =
            (target_channel_index / channel_item_cols) * channel_item_height;
        if (scroll_y > new_y) {
            scroll_y_add = 0;
            int target   = scroll_y - new_y;
            while (0 < (target += --scroll_y_add))
                ;
            return true;
        } else if (scroll_y + scroll_height - channel_item_height < new_y) {
            scroll_y_add = 0;
            int target =
                new_y - (scroll_y + scroll_height - channel_item_height);
            while (0 < (target -= ++scroll_y_add))
                ;
            return true;
        }
        return false;
    }

    void drawFocusBox(LovyanGFX* gfx, int x, int y, int w, int h, int fw) {
        int horizon  = w >> 2;
        int vertical = h >> 3;
        gfx->fillRect(x, y, horizon, fw);
        gfx->fillRect(x + w - horizon, y, horizon, fw);
        gfx->fillRect(x, y + h - fw, horizon, fw);
        gfx->fillRect(x + w - horizon, y + h - fw, horizon, fw);
        gfx->fillRect(x, y + fw, fw, vertical);
        gfx->fillRect(x + w - fw, y + fw, fw, vertical);
        gfx->fillRect(x, y + h - fw - vertical, fw, vertical);
        gfx->fillRect(x + w - fw, y + h - fw - vertical, fw, vertical);
    }

    void hideUIValueSet(void) {
        M5.Display.fillRect(scroll_width, 0, 320 - scroll_width,
                            M5.Display.height(), TFT_BLACK);
        M5.Display.fillRect(scroll_width, scroll_height, 1,
                            M5.Display.height() - (scroll_height << 1),
                            TFT_DARKGRAY);
        M5.Display.pushImage(scroll_width + 8, 120, logo_sender_width,
                             logo_sender_height,
                             (const m5gfx::swap565_t*)logo_sender);
    }

    void drawUIValueUpDown(int sign = 0) {
        int x = scroll_width + 10;
        int w = 320 - scroll_width - 20;
        // M5.Display.drawRoundRect(x,              2, w, slider_btn_height - 4,
        // 4, sign > 0 ? TFT_WHITE : TFT_DARKGRAY);
        M5.Display.drawRoundRect(x, slider_y - 10 - (slider_btn_height - 4), w,
                                 slider_btn_height - 4, 4,
                                 sign > 0 ? TFT_WHITE : TFT_DARKGRAY);
        M5.Display.drawRoundRect(x, slider_y + 128 + 10, w,
                                 slider_btn_height - 4, 4,
                                 sign < 0 ? TFT_WHITE : TFT_DARKGRAY);
    }

    void drawUIValueSet(uint16_t new_value, bool force_redraw = false) {
        M5.Display.setTextDatum(textdatum_t::middle_center);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.setFont(&fonts::AsciiFont8x16);
        char str[8];
        uint32_t color = bar_color_table[target_channel & 15];

        int y  = ((256 - new_value) >> 1) + slider_y;
        int py = ((256 - target_value) >> 1) + slider_y;

        int text_x = (scroll_width + M5.Display.width()) >> 1;
        // int text_y = ((slider_y * 2 + 128)+M5.Display.height()) >> 1;
        int text_y = 16;
        if (force_redraw) {
            M5.Display.fillRect(scroll_width + 1, 0, 320 - scroll_width,
                                M5.Display.height(), TFT_BLACK);
            py = 128 + slider_y;

            M5.Display.setColor(TFT_WHITE);
            drawFocusBox(&M5.Display, slider_x - 8, slider_y - 5, slider_w + 16,
                         128 + 10, 2);

            fillBar(&M5.Display, slider_x, slider_y, slider_w, y - slider_y,
                    target_channel);

            int text_x = (scroll_width + M5.Display.width()) >> 1;
            M5.Display.drawString("+", text_x,
                                  slider_y - 8 - (slider_btn_height >> 1));
            M5.Display.drawString(
                "-", text_x, slider_y + 128 + 8 + (slider_btn_height >> 1));
            snprintf(str, sizeof(str), "%3dch", target_channel);
            M5.Display.drawString(str, text_x, text_y - 8);
            drawUIValueUpDown();
        }
        if (force_redraw || target_value != new_value) {
            snprintf(str, sizeof(str), "%3d", new_value);
            M5.Display.drawString(str, text_x, text_y + 8);
            target_value = new_value;
        }

        fillBar(&M5.Display, slider_x, py, slider_w, y - py, target_channel);
    }

    uint32_t getBarColor(int32_t y) {
        int32_t v  = 63 - (y >> 1);
        int32_t v2 = 63 - ((y + 1) >> 1);
        if ((v >> 4) != (v2 >> 4)) {
            return 0x887766u;
        }
        return m5gfx::color888(v + 2, v, v + 6);
    }

    void fillBar(LovyanGFX* gfx, int32_t x, int32_t y, int32_t w, int32_t h,
                 size_t ch = 0) {
        uint32_t color_add = 0;
        if (h < 0) {
            y += h;
            h         = -h;
            color_add = (bar_color_table[ch & 15] >> 1) & 0x7F7F7Fu;
        }

        gfx->setAddrWindow(x, y, w, h);
        uint32_t prev_color =
            (color_add + getBarColor(y - slider_y)) & 0xF8FCF8u;
        uint32_t py = y;
        while (h--) {
            uint32_t color =
                (color_add + getBarColor(++y - slider_y)) & 0xF8FCF8u;
            if (prev_color != color || h == 0) {
                gfx->pushBlock(prev_color, w * (y - py));
                prev_color = color;
                py         = y;
            }
        }
    }

    void updateDisplay(bool full_redraw = false) {
        int idx     = 0;
        bool drawed = false;

        size_t drawcount = 0;
        for (int ch = 1; ch < DMX_MAX_PACKET_SIZE; ++ch) {
            if (!visible[ch]) {
                if (data[0][ch] == data[1][ch]) {
                    continue;
                }
                visible[ch] = true;
                full_redraw = true;
            }
            ++drawcount;
        }

        M5.Display.setClipRect(0, 0, 320, scroll_height);
        M5.Display.startWrite();

        static constexpr int circle_x = channel_item_width >> 1;
        static constexpr int circle_y = (channel_item_height - 8) >> 1;

        for (int ch = 1; ch < 512 + channel_item_cols; ++ch) {
            uint32_t current_data = 0;
            uint32_t prev_data    = 0;
            if (ch < DMX_MAX_PACKET_SIZE) {
                current_data = data[data_idx][ch];
                prev_data    = data[!data_idx][ch];
            }
            if (full_redraw || current_data != prev_data) {
                int y = (idx / channel_item_cols) * channel_item_height;
                y -= scroll_y;
                if (y >= scroll_height) {
                    break;
                }
                if (y + channel_item_height > 0) {
                    int x = (idx % channel_item_cols) * channel_item_width;

                    auto& c     = canvas[canvas_flip];
                    canvas_flip = !canvas_flip;
                    c.createSprite(channel_item_width, channel_item_height);

                    if (ch < DMX_MAX_PACKET_SIZE) {
                        if (ch == target_channel) {
                            int fw = (ui_mode == ui_mode_t::mode_channel_select)
                                         ? 2
                                         : 1;
                            c.setColor(0xFFFFFFu);
                            drawFocusBox(&c, 0, 0, channel_item_width,
                                         channel_item_height, fw);
                        }
                        bool dimmer =
                            (ui_mode != ui_mode_t::mode_channel_select) &&
                            (ch != target_channel);

                        c.setTextDatum(textdatum_t::middle_center);
                        c.setTextColor(dimmer ? TFT_DARKGRAY : TFT_WHITE);
                        c.setFont(&fonts::Font2);
                        c.drawNumber(ch, circle_x + 1, circle_y);
                        c.setFont(&fonts::Font0);
                        c.setTextDatum(textdatum_t::top_center);
                        c.drawNumber(current_data, circle_x + 1, circle_y + 20);
                        float angle = current_data * 360.0f / 255;

                        c.fillArc(circle_x, circle_y, 17, 15, 270 + angle, 269,
                                  dimmer ? 0x131313u : 0x333333u);
                        uint32_t color = bar_color_table[ch & 15];
                        if (dimmer) {
                            color = (0x3F3F3F & (color >> 2)) + 0x2F2F2Fu;
                        }
                        c.fillArc(circle_x, circle_y, 19, 13, 270, 270 + angle,
                                  color);
                    }
                    c.pushSprite(&M5.Display, x, y);
                }
            }
            ++idx;
        }
        M5.Display.clearClipRect();
        M5.Display.endWrite();
    }
};
