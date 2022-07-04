#pragma once

#include <esp_dmx.h>      // https://github.com/someweisguy/esp_dmx/
#include <M5Unified.hpp>  // https://github.com/M5Stack/M5Unified/

#if defined(DMX_PACKET_TIMEOUT_TICK)
#define ESP_DMX_VERSION 2
#else
#define ESP_DMX_VERSION 1
#endif

/* lets decide which DMX port to use. The ESP32 has either 2 or 3 ports.
  Port 0 is typically used to transmit serial data back to your Serial Monitor,
  so we shouldn't use that port. Lets use port 1! */
static constexpr const dmx_port_t dmxPort       = 1;
static constexpr const int dmxInterruptPriority = 1;
static constexpr const int dmxQueueSize         = 1;
extern QueueHandle_t queue;

static constexpr uint32_t bar_color_table[] = {
    0xED7F11u, 0xCDB001u, 0xA0D904u, 0x6EF51Au, 0x3FFF3Fu, 0x1AF56Eu,
    0x04D9A0u, 0x01B0CDu, 0x117FEDu, 0x314EFDu, 0x5E25FAu, 0x9009E4u,
    0xBF00BFu, 0xE40990u, 0xFA255Eu, 0xFD4E31u};

struct ui_rect_t {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    ui_rect_t(int src_x, int src_y, int src_w, int src_h)
        : x{src_x}, y{src_y}, w{src_w}, h{src_h} {};
    bool contain(int src_x, int src_y) {
        return src_x >= x && src_x < x + w && src_y >= y && src_y < y + h;
    }
};

struct ui_button_t : public ui_rect_t {
    ui_button_t(int src_x, int src_y, int src_w, int src_h,
                const char* src_caption)
        : ui_rect_t{src_x, src_y, src_w, src_h}, caption{src_caption} {};

    const char* caption = nullptr;
    void draw(LovyanGFX* gfx, bool has_focus, bool is_pressed,
              bool invalidated = false) {
        auto rim_color = TFT_DARKGRAY;
        auto bg_color  = TFT_BLACK;
        auto fg_color  = TFT_WHITE;

        if (invalidated) {
            _prev_focus   = !has_focus;
            _prev_pressed = !is_pressed;
        }

        if (has_focus) {
            rim_color = TFT_WHITE;
        }
        if (is_pressed) {
            std::swap(fg_color, bg_color);
        }
        gfx->setTextDatum(textdatum_t::middle_center);
        gfx->setTextColor(fg_color);

        if (_prev_pressed != is_pressed) {
            gfx->fillRoundRect(x, y, w, h, 3, bg_color);
            _prev_focus = !has_focus;
        }
        if (_prev_focus != has_focus) {
            gfx->drawRoundRect(x, y, w, h, 3, rim_color);
        }
        if (_prev_pressed != is_pressed) {
            if (caption) {
                gfx->drawString(caption, x + (w >> 1), y + (h >> 1));
            }
        }
        _prev_focus   = has_focus;
        _prev_pressed = is_pressed;
    }

   private:
    bool _prev_focus   = false;
    bool _prev_pressed = false;
};

class view_t {
   public:
    virtual void setup(void) = 0;
    virtual bool loop(void)  = 0;
    virtual void close(void) = 0;
};
