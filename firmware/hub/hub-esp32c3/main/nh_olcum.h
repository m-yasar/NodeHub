/*
 * NodeHub — olcum planlayicisi.
 *
 * Kayitli node'lari kendi periyotlariyla okur ve MQTT'ye yayinlar.
 * Sayfadan gelen uc ayar da burada kullanilir:
 *
 *   periyot_ms   node kac ms'de bir okunacak
 *   aktif        0 ise siraya hic alinmaz
 *   ad           yayinlanan JSON icindeki "ad" alani
 *
 * Ne okunacagini node'un tip kodu belirler — nh_tipler.c, o da
 * contract/registers.yaml'dan gelir. Yeni bir sensor tipi eklemek
 * bu dosyayi degistirmez.
 */

#ifndef NH_OLCUM_H
#define NH_OLCUM_H

#include "mqtt_client.h"

/*
 * Planlayiciyi baslatir. Bir kez.
 *
 * mqtt NULL olabilir ya da baglanti kopuk olabilir — okuma yine yapilir,
 * degerler loga basilir. Boylece ag olmadan da hat sinanabilir.
 */
void nh_olcum_baslat(esp_mqtt_client_handle_t mqtt, const char *konu_oneki);

#endif /* NH_OLCUM_H */
