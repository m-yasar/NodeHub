/*
 * NodeHub — node tipi tablosu.
 *
 * Kaynak: contract/registers.yaml "node_tipleri" bolumu.
 *
 * SU AN ELLE YAZILDI. Uretec betigi yazilinca bu dosya YAML'dan
 * uretilecek. Ikisi ayrisirsa gecerli olan YAML'dir.
 *
 * Hub'in tip koduna bakip "bu node'da hangi olcumler var, hangi
 * register'dan kac tane okuyacagim, ham degeri neyle carpacagim"
 * sorularini cevapladigi yer burasi. Node degisse bile hub'in kodu
 * degismez; bu tablo degisir.
 */

#ifndef NH_TIPLER_H
#define NH_TIPLER_H

#include <stdint.h>

typedef enum {
    NH_VT_INT16,
    NH_VT_UINT16,
    NH_VT_INT32,
    NH_VT_UINT32,
} nh_veri_tipi_t;

typedef struct {
    const char     *ad;         /* "sicaklik"            */
    uint16_t        offset;     /* 0x0010                */
    nh_veri_tipi_t  tip;
    float           olcek;      /* gercek = ham * olcek  */
    const char     *birim;      /* "°C"                  */
} nh_tip_olcum_t;

typedef struct {
    uint16_t              tip;          /* 0x0001                        */
    const char           *ad;           /* "Sicaklik"                    */
    const nh_tip_olcum_t *olcumler;
    uint8_t               olcum_adet;
    uint16_t              ilk_offset;   /* okumanin baslayacagi register */
    uint16_t              reg_adet;     /* kac register okunacak         */
} nh_tip_t;

/* Tip bilinmiyorsa NULL. */
const nh_tip_t *nh_tip_bul(uint16_t tip);

/* Tipin adi, bilinmiyorsa NULL. */
const char *nh_tip_adi(uint16_t tip);

/* Ham register'lari gercek degere cevirir. */
float nh_tip_degeri(const nh_tip_olcum_t *olcum, const uint16_t *ham,
                    uint16_t ilk_offset);

/* Olcum gecersiz deger mi dondurdu (sensor okunamiyor vb). */
bool nh_tip_gecersiz(const nh_tip_olcum_t *olcum, const uint16_t *ham,
                     uint16_t ilk_offset);

#endif /* NH_TIPLER_H */
