#pragma once

#include "common.h"

class view_receiver_t : public view_t {
   public:
    void setup(void) override {
#if ESP_DMX_VERSION == 1
        dmx_set_mode(dmxPort, DMX_MODE_RX);
#else
        dmx_set_mode(dmxPort, DMX_MODE_READ);
#endif
        memset(data, 0, sizeof(data));

        M5.Display.setFont(&fonts::Font0);
        M5.Display.setTextDatum(textdatum_t::top_left);
        M5.Display.setTextWrap(false);
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.drawFastHLine(0, CONSOLE_Y - 3, M5.Display.width(),
                                 TFT_DARKGRAY);

        canvas.setColorDepth(8);
        canvas.createSprite(M5.Display.width(),
                            M5.Display.height() - CONSOLE_Y);
        canvas.setTextScroll(true);

        fillBar(&M5.Display, 0, 0, M5.Display.width(), BAR_HEIGHT);
    }

    bool loop(void) override {
        bool change_view = M5.BtnB.wasClicked();
        if (!change_view) {
            auto td     = M5.Touch.getDetail();
            change_view = (td.wasClicked() && td.y > CONSOLE_Y &&
                           td.y < M5.Display.height());
        }
        if (change_view) {
            graph_view = !graph_view;
        }

        dmx_event_t packet;

        if (xQueueReceive(queue, &packet, DMX_RX_PACKET_TOUT_TICK)) {
            if (packet.status == DMX_OK) {
                if (!dmxIsConnected) {
                    dmxIsConnected = true;
                    canvas.println("DMX connected!");
                    canvas.pushSprite(&M5.Display, 0, CONSOLE_Y);
                }
                if (ESP_OK ==
                    dmx_read_packet(dmxPort, data[data_idx], packet.size)) {
                    updateDisplay(packet.duration);
                    data_idx = !data_idx;
                }
            } else {
                canvas.println("\nDMX error!");
                canvas.pushSprite(&M5.Display, 0, CONSOLE_Y);
            }
        }
        return true;
    }

    void close(void) override {
        canvas.deleteSprite();
    }

   private:
    static constexpr int BAR_HEIGHT = 128;
    static constexpr int CONSOLE_Y  = 152;

    uint32_t timer;
    size_t data_idx = 0;
    M5Canvas canvas;
    bool graph_view = false;
    uint8_t visible[DMX_MAX_PACKET_SIZE];
    uint8_t data[2][DMX_MAX_PACKET_SIZE];
    bool dmxIsConnected = false;

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
        uint32_t prev_color = (color_add + getBarColor(y)) & 0xF8FCF8u;
        uint32_t py         = y;
        while (h--) {
            uint32_t color = (color_add + getBarColor(++y)) & 0xF8FCF8u;
            if (prev_color != color || h == 0) {
                gfx->pushBlock(prev_color, w * (y - py));
                prev_color = color;
                py         = y;
            }
        }
    }

    void updateDisplay(int32_t duration) {
        timer += duration;
        int idx          = 0;
        bool full_redraw = false;
        bool drawed      = false;

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

        M5.Display.startWrite();
        for (int ch = 1; ch < DMX_MAX_PACKET_SIZE; ++ch) {
            if (!visible[ch]) {
                if (data[0][ch] == data[1][ch]) {
                    continue;
                }
                visible[ch] = true;
                full_redraw = true;
            }
            uint32_t current_data = data[data_idx][ch];
            uint32_t prev_data    = data[!data_idx][ch];
            if (full_redraw || current_data != prev_data) {
                int x = (idx * M5.Display.width()) / drawcount;
                int w = (((idx + 1) * M5.Display.width()) / drawcount) - x;
                if (full_redraw) {
                    M5.Display.fillRect(x, BAR_HEIGHT, w, 20, TFT_BLACK);
                    M5.Display.setTextColor(bar_color_table[ch & 15]);
                    M5.Display.setCursor(x - 3, BAR_HEIGHT + 12);
                    M5.Display.printf("% 3d", ch);
                }
                M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
                M5.Display.setCursor(x + 2, BAR_HEIGHT + 2);
                M5.Display.printf("%02x", current_data);

                int y = BAR_HEIGHT - ((current_data + 1) >> 1);
                int py =
                    BAR_HEIGHT - (full_redraw ? 0 : ((prev_data + 1) >> 1));
                if (full_redraw) {
                    fillBar(&M5.Display, x, 0, w, BAR_HEIGHT, ch);
                }
                if (w > 2) {
                    --w;
                }
                if (w > 2) {
                    ++x;
                    --w;
                }
                fillBar(&M5.Display, x, py, w, y - py, ch);
            }

            if (graph_view) {
                if (!drawed) {
                    drawed = true;
                }
                int32_t x1 = current_data;
                int32_t x2 = prev_data;
                if (x1 > x2) {
                    std::swap(x1, x2);
                }
                canvas.fillRectAlpha(32 + x1, canvas.height() - 1, x2 - x1 + 1,
                                     1, 128, bar_color_table[ch & 15]);
            } else {
                if (prev_data != current_data) {
                    if (!drawed) {
                        drawed = true;
                        canvas.setTextColor(TFT_SKYBLUE);
                        int msec   = timer / 1000;
                        int second = msec / 1000;
                        int minute = second / 60;
                        msec -= second * 1000;
                        second -= minute * 60;
                        canvas.printf("%2d:%02d.%03d", minute, second, msec);
                    }
                    if (canvas.getCursorX() > 280) {
                        canvas.println();
                        canvas.setCursor(9 * canvas.fontWidth(),
                                         canvas.getCursorY());
                    }
                    canvas.setTextColor(TFT_DARKGRAY);
                    canvas.printf(" [", timer / 1000);
                    canvas.setTextColor(bar_color_table[ch & 15]);
                    canvas.printf("%3d", ch);
                    canvas.setTextColor((uint8_t)0x29u);
                    canvas.print("]");
                    canvas.setTextColor(TFT_WHITE);
                    canvas.printf("%02x", current_data);
                }
            }
            ++idx;
        }
        if (drawed) {
            canvas.pushSprite(&M5.Display, 0, CONSOLE_Y);
            if (graph_view) {
                canvas.scroll(0, -1);
            } else {
                canvas.println();
            }
        }
        M5.Display.endWrite();
    }
};
