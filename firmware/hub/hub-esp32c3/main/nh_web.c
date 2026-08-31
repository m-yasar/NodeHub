/* NodeHub — kurulum sayfasi ve JSON API. */

#include <stdlib.h>
#include <string.h>

#include "nh_web.h"
#include "nh_ayarlar.h"
#include "nh_hub_kurulum.h"
#include "nh_tipler.h"
#include "nodehub_protocol.h"

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "nh_web";

#define GOVDE_EN_FAZLA  1024

extern const uint8_t kurulum_html_start[] asm("_binary_kurulum_html_start");
extern const uint8_t kurulum_html_end[]   asm("_binary_kurulum_html_end");

static httpd_handle_t s_sunucu;

/* Node tipi adi — nh_tipler.c, kaynagi contract/registers.yaml. */
static const char *tip_adi(uint16_t tip)
{
    return nh_tip_adi(tip);
}

/* ---------------------------------------------------------------- */
/* Yardimcilar                                                       */
/* ---------------------------------------------------------------- */

static esp_err_t json_gonder(httpd_req_t *istek, cJSON *kok)
{
    char *metin = cJSON_PrintUnformatted(kok);
    cJSON_Delete(kok);

    if (metin == NULL) {
        httpd_resp_send_500(istek);
        return ESP_FAIL;
    }

    httpd_resp_set_type(istek, "application/json");
    esp_err_t r = httpd_resp_sendstr(istek, metin);
    cJSON_free(metin);
    return r;
}

/* Govdeyi okur ve ayristirir. Cagiran cJSON_Delete etmeli. */
static cJSON *govde_al(httpd_req_t *istek)
{
    if (istek->content_len <= 0 || istek->content_len >= GOVDE_EN_FAZLA) {
        return NULL;
    }

    char *tampon = malloc(istek->content_len + 1);
    if (tampon == NULL) {
        return NULL;
    }

    int okunan = 0;
    while (okunan < istek->content_len) {
        int n = httpd_req_recv(istek, tampon + okunan, istek->content_len - okunan);
        if (n <= 0) {
            free(tampon);
            return NULL;
        }
        okunan += n;
    }
    tampon[okunan] = '\0';

    cJSON *kok = cJSON_Parse(tampon);
    free(tampon);
    return kok;
}

/* Metin alanini kopyalar. Alan yoksa hedefe dokunmaz. */
static void metin_al(const cJSON *kok, const char *ad, char *hedef, size_t n)
{
    const cJSON *e = cJSON_GetObjectItemCaseSensitive(kok, ad);
    if (cJSON_IsString(e) && e->valuestring != NULL) {
        strlcpy(hedef, e->valuestring, n);
    }
}

/* Parola alani: bos gelirse oncekini korur. */
static void parola_al(const cJSON *kok, const char *ad, char *hedef, size_t n)
{
    const cJSON *e = cJSON_GetObjectItemCaseSensitive(kok, ad);
    if (cJSON_IsString(e) && e->valuestring != NULL && e->valuestring[0] != '\0') {
        strlcpy(hedef, e->valuestring, n);
    }
}

static void uid_metni(const uint8_t uid[12], char *tampon, size_t n)
{
    size_t k = 0;
    for (int i = 0; i < 12 && k + 2 < n; i++) {
        k += snprintf(tampon + k, n - k, "%02X", uid[i]);
    }
}

static cJSON *node_json(const nh_node_kayit_t *k)
{
    cJSON *n = cJSON_CreateObject();
    char uid[25];
    char fw[12];

    uid_metni(k->uid, uid, sizeof(uid));
    snprintf(fw, sizeof(fw), "%u.%u", k->fw_surum >> 8, k->fw_surum & 0xFF);

    cJSON_AddNumberToObject(n, "adres", k->adres);
    cJSON_AddNumberToObject(n, "tip", k->tip);
    cJSON_AddStringToObject(n, "uid", uid);
    cJSON_AddStringToObject(n, "fw", fw);
    cJSON_AddStringToObject(n, "ad", k->ad);
    cJSON_AddNumberToObject(n, "periyot_ms", k->periyot_ms);
    cJSON_AddNumberToObject(n, "aktif", k->aktif);
    cJSON_AddNumberToObject(n, "kayip", k->kayip);

    const char *ta = tip_adi(k->tip);
    if (ta != NULL) {
        cJSON_AddStringToObject(n, "tip_ad", ta);
    }
    return n;
}

static esp_err_t node_listesi_gonder(httpd_req_t *istek)
{
    static nh_node_kayit_t liste[NH_HUB_EN_FAZLA_NODE];
    uint8_t adet = 0;
    nh_node_listele(liste, NH_HUB_EN_FAZLA_NODE, &adet);

    cJSON *kok = cJSON_CreateObject();
    cJSON *dizi = cJSON_AddArrayToObject(kok, "nodelar");
    for (uint8_t i = 0; i < adet; i++) {
        cJSON_AddItemToArray(dizi, node_json(&liste[i]));
    }
    return json_gonder(istek, kok);
}

/* URI sonundaki adresi cozer: /api/nodelar/3 -> 3 */
static int adres_coz(const char *uri)
{
    const char *son = strrchr(uri, '/');
    if (son == NULL || son[1] == '\0') {
        return -1;
    }
    int a = atoi(son + 1);
    if (a < NH_ADRES_ILK || a > NH_ADRES_SON) {
        return -1;
    }
    return a;
}

/* ---------------------------------------------------------------- */
/* Ucler                                                             */
/* ---------------------------------------------------------------- */

static esp_err_t sayfa_getir(httpd_req_t *istek)
{
    httpd_resp_set_type(istek, "text/html");
    return httpd_resp_send(istek, (const char *)kurulum_html_start,
                           kurulum_html_end - kurulum_html_start - 1);
}

static esp_err_t ayar_getir(httpd_req_t *istek)
{
    nh_hub_ayar_t a;
    nh_ayar_oku(&a);            /* yoksa sifirlanmis doner */

    char hub_id[16];
    nh_ayar_hub_id(hub_id, sizeof(hub_id));

    cJSON *kok = cJSON_CreateObject();
    cJSON_AddStringToObject(kok, "hub_id", hub_id);
    cJSON_AddStringToObject(kok, "fw", "1.0");
    cJSON_AddStringToObject(kok, "wifi_ssid", a.wifi_ssid);
    cJSON_AddStringToObject(kok, "broker_uri", a.broker_uri);
    cJSON_AddStringToObject(kok, "broker_kullanici", a.broker_kullanici);
    cJSON_AddStringToObject(kok, "konu_oneki", a.konu_oneki);
    cJSON_AddStringToObject(kok, "hub_adi", a.hub_adi);
    /* Parolalar bilerek yok — sozlesme geri gostermeyi yasakliyor. */

    return json_gonder(istek, kok);
}

static esp_err_t ayar_yaz(httpd_req_t *istek)
{
    cJSON *g = govde_al(istek);
    if (g == NULL) {
        httpd_resp_send_err(istek, HTTPD_400_BAD_REQUEST, "gecersiz govde");
        return ESP_FAIL;
    }

    nh_hub_ayar_t a;
    nh_ayar_oku(&a);            /* onceki degerlerin uzerine yazilir */

    metin_al(g,  "wifi_ssid",        a.wifi_ssid,        sizeof(a.wifi_ssid));
    parola_al(g, "wifi_parola",      a.wifi_parola,      sizeof(a.wifi_parola));
    metin_al(g,  "broker_uri",       a.broker_uri,       sizeof(a.broker_uri));
    metin_al(g,  "broker_kullanici", a.broker_kullanici, sizeof(a.broker_kullanici));
    parola_al(g, "broker_parola",    a.broker_parola,    sizeof(a.broker_parola));
    metin_al(g,  "konu_oneki",       a.konu_oneki,       sizeof(a.konu_oneki));
    metin_al(g,  "hub_adi",          a.hub_adi,          sizeof(a.hub_adi));

    cJSON_Delete(g);

    if (nh_ayar_yaz(&a) != ESP_OK) {
        httpd_resp_send_500(istek);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ayarlar kaydedildi — ag: %s", a.wifi_ssid);

    cJSON *kok = cJSON_CreateObject();
    cJSON_AddBoolToObject(kok, "tamam", true);
    return json_gonder(istek, kok);
}

static esp_err_t node_getir(httpd_req_t *istek)
{
    return node_listesi_gonder(istek);
}

static esp_err_t node_yaz(httpd_req_t *istek)
{
    int adres = adres_coz(istek->uri);
    if (adres < 0) {
        httpd_resp_send_err(istek, HTTPD_400_BAD_REQUEST, "gecersiz adres");
        return ESP_FAIL;
    }

    nh_node_kayit_t k;
    if (nh_node_oku((uint8_t)adres, &k) != ESP_OK) {
        httpd_resp_send_err(istek, HTTPD_404_NOT_FOUND, "node kayitli degil");
        return ESP_FAIL;
    }

    cJSON *g = govde_al(istek);
    if (g == NULL) {
        httpd_resp_send_err(istek, HTTPD_400_BAD_REQUEST, "gecersiz govde");
        return ESP_FAIL;
    }

    metin_al(g, "ad", k.ad, sizeof(k.ad));

    const cJSON *p = cJSON_GetObjectItemCaseSensitive(g, "periyot_ms");
    if (cJSON_IsNumber(p) && p->valuedouble >= 1000) {
        k.periyot_ms = (uint32_t)p->valuedouble;
    }

    const cJSON *ak = cJSON_GetObjectItemCaseSensitive(g, "aktif");
    if (cJSON_IsNumber(ak)) {
        k.aktif = ak->valueint ? 1 : 0;
    }

    cJSON_Delete(g);

    if (nh_node_yaz(&k) != ESP_OK) {
        httpd_resp_send_500(istek);
        return ESP_FAIL;
    }

    cJSON *kok = cJSON_CreateObject();
    cJSON_AddBoolToObject(kok, "tamam", true);
    return json_gonder(istek, kok);
}

static esp_err_t node_sil(httpd_req_t *istek)
{
    int adres = adres_coz(istek->uri);
    if (adres < 0) {
        httpd_resp_send_err(istek, HTTPD_400_BAD_REQUEST, "gecersiz adres");
        return ESP_FAIL;
    }

    nh_node_sil((uint8_t)adres);

    cJSON *kok = cJSON_CreateObject();
    cJSON_AddBoolToObject(kok, "tamam", true);
    return json_gonder(istek, kok);
}

static esp_err_t tarama_yap(httpd_req_t *istek)
{
    ESP_LOGI(TAG, "sayfadan tarama istendi");

    uint8_t bulunan = nh_hub_tara();

    uint8_t adet = 0;
    const nh_hub_node_t *liste = nh_hub_liste(&adet);
    for (uint8_t i = 0; i < adet; i++) {
        nh_node_tazele(liste[i].adres, liste[i].uid,
                       liste[i].tip, liste[i].fw_surum);
    }

    ESP_LOGI(TAG, "tarama bitti — %u node kaydedildi", bulunan);
    return node_listesi_gonder(istek);
}

/*
 * Kimlik silme.
 *
 * Govde: { "hedef": 0 }  -> hattaki herkes
 *        { "hedef": 3 }  -> yalnizca 0x03
 *
 * Hedef pakette tasindigi icin tek uc iki isi de goruyor; sozlesmedeki
 * nh_unut_t ile birebir.
 */
static esp_err_t unut_yap(httpd_req_t *istek)
{
    cJSON *g = govde_al(istek);
    if (g == NULL) {
        httpd_resp_send_err(istek, HTTPD_400_BAD_REQUEST, "gecersiz govde");
        return ESP_FAIL;
    }

    const cJSON *h = cJSON_GetObjectItemCaseSensitive(g, "hedef");
    int hedef = cJSON_IsNumber(h) ? h->valueint : -1;
    cJSON_Delete(g);

    if (hedef != NH_ADRES_YAYIN &&
        (hedef < NH_ADRES_ILK || hedef > NH_ADRES_SON)) {
        httpd_resp_send_err(istek, HTTPD_400_BAD_REQUEST, "gecersiz hedef");
        return ESP_FAIL;
    }

    nh_hub_unut((uint8_t)hedef);

    /* Node unuttuysa hub'in kaydi da gecersiz — birlikte silinmeli. */
    if (hedef == NH_ADRES_YAYIN) {
        static nh_node_kayit_t liste[NH_HUB_EN_FAZLA_NODE];
        uint8_t adet = 0;
        nh_node_listele(liste, NH_HUB_EN_FAZLA_NODE, &adet);
        for (uint8_t i = 0; i < adet; i++) {
            nh_node_sil(liste[i].adres);
        }
        ESP_LOGW(TAG, "%u node kaydi silindi", adet);
    } else {
        nh_node_sil((uint8_t)hedef);
        ESP_LOGW(TAG, "0x%02X kaydi silindi", hedef);
    }

    return node_listesi_gonder(istek);
}

static void yeniden_baslat_gorevi(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(500));   /* cevap gitsin */
    esp_restart();
}

static esp_err_t yeniden_baslat(httpd_req_t *istek)
{
    cJSON *kok = cJSON_CreateObject();
    cJSON_AddBoolToObject(kok, "tamam", true);
    esp_err_t r = json_gonder(istek, kok);

    xTaskCreate(yeniden_baslat_gorevi, "yeniden", 2048, NULL, 5, NULL);
    return r;
}

/* ---------------------------------------------------------------- */

static const httpd_uri_t s_ucler[] = {
    { .uri = "/",                  .method = HTTP_GET,    .handler = sayfa_getir },
    { .uri = "/api/ayarlar",       .method = HTTP_GET,    .handler = ayar_getir },
    { .uri = "/api/ayarlar",       .method = HTTP_POST,   .handler = ayar_yaz },
    { .uri = "/api/nodelar",       .method = HTTP_GET,    .handler = node_getir },
    { .uri = "/api/nodelar/*",     .method = HTTP_POST,   .handler = node_yaz },
    { .uri = "/api/nodelar/*",     .method = HTTP_DELETE, .handler = node_sil },
    { .uri = "/api/tarama",        .method = HTTP_POST,   .handler = tarama_yap },
    { .uri = "/api/unut",          .method = HTTP_POST,   .handler = unut_yap },
    { .uri = "/api/yeniden-baslat",.method = HTTP_POST,   .handler = yeniden_baslat },
};

esp_err_t nh_web_baslat(void)
{
    if (s_sunucu != NULL) {
        return ESP_OK;
    }

    httpd_config_t vars = HTTPD_DEFAULT_CONFIG();
    vars.max_uri_handlers = 12;
    vars.stack_size       = 6144;   /* cJSON + tarama icin varsayilan 4096 dar */
    vars.lru_purge_enable = true;
    vars.uri_match_fn     = httpd_uri_match_wildcard;

    /*
     * Tarama RS485 hattinda saniyeler suruyor ve handler icinde
     * calisiyor; tarayici bu sirada baglantiyi dusurmesin diye
     * zaman asimlari genisletildi.
     */
    vars.recv_wait_timeout = 30;
    vars.send_wait_timeout = 30;

    esp_err_t r = httpd_start(&s_sunucu, &vars);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "sunucu baslamadi: %s", esp_err_to_name(r));
        return r;
    }

    for (size_t i = 0; i < sizeof(s_ucler) / sizeof(s_ucler[0]); i++) {
        httpd_register_uri_handler(s_sunucu, &s_ucler[i]);
    }

    ESP_LOGI(TAG, "kurulum sayfasi yayinda");
    return ESP_OK;
}

void nh_web_durdur(void)
{
    if (s_sunucu != NULL) {
        httpd_stop(s_sunucu);
        s_sunucu = NULL;
        ESP_LOGI(TAG, "sunucu durduruldu");
    }
}
