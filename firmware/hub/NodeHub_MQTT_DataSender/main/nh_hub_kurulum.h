/* NodeHub — hub tarafi kimlik atama. */

#ifndef NH_HUB_KURULUM_H
#define NH_HUB_KURULUM_H

#include <stdint.h>

#define NH_HUB_EN_FAZLA_NODE   32

typedef struct {
    uint8_t  uid[12];
    uint16_t tip;
    uint16_t fw_surum;
    uint8_t  adres;
} nh_hub_node_t;

/* RS485 hattini kurar. Bir kez, acilista. */
void nh_hub_baslat(void);

/* Bir tarama turu yapar. Bulunan node sayisini dondurur. */
uint8_t nh_hub_tara(void);

/* Son taramanin sonucu. */
const nh_hub_node_t *nh_hub_liste(uint8_t *adet);

#endif
