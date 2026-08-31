/* NodeHub — olcum planlayicisi. */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "nh_olcum.h"
#include "nh_ayarlar.h"
#include "nh_hub_kurulum.h"
#include "nh_tipler.h"
#include "nodehub_protocol.h"

#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "nh_olcum";

/* Dongunun cozunurlugu. En kisa periyot 1 sn oldugu icin 100 ms yeter. */
#define TIK_MS              100

/* Node listesi NVS'ten bu araliklarla tazelenir — sayfadan yapilan
   degisiklik en gec bu kadar sonra devreye girer. */
#define LISTE_TAZELEME_MS   5000

#define KONU_UZ             160

static esp_mqtt_client_handle_t s_mqtt;
static char                     s_konu_oneki[NH_KONU_UZ];

/* Adres basina son okuma zamani. Indeks = Modbus adresi. */
static uint32_t s_son_okuma[NH_ADRES_SON + 1];

static uint32_t simdi_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void oku_ve_yayinla(const nh_node_kayit_t *node)
{
    const nh_tip_t *tip = nh_tip_bul(node->tip);
    if (tip == NULL) {
        ESP_LOGW(TAG, "0x%02X: tip 0x%04X sozlesmede yok, atlaniyor",
                 node->adres, node->tip);
        return;
    }

    uint16_t ham[NH_EN_FAZLA_REGISTER];
    if (!nh_hub_oku(node->adres, tip->ilk_offset, tip->reg_adet, ham)) {
        ESP_LOGW(TAG, "0x%02X: cevap yok", node->adres);
        return;
    }

    cJSON *kok = cJSON_CreateObject();
    if (kok == NULL) {
        return;
    }

    cJSON_AddNumberToObject(kok, "adres", node->adres);
    cJSON_AddStringToObject(kok, "ad", node->ad[0] ? node->ad : tip->ad);
    cJSON_AddNumberToObject(kok, "tip", node->tip);

    for (uint8_t i = 0; i < tip->olcum_adet; i++) {
        const nh_tip_olcum_t *o = &tip->olcumler[i];

        if (nh_tip_gecersiz(o, ham, tip->ilk_offset)) {
            /* Sozlesmedeki gecersiz deger — sensor okunamiyor demek.
               Sayi yerine null gonderiliyor ki grafikte bosluk olsun,
               sifir gibi gorunmesin. */
            cJSON_AddNullToObject(kok, o->ad);
            continue;
        }

        double d = (double)nh_tip_degeri(o, ham, tip->ilk_offset);

        /* olcek float oldugu icin 21.3 yerine 21.300000476 cikiyor.
           Uc basamaga yuvarlaninca cJSON kisa hali basiyor. */
        d = round(d * 1000.0) / 1000.0;

        cJSON_AddNumberToObject(kok, o->ad, d);
    }

    char *govde = cJSON_PrintUnformatted(kok);
    cJSON_Delete(kok);
    if (govde == NULL) {
        return;
    }

    char konu[KONU_UZ];
    snprintf(konu, sizeof(konu), "%s/node-%02x",
             s_konu_oneki[0] ? s_konu_oneki : "nodehub", node->adres);

    int msg_id = -1;
    if (s_mqtt != NULL) {
        msg_id = esp_mqtt_client_publish(s_mqtt, konu, govde, 0, 1, 0);
    }

    if (msg_id >= 0) {
        ESP_LOGI(TAG, "%s  %s", konu, govde);
    } else {
        /* Yayinlanamadi — hat calisiyor ama ag yok. Deger yine gorulsun. */
        ESP_LOGI(TAG, "(yayinlanamadi) %s  %s", konu, govde);
    }

    cJSON_free(govde);
}

static void gorev(void *arg)
{
    (void)arg;

    static nh_node_kayit_t liste[NH_HUB_EN_FAZLA_NODE];
    uint8_t  adet = 0;
    uint32_t son_tazeleme = 0;
    bool     ilk = true;

    for (;;) {
        uint32_t simdi = simdi_ms();

        if (ilk || (simdi - son_tazeleme) >= LISTE_TAZELEME_MS) {
            nh_node_listele(liste, NH_HUB_EN_FAZLA_NODE, &adet);
            son_tazeleme = simdi;
            ilk = false;
        }

        for (uint8_t i = 0; i < adet; i++) {
            const nh_node_kayit_t *n = &liste[i];

            if (!n->aktif) {
                continue;
            }
            if ((simdi - s_son_okuma[n->adres]) < n->periyot_ms) {
                continue;
            }

            s_son_okuma[n->adres] = simdi;
            oku_ve_yayinla(n);

            /* Okuma hatti saniyeler tutabilir; zamani tazele. */
            simdi = simdi_ms();
        }

        vTaskDelay(pdMS_TO_TICKS(TIK_MS));
    }
}

void nh_olcum_baslat(esp_mqtt_client_handle_t mqtt, const char *konu_oneki)
{
    s_mqtt = mqtt;
    if (konu_oneki != NULL) {
        strlcpy(s_konu_oneki, konu_oneki, sizeof(s_konu_oneki));
    }

    xTaskCreate(gorev, "nh_olcum", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "planlayici basladi — konu oneki '%s'",
             s_konu_oneki[0] ? s_konu_oneki : "nodehub");
}
