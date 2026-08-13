# NodeHub

Ortamdaki sıcaklık, nem, basınç, hız, gerilim, güç harcanımı gibi her türlü değeri ölçen,
internete taşıyan ve telefondan kurulan açık kaynaklı bir izleme sistemi.

Projenin ayırt edici yanı şu: ölçümü yapan küçük kartlar da bu projenin parçası.
Sistem hazır parçaların birleştirilmesi değil, en alttan en üste kadar kurulan bütün bir zincir.

## Sistem katmanları

| Katman | Görevi | Klasör |
|---|---|---|
| **Mobil Uygulama** | Node'lar ve bağlantı ayarları yapılarak sistemin kurulumu sağlanır | [`mobile/`](mobile/) |
| **Node** | Ölçüm yapar ve sorguya cevap verir (Modbus RTU slave) | [`firmware/node/`](firmware/node/) |
| **Hub** | Node'lardan veri toplar, MQTT ile sunucuya gönderir (Modbus master) | [`firmware/hub/`](firmware/hub/) |
| **Sunucu** | Verileri saklar | [`server/`](server/) |

İzleme arayüzü (**NodeHub Panel**) bu deponun dışında, ayrı bir proje olarak yürüyor.

## Mimarinin kalbi: node

Node = bir MCU + ölçüm elemanı. Hub'a RS485 üzerinden Modbus RTU ile cevap veren kart.

**Değişkenlik node'un içinde soğurulur.** Ölçüm elemanı değişse bile node'un içindeki hesaplama
değişir, dışarıya verdiği cevap aynı kalır. Hub, sunucu ve panel hiç etkilenmez.

Bunu mümkün kılan şey tüm node'ların uyduğu ortak register haritasıdır — projenin sözleşmesi:
[`docs/modbus-sozlesmesi.md`](docs/modbus-sozlesmesi.md)

## Belgeler

- [Sistem mimarisi](docs/mimari.md)
- [Yol haritası](docs/yol-haritasi.md)
- [Modbus sözleşmesi](docs/modbus-sozlesmesi.md)

## Durum

Şu an **Adım 1**: hub'dan sunucuya ilk verinin MQTT ile gönderilmesi.

Tüm adımlar için: [yol haritası](docs/yol-haritasi.md)

## Kurulum

```bash
git clone https://github.com/m-yasar/NodeHub.git
cd NodeHub
```

Hub firmware'ini derlemek için ESP-IDF gerekir: [`firmware/hub/`](firmware/hub/)

## Lisans

[MIT](LICENSE) — Mustafa Yaşar
