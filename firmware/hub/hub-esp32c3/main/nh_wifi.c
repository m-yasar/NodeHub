/* NodeHub — Wi-Fi (yalnizca station). */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "nh_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

static const char *TAG = "nh_wifi";

#define BIT_BAGLI   BIT0

/*
 * Denemeler arasi bekleme.
 *
 * Deneme SAYISI ile degil SURE ile siniriyoruz: cagiran "30 saniye ara"
 * diyorsa 30 saniye aranmali. Sayi ile sinirlanirsa, ortada ag yokken
 * ESP her denemeyi aninda "AP bulunamadi" ile bitirdigi icin hak birkac
 * saniyede tukenir ve sure hic dolmadan vazgecilir.
 */
#define DENEME_ARASI_MS 1000

/* Baglanti koptuysa bakim gorevi bu arayla toparlamayi dener. */
#define BAKIM_ARASI_MS  2000

static EventGroupHandle_t s_olaylar;
static esp_netif_t       *s_netif;
static bool               s_kuruldu;

/*
 * Baglanti korunsun mu.
 *
 * Yalnizca basarili bir nh_wifi_baglan sonrasi true olur. Baglanma
 * denemesi surerken false'tur — yoksa bakim gorevi ile cagiranin
 * kendi deneme dongusu ayni anda esp_wifi_connect cagirir ve
 * "sta is connecting, return error" alinir.
 */
static bool s_koru;

/*
 * Olay isleyici yalnizca bayrak degistirir.
 *
 * Burasi olay dongusu gorevinde calisiyor; icinde beklemek ya da
 * yeniden baglanmak butun olay dagitimini geciktirir. Tekrar deneme
 * isi cagirana ve bakim gorevine ait.
 */
static void olay_isleyici(void *arg, esp_event_base_t taban,
                          int32_t olay_id, void *veri)
{
    (void)arg;

    if (taban == WIFI_EVENT && olay_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_olaylar, BIT_BAGLI);
        return;
    }

    if (taban == IP_EVENT && olay_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)veri;
        ESP_LOGI(TAG, "IP alindi: " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_olaylar, BIT_BAGLI);
    }
}

/* Isletmede ag duserse geri getirir. */
static void bakim_gorevi(void *arg)
{
    (void)arg;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(BAKIM_ARASI_MS));

        if (s_koru && !nh_wifi_bagli()) {
            ESP_LOGW(TAG, "baglanti dustu — yeniden deneniyor");
            esp_wifi_connect();
        }
    }
}

esp_err_t nh_wifi_baslat(void)
{
    if (s_kuruldu) {
        return ESP_OK;
    }

    s_olaylar = xEventGroupCreate();
    if (s_olaylar == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_netif = esp_netif_create_default_wifi_sta();
    if (s_netif == NULL) {
        return ESP_FAIL;
    }

    wifi_init_config_t vars = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&vars));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &olay_isleyici, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &olay_isleyici, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    esp_err_t r = esp_wifi_start();
    if (r != ESP_OK && r != ESP_ERR_WIFI_NOT_STOPPED) {
        ESP_LOGE(TAG, "esp_wifi_start basarisiz: %s", esp_err_to_name(r));
        return r;
    }

    xTaskCreate(bakim_gorevi, "nh_wifi_bakim", 2560, NULL, 4, NULL);

    s_kuruldu = true;
    ESP_LOGI(TAG, "station modunda hazir");
    return ESP_OK;
}

esp_err_t nh_wifi_baglan(const char *ssid, const char *parola,
                         uint32_t zaman_asimi_ms)
{
    if (!s_kuruldu || ssid == NULL || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    /* Once eski baglanti ve otomatik toparlama kapatilir. */
    nh_wifi_kes();

    wifi_config_t yapilandirma = {0};
    strlcpy((char *)yapilandirma.sta.ssid, ssid,
            sizeof(yapilandirma.sta.ssid));
    if (parola != NULL) {
        strlcpy((char *)yapilandirma.sta.password, parola,
                sizeof(yapilandirma.sta.password));
    }
    /*
     * Esik ACIK birakildi; parola varsa surucu kendisi WPA2'ye cekiyor.
     * Zorlanirsa acik bir kurulum agina hic baglanilamaz.
     */
    yapilandirma.sta.threshold.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &yapilandirma));

    ESP_LOGI(TAG, "'%s' araniyor — %" PRIu32 " sn",
             ssid, zaman_asimi_ms / 1000);

    TickType_t bitis = xTaskGetTickCount() + pdMS_TO_TICKS(zaman_asimi_ms);

    /*
     * Deneme dongusu cagiranin gorevinde doner.
     *
     * Baglantiyi her turda ACIKCA baslatiyoruz. Eskiden STA_START
     * olayina guveniliyordu; o olay yalnizca WiFi durmus haldeyken
     * basladiginda geliyor, ikinci cagrida hic gelmiyor ve baglanti
     * baslamiyordu.
     */
    while (xTaskGetTickCount() < bitis) {
        xEventGroupClearBits(s_olaylar, BIT_BAGLI);

        esp_err_t r = esp_wifi_connect();
        if (r != ESP_OK && r != ESP_ERR_WIFI_CONN) {
            ESP_LOGW(TAG, "esp_wifi_connect: %s", esp_err_to_name(r));
        }

        TickType_t kalan = bitis - xTaskGetTickCount();
        TickType_t bekle = pdMS_TO_TICKS(DENEME_ARASI_MS);
        if (kalan < bekle) {
            bekle = kalan;
        }

        EventBits_t bitler = xEventGroupWaitBits(
            s_olaylar, BIT_BAGLI, pdFALSE, pdFALSE, bekle);

        if (bitler & BIT_BAGLI) {
            s_koru = true;          /* bundan sonra bakim gorevi korur */
            return ESP_OK;
        }
    }

    esp_wifi_disconnect();
    ESP_LOGW(TAG, "'%s' bulunamadi (%" PRIu32 " sn)",
             ssid, zaman_asimi_ms / 1000);
    return ESP_ERR_TIMEOUT;
}

void nh_wifi_kes(void)
{
    if (!s_kuruldu) {
        return;
    }
    s_koru = false;
    esp_wifi_disconnect();
    xEventGroupClearBits(s_olaylar, BIT_BAGLI);
}

bool nh_wifi_bagli(void)
{
    if (!s_kuruldu) {
        return false;
    }
    return (xEventGroupGetBits(s_olaylar) & BIT_BAGLI) != 0;
}

void nh_wifi_ip(char *tampon, size_t n)
{
    esp_netif_ip_info_t bilgi = {0};

    if (s_netif != NULL && esp_netif_get_ip_info(s_netif, &bilgi) == ESP_OK) {
        snprintf(tampon, n, IPSTR, IP2STR(&bilgi.ip));
    } else {
        strlcpy(tampon, "0.0.0.0", n);
    }
}
