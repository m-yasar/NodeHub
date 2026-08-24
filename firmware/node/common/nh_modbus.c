/*
 * NodeHub — Modbus RTU slave, node tarafı.
 *
 * Register haritası sözleşmeden gelir:
 *
 *   0x0000  node tipi
 *   0x0001  firmware surumu
 *   0x0002  durum bayraklari
 *   0x0003  uid (6 register)
 *   0x0009  ayrilmis
 *   0x0010  olcumler — nh_node_tanim'dan
 *
 * 16 bitten buyuk degerler islemcinin bellek duzeninde tasinir: dusuk word
 * once. Her register hatta standart Modbus'a gore yuksek bayt once gider.
 */

#include <string.h>

#include "nh_kurulum.h"
#include "nh_node.h"
#include "nh_port.h"
#include "nh_crc.h"
#include "nodehub_protocol.h"

#define REG_NODE_TIPI    0x0000u
#define REG_FW_SURUM     0x0001u
#define REG_DURUM        0x0002u
#define REG_UID          0x0003u
#define REG_OLCUM_ILK    0x0010u

#define HATA_FONKSIYON   0x01u
#define HATA_ADRES       0x02u

static uint8_t s_uid[NH_UID_UZUNLUK];
static bool    s_uid_hazir;

/* --------------------------------------------------------------- */

static uint8_t olcum_genislik(nh_veri_tipi_t tip)
{
    return (tip == NH_TIP_UINT32 || tip == NH_TIP_INT32) ? 2u : 1u;
}

static uint32_t olcum_ham(const nh_olcum_t *o)
{
    switch (o->tip) {
    case NH_TIP_UINT16: return *(const uint16_t *)o->kaynak;
    case NH_TIP_INT16:  return (uint32_t)(uint16_t)*(const int16_t *)o->kaynak;
    case NH_TIP_UINT32: return *(const uint32_t *)o->kaynak;
    case NH_TIP_INT32:  return (uint32_t)*(const int32_t *)o->kaynak;
    default:            return 0;
    }
}

/* Adresteki register degerini uretir. Tanimsiz adres icin bulundu=false. */
static uint16_t reg_oku(uint16_t adres, bool *bulundu)
{
    *bulundu = true;

    if (!s_uid_hazir) {
        nh_port_uid_oku(s_uid);
        s_uid_hazir = true;
    }

    if (adres == REG_NODE_TIPI) return nh_tanim()->tip;
    if (adres == REG_FW_SURUM)  return nh_tanim()->fw_surum;
    if (adres == REG_DURUM)     return nh_durum_oku();

    if (adres >= REG_UID && adres < REG_UID + (NH_UID_UZUNLUK / 2)) {
        uint16_t i = (uint16_t)((adres - REG_UID) * 2);
        return (uint16_t)(((uint16_t)s_uid[i] << 8) | s_uid[i + 1]);
    }

    if (adres >= REG_OLCUM_ILK) {
        for (uint8_t k = 0; k < nh_tanim()->olcum_adet; k++) {
            const nh_olcum_t *o = &nh_tanim()->olcumler[k];
            uint8_t g = olcum_genislik(o->tip);
            if (adres >= o->offset && adres < o->offset + g) {
                uint32_t v = olcum_ham(o);
                if (g == 1u) {
                    return (uint16_t)v;
                }
                /* dusuk word once — bellek duzeni */
                return (adres == o->offset) ? (uint16_t)(v & 0xFFFFu)
                                            : (uint16_t)(v >> 16);
            }
        }
    }

    /* Ayrilmis alanlar sifir doner, tanimsiz alanlar hata. */
    if (adres < REG_OLCUM_ILK) {
        return 0;
    }

    *bulundu = false;
    return 0;
}

/* --------------------------------------------------------------- */

static void hata_gonder(uint8_t adres, uint8_t fk, uint8_t kod)
{
    uint8_t c[5];
    c[0] = adres;
    c[1] = (uint8_t)(fk | 0x80u);
    c[2] = kod;
    uint16_t n = nh_crc_ekle(c, 3);
    nh_port_gonder(c, n);
}

static void oku_isle(const uint8_t *istek)
{
    uint8_t  adres = istek[0];
    uint16_t ilk   = (uint16_t)(((uint16_t)istek[2] << 8) | istek[3]);
    uint16_t sayi  = (uint16_t)(((uint16_t)istek[4] << 8) | istek[5]);

    if (sayi == 0 || sayi > NH_EN_FAZLA_REGISTER) {
        hata_gonder(adres, NH_OKUMA_FK, HATA_ADRES);
        return;
    }

    uint8_t c[5 + 2 * NH_EN_FAZLA_REGISTER];
    uint16_t n = 0;

    c[n++] = adres;
    c[n++] = NH_OKUMA_FK;
    c[n++] = (uint8_t)(sayi * 2);

    for (uint16_t i = 0; i < sayi; i++) {
        bool bulundu;
        uint16_t v = reg_oku((uint16_t)(ilk + i), &bulundu);
        if (!bulundu) {
            hata_gonder(adres, NH_OKUMA_FK, HATA_ADRES);
            return;
        }
        c[n++] = (uint8_t)(v >> 8);      /* Modbus: yuksek bayt once */
        c[n++] = (uint8_t)(v & 0xFFu);
    }

    n = nh_crc_ekle(c, n);
    nh_port_gonder(c, n);
}

/* --------------------------------------------------------------- */

bool nh_modbus_cerceve(const uint8_t *c, uint16_t n)
{
    uint8_t kimlik = nh_kayit_kimlik();

    if (kimlik == 0 || n < 4 || c[0] != kimlik) {
        return false;                 /* bize degil */
    }

    switch (c[1]) {
    case NH_OKUMA_FK:
        if (n == 8) {
            oku_isle(c);
        }
        return true;

    case NH_YAZMA_FK:
        /* Ayarlar blogu sozlesmede ayrildi ama henuz tanimli degil. */
        hata_gonder(kimlik, NH_YAZMA_FK, HATA_ADRES);
        return true;

    default:
        hata_gonder(kimlik, c[1], HATA_FONKSIYON);
        return true;
    }
}
