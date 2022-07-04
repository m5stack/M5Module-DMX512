# Module DMX512 for M5Stack

### target devices
 - M5Stack BASIC / GRAY / GO / FIRE
 - M5Stack Core2

### pin map

release ver:

|    |Core1 |Core2 |
|:--:|:----:|:----:|
| TX |  13  |  19  |
| RX |  35  |  35  |
| EN |  12  |  27  |


beta ver:

|    |Core1 |Core2 |
|:--:|:----:|:-----:|
| TX |  15  |   2  |
| RX |  13  |  19  |
| EN |  12  |  27  |

※ Core1 : BASIC / GRAY / GO / FIRE

---
There is currently no official M5Stack library for the DMX512 module.<br>
We recommend using the [esp_dmx](https://github.com/someweisguy/esp_dmx) library.

[someweisguy/esp_dmx](https://github.com/someweisguy/esp_dmx)

### sample application.
 - [DMX512Tools](examples/DMX512Tools/)

