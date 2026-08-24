/* Ortak katmanın iç arayüzü — node yazarını ilgilendirmez. */

#ifndef NH_KURULUM_H
#define NH_KURULUM_H

#include <stdint.h>
#include <stdbool.h>
#include "nh_node.h"

/* Aktif node tanımı. nh_baslat() ile verilir; ortak katman içindeki
   dosyalar node tanımına yalnızca buradan erişir. Böylece kütüphane
   projedeki bir sembole bağımlı olmaz. */
const nh_node_tanim_t *nh_tanim(void);

/* --- kimlik saklama --- */
void    nh_kayit_yukle(void);
uint8_t nh_kayit_kimlik(void);
bool    nh_kayit_ata(uint8_t adres);
void    nh_kayit_sil(void);

/* --- kurulum protokolü --- */
void nh_kurulum_baslat(void);
bool nh_kurulum_cerceve(const uint8_t *cerceve, uint16_t uzunluk);

/* --- Modbus slave --- */
bool nh_modbus_cerceve(const uint8_t *cerceve, uint16_t uzunluk);

#endif
