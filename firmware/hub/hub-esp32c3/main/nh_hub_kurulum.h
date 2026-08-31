/* NodeHub — hub tarafi kimlik atama. */

#ifndef NH_HUB_KURULUM_H
#define NH_HUB_KURULUM_H

#include <stdbool.h>
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

/*
 * Isletme okumasi — FC 03.
 *
 * adet register'i tek istekte okur, cikti[] dizisine yazar.
 * NH_DENEME kez dener. Hat kilidini kendisi alir.
 */
bool nh_hub_oku(uint8_t adres, uint16_t ilk_reg, uint16_t adet,
                uint16_t *cikti);

/* Son taramanin sonucu. */
const nh_hub_node_t *nh_hub_liste(uint8_t *adet);

/*
 * Node'lara "kimligini unut" der. Cevap beklenmez.
 *
 *   hedef = NH_ADRES_YAYIN (0x00)  hattaki butun node'lar
 *   hedef = 0x01 .. 0xF7           yalnizca o adrestekI node
 *
 * Silinen node bakir hale doner ve bir sonraki taramada yeniden
 * kimlik alir. Hattan cikmis bir node yayin silmeyi duymaz.
 */
void nh_hub_unut(uint8_t hedef);

#endif
