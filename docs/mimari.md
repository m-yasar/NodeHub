# Sistem Mimarisi

## Dört katman

```
Mobil Uygulama          sistemi kurar: hangi node var, ne sıklıkta okunacak
      │
      ▼
    Hub  ◄──── RS485 / Modbus RTU ────►  Node  ─── ölçüm elemanı
   (ESP32)                              (MCU)
      │
      ▼  MQTT
   Sunucu                verileri saklar
      │
      ▼
NodeHub Panel            grafiğe döker (ayrı proje)
```

## Node

Ölçümü yapan karttır. Üzerinde bir MCU ve bir ölçüm elemanı bulunur. Sürekli ölçüm yapar,
hub sorgu attığında cevabı verir. Kendi başına konuşmaz, yalnızca sorulduğunda cevap verir
(Modbus RTU slave).

Her node'un bir adresi vardır. Aynı hat üzerinde birden fazla node bulunur.

## Hub

ESP32. İki işi var:

1. Node'ları sırayla okumak (Modbus RTU master)
2. Okuduğunu MQTT ile sunucuya göndermek

Hangi node'un ne sıklıkta okunacağı mobil uygulamadan belirlenir. Node'ların hepsi tek hat
üzerinde olduğu için hub aynı anda tek node'la konuşur; okuma işini periyotlara göre sıraya
dizip sürekli döner.

Örnek:

| Node | Periyot |
|---|---|
| Sıcaklık 1, 2, 3 | 30 saniyede bir |
| Nem 1 | 1 dakikada bir |
| Basınç 1 | 1 saniyede bir |

## Mobil uygulama

İzleme aracı değildir — sistemi kuran katmandır. Node'lar tanımlanır, adresleri ve okuma
periyotları belirlenir, bağlantı ayarları yapılır.

## Neden bu ayrım önemli

Ölçüm elemanı değişse bile — üretimi dursa, başka model takılsa — yalnızca node'un içindeki
hesaplama değişir. Node'un dışarıya verdiği cevabın biçimi aynı kalır.

Bundan iki sonuç çıkar:

- **Hub'a driver yüklemek gerekmez.** Hub ömrü boyunca yalnızca Modbus konuşur.
- **Yeni sensör = kod değil, tanım.** Bir node'u sisteme tanıtmak için gereken şey veridir:
  adres, function code, register offset, sayı, veri tipi, ölçek, birim, periyot.

Asıl güncelleme ihtiyacı node tarafındadır: sahadaki node firmware'ini RS485 üzerinden,
hub aracılığıyla güncelleyebilmek.
