# Hub Firmware

ESP32. Node'lardan RS485/Modbus RTU ile veri toplar, MQTT ile sunucuya gönderir.

**Ortam:** ESP-IDF

## Projeler

| Proje | İçerik |
|---|---|
| [`hub-esp32c3`](hub-esp32c3/) | ESP32-C3. BOOT butonuna basınca node taraması başlatır, MQTT ile sunucuya yayın yapar |

## Köken

Bu proje ESP-IDF'in `protocols/mqtt/ssl` örneğinden başlatıldı
(Espressif Systems, Unlicense / CC0-1.0). Buton tetiklemeli yayın, sayaçlı yük ve
NodeHub'a özgü yapılandırma sonradan eklendi.

## Hub'a driver eklenmez

Hub ömrü boyunca yalnızca Modbus konuşur. Yeni bir sensör tipi geldiğinde bu klasördeki
kod değişmez — değişen şey [`contract/`](../../contract/) içindeki tanımdır.

Hub tarafında kod değiştirmek gerekiyorsa, sözleşmede bir eksik var demektir.
