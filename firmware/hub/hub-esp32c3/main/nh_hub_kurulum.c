/*
 * NodeHub — hub tarafi kimlik atama.
 *
 * Tarama donguSU:
 *
 *   1. Yayina "kimligi olmayan var mi" sorulur          (0x64)
 *   2. Bir node kendini tanitir                          (0x65)
 *   3. Hub bos bir adres secip UID'ye hitaben verir      (0x66)
 *   4. Verilen adres yoklanarak dogrulanir               (FC 03)
 *   5. Ust uste NH_TARAMA_BITIS kez sessizlik gelirse biter
 *
 * Bozuk cerceve sessizlik sayilmaz — hatta hala node var demektir.
 */

#include <string.h>

#include "nh_hub_kurulum.h"
#include "nodehub_protocol.h"
#include "nh_crc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "nh_hub";

/* --- Pinler — ESP32-C3-DevKitC-02 ------------------------------- */
#define HUB_UART        UART_NUM_1
#define HUB_PIN_TX      4
#define HUB_PIN_RX      5
#define HUB_PIN_DE      6

#define TAMPON          128

static nh_hub_node_t s_liste[NH_HUB_EN_FAZLA_NODE];
static uint8_t       s_adet;
static uint8_t       s_deneme_no;

/* Son gonderilen cerceve. RE surekli dusuk oldugu icin kendi yayinimizi
   geri duyuyoruz; birebir ayni cerceve gelirse yanki sayilip atiliyor.
   Zamanlamaya dayanmadigi icin guvenilir. */
static uint8_t       s_son_tx[64];
static uint16_t      s_son_tx_n;

/* --------------------------------------------------------------- */

void nh_hub_baslat(void)
{
    const uart_config_t ayar = {
        .baud_rate  = NH_HIZ,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(HUB_UART, TAMPON * 2, TAMPON * 2, 0, NULL, 0);
    uart_param_config(HUB_UART, &ayar);
    uart_set_pin(HUB_UART, HUB_PIN_TX, HUB_PIN_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    /* DE'yi donanima birakmiyoruz.
     *
     * ESP-IDF'in RS485 yarim cift yonlu modunda alici, gonderim sirasinda
     * donanimsal olarak kapaniyor ve ne zaman geri actigi belirsiz. Elle
     * kontrol bu belirsizligi ortadan kaldiriyor. */
    gpio_config_t de = {
        .pin_bit_mask = 1ULL << HUB_PIN_DE,
        .mode         = GPIO_MODE_OUTPUT,
    };
    gpio_config(&de);
    gpio_set_level(HUB_PIN_DE, 0);          /* boşta dinle */

    ESP_LOGI(TAG, "RS485 hazir — %d baud, tx=%d rx=%d de=%d",
             NH_HIZ, HUB_PIN_TX, HUB_PIN_RX, HUB_PIN_DE);
}

static void gonder(const uint8_t *c, uint16_t n)
{
    uart_flush_input(HUB_UART);

    gpio_set_level(HUB_PIN_DE, 1);          /* surucuyu ac */
    uart_write_bytes(HUB_UART, (const char *)c, n);
    uart_wait_tx_done(HUB_UART, pdMS_TO_TICKS(100));
    gpio_set_level(HUB_PIN_DE, 0);          /* hatti hemen birak */

    if (n <= sizeof(s_son_tx)) {
        memcpy(s_son_tx, c, n);
        s_son_tx_n = n;
    }

    ESP_LOGI(TAG, "TX %u bayt", n);
    ESP_LOG_BUFFER_HEX(TAG, c, n);
}

/* Bir cerceve bekler. Donus: bayt sayisi, sessizlikte 0. */
/*
 * Bir cerceve bekler.
 *
 * Kendi yankimizi atlar ve kalan sure boyunca dinlemeye devam eder —
 * yanki bir "sessizlik" olarak sayilirsa node'un gercek cevabi kacar.
 *
 * Donus: bayt sayisi, sure dolarsa 0.
 */
static uint16_t al(uint8_t *tampon, uint32_t bekle_ms)
{
    TickType_t bitis = xTaskGetTickCount() + pdMS_TO_TICKS(bekle_ms);

    for (;;) {
        TickType_t simdi = xTaskGetTickCount();
        if (simdi >= bitis) {
            return 0;
        }

        int n = uart_read_bytes(HUB_UART, tampon, 1, bitis - simdi);
        if (n <= 0) {
            return 0;
        }

        uint16_t toplam = 1;
        while (toplam < TAMPON) {
            int m = uart_read_bytes(HUB_UART, tampon + toplam,
                                    (uint32_t)(TAMPON - toplam), 1);
            if (m <= 0) {
                break;
            }
            toplam = (uint16_t)(toplam + m);
        }

        /* Kendi yankimiz mi? Oyleyse yok say, dinlemeye devam et. */
        if (toplam == s_son_tx_n && memcmp(tampon, s_son_tx, toplam) == 0) {
            continue;
        }

        if (toplam >= 4) {
            ESP_LOGI(TAG, "RX %u bayt", toplam);
            ESP_LOG_BUFFER_HEX(TAG, tampon, toplam);
        }
        return toplam;
    }
}

static void sorgu_gonder(uint8_t deneme_no)
{
    uint8_t c[8];
    uint16_t n = 0;
    c[n++] = NH_ADRES_YAYIN;
    c[n++] = NH_FK_SORGU;
    c[n++] = deneme_no;
    n = nh_crc_ekle(c, n);
    gonder(c, n);
}

static void onay_gonder(const uint8_t *uid, uint8_t adres)
{
    uint8_t c[24];
    uint16_t n = 0;
    c[n++] = NH_ADRES_YAYIN;
    c[n++] = NH_FK_ONAY;
    memcpy(&c[n], uid, NH_UID_UZUNLUK);
    n += NH_UID_UZUNLUK;
    c[n++] = adres;
    n = nh_crc_ekle(c, n);
    gonder(c, n);
}

/* Adresi bir kez yoklar: node tipi register'ini okur. */
static bool yokla(uint8_t adres, uint16_t *tip_cikti)
{
    uint8_t c[8];
    uint16_t n = 0;
    c[n++] = adres;
    c[n++] = NH_OKUMA_FK;
    c[n++] = 0x00;  c[n++] = 0x00;      /* ilk register 0x0000 */
    c[n++] = 0x00;  c[n++] = 0x01;      /* 1 adet              */
    n = nh_crc_ekle(c, n);
    gonder(c, n);

    uint8_t cevap[TAMPON];
    uint16_t m = al(cevap, NH_ZAMAN_ASIMI_MS);

    if (m == 0) {
        return false;                 /* sessizlik — bu adreste kimse yok */
    }
    if (m != 7 || !nh_crc_dogru(cevap, m)) {
        ESP_LOGW(TAG, "0x%02X: bozuk cevap, %u bayt", adres, m);
        return false;
    }
    if (cevap[0] != adres || cevap[1] != NH_OKUMA_FK || cevap[2] != 2) {
        ESP_LOGW(TAG, "0x%02X: beklenmeyen cevap %02X %02X %02X",
                 adres, cevap[0], cevap[1], cevap[2]);
        return false;
    }
    if (tip_cikti) {
        *tip_cikti = (uint16_t)(((uint16_t)cevap[3] << 8) | cevap[4]);
    }
    return true;
}

/*
 * Atamayi dogrular.
 *
 * Node onayi aldiktan sonra kimligi flash'a yaziyor; NVS yazmasi onlarca
 * milisaniye surebiliyor ve o sirada UART'i islemiyor. Tek bir yoklama
 * cok erken gelip bosa cikiyordu — bu yuzden birkac kez deneniyor.
 */
static bool dogrula(uint8_t adres, uint16_t beklenen_tip)
{
    for (uint8_t deneme = 0; deneme < 3; deneme++) {
        vTaskDelay(pdMS_TO_TICKS(100));

        uint16_t tip = 0;
        if (yokla(adres, &tip)) {
            if (tip == beklenen_tip) {
                return true;
            }
            ESP_LOGW(TAG, "0x%02X cevap verdi ama tip 0x%04X (beklenen 0x%04X)",
                     adres, tip, beklenen_tip);
            return false;
        }
    }
    return false;
}

static uint8_t bos_adres(void)
{
    for (uint8_t a = NH_ADRES_ILK; a <= NH_ADRES_SON; a++) {
        bool kullanimda = false;
        for (uint8_t i = 0; i < s_adet; i++) {
            if (s_liste[i].adres == a) {
                kullanimda = true;
                break;
            }
        }
        if (!kullanimda) {
            return a;
        }
    }
    return 0;
}

/* --------------------------------------------------------------- */

uint8_t nh_hub_tara(void)
{
    ESP_LOGI(TAG, "tarama basliyor");

    uint8_t sessiz = 0;
    uint8_t cevap[TAMPON];

    while (sessiz < NH_TARAMA_BITIS && s_adet < NH_HUB_EN_FAZLA_NODE) {

        sorgu_gonder(s_deneme_no++);

        uint16_t n = al(cevap, NH_CEVAP_BEKLEME_MS);

        /* Kutuplamasiz hatta gurultuden tek baytlik cop geliyor.
           Bunlari cevap sayarsak sayac hic dolmaz, tarama bitmez. */
        if (n < 4) {
            sessiz++;
            continue;
        }

        /* Bozuk cerceve = carpisma. Hatta hala node var, sayaci sifirla. */
        if (n != 20 || !nh_crc_dogru(cevap, n) ||
            cevap[0] != NH_ADRES_KURULUM || cevap[1] != NH_FK_CEVAP) {
            ESP_LOGW(TAG, "gecersiz cerceve: %u bayt, ilk baytlar %02X %02X",
                     n, cevap[0], n > 1 ? cevap[1] : 0);
            sessiz = 0;
            continue;
        }

        sessiz = 0;

        nh_hub_node_t yeni;
        memcpy(yeni.uid, &cevap[2], NH_UID_UZUNLUK);
        yeni.tip      = (uint16_t)(((uint16_t)cevap[14] << 8) | cevap[15]);
        yeni.fw_surum = (uint16_t)(((uint16_t)cevap[16] << 8) | cevap[17]);
        yeni.adres    = bos_adres();

        if (yeni.adres == 0) {
            ESP_LOGE(TAG, "bos adres kalmadi");
            break;
        }

        onay_gonder(yeni.uid, yeni.adres);

        if (!dogrula(yeni.adres, yeni.tip)) {
            ESP_LOGW(TAG, "0x%02X adresi dogrulanamadi, atama tutmadi",
                     yeni.adres);
            continue;                        /* adres havuza geri doner */
        }

        s_liste[s_adet++] = yeni;

        ESP_LOGI(TAG, "node bulundu — adres 0x%02X  tip 0x%04X  fw %u.%u  "
                      "uid %02X%02X%02X%02X%02X%02X",
                 yeni.adres, yeni.tip,
                 yeni.fw_surum >> 8, yeni.fw_surum & 0xFFu,
                 yeni.uid[0], yeni.uid[1], yeni.uid[2],
                 yeni.uid[3], yeni.uid[4], yeni.uid[5]);
    }

    ESP_LOGI(TAG, "tarama bitti — toplam %u node", s_adet);
    return s_adet;
}

const nh_hub_node_t *nh_hub_liste(uint8_t *adet)
{
    if (adet) {
        *adet = s_adet;
    }
    return s_liste;
}
