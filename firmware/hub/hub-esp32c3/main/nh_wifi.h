/*
 * NodeHub — Wi-Fi (yalnizca station).
 *
 * Hub AP ACMAZ. Omru boyunca istemcidir; degisen tek sey hangi aga
 * katildigi:
 *
 *   kurulum modu  -> telefonun paylastigi "nodehub" agi
 *   isletme modu  -> kullanicinin NVS'e yazdigi ev agi
 *
 * Bu yuzden protocol_examples_common kullanilmaz: o bilesen SSID'yi
 * derleme zamaninda sabitler ve modu WIFI_MODE_STA'ya kilitler.
 */

#ifndef NH_WIFI_H
#define NH_WIFI_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

/* netif + wifi surucusunu kurar, STA modunda baslatir. Bir kez. */
esp_err_t nh_wifi_baslat(void);

/*
 * Verilen aga baglanir ve IP alinana kadar bekler.
 *
 * Zaten baska bir aga bagliysa once o baglanti kesilir.
 * Zaman asiminda ESP_ERR_TIMEOUT doner.
 */
esp_err_t nh_wifi_baglan(const char *ssid, const char *parola,
                         uint32_t zaman_asimi_ms);

void nh_wifi_kes(void);

bool nh_wifi_bagli(void);

/* "192.168.43.107" — bagli degilse "0.0.0.0". */
void nh_wifi_ip(char *tampon, size_t n);

#endif /* NH_WIFI_H */
