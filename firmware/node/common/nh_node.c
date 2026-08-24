/*
 * NodeHub — ortak katmanin baslangic noktasi ve gorevleri.
 *
 * Iki gorev calisir:
 *   hat    : gelen cerceveleri alir, kuruluma ya da Modbus'a yonlendirir
 *   olcum  : nh_tanim()->olc() fonksiyonunu periyodik cagirir
 *
 * Ikisi ayri durur cunku olcum uzun surebilir; hat gorevi 50 ms icinde
 * cevap verme yukumlulugunu tasir ve bloklanmamalidir.
 */

#include "nh_node.h"
#include "nh_port.h"
#include "nh_kurulum.h"
#include "nodehub_protocol.h"
#include "nh_crc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "nodehub";

#define CERCEVE_EN_FAZLA   64

static volatile uint16_t s_durum_bayrak = NH_DURUM_HAZIR_DEGIL;
static const nh_node_tanim_t *s_tanim;

const nh_node_tanim_t *nh_tanim(void)
{
    return s_tanim;
}

/* --------------------------------------------------------------- */

void nh_durum_ayarla(uint16_t bayrak)
{
    s_durum_bayrak |= bayrak;
}

void nh_durum_temizle(uint16_t bayrak)
{
    s_durum_bayrak &= (uint16_t)~bayrak;
}

uint16_t nh_durum_oku(void)
{
    return s_durum_bayrak;
}

uint8_t nh_kimlik(void)
{
    return nh_kayit_kimlik();
}

/* --------------------------------------------------------------- */

static void hat_gorevi(void *arg)
{
    (void)arg;
    uint8_t cerceve[CERCEVE_EN_FAZLA];

    for (;;) {
        uint16_t n = nh_port_al(cerceve, sizeof(cerceve), 100);
        if (n == 0) {
            continue;
        }

        /* Kutuplamasiz hatta gurultuden sahte 0x00 baytlari geliyor.
           Anlamli en kisa cerceve 4 bayt; altindakiler sessizce atilir. */
        if (n < 4) {
            continue;
        }

        ESP_LOGI(TAG, "RX %u bayt", n);
        ESP_LOG_BUFFER_HEX(TAG, cerceve, n);
        if (!nh_crc_dogru(cerceve, n)) {
            ESP_LOGW(TAG, "CRC tutmadi (%u bayt)", n);
            continue;
        }

        if (nh_kurulum_cerceve(cerceve, n)) {
            continue;
        }
        nh_modbus_cerceve(cerceve, n);
    }
}

static void olcum_gorevi(void *arg)
{
    (void)arg;

    if (nh_tanim()->baslat) {
        nh_tanim()->baslat();
    }

    TickType_t periyot = pdMS_TO_TICKS(nh_tanim()->periyot_ms);
    if (periyot == 0) {
        periyot = pdMS_TO_TICKS(1000);
    }

    for (;;) {
        if (nh_tanim()->olc) {
            nh_tanim()->olc();
            nh_durum_temizle(NH_DURUM_HAZIR_DEGIL);
        }
        vTaskDelay(periyot);
    }
}

/* --------------------------------------------------------------- */

void nh_baslat(const nh_node_tanim_t *tanim)
{
    s_tanim = tanim;

    nh_port_baslat(NH_HIZ);
    nh_kayit_yukle();
    nh_kurulum_baslat();

    ESP_LOGI(TAG, "tasiyici algilama: %s",
             NH_TASIYICI_ALGILA ? "acik" : "KAPALI — kutuplama direnci bekliyor");

    uint8_t k = nh_kayit_kimlik();
    if (k) {
        ESP_LOGI(TAG, "kayitli node, kimlik 0x%02X, tip 0x%04X",
                 k, nh_tanim()->tip);
    } else {
        ESP_LOGI(TAG, "bakir node, tip 0x%04X — kimlik bekleniyor",
                 nh_tanim()->tip);
    }

    xTaskCreate(hat_gorevi,   "nh_hat",   4096, NULL, 10, NULL);
    xTaskCreate(olcum_gorevi, "nh_olcum", 4096, NULL,  5, NULL);
}
