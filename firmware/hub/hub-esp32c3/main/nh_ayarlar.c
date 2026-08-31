/* NodeHub — kalici ayarlar (NVS). Sozlesme: contract/settings.yaml */

#include <stdio.h>
#include <string.h>

#include "nh_ayarlar.h"
#include "nodehub_protocol.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "nh_ayar";

#define AD_ALANI    "nodehub"
#define ANAHTAR_HUB "hub"

static nvs_handle_t s_nvs;
static bool         s_acik;

/* "node_1f" — NVS anahtari en fazla 15 karakter, bu 7. */
static void node_anahtari(uint8_t adres, char *tampon, size_t n)
{
    snprintf(tampon, n, "node_%02x", adres);
}

esp_err_t nh_ayar_baslat(void)
{
    if (s_acik) {
        return ESP_OK;
    }

    esp_err_t r = nvs_open(AD_ALANI, NVS_READWRITE, &s_nvs);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open basarisiz: %s", esp_err_to_name(r));
        return r;
    }

    s_acik = true;
    ESP_LOGI(TAG, "ayar deposu acildi — %u node kayitli", nh_node_adet());
    return ESP_OK;
}

/* ---------------------------------------------------------------- */
/* Hub ayarlari                                                      */
/* ---------------------------------------------------------------- */

esp_err_t nh_ayar_oku(nh_hub_ayar_t *ayar)
{
    if (!s_acik || ayar == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t boy = sizeof(*ayar);
    esp_err_t r = nvs_get_blob(s_nvs, ANAHTAR_HUB, ayar, &boy);

    if (r == ESP_ERR_NVS_NOT_FOUND || boy != sizeof(*ayar)) {
        /* Hic kaydedilmemis ya da yapinin boyu degismis — bostan basla. */
        memset(ayar, 0, sizeof(*ayar));
        return ESP_ERR_NVS_NOT_FOUND;
    }
    return r;
}

esp_err_t nh_ayar_yaz(const nh_hub_ayar_t *ayar)
{
    if (!s_acik || ayar == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t r = nvs_set_blob(s_nvs, ANAHTAR_HUB, ayar, sizeof(*ayar));
    if (r == ESP_OK) {
        r = nvs_commit(s_nvs);
    }
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "hub ayari yazilamadi: %s", esp_err_to_name(r));
    }
    return r;
}

bool nh_ayar_var(void)
{
    nh_hub_ayar_t a;
    if (nh_ayar_oku(&a) != ESP_OK) {
        return false;
    }
    return a.wifi_ssid[0] != '\0';
}

void nh_ayar_hub_id(char *tampon, size_t n)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(tampon, n, "hub-%02x%02x", mac[4], mac[5]);
}

/* ---------------------------------------------------------------- */
/* Node kayitlari                                                    */
/* ---------------------------------------------------------------- */

esp_err_t nh_node_oku(uint8_t adres, nh_node_kayit_t *kayit)
{
    if (!s_acik || kayit == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    char anahtar[16];
    node_anahtari(adres, anahtar, sizeof(anahtar));

    size_t boy = sizeof(*kayit);
    esp_err_t r = nvs_get_blob(s_nvs, anahtar, kayit, &boy);
    if (r != ESP_OK) {
        return r;
    }
    if (boy != sizeof(*kayit)) {
        return ESP_ERR_NVS_INVALID_LENGTH;
    }

    kayit->kayip = 0;   /* calisma zamani alani — diskten gelen deger anlamsiz */
    return ESP_OK;
}

esp_err_t nh_node_yaz(const nh_node_kayit_t *kayit)
{
    if (!s_acik || kayit == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (kayit->adres < NH_ADRES_ILK || kayit->adres > NH_ADRES_SON) {
        return ESP_ERR_INVALID_ARG;
    }

    char anahtar[16];
    node_anahtari(kayit->adres, anahtar, sizeof(anahtar));

    esp_err_t r = nvs_set_blob(s_nvs, anahtar, kayit, sizeof(*kayit));
    if (r == ESP_OK) {
        r = nvs_commit(s_nvs);
    }
    return r;
}

esp_err_t nh_node_sil(uint8_t adres)
{
    if (!s_acik) {
        return ESP_ERR_INVALID_STATE;
    }

    char anahtar[16];
    node_anahtari(adres, anahtar, sizeof(anahtar));

    esp_err_t r = nvs_erase_key(s_nvs, anahtar);
    if (r == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;          /* zaten yok — istenen sonuc bu */
    }
    if (r == ESP_OK) {
        r = nvs_commit(s_nvs);
    }
    return r;
}

esp_err_t nh_node_listele(nh_node_kayit_t *dizi, uint8_t en_fazla, uint8_t *adet)
{
    if (!s_acik || dizi == NULL || adet == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t n = 0;
    for (uint16_t a = NH_ADRES_ILK; a <= NH_ADRES_SON && n < en_fazla; a++) {
        if (nh_node_oku((uint8_t)a, &dizi[n]) == ESP_OK) {
            n++;
        }
    }

    *adet = n;
    return ESP_OK;
}

uint8_t nh_node_adet(void)
{
    if (!s_acik) {
        return 0;
    }

    uint8_t n = 0;
    nh_node_kayit_t gecici;
    for (uint16_t a = NH_ADRES_ILK; a <= NH_ADRES_SON; a++) {
        if (nh_node_oku((uint8_t)a, &gecici) == ESP_OK) {
            n++;
        }
    }
    return n;
}

esp_err_t nh_node_tazele(uint8_t adres, const uint8_t uid[12],
                         uint16_t tip, uint16_t fw_surum)
{
    nh_node_kayit_t k;

    if (nh_node_oku(adres, &k) != ESP_OK) {
        /* Yeni node — kullanici alanlari varsayilanla baslar. */
        memset(&k, 0, sizeof(k));
        k.periyot_ms = NH_PERIYOT_VARSAYILAN;
        k.aktif      = 1;
    }
    /* Kayitliysa ad / periyot_ms / aktif oldugu gibi kalir. */

    k.adres    = adres;
    k.tip      = tip;
    k.fw_surum = fw_surum;
    memcpy(k.uid, uid, sizeof(k.uid));

    return nh_node_yaz(&k);
}
