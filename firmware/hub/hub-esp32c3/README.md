# Hub Firmware — ESP32-C3

Node'lara kimlik dağıtır, RS485 hattı üzerinden ölçümlerini okur ve MQTT ile sunucuya
gönderir.

**Hedef donanım:** ESP32-C3
**Ortam:** ESP-IDF

## Ne yapıyor

**Kimlik atama.** BOOT butonuna basınca hatta bağlı, kimliği olmayan node'ları tarar ve
her birine bir Modbus adresi verir. Node'lar aynı anda konuşmasın diye kendine özgü bir
süre bekleyip hattı dinler; hub verdiği adresi yoklayarak atamayı doğrular.

Protokolün ayrıntısı: [`docs/modbus-sozlesmesi.md`](../../../docs/modbus-sozlesmesi.md)

**Ölçüm okuma.** Henüz yazılmadı. Kimliği bilinen node'lar Modbus ile periyodik olarak
okunacak.

**MQTT.** Broker'a TLS üzerinden bağlanır. Şu an yalnızca bağlantı kuruluyor; ölçüm akışı
yukarıdaki adım tamamlanınca bağlanacak.

Wi-Fi bağlantısı kurulamazsa hub yeniden başlamaz — RS485 tarafı ağdan bağımsız çalışır.

## RS485 bağlantısı

| ESP32-C3 | Transceiver |
|---|---|
| GPIO4 | DI |
| GPIO5 | RO |
| GPIO6 | DE |
| GND | RE |

`RE` sabit düşük tutulur; alıcı sürekli açık kalır. Hub kendi yayınını da duyar, gelen
çerçeve son gönderilenle birebir aynıysa yankı sayılıp atılır.

## Şu anki ayarlar

| Ayar | Değer |
|---|---|
| RS485 | 19200 baud, 8N1 |
| Broker | `mqtts://broker.emqx.io:8883` |
| Konu (topic) | `mYasar/NodeHub` |
| Buton | GPIO9 (BOOT) |

Broker halka açık bir test sunucusu, kimlik doğrulama istemiyor.

## TLS doğrulama

Sunucu sertifikası varsayılan olarak ESP-IDF'in sertifika paketiyle doğrulanır; bu yöntem
herhangi bir genel broker ile çalışır.

`menuconfig` içinde ikinci bir seçenek daha var: gömülü Mosquitto CA. O seçenek projedeki
`main/mosquitto.org.crt` dosyasını kullandığı için **yalnızca `test.mosquitto.org` ile**
çalışır.

**Dikkat:** doğrulama yöntemini değiştirmek broker adresini kendiliğinden güncellemez.
Yöntemi değiştirdiysen adresi ve portu da elle düzelt.

## Yapılandırma

```
idf.py set-target esp32c3
idf.py menuconfig
```

- **Example Connection Configuration** — Wi-Fi ağı ve parolası
- **Example Configuration** — broker adresi, yayın konusu, buton GPIO'su

Wi-Fi bilgileri depoda tutulmaz; `sdkconfig` takip edilmiyor. Diğer ayarların varsayılanları
`sdkconfig.defaults` içinde.

## Derleme ve yükleme

```
idf.py -p PORT flash monitor
```

Seri izleyiciden çıkmak için `Ctrl-]`.

## Köken

Bu proje ESP-IDF'in `protocols/mqtt/ssl` örneğinden başlatıldı
(Espressif Systems, Unlicense / CC0-1.0). Kimlik atama katmanı, RS485 haberleşmesi ve
NodeHub'a özgü yapılandırma sonradan eklendi.
