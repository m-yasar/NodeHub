# Modbus Sözleşmesi

Tüm node'ların uyduğu ortak kurallar. Hub'ın, ne ölçtüğünü bilmeden herhangi bir node ile
konuşabilmesini sağlayan şey budur.

Sözleşme iki bölümden oluşuyor:

| Bölüm | Ne zaman kullanılır |
|---|---|
| **Kurulum paketleri** | Node'a adres verilirken |
| **Register haritası** | Node okunurken |

## Hat iki fazda çalışır

**Kurulum** — adresi olmayan node'lara adres dağıtılır. Kullanıcı mobil uygulamadan
başlatır, kısa sürer.

**İşletme** — hub adresi bilinen node'ları sırayla okur. Sürekli çalışır.

İkisi aynı anda çalışmaz. Hub kurulum moduna geçerken okuma turunu durdurur.

## Hat ayarları

| Ayar | Değer |
|---|---|
| Fiziksel katman | RS485, yarım çift yönlü, tek hat |
| Hız | 19200 baud |
| Çerçeve | 8N1 — 8 veri biti, eşlik yok, 1 durdurma biti |
| Protokol | Modbus RTU |
| Okuma | FC 03, tek seferde en fazla 24 register |
| Yazma | FC 16 |

Hız sabittir, çalışma sırasında değiştirilmez. Bütün node'lar 19200'de doğar.

**16 bitten büyük değerler işlemcinin bellek düzeninde taşınır (little-endian).** Hub ve
node'un ikisi de little-endian olduğu için arada dönüşüm yapılmaz. Bu bilinçli bir tercihtir;
sözleşmeye uyacak üçüncü taraf bir cihaz bu düzene göre yorumlamalıdır.

## Adresler

| Değer | Anlamı |
|---|---|
| `0x00` | Yayın — hub'dan bütün node'lara |
| `0x01` – `0xF7` | Atanabilir node adresleri (1-247) |
| `0xFF` | Kurulum cevabı — adresi olmayan node'dan hub'a |

`0xFF` atanabilir aralığın dışında olduğu için kayıtlı bir node ile karışmaz.

## CRC

Bütün çerçeveler standart Modbus RTU CRC'si ile korunur:

| | |
|---|---|
| Ad | CRC-16/MODBUS |
| Polinom | `0xA001` (yansımalı) |
| Başlangıç | `0xFFFF` |
| Son XOR | yok |
| Hatta sıra | **önce düşük bayt** |

Son satıra dikkat: CRC hatta önce düşük bayt olarak gider, oysa diğer bütün çok baytlı
değerler işlemcinin bellek düzeninde taşınır. İkisi birbirinin tersi yönde çalışır.

## Kurulum paketleri

Üç paket var. C tanımları [`contract/nodehub_protocol.h`](../contract/nodehub_protocol.h)
dosyasında; hem hub hem node aynı dosyayı kullanır.

### 1. Sorgu — hub sorar

```
bayt:  0     1     2          3-4
      0x00  0x64  deneme_no  CRC
```

Hub "adresi olmayan var mı" diye sorar. Adresi olan node'lar bu paketi yok sayar.

`deneme_no` her sorguda bir artar. Bekleme süresi hesabına girdiği için, aynı anda konuşup
çakışan iki node bir sonraki turda farklı süre seçer.

### 2. Cevap — node kendini tanıtır

```
bayt:  0     1     2-13   14-15      16-17      18-19
      0xFF  0x65  uid    node_tipi  fw_surum   CRC
```

Sorguyu alan bakir node hemen konuşmaz. Önce kendine özgü bir süre bekler:

```
r          = crc16(uid) XOR crc16(deneme_no)
bekleme_us = r + (r >> 1)                      // r × 1,5  →  0 – 98 ms
```

`deneme_no`'nun karışıma girmesi şarttır. Çıkarılırsa çakışan iki node her turda aynı süreyi
seçer ve tarama hiç bitmez.

Bu süre boyunca hattı dinler. Başka biri konuşmaya başlarsa vazgeçer ve sıradaki sorguyu
bekler. Süre dolduğunda hat hâlâ boşsa kendini tanıtır.

Rastgele sayı üretecine gerek yoktur; UID her kartta farklı olduğu için dağılım kendiliğinden
düzgündür.

### 3. Onay — hub adresi verir

```
bayt:  0     1     2-13   14           15-16
      0x00  0x66  uid    yeni_adres   CRC
```

Hub temiz bir cevap aldıysa boş bir adres seçip verir.

Paket yayın adresine gider ama içinde **UID taşır**. Node ancak kendi UID'sini görürse adresi
kabul eder. İki node aynı anda konuşup hub bunlardan birini temiz almış olsa bile, diğeri
adresi üstüne almaz.

Node adresi aldıktan sonra ayrı bir onay göndermez. Hub, verdiği adresi normal bir okuma
komutuyla yoklayarak doğrular — cevap geliyorsa atama tutmuştur.

### Tarama nasıl biter

Hub sorguyu tekrarlar. Üst üste birkaç sorguya hiç cevap gelmezse tarama biter.

Bozuk çerçeve gelmesi cevapsızlık sayılmaz — hatta hâlâ node var demektir, sayaç sıfırlanır.

## Node'un halleri

| Hal | Anlamı |
|---|---|
| **Bakir** | Adresi yok. Yalnızca kurulum sorgularına cevap verir |
| **Kayıtlı** | Adresi var, flash'a yazılı. Normal Modbus slave gibi davranır |

Kayıtlı node kurulum paketlerini yok sayar. Kart üzerindeki butona uzun basılırsa adres
silinir ve node bakir hale döner.

Adres flash'ta saklandığı için kapanıp açılmada korunur.

## Register haritası

Node adres aldıktan sonra normal bir Modbus slave gibi okunur. Makine-okunur hâli
[`contract/registers.yaml`](../contract/registers.yaml) dosyasındadır.

### Adres alanı

| Aralık | İçerik | Erişim |
|---|---|---|
| `0x0000` – `0x000F` | Ortak alanlar, her node'da aynı | Okunur |
| `0x0010` – `0x00FF` | Node tipine özel ölçümler | Okunur |
| `0x0100` – `0x01FF` | Ayarlar | Yazılır |

Ayarlar bölümü şimdilik boş; yeri ileride kullanılmak üzere ayrıldı.

### Ortak alanlar

| Offset | Alan | Tip | Açıklama |
|---|---|---|---|
| `0x0000` | node_tipi | uint16 | Node tipi kodu |
| `0x0001` | fw_surum | uint16 | Yüksek bayt ana sürüm, düşük bayt alt sürüm |
| `0x0002` | durum | uint16 | Durum bayrakları |
| `0x0003` | uid | 12 bayt | MCU benzersiz kimliği, 6 register kaplar |
| `0x0009` – `0x000F` | — | — | Ayrılmış |

Boş bırakılan 7 register ileride ortak alan eklemek içindir. Bu boşluk olmasaydı, yeni bir
ortak alan eklemek bütün ölçüm offset'lerini kaydırmak anlamına gelirdi — yani sözleşmenin
bozulması.

### Ölçüm bloğu

İçeriği node tipine göre değişir; hangi tipte hangi ölçümün nerede olduğunu
[`registers.yaml`](../contract/registers.yaml) içindeki `node_tipleri` belirler.

Tipten bağımsız dört kural var:

- Ölçümler **`0x0010`'dan başlar**, tip ne olursa olsun
- **Aralıksız dizilir.** Araya boş register konmaz
- 32 bitlik bir ölçüm **iki ardışık register** kaplar
- Sıra, `registers.yaml` içindeki yazılış sırasıdır

Aralıksız olma şartı verimlilik içindir: hub bütün ölçümleri **tek istekle** okuyabilir.
Araya boşluk girseydi ya birden fazla istek atması ya da boşa bayt taşıması gerekirdi.

### Ayarlar bloğu

**Şu an boş.** Yeri ileride kullanılmak üzere ayrıldı; ilk ayar tanımlandığında kuralları
buraya yazılacak.

Blok yazılabilir olduğu için okuma bloklarından ayrı tutuldu. Yanlış bir yazma komutu ölçüm
ya da ortak alanlara ulaşamaz.

### Durum bayrakları

Register `0x0002`:

| Bit | Anlamı |
|---|---|
| 0 | Sensör okunamıyor |
| 1 | Ölçüm henüz hazır değil (ilk ölçüm tamamlanmadı) |
| 2 | Kalibrasyon gerekli |
| 3 | Besleme gerilimi düşük |
| 4 – 15 | Ayrılmış |

### Geçersiz ölçüm

Ölçüm alınamadığında node sıfır döndürmez — sıfır çoğu birimde geçerli bir değerdir.
Bunun yerine tipe göre şu değerler kullanılır:

| Tip | Geçersiz değer |
|---|---|
| uint16 | `0xFFFF` |
| int16 | `0x8000` |
| uint32 | `0xFFFFFFFF` |
| int32 | `0x80000000` |

Aynı anda ilgili durum bayrağı da kalkar, yani durum iki yerden anlaşılır.

### Node tipi kodları

| Aralık | Adı | Numarayı kim verir |
|---|---|---|
| `0x0000` | Tanımsız | — |
| `0x0001` – `0x00FF` | Kayıtlı | Depo. Listede tutulur, çakışmaz |
| `0x0100` – `0x0FFF` | Ayrılmış | İleride yeni bir kategori gerekirse |
| `0x1000` – `0xFFFE` | Serbest | Kişi kendi alır, çakışma sorumluluğu onda |
| `0xFFFF` | Tanımsız | — |

Ayrım "kimin node'u" değil, **numarayı kimin verdiği**. Kayıtlı aralıktaki her numara bu
depodaki listeye işlenir, o yüzden tek otorite vardır ve iki kart aynı numarayı taşıyamaz.
Serbest aralıkta koordinasyon yoktur; sözleşmeye uyan bir node tasarlayan herkes oradan
istediğini alır, ama iki kişinin aynı numarayı seçmesi mümkündür.

`0x0000` ve `0xFFFF` bilerek dışarıda bırakıldı: silinmiş flash `0xFFFF`, yazılmamış alan
`0x0000` okunur. İkisi de "tip kodu hiç yazılmamış" demektir. Geçersiz sayılmaları,
firmware'de unutulmuş bir atamayı sessiz bir hata olmaktan çıkarıp görünür kılıyor.

### Ölçümler

Her ölçüm şu alanlarla tanımlanır:

| Alan | İşi |
|---|---|
| offset | Hangi register'dan başladığı |
| tip | uint16, int16, uint32, int32 |
| olcek | Gerçek değer = ham değer × olcek |
| birim | °C, %RH, bar, rpm … |

Örnek: ham değer `236`, ölçek `0.1`, birim `°C` → **23,6 °C**

Ondalık sayı hattan hiç geçmez; node tamsayı gönderir, çevrimi hub yapar.

**Tip kodu ölçüm kümesini tam olarak belirler.** Bir node tipine ölçüm eklenir ya da
çıkarılırsa o artık yeni bir tiptir ve yeni kod alır. Aynı kod farklı ölçüm kümesiyle
kullanılamaz — hub tip kodunu gördüğü anda hangi register'ları okuyacağını kesin olarak
bilmelidir.

## Zamanlama

Bu değerlerin C karşılıkları [`contract/nodehub_protocol.h`](../contract/nodehub_protocol.h)
içinde `#define` olarak durur. Firmware'de koda gömülmez, oradan alınır.

| Parametre | Değer | Sabit |
|---|---|---|
| Cevap zaman aşımı | 50 ms | `NH_ZAMAN_ASIMI_MS` |
| Tekrar denemesi | 3 | `NH_DENEME` |
| Kayıp sayılma | 3 başarısız tur | `NH_KAYIP_TUR` |
| Kurulum beklemesi | `(crc16(uid) XOR crc16(deneme_no)) × 1,5` → 0 – 98 ms | `NH_BEKLEME_US(r)` |
| Onay bekleme | 100 ms | `NH_ONAY_BEKLEME_MS` |
| Tarama bitişi | 3 tamamen sessiz sorgu | `NH_TARAMA_BITIS` |

## Node'un yükümlülükleri

Sözleşme yalnızca veri biçimi değil, davranış da tanımlar. Node firmware'i şunlara uymak
zorundadır:

- **50 ms içinde cevap vermek.** Flash yazma gibi uzun işler sorgular arasına sıkıştırılamaz;
  bir flash sayfası silmek 30 ms sürebilir ve o sırada işlemci cevap veremez.
- **Kendiliğinden konuşmamak.** Yalnızca sorulduğunda cevap verir.
- **Ölçümü hazır değilken geçersiz değer döndürmek**, eski bir değeri tekrar etmemek.
- **Adresini flash'ta saklamak**, kapanıp açılmada korumak.
