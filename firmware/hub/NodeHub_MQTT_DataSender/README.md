# NodeHub — MQTT Veri Göndericisi

Hub'ın sunucuya veri gönderme altyapısı. Yol haritasındaki **Adım 1**'in kodu.

ESP32-C3 karta yüklenir, Wi-Fi'a bağlanır ve BOOT butonuna (GPIO9) her basıldığında
sayaçlı bir mesajı MQTT ile broker'a yayınlar. Amaç zincirin son halkasını —
karttan sunucuya veri akışını — çalışır hâle getirmek.

**Hedef donanım:** ESP32-C3
**Ortam:** ESP-IDF

## Şu anki ayarlar

| Ayar | Değer |
|---|---|
| Broker | `mqtts://broker.emqx.io:8883` |
| Konu (topic) | `mYasar/NodeHub` |
| Buton | GPIO9 (BOOT) |

Broker halka açık bir test sunucusu, kimlik doğrulama istemiyor.

## TLS doğrulama

Bağlantı TLS üzerinden kuruluyor. Sunucu sertifikası varsayılan olarak ESP-IDF'in
sertifika paketiyle doğrulanıyor; bu yöntem herhangi bir genel broker ile çalışır.

`menuconfig` içinde ikinci bir seçenek daha var: gömülü Mosquitto CA. O seçenek
projedeki `main/mosquitto.org.crt` dosyasını kullandığı için **yalnızca
`test.mosquitto.org` ile** çalışır, başka bir broker ile kullanılamaz.

**Dikkat:** doğrulama yöntemini değiştirmek broker adresini kendiliğinden güncellemez.
Yöntemi değiştirdiysen adresi ve portu da elle düzelt, yoksa bağlantı kurulmaz.

## Yapılandırma

```
idf.py set-target esp32c3
idf.py menuconfig
```

Menüde iki yer var:

- **Example Connection Configuration** — Wi-Fi ağı ve parolası
- **Example Configuration** — broker adresi, yayın konusu, buton GPIO'su

## Derleme ve yükleme

```
idf.py -p PORT flash monitor
```

Seri izleyiciden çıkmak için `Ctrl-]`.

MQTT bileşeni `main/idf_component.yml` içinde tanımlı; ESP-IDF bileşen yöneticisi
ilk derlemede otomatik indirir.

## Köken

Bu proje ESP-IDF'in `protocols/mqtt/ssl` örneğinden başlatıldı
(Espressif Systems, Unlicense / CC0-1.0). Buton tetiklemeli yayın, sayaçlı yük ve
NodeHub'a özgü yapılandırma sonradan eklendi.
