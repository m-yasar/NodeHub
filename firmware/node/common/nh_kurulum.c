/*
 * NodeHub — kimlik atama protokolü, node tarafı.
 *
 * Hal makinesi:
 *
 *   BAKIR            0x64 geldi  -> bekleme hesapla, hattı dinle -> BEKLIYOR
 *   BEKLIYOR         süre doldu  -> hat boşsa 0x65 gönder        -> ATAMA_BEKLIYOR
 *                    kenar geldi -> biri önce başladı            -> BAKIR
 *   ATAMA_BEKLIYOR   0x66 geldi  -> UID benimse kimliği yaz      -> KAYITLI
 *                    zaman aşımı                                 -> BAKIR
 *   KAYITLI          kurulum çerçevelerini yok sayar
 *
 * Bekleme süresi UID'den türer ve her turda değişir. deneme_no karışıma
 * girmezse çakışan iki node her turda aynı süreyi seçer ve tarama hiç bitmez.
 */

#include <string.h>

#include "nh_kurulum.h"
#include "nh_node.h"
#include "nh_port.h"
#include "nh_crc.h"
#include "nodehub_protocol.h"

#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "nh_kurulum";

typedef enum {
    ST_BAKIR,
    ST_BEKLIYOR,
    ST_ATAMA_BEKLIYOR,
    ST_KAYITLI,
} durum_t;

static durum_t             s_durum;
static uint8_t             s_uid[NH_UID_UZUNLUK];
static esp_timer_handle_t  s_bekleme;
static esp_timer_handle_t  s_atama_asimi;

/* --------------------------------------------------------------- */

static void cevap_gonder(void)
{
    uint8_t c[32];
    uint16_t n = 0;

    c[n++] = NH_ADRES_KURULUM;
    c[n++] = NH_FK_CEVAP;
    memcpy(&c[n], s_uid, NH_UID_UZUNLUK);
    n += NH_UID_UZUNLUK;
    c[n++] = (uint8_t)(nh_tanim()->tip >> 8);
    c[n++] = (uint8_t)(nh_tanim()->tip & 0xFFu);
    c[n++] = (uint8_t)(nh_tanim()->fw_surum >> 8);
    c[n++] = (uint8_t)(nh_tanim()->fw_surum & 0xFFu);

    n = nh_crc_ekle(c, n);
    nh_port_gonder(c, n);
}

/* Bekleme doldu. Bu geri çağırma esp_timer görevinde çalışır; hattı son bir
   kez kontrol edip gönderir. Gönderim burada yapılıyor çünkü araya görev
   zamanlaması girerse çakışma penceresi büyür. */
static void bekleme_bitti(void *arg)
{
    (void)arg;

    if (s_durum != ST_BEKLIYOR) {
        return;
    }

#if NH_TASIYICI_ALGILA
    bool kenar  = nh_port_kenar_oldu();
    bool seviye = nh_port_hat_bos();
    nh_port_kenar_izle(false);

    if (kenar || !seviye) {
        ESP_LOGW(TAG, "gonderemedim — kenar=%d hat_bos=%d",
                 (int)kenar, (int)seviye);
        s_durum = ST_BAKIR;
        return;
    }
#else
    nh_port_kenar_izle(false);
#endif

    cevap_gonder();
    ESP_LOGI(TAG, "cevap gonderildi, onay bekleniyor");
    s_durum = ST_ATAMA_BEKLIYOR;
    esp_timer_start_once(s_atama_asimi, NH_ONAY_BEKLEME_MS * 1000);
}

static void atama_asimi(void *arg)
{
    (void)arg;
    if (s_durum == ST_ATAMA_BEKLIYOR) {
        ESP_LOGW(TAG, "onay gelmedi, bakir hale donuluyor");
        s_durum = ST_BAKIR;
    }
}

/* --------------------------------------------------------------- */

static void sorgu_geldi(uint8_t deneme_no)
{
    if (s_durum == ST_KAYITLI) {
        ESP_LOGI(TAG, "sorgu geldi ama kayitliyim (kimlik 0x%02X), yok sayiyorum",
                 nh_kayit_kimlik());
        return;
    }

    esp_timer_stop(s_bekleme);
    esp_timer_stop(s_atama_asimi);

    uint16_t r  = (uint16_t)(nh_crc16(s_uid, NH_UID_UZUNLUK) ^
                             nh_crc16(&deneme_no, 1));
    uint32_t us = NH_BEKLEME_US(r);

    s_durum = ST_BEKLIYOR;
#if NH_TASIYICI_ALGILA
    nh_port_kenar_izle(true);
#endif
    esp_timer_start_once(s_bekleme, us);

    ESP_LOGD(TAG, "sorgu %u, bekleme %u us", deneme_no, (unsigned)us);
}

static void onay_geldi(const uint8_t *c)
{
    if (s_durum != ST_ATAMA_BEKLIYOR) {
        ESP_LOGW(TAG, "onay geldi ama durum %d (ATAMA_BEKLIYOR degil)",
                 (int)s_durum);
        return;
    }

    if (memcmp(&c[2], s_uid, NH_UID_UZUNLUK) != 0) {
        ESP_LOGW(TAG, "onay baskasina — gelen uid %02X%02X%02X%02X%02X%02X, "
                      "benimki %02X%02X%02X%02X%02X%02X",
                 c[2], c[3], c[4], c[5], c[6], c[7],
                 s_uid[0], s_uid[1], s_uid[2], s_uid[3], s_uid[4], s_uid[5]);
        s_durum = ST_BAKIR;
        return;
    }

    uint8_t adres = c[2 + NH_UID_UZUNLUK];
    if (adres < NH_ADRES_ILK || adres > NH_ADRES_SON) {
        s_durum = ST_BAKIR;
        return;
    }

    esp_timer_stop(s_atama_asimi);

    if (nh_kayit_ata(adres)) {
        s_durum = ST_KAYITLI;
        ESP_LOGI(TAG, "kimlik alindi: 0x%02X", adres);
    } else {
        ESP_LOGE(TAG, "kimlik flash'a yazilamadi");
        s_durum = ST_BAKIR;
    }
}

/* --------------------------------------------------------------- */

void nh_kurulum_baslat(void)
{
    nh_port_uid_oku(s_uid);

    const esp_timer_create_args_t a1 = { .callback = bekleme_bitti,
                                         .name     = "nh_bekleme" };
    const esp_timer_create_args_t a2 = { .callback = atama_asimi,
                                         .name     = "nh_atama" };
    esp_timer_create(&a1, &s_bekleme);
    esp_timer_create(&a2, &s_atama_asimi);

    s_durum = nh_kayit_kimlik() ? ST_KAYITLI : ST_BAKIR;
}

/*
 * Kimligi siler, bakir hale doner.
 *
 * hedef = NH_ADRES_YAYIN  -> hattaki herkes unutur
 * hedef = bir adres       -> yalnizca o adrestekI node unutur
 */
static void unut_geldi(const uint8_t *c)
{
    if (c[3] != NH_UNUT_ANAHTAR) {
        ESP_LOGW(TAG, "unut paketi anahtarsiz (0x%02X) — yok sayildi", c[3]);
        return;
    }

    uint8_t hedef  = c[2];
    uint8_t kimlik = nh_kayit_kimlik();

    if (kimlik == 0) {
        return;                     /* zaten bakirim */
    }
    if (hedef != NH_ADRES_YAYIN && hedef != kimlik) {
        return;                     /* baskasina soylenmis */
    }

    esp_timer_stop(s_bekleme);
    esp_timer_stop(s_atama_asimi);

    nh_kayit_sil();
    s_durum = ST_BAKIR;

    ESP_LOGW(TAG, "kimlik 0x%02X silindi — bakir hale donuldu", kimlik);
}

/* Çerçeve kurulum protokolüne aitse işler ve true döner. */
bool nh_kurulum_cerceve(const uint8_t *c, uint16_t n)
{
    if (n < 4 || c[0] != NH_ADRES_YAYIN) {
        return false;
    }

    switch (c[1]) {
    case NH_FK_SORGU:
        if (n == 5) {
            sorgu_geldi(c[2]);
        } else {
            ESP_LOGW(TAG, "sorgu 5 bayt olmali, %u geldi", n);
        }
        return true;

    case NH_FK_ONAY:
        if (n == 17) {
            onay_geldi(c);
        } else {
            ESP_LOGW(TAG, "onay 17 bayt olmali, %u geldi", n);
        }
        return true;

    case NH_FK_UNUT:
        if (n == 6) {
            unut_geldi(c);
        } else {
            ESP_LOGW(TAG, "unut 6 bayt olmali, %u geldi", n);
        }
        return true;

    default:
        return false;
    }
}
