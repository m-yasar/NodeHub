# Yol Haritası

İki paralel hat: **H** = hub/sunucu tarafı, **N** = node tarafı.

| # | Adım | Hat | Durum |
|---|---|---|---|
| 1 | İlk veriyi sunucuya göndermek (MQTT) | H | 🔄 sürüyor |
| 2 | Node haberleşme sözleşmesini yazmak | N | |
| 3 | İlk node'u kurmak | N | |
| 4 | Hub'ın node'ları sırayla okuması | H+N | |
| 5 | Farklı bir node tipi eklemek — sözleşmenin sınavı | N | |
| 6 | Node'ları uzaktan tanımlayabilmek (ayarlar kalıcı) | H | |
| 7 | Hub'ı uzaktan güncellemek (OTA) | H | |
| 8 | Node'ları uzaktan güncellemek (RS485 üzerinden) | N | |
| 9 | Mobil uygulamayı yapmak | H | |
| 10 | Node'ları gerçek karta taşımak (PCB) | N | |
| 11 | Yeni node tipleri eklemek | N | |

Tek katı bağımlılık **2 → 3**, ve **5 adımı 2'nin doğrulamasıdır.** Gerisi esnek.

## Adım 5 neden önemli

İkinci node tipi eklenirken hub tarafında kod değiştirmek gerekiyorsa, sözleşme hatalıdır.
Bu adım bir özellik eklemek değil, mimariyi sınamaktır.
