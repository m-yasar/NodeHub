/*
 * NodeHub — donanım katmanının ESP32 gerçeklemesi.
 *
 * Başka bir işlemciye geçildiğinde değişecek tek dosya budur.
 */

#include <string.h>
#include "nh_port.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs.h"
#include "nvs_flash.h"

/* --- Pinler — kart değişirse burayı düzenle --------------------- */
#ifndef NH_UART_NUM
#define NH_UART_NUM     UART_NUM_1
#endif
#ifndef NH_PIN_TX
#define NH_PIN_TX       17
#endif
#ifndef NH_PIN_RX
#define NH_PIN_RX       16
#endif
#ifndef NH_PIN_DE
#define NH_PIN_DE       4
#endif

#define NH_RX_TAMPON    256
#define NH_NVS_ALAN     "nodehub"
#define NH_NVS_ANAHTAR  "kimlik"

static volatile bool s_kenar_oldu;

/* Son gönderilen çerçeve — kendi yankımızı tanıyıp atmak için. */
static uint8_t  s_son_tx[64];
static uint16_t s_son_tx_n;

/* Kurulum beklemesi sırasında hattın kullanıldığını yakalar.
   Yalnızca beklerken açılır — normal alım sırasında her bit geçişinde
   tetiklenir ve işlemciyi boğar. */
static void IRAM_ATTR rx_kenar_isr(void *arg)
{
    (void)arg;
    s_kenar_oldu = true;
}

void nh_port_baslat(uint32_t baud)
{
    const uart_config_t ayar = {
        .baud_rate  = (int)baud,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,        /* 8N1, sözleşme */
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(NH_UART_NUM, NH_RX_TAMPON, NH_RX_TAMPON, 0, NULL, 0);
    uart_param_config(NH_UART_NUM, &ayar);
    uart_set_pin(NH_UART_NUM, NH_PIN_TX, NH_PIN_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    /* DE elle kontrol ediliyor.
     *
     * ESP-IDF'in RS485 yarım çift yönlü modunda alıcı gönderim sırasında
     * donanımsal kapanıyor ve ne zaman geri açıldığı belgelenmemiş. Elle
     * sürmek hem öngörülebilir hem başka işlemciye taşınabilir. */
    gpio_config_t de = {
        .pin_bit_mask = 1ULL << NH_PIN_DE,
        .mode         = GPIO_MODE_OUTPUT,
    };
    gpio_config(&de);
    gpio_set_level(NH_PIN_DE, 0);            /* boşta dinle */

    /* RX pini aynı anda kenar kesmesi kaynağı. ESP32'nin GPIO matrisi
       bir girişi hem UART'a hem GPIO'ya dağıtabildiği için mümkün. */
    gpio_set_intr_type(NH_PIN_RX, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(NH_PIN_RX, rx_kenar_isr, NULL);
    gpio_intr_disable(NH_PIN_RX);

    nvs_flash_init();
}

void nh_port_gonder(const uint8_t *veri, uint16_t uzunluk)
{
    gpio_set_level(NH_PIN_DE, 1);            /* sürücüyü aç */
    uart_write_bytes(NH_UART_NUM, (const char *)veri, uzunluk);

    /* Son bit hattan çıkmadan DE inmemeli. */
    uart_wait_tx_done(NH_UART_NUM, pdMS_TO_TICKS(100));
    gpio_set_level(NH_PIN_DE, 0);            /* hattı hemen bırak */

    if (uzunluk <= sizeof(s_son_tx)) {
        memcpy(s_son_tx, veri, uzunluk);
        s_son_tx_n = uzunluk;
    }

    ESP_LOGI("nh_port", "TX %u bayt", uzunluk);
    ESP_LOG_BUFFER_HEX("nh_port", veri, uzunluk);
}

uint16_t nh_port_al(uint8_t *tampon, uint16_t en_fazla, uint32_t bekle_ms)
{
    int n = uart_read_bytes(NH_UART_NUM, tampon, 1, pdMS_TO_TICKS(bekle_ms));
    if (n <= 0) {
        return 0;
    }

    uint16_t toplam = 1;
    /* Karakterler arası boşluk açılınca çerçeve bitmiş sayılır.
       En az 1 tik bekle: pdMS_TO_TICKS(3) varsayılan 100 Hz tikte 0'a
       yuvarlanıyor ve çerçeve yarıda kesilebiliyordu. */
    while (toplam < en_fazla) {
        int m = uart_read_bytes(NH_UART_NUM, tampon + toplam,
                                (uint32_t)(en_fazla - toplam), 1);
        if (m <= 0) {
            break;
        }
        toplam = (uint16_t)(toplam + m);
    }

    /* Kendi yankımız mı? Öyleyse yok say. */
    if (toplam == s_son_tx_n && memcmp(tampon, s_son_tx, toplam) == 0) {
        return 0;
    }

    return toplam;
}

uint32_t nh_port_us(void)
{
    return (uint32_t)esp_timer_get_time();
}

void nh_port_kenar_izle(bool acik)
{
    if (acik) {
        s_kenar_oldu = false;
        gpio_intr_enable(NH_PIN_RX);
    } else {
        gpio_intr_disable(NH_PIN_RX);
    }
}

bool nh_port_kenar_oldu(void)
{
    return s_kenar_oldu;
}

bool nh_port_hat_bos(void)
{
    /* Boştaki hat yüksek. Düşükse biri konuşuyor. */
    return gpio_get_level(NH_PIN_RX) != 0;
}

void nh_port_uid_oku(uint8_t uid[NH_UID_UZUNLUK])
{
    uint8_t mac[6] = {0};
    esp_efuse_mac_get_default(mac);

    /* ESP32'de fabrika kimliği 48 bitlik MAC adresi — IEEE kayıtlı,
       benzersizliği garanti. Sözleşme 12 bayt istediği için kalanı
       sıfırla dolduruyoruz. */
    memset(uid, 0, NH_UID_UZUNLUK);
    memcpy(uid, mac, sizeof(mac));
}

bool nh_port_kimlik_yaz(uint8_t adres)
{
    nvs_handle_t h;
    if (nvs_open(NH_NVS_ALAN, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    bool tamam = (nvs_set_u8(h, NH_NVS_ANAHTAR, adres) == ESP_OK) &&
                 (nvs_commit(h) == ESP_OK);
    nvs_close(h);
    return tamam;
}

uint8_t nh_port_kimlik_oku(void)
{
    nvs_handle_t h;
    if (nvs_open(NH_NVS_ALAN, NVS_READONLY, &h) != ESP_OK) {
        return 0;
    }
    uint8_t adres = 0;
    if (nvs_get_u8(h, NH_NVS_ANAHTAR, &adres) != ESP_OK) {
        adres = 0;
    }
    nvs_close(h);
    return adres;
}
