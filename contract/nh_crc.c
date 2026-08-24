#include <stdbool.h>
#include "nh_crc.h"

uint16_t nh_crc16(const uint8_t *veri, uint16_t uzunluk)
{
    uint16_t crc = 0xFFFFu;

    for (uint16_t i = 0; i < uzunluk; i++) {
        crc ^= veri[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 1u) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001u);
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

uint16_t nh_crc_ekle(uint8_t *cerceve, uint16_t uzunluk)
{
    uint16_t crc = nh_crc16(cerceve, uzunluk);
    cerceve[uzunluk]     = (uint8_t)(crc & 0xFFu);        /* önce düşük bayt */
    cerceve[uzunluk + 1] = (uint8_t)(crc >> 8);
    return (uint16_t)(uzunluk + 2);
}

bool nh_crc_dogru(const uint8_t *cerceve, uint16_t uzunluk)
{
    if (uzunluk < 3) {
        return false;
    }
    uint16_t hesap = nh_crc16(cerceve, (uint16_t)(uzunluk - 2));
    uint16_t gelen = (uint16_t)cerceve[uzunluk - 2] |
                     (uint16_t)((uint16_t)cerceve[uzunluk - 1] << 8);
    return hesap == gelen;
}
