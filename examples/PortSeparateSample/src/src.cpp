

/// M5ライブラリは M5Stack.hやM5Core2.hを使用しても良いが、
/// この例では 旧CoreとCore2のどちらにも対応可能なM5Unifiedを使用する。

#include <M5Unified.hpp>  // https://github.com/M5Stack/M5Unified/
#include <esp_dmx.h>      // https://github.com/someweisguy/esp_dmx/

/// ESP_DMXライブラリはメジャーバージョンアップ時に色々な名称変更があったため、
/// バージョン1.x, 2.x, 3.1.x に対応できるようにdefine値に基づいたチェックを行う。
#if defined(ESP_DMX_VERSION_MAJOR)
    #define ESP_DMX_VERSION 3
#elif defined(DMX_PACKET_TIMEOUT_TICK)
    #define ESP_DMX_VERSION 2
#else
    #define ESP_DMX_VERSION 1
#endif

// パススルー構成の時は送受信をひとつのシリアルポートで行うが、
// セパレート構成の時は送信と受信を別々のシリアルポートで行う。

/// 送信に使用するUARTのポート番号
static constexpr const dmx_port_t dmxTxPort = 1;

/// 受信に使用するUARTのポート番号
static constexpr const dmx_port_t dmxRxPort = 2;

static constexpr const int dmxInterruptPriority = 1;
static constexpr const int dmxQueueSize         = 1;

QueueHandle_t dmxTxQueue;
QueueHandle_t dmxRxQueue;

/// 送受信データ
#if ESP_DMX_VERSION == 3
uint8_t in_data[DMX_PACKET_SIZE];
uint8_t out_data[DMX_PACKET_SIZE];
#else
uint8_t in_data[DMX_MAX_PACKET_SIZE];
uint8_t out_data[DMX_MAX_PACKET_SIZE];
#endif

/// デバッグ用
unsigned long lastUpdate = millis();

void setup(void) {
    M5.begin();
    /// デバッグ用
    Serial.begin(115200);

    /// Core2用のピン設定を用意。
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

    /// 送信用と受信用の両方のシリアルポートを準備する。
#if ESP_DMX_VERSION == 3
    dmx_config_t dmxConfig = DMX_CONFIG_DEFAULT;
#else
    dmx_config_t dmxConfig = DMX_DEFAULT_CONFIG;
    dmx_param_config(dmxTxPort, &dmxConfig);
    dmx_param_config(dmxRxPort, &dmxConfig);
#endif

///Ver2まではこのタイミングでdmx_set_pin
#if ESP_DMX_VERSION < 3
    /// 送信側ポートは Tx と En のみ使用する。Rx は使用しない。
    dmx_set_pin(dmxTxPort, transmitPin, -1, enablePin);

    /// 受信側ポートは Rx のみを使用する。Tx と En は使用しない。
    dmx_set_pin(dmxRxPort, -1, receivePin, -1);
#endif
    
    /// esp_dmxドライバを準備する。
#if ESP_DMX_VERSION == 3
    dmx_driver_install(dmxTxPort, &dmxConfig, DMX_INTR_FLAGS_DEFAULT);
    dmx_driver_install(dmxRxPort, &dmxConfig, DMX_INTR_FLAGS_DEFAULT);
#else
    dmx_driver_install(dmxTxPort, DMX_MAX_PACKET_SIZE, dmxQueueSize,
                       &dmxTxQueue, dmxInterruptPriority);
    dmx_driver_install(dmxRxPort, DMX_MAX_PACKET_SIZE, dmxQueueSize,
                       &dmxRxQueue, dmxInterruptPriority);
#endif

#if ESP_DMX_VERSION == 1
    dmx_set_mode(dmxRxPort, DMX_MODE_RX);
    dmx_set_mode(dmxTxPort, DMX_MODE_TX);
#elif ESP_DMX_VERSION == 2
    dmx_set_mode(dmxRxPort, DMX_MODE_READ);
    dmx_set_mode(dmxTxPort, DMX_MODE_WRITE);
#elif ESP_DMX_VERSION == 3
    dmx_set_pin(dmxTxPort, transmitPin, -1, enablePin);
    dmx_set_pin(dmxRxPort, -1, receivePin, -1);
#endif
}

void loop(void) {

#if ESP_DMX_VERSION == 3
    dmx_packet_t packet;
#else
    dmx_event_t packet;
#endif

/// RX側からDMXデータを受信する。
#if ESP_DMX_VERSION == 3
    if (dmx_receive(dmxRxPort, &packet, DMX_TIMEOUT_TICK)) {
        unsigned long now = millis(); //デバッグ用
        if (!packet.err) {
            dmx_read(dmxRxPort, in_data, packet.size);
// このサンプルでは memcpyを使って、入力内容をそのまま出力内容にコピーする。
                memcpy(out_data, in_data, DMX_PACKET_SIZE);
// ※ ここで値を変更したり、MIDI等から信号をミックスすることができる。
                    if (now - lastUpdate > 1000) {
                        Serial.printf("Start code: %d Slot 1: %d Number of Slot: %d\n", in_data[0], in_data[1], packet.size);
                        lastUpdate = now;
                    }
        }
    }
#else
    if (xQueueReceive(dmxRxQueue, &packet, DMX_RX_PACKET_TOUT_TICK)) {
        unsigned long now = millis(); //デバッグ用
        if (packet.status == DMX_OK) {
            if (ESP_OK ==
                dmx_read_packet(dmxRxPort, in_data, packet.size)) {

// このサンプルでは memcpyを使って、入力内容をそのまま出力内容にコピーする。
                memcpy(out_data, in_data, DMX_MAX_PACKET_SIZE);
// ※ ここで値を変更したり、MIDI等から信号をミックスすることができる。
                    if (now - lastUpdate > 1000) {
                        Serial.printf("Start code: %d Slot 1: %d Number of Slot: %d\n", in_data[0], in_data[1], packet.size);
                        lastupdate = now;
                    }
            }
        }
    }
#endif

// TX側からDMXデータを送信する。
#if ESP_DMX_VERSION == 3
    dmx_write(dmxTxPort, out_data, DMX_PACKET_SIZE);
#else
    dmx_write_packet(dmxTxPort, out_data, DMX_MAX_PACKET_SIZE);
#endif

#if ESP_DMX_VERSION == 1
    if (ESP_ERR_TIMEOUT != dmx_wait_tx_done(dmxTxPort, DMX_TX_PACKET_TOUT_TICK)) {
        dmx_tx_packet(dmxTxPort);
    }
#elif ESP_DMX_VERSION == 2
    if (ESP_ERR_TIMEOUT != dmx_wait_send_done(dmxTxPort, DMX_TX_PACKET_TOUT_TICK)) {
        dmx_send_packet(dmxTxPort, DMX_MAX_PACKET_SIZE);
    }
#elif ESP_DMX_VERSION == 3
    if (ESP_ERR_TIMEOUT != dmx_wait_sent(dmxTxPort, DMX_TIMEOUT_TICK)) {
        dmx_send(dmxTxPort, DMX_PACKET_SIZE);
    }
#endif
}
