/*
 * NodeHub — kurulum sayfasi ve JSON API.
 *
 * Uclar: contract/settings.yaml "uclar" bolumu.
 *
 * Yalnizca kurulum modunda calisir. Isletmede sunucu hic acilmaz —
 * sahada surekli acik bir kapi birakmamak icin.
 */

#ifndef NH_WEB_H
#define NH_WEB_H

#include "esp_err.h"

esp_err_t nh_web_baslat(void);
void      nh_web_durdur(void);

#endif /* NH_WEB_H */
