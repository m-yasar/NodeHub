# Hub Firmware

ESP32. Node'lardan RS485/Modbus RTU ile veri toplar, MQTT ile sunucuya gönderir.

**Ortam:** ESP-IDF

## Projeler

| Proje | İçerik |
|---|---|
| `nodehub_mqtt_datasender` | Adım 1 — ESP32-C3, butona basınca MQTT publish |

## Hub'a driver eklenmez

Hub ömrü boyunca yalnızca Modbus konuşur. Yeni bir sensör tipi geldiğinde bu klasördeki
kod değişmez — değişen şey [`contract/`](../../contract/) içindeki tanımdır.

Hub tarafında kod değiştirmek gerekiyorsa, sözleşmede bir eksik var demektir.
