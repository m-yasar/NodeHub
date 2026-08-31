/*
 * NodeHub — hub uygulamasi.
 *
 * Her acilis ayni sirayi izler:
 *
 *   KURULUM   Ilk 30 saniye "nodehub" agi aranir. Kullanici o sirada
 *             telefonundan bu agi paylasiyorsa hub katilir ve kurulum
 *             sayfasini servis eder; kaydedince yeniden baslar.
 *             Ag yoksa sure sonunda isletmeye gecilir.
 *
 *   ISLETME   Ayarlar NVS'te. Ev agina baglanir, MQTT'ye baglanir,
 *             RS485 hattini surer. Ayar yoksa yalnizca RS485 doner.
 *
 * Hub HICBIR ZAMAN AP acmaz — omru boyunca station'dir. Gerekcesi
 * contract/settings.yaml "kurulum_agi" bolumunde.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#include "nh_ayarlar.h"
#include "nh_hub_kurulum.h"
#include "nh_olcum.h"
#include "nh_web.h"
#include "nh_wifi.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "esp_crt_bundle.h"
#include "mqtt_client.h"

static const char *TAG = "nodehub";

static esp_mqtt_client_handle_t s_mqtt_client;
static QueueHandle_t            s_dugme_kuyrugu;

/* ---------------------------------------------------------------- */
/* MQTT                                                              */
/* ---------------------------------------------------------------- */

static void mqtt_olay(void *arg, esp_event_base_t taban,
                      int32_t olay_id, void *veri)
{
    (void)arg; (void)taban;
    esp_mqtt_event_handle_t olay = veri;

    switch ((esp_mqtt_event_id_t)olay_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT baglandi");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT koptu");
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "yayinlandi, msg_id=%d", olay->msg_id);
        break;
    case MQTT_EVENT_ERROR:
        if (olay->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "tasima hatasi: esp-tls 0x%x, tls yigini 0x%x",
                     olay->error_handle->esp_tls_last_esp_err,
                     olay->error_handle->esp_tls_stack_err);
        } else if (olay->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGE(TAG, "broker reddetti: 0x%x",
                     olay->error_handle->connect_return_code);
        }
        break;
    default:
        break;
    }
}

static void mqtt_baslat(const nh_hub_ayar_t *ayar)
{
    if (ayar->broker_uri[0] == '\0') {
        ESP_LOGW(TAG, "broker adresi bos — MQTT atlaniyor");
        return;
    }

    esp_mqtt_client_config_t vars = {
        .broker.address.uri = ayar->broker_uri,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
    };

    if (ayar->broker_kullanici[0] != '\0') {
        vars.credentials.username = ayar->broker_kullanici;
        vars.credentials.authentication.password = ayar->broker_parola;
    }

    s_mqtt_client = esp_mqtt_client_init(&vars);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT istemcisi kurulamadi");
        return;
    }

    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_olay, NULL);
    esp_mqtt_client_start(s_mqtt_client);
    ESP_LOGI(TAG, "MQTT baslatildi: %s", ayar->broker_uri);
}

/* ---------------------------------------------------------------- */
/* BOOT tusu — isletmede tarama tetikler                             */
/* ---------------------------------------------------------------- */

static void IRAM_ATTR dugme_isr(void *arg)
{
    uint32_t pin = (uint32_t)arg;
    xQueueSendFromISR(s_dugme_kuyrugu, &pin, NULL);
}

static void dugme_gorevi(void *arg)
{
    (void)arg;
    uint32_t pin;

    for (;;) {
        if (!xQueueReceive(s_dugme_kuyrugu, &pin, portMAX_DELAY)) {
            continue;
        }

        /* Sicrama filtresi — hala basili mi (aktif dusuk) */
        vTaskDelay(pdMS_TO_TICKS(50));
        if (gpio_get_level(CONFIG_EXAMPLE_BOOT_BUTTON_GPIO) != 0) {
            continue;
        }

        ESP_LOGI(TAG, "BOOT — tarama baslatiliyor");
        uint8_t bulunan = nh_hub_tara();

        uint8_t adet = 0;
        const nh_hub_node_t *liste = nh_hub_liste(&adet);
        for (uint8_t i = 0; i < adet; i++) {
            nh_node_tazele(liste[i].adres, liste[i].uid,
                           liste[i].tip, liste[i].fw_surum);
        }
        ESP_LOGI(TAG, "tarama sonucu: %u node kaydedildi", bulunan);
    }
}

static void dugme_kur(void)
{
    gpio_config_t vars = {
        .pin_bit_mask = 1ULL << CONFIG_EXAMPLE_BOOT_BUTTON_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&vars);

    s_dugme_kuyrugu = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(dugme_gorevi, "dugme", 4096, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(CONFIG_EXAMPLE_BOOT_BUTTON_GPIO, dugme_isr,
                         (void *)CONFIG_EXAMPLE_BOOT_BUTTON_GPIO);
}

/* ---------------------------------------------------------------- */
/* Modlar                                                            */
/* ---------------------------------------------------------------- */

static void kurulum_modu(void)
{
    ESP_LOGI(TAG, "=== KURULUM MODU ===");
    ESP_LOGI(TAG, "telefonundan '%s' adli agi paylas (parola: %s)",
             NH_KURULUM_SSID, NH_KURULUM_PAROLA);

    esp_err_t r = nh_wifi_baglan(NH_KURULUM_SSID, NH_KURULUM_PAROLA,
                                 NH_KURULUM_ARAMA_MS);
    if (r != ESP_OK) {
        ESP_LOGW(TAG, "kurulum agi bulunamadi (%s)", esp_err_to_name(r));
        return;
    }

    if (mdns_init() == ESP_OK) {
        mdns_hostname_set(NH_KURULUM_MDNS);
        mdns_instance_name_set("NodeHub kurulum");
        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    }

    if (nh_web_baslat() != ESP_OK) {
        return;
    }

    char ip[16];
    nh_wifi_ip(ip, sizeof(ip));
    ESP_LOGI(TAG, "sayfayi ac: http://%s.local  ya da  http://%s",
             NH_KURULUM_MDNS, ip);

    /*
     * Oturum suresince beklenir. Kullanici kaydedince sayfa
     * /api/yeniden-baslat cagirir ve buraya hic donulmez.
     */
    vTaskDelay(pdMS_TO_TICKS(NH_KURULUM_OTURUM_MS));

    ESP_LOGW(TAG, "kurulum suresi doldu — isletmeye geciliyor");
    nh_web_durdur();
    mdns_free();
    nh_wifi_kes();
}

static void isletme_modu(void)
{
    nh_hub_ayar_t ayar;
    bool kurulu = (nh_ayar_oku(&ayar) == ESP_OK && ayar.wifi_ssid[0] != '\0');

    if (kurulu) {
        ESP_LOGI(TAG, "=== ISLETME MODU === hub: %s",
                 ayar.hub_adi[0] ? ayar.hub_adi : "(adsiz)");

        esp_err_t r = nh_wifi_baglan(ayar.wifi_ssid, ayar.wifi_parola, 20000);
        if (r == ESP_OK) {
            mqtt_baslat(&ayar);
        } else {
            ESP_LOGW(TAG, "'%s' agina baglanilamadi (%s)",
                     ayar.wifi_ssid, esp_err_to_name(r));
        }
    } else {
        memset(&ayar, 0, sizeof(ayar));
        ESP_LOGW(TAG, "ayar yok — ag kurulmadi");
    }

    /*
     * Planlayici her durumda baslar.
     *
     * Ag ya da broker olmasa bile node'lar okunur ve degerler loga
     * basilir; boylece RS485 tarafi agdan bagimsiz sinanabilir.
     */
    nh_olcum_baslat(s_mqtt_client, ayar.konu_oneki);
}

/* ---------------------------------------------------------------- */

void app_main(void)
{
    ESP_LOGI(TAG, "NodeHub hub — IDF %s, bos bellek %" PRIu32 " bayt",
             esp_get_idf_version(), esp_get_free_heap_size());

    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    ESP_ERROR_CHECK(r);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(nh_ayar_baslat());

    /* RS485 aga bagimli degil, her modda calisir. */
    nh_hub_baslat();
    dugme_kur();

    ESP_ERROR_CHECK(nh_wifi_baslat());

    /*
     * Her acilista once kurulum agi aranir.
     *
     * Bulunursa hub orada kalir ve sayfasini servis eder; bulunmazsa
     * NH_KURULUM_ARAMA_MS sonunda isletmeye gecer. Ayar olsun olmasin
     * bu adim hep calisir — kurulum icin tus kombinasyonu ezberlemek
     * gerekmesin diye.
     */
    kurulum_modu();
    isletme_modu();
}
