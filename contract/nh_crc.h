/* CRC-16/MODBUS — sözleşme: polinom 0xA001, başlangıç 0xFFFF, son XOR yok */

#ifndef NH_CRC_H
#define NH_CRC_H

#include <stdint.h>
#include <stdbool.h>

uint16_t nh_crc16(const uint8_t *veri, uint16_t uzunluk);

/* Çerçevenin sonuna CRC'yi ekler (önce düşük bayt) ve yeni uzunluğu döndürür. */
uint16_t nh_crc_ekle(uint8_t *cerceve, uint16_t uzunluk);

/* Son iki baytı CRC olarak doğrular. */
bool nh_crc_dogru(const uint8_t *cerceve, uint16_t uzunluk);

#endif
