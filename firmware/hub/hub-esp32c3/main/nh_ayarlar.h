/*
 * NodeHub — kalici ayarlar (NVS).
 *
 * Sozlesme: contract/settings.yaml. Alan adlari ve uzunluklar oradan gelir.
 *
 * NVS yerlesimi, ad alani "nodehub":
 *
 *   "hub"       -> nh_hub_ayar_t          tek blob
 *   "node_01"   -> nh_node_kayit_t        adres basina bir blob
 *   "node_02"   -> ...
 *
 * Node'lar ayri bloblarda tutulur; bir node'un adini degistirmek
 * digerlerini yeniden yazmayi gerektirmesin diye.
 */

#ifndef NH_AYARLAR_H
#define NH_AYARLAR_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Uzunluklar — settings.yaml "uzunluklar" bolumu. Sonlandirici dahil. */
#define NH_AD_UZ        32
#define NH_SSID_UZ      33
#define NH_PAROLA_UZ    65
#define NH_URI_UZ      128
#define NH_KONU_UZ      64

/* Kurulum agi — settings.yaml "kurulum_agi" bolumu. */
#define NH_KURULUM_SSID         "nodehub"
#define NH_KURULUM_PAROLA       "nodehub12"
#define NH_KURULUM_ARAMA_MS     30000
#define NH_KURULUM_MDNS         "nodehub"
#define NH_KURULUM_OTURUM_MS    600000

#define NH_PERIYOT_VARSAYILAN   10000u

typedef struct {
    char wifi_ssid[NH_SSID_UZ];
    char wifi_parola[NH_PAROLA_UZ];
    char broker_uri[NH_URI_UZ];
    char broker_kullanici[NH_AD_UZ];
    char broker_parola[NH_PAROLA_UZ];
    char konu_oneki[NH_KONU_UZ];
    char hub_adi[NH_AD_UZ];
} nh_hub_ayar_t;

typedef struct {
    /* Node verir — hattan gelir, kullanici degistiremez */
    uint8_t  adres;
    uint8_t  uid[12];
    uint16_t tip;
    uint16_t fw_surum;

    /* Kullanici yazar */
    char     ad[NH_AD_UZ];
    uint32_t periyot_ms;
    uint8_t  aktif;

    /* Calisma zamani — NVS'e yazilmaz */
    uint8_t  kayip;
} nh_node_kayit_t;

/* NVS'i acar. app_main basinda bir kez. */
esp_err_t nh_ayar_baslat(void);

/* Hub kurulmus mu — "hub" blobu var ve wifi_ssid dolu mu. */
bool nh_ayar_var(void);

esp_err_t nh_ayar_oku(nh_hub_ayar_t *ayar);
esp_err_t nh_ayar_yaz(const nh_hub_ayar_t *ayar);

/* "hub-a4f2" — MAC'ten turer, ayar degildir. tampon >= 12 bayt. */
void nh_ayar_hub_id(char *tampon, size_t n);

/* Kayitli node sayisi. */
uint8_t nh_node_adet(void);

/* Kayitli node'lari adres sirasiyla doldurur. */
esp_err_t nh_node_listele(nh_node_kayit_t *dizi, uint8_t en_fazla, uint8_t *adet);

esp_err_t nh_node_oku(uint8_t adres, nh_node_kayit_t *kayit);
esp_err_t nh_node_yaz(const nh_node_kayit_t *kayit);
esp_err_t nh_node_sil(uint8_t adres);

/*
 * Taramadan gelen node'u kaydeder.
 *
 * Adres zaten kayitliysa kullanicinin yazdigi alanlar (ad, periyot,
 * aktif) KORUNUR — yalnizca node'dan gelenler tazelenir. Boylece
 * her taramada kullanicinin verdigi isimler silinmez.
 */
esp_err_t nh_node_tazele(uint8_t adres, const uint8_t uid[12],
                         uint16_t tip, uint16_t fw_surum);

#endif /* NH_AYARLAR_H */
