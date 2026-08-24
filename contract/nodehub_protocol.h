/*
 * NodeHub — kurulum paketleri
 *
 * Hub ile node arasında adres atama protokolü. Bu dosya hem hub hem node
 * tarafından kullanılır; sözleşmenin tek kaynağı burasıdır.
 *
 * Kablo biçimi Modbus RTU çerçevesidir:
 *
 *     [adres][fonksiyon][veri...][crc_lo][crc_hi]
 *
 * Modbus'ın iki kuralı burada da geçerli ve ikisi birbirinin tersi:
 *   - CRC önce DÜŞÜK bayt gider
 *   - Diğer çok baytlı değerler önce YÜKSEK bayt gider (big-endian)
 *
 * Bu yüzden alanlar uint16_t olarak değil bayt dizisi olarak tanımlandı.
 * Derleyici hangi mimaride çalışırsa çalışsın hattaki çerçeve aynı çıkar.
 */

#ifndef NODEHUB_PROTOCOL_H
#define NODEHUB_PROTOCOL_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Adresler                                                            */
/* ------------------------------------------------------------------ */

#define NH_ADRES_YAYIN      0x00u   /* hub -> bütün node'lar            */
#define NH_ADRES_KURULUM    0xFFu   /* adresi olmayan node -> hub       */
#define NH_ADRES_ILK        0x01u   /* atanabilir aralığın başı         */
#define NH_ADRES_SON        0xF7u   /* 247, Modbus'ın izin verdiği son  */

/* ------------------------------------------------------------------ */
/* Fonksiyon kodları                                                   */
/*                                                                     */
/* Modbus 0x64-0x6E arasını özel kullanıma ayırmıştır.                 */
/* ------------------------------------------------------------------ */

#define NH_FK_SORGU         0x64u   /* Kim var                          */
#define NH_FK_CEVAP         0x65u   /* Buradayım                        */
#define NH_FK_ONAY          0x66u   /* Adres senin                      */

#define NH_UID_UZUNLUK      12u     /* STM32 benzersiz kimliği, 96 bit  */

/* ------------------------------------------------------------------ */
/* Hat ayarları                                                        */
/* ------------------------------------------------------------------ */

#define NH_HIZ                  19200u  /* baud, 8N1                    */
#define NH_EN_FAZLA_REGISTER    24u     /* tek okumada                  */
#define NH_OKUMA_FK             0x03u
#define NH_YAZMA_FK             0x10u

/* ------------------------------------------------------------------ */
/* CRC — standart Modbus RTU                                           */
/*                                                                     */
/* Hatta ÖNCE DÜŞÜK BAYT gider; diğer çok baytlı değerlerin tersi.     */
/* ------------------------------------------------------------------ */

#define NH_CRC_POLINOM      0xA001u     /* yansımalı                    */
#define NH_CRC_BASLANGIC    0xFFFFu

/* ------------------------------------------------------------------ */
/* Zamanlama                                                           */
/*                                                                     */
/* Bu sabitler sözleşmenin parçasıdır; hub ve node aynı değerleri       */
/* kullanmak zorundadır. Koda gömmeyin, buradan alın.                   */
/* ------------------------------------------------------------------ */

/* İşletme */
#define NH_ZAMAN_ASIMI_MS   50u     /* node cevabı için beklenen süre   */
#define NH_DENEME           3u      /* cevapsız kalırsa tekrar sayısı   */
#define NH_KAYIP_TUR        3u      /* kaç turdan sonra node kayıp      */

/* Kurulum */
#define NH_ONAY_BEKLEME_MS  100u    /* "Buradayım" sonrası onay süresi  */
#define NH_TARAMA_BITIS     3u      /* kaç sessiz sorgudan sonra biter  */

/*
 * Hub, sorgudan sonra cevabı bu kadar bekler.
 *
 * Node en fazla 98 ms bekleyip konuşmaya başlıyor, 20 baytlık cevap
 * 19200'de ~11 ms sürüyor. Yani alt sınır 109 ms; pay bırakıldı.
 */
#define NH_CEVAP_BEKLEME_MS 150u

/*
 * Kurulum bekleme süresi.
 *
 * Her node kendine özgü, her turda değişen bir süre bekler. UID her
 * kartta farklı olduğu için dağılım kendiliğinden düzgündür; rastgele
 * sayı üretecine gerek yoktur.
 *
 * deneme_no'nun karışıma girmesi şarttır — çıkarılırsa çakışan iki node
 * her turda aynı süreyi seçer ve tarama hiç bitmez.
 *
 *     r = crc16(uid, 12) ^ crc16(&deneme_no, 1)
 *     bekleme_us = NH_BEKLEME_US(r)        // 0 .. 98302 us
 */
#define NH_BEKLEME_US(r)    ((uint32_t)(r) + ((uint32_t)(r) >> 1))
#define NH_BEKLEME_EN_FAZLA 98302u

/* ------------------------------------------------------------------ */
/* 1. Sorgu paketi        hub -> yayın                                 */
/*                                                                     */
/* Hub "adresi olmayan var mı" diye sorar. Bakir node bunu alınca       */
/* kendi bekleme süresini hesaplar ve hattı dinlemeye başlar.           */
/*                                                                     */
/* deneme_no her sorguda artar. Bekleme süresi hesabına girdiği için    */
/* aynı anda konuşup çakışan iki node bir sonraki turda farklı süre     */
/* seçer; yoksa sonsuza kadar çakışırlardı.                             */
/* ------------------------------------------------------------------ */

typedef struct __attribute__((packed)) {
    uint8_t adres;          /* NH_ADRES_YAYIN                           */
    uint8_t fk;             /* NH_FK_SORGU                              */
    uint8_t deneme_no;      /* tur sayacı, 0..255 döner                 */
    uint8_t crc_lo;
    uint8_t crc_hi;
} nh_sorgu_t;

/* ------------------------------------------------------------------ */
/* 2. Cevap paketi        node -> hub                                  */
/*                                                                     */
/* Bekleme süresi dolan ve hattı boş bulan node kendini tanıtır.        */
/*                                                                     */
/* Adresi olmadığı için adres alanına NH_ADRES_KURULUM yazar. Bu değer  */
/* atanabilir aralığın (1-247) dışında olduğu için hiçbir kayıtlı       */
/* node ile karışmaz.                                                   */
/* ------------------------------------------------------------------ */

typedef struct __attribute__((packed)) {
    uint8_t adres;                      /* NH_ADRES_KURULUM             */
    uint8_t fk;                         /* NH_FK_CEVAP                  */
    uint8_t uid[NH_UID_UZUNLUK];        /* MCU'nun benzersiz kimliği    */
    uint8_t tip_hi;                     /* node tipi, big-endian        */
    uint8_t tip_lo;
    uint8_t fw_hi;                      /* firmware sürümü, big-endian  */
    uint8_t fw_lo;
    uint8_t crc_lo;
    uint8_t crc_hi;
} nh_cevap_t;

/* ------------------------------------------------------------------ */
/* 3. Onay paketi         hub -> yayın, UID'ye hitaben                 */
/*                                                                     */
/* Hub temiz bir cevap aldıysa boş bir adres seçip verir.               */
/*                                                                     */
/* Paket yayın adresine gider ama içinde UID taşır. Node ancak kendi    */
/* UID'sini görürse adresi kabul eder. Bu, iki node aynı anda konuşup   */
/* hub bunlardan birini temiz almış olsa bile ikisinin aynı adresi      */
/* almasını engeller — çakışmaya karşı asıl güvence budur.              */
/* ------------------------------------------------------------------ */

typedef struct __attribute__((packed)) {
    uint8_t adres;                      /* NH_ADRES_YAYIN               */
    uint8_t fk;                         /* NH_FK_ONAY                   */
    uint8_t uid[NH_UID_UZUNLUK];        /* kime verildiği               */
    uint8_t yeni_adres;                 /* NH_ADRES_ILK .. NH_ADRES_SON */
    uint8_t crc_lo;
    uint8_t crc_hi;
} nh_onay_t;

/* ------------------------------------------------------------------ */
/* Boyutlar hatta gidecek bayt sayısıdır; dolgu olmamalı.              */
/* ------------------------------------------------------------------ */

_Static_assert(sizeof(nh_sorgu_t) ==  5, "nh_sorgu_t dolgu almis");
_Static_assert(sizeof(nh_cevap_t) == 20, "nh_cevap_t dolgu almis");
_Static_assert(sizeof(nh_onay_t)  == 17, "nh_onay_t dolgu almis");

#endif /* NODEHUB_PROTOCOL_H */
