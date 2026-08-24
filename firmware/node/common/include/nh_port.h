/*
 * NodeHub — donanım katmanı.
 *
 * Protokolün tamamı bu sekiz fonksiyonun üstünde çalışır. Başka bir işlemciye
 * geçildiğinde yalnızca port/ altındaki gerçekleme dosyası değişir.
 */

#ifndef NH_PORT_H
#define NH_PORT_H

#include <stdint.h>
#include <stdbool.h>

#define NH_UID_UZUNLUK  12u

void     nh_port_baslat(uint32_t baud);

/* Gönderir; DE'yi kaldırır, son bit hattan çıkana kadar bekler, indirir. */
void     nh_port_gonder(const uint8_t *veri, uint16_t uzunluk);

/* Bir çerçeve alır. Karakterler arası boşluk açılınca çerçeve bitmiş sayılır.
   Dönüş: alınan bayt sayısı, zaman aşımında 0. */
uint16_t nh_port_al(uint8_t *tampon, uint16_t en_fazla, uint32_t bekle_ms);

uint32_t nh_port_us(void);

/* Kurulum beklemesi sırasında hattı dinlemek için. */
void     nh_port_kenar_izle(bool acik);
bool     nh_port_kenar_oldu(void);
bool     nh_port_hat_bos(void);

void     nh_port_uid_oku(uint8_t uid[NH_UID_UZUNLUK]);

/* Kimliği kalıcı saklama. 0 = kimlik yok. */
bool     nh_port_kimlik_yaz(uint8_t adres);
uint8_t  nh_port_kimlik_oku(void);

#endif
