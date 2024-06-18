
#include "common.h"
#include "view_receiver.h"
#include "view_sender.h"

QueueHandle_t queue;

enum scene_mode_t {
    mode_select,
    mode_receiver,
    mode_sender,
};

static scene_mode_t scene_mode = mode_select;
static ui_button_t btns[2]     = {{0, 200, 120, 40, "Receiver"},
                                  {200, 200, 120, 40, "Sender"}};
static view_receiver_t view_receiver;
static view_sender_t view_sender;

void drawSelectSetup(void) {
    scene_mode = mode_select;
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(textdatum_t::baseline_center);
    M5.Display.setFont(&fonts::Font4);
    M5.Display.drawString("DMX512Tools", M5.Display.width() >> 1,
                          M5.Display.height() >> 2);

    M5.Display.setFont(&fonts::AsciiFont8x16);
    M5.Display.drawString("Select mode", M5.Display.width() >> 1,
                          M5.Display.height() * 12 >> 4);
    for (size_t i = 0; i < 2; ++i) {
        btns[i].draw(&M5.Display, false, false, true);
    }
}

void setup(void) {
    M5.begin();

    /* Configure the DMX hardware to the default DMX settings and tell the DMX
      driver which hardware pins we are using. */
    dmx_config_t dmxConfig = DMX_DEFAULT_CONFIG;
    dmx_param_config(dmxPort, &dmxConfig);

    /// For M5Stack Core2/Tough pin setting: TX:19  RX:35  EN:27
    gpio_num_t transmitPin = GPIO_NUM_19;
    gpio_num_t receivePin  = GPIO_NUM_35;
    gpio_num_t enablePin   = GPIO_NUM_27;

    /// M5Unifiedの実行環境チェックを利用し、CoreS3,旧Coreでの動作の場合はピン設定を変更する。
    if (M5.getBoard() == m5::board_t::board_M5StackCoreS3
     || M5.getBoard() == m5::board_t::board_M5StackCoreS3SE) {
        /// M5Stack CoreS3 pin setting: TX:7  RX:10  EN:6
        transmitPin = GPIO_NUM_7;
        receivePin  = GPIO_NUM_10;
        enablePin   = GPIO_NUM_6;
    }
    else if (M5.getBoard() == m5::board_t::board_M5Stack) {
        /// M5Stack(BASIC/GRAY/GO/FIRE) pin setting: TX:13  RX:35  EN:12
        transmitPin = GPIO_NUM_13;
        receivePin  = GPIO_NUM_35;
        enablePin   = GPIO_NUM_12;
    }
    dmx_set_pin(dmxPort, transmitPin, receivePin, enablePin);

    dmx_driver_install(dmxPort, DMX_MAX_PACKET_SIZE, dmxQueueSize, &queue,
                       dmxInterruptPriority);

    drawSelectSetup();
}

int getBtnIndex(int x, int y) {
    for (int i = 0; i < 2; ++i) {
        if (btns[i].contain(x, y)) {
            return i;
        }
    }
    return -1;
}

int focus_idx = -1;

void loop(void) {
    M5.update();
    switch (scene_mode) {
        case mode_receiver:
            if (!view_receiver.loop()) {
                view_receiver.close();
                drawSelectSetup();
            }
            return;

        case mode_sender:
            if (!view_sender.loop()) {
                view_sender.close();
                drawSelectSetup();
            }
            return;

        default:
            break;
    }

    int clicked_idx = -1;

    auto tp = M5.Touch.getDetail();
    if (tp.wasPressed()) {
        focus_idx = getBtnIndex(tp.base_x, tp.base_y);
    }
    if (tp.wasClicked()) {
        clicked_idx = getBtnIndex(tp.base_x, tp.base_y);
    }

    if (M5.BtnA.wasPressed()) {
        focus_idx = 0;
    } else if (M5.BtnA.wasReleased() && focus_idx == 0) {
        focus_idx = -1;
    }
    if (M5.BtnC.wasPressed()) {
        focus_idx = 1;
    } else if (M5.BtnC.wasReleased() && focus_idx == 1) {
        focus_idx = -1;
    }
    if (M5.BtnA.wasClicked()) {
        clicked_idx = 0;
    }
    if (M5.BtnC.wasClicked()) {
        clicked_idx = 1;
    }
    for (size_t i = 0; i < 2; ++i) {
        btns[i].draw(&M5.Display, i == focus_idx, i == clicked_idx);
    }
    delay(10);

    if (clicked_idx >= 0) {
        scene_mode = (clicked_idx == 0) ? mode_receiver : mode_sender;
        switch (scene_mode) {
            case mode_receiver:
                view_receiver.setup();
                break;

            case mode_sender:
                view_sender.setup();
                break;

            default:
                break;
        }
    }
}
