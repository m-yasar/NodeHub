/*
 * NodeHub — node tipi tablosu.
 * Kaynak: contract/registers.yaml. Elle yazildi, ureteci bekliyor.
 */

#include <stdbool.h>
#include <stddef.h>

#include "nh_tipler.h"

/* --- 0x0001 · Sicaklik ------------------------------------------ */

static const nh_tip_olcum_t s_tip_0001[] = {
    { .ad = "sicaklik", .offset = 0x0010, .tip = NH_VT_INT16,
      .olcek = 0.1f, .birim = "°C" },
};

/* --- Tablo ------------------------------------------------------- */

static const nh_tip_t s_tipler[] = {
    { .tip        = 0x0001,
      .ad         = "Sicaklik",
      .olcumler   = s_tip_0001,
      .olcum_adet = 1,
      .ilk_offset = 0x0010,
      .reg_adet   = 1 },
};

#define TIP_ADET (sizeof(s_tipler) / sizeof(s_tipler[0]))

const nh_tip_t *nh_tip_bul(uint16_t tip)
{
    for (size_t i = 0; i < TIP_ADET; i++) {
        if (s_tipler[i].tip == tip) {
            return &s_tipler[i];
        }
    }
    return NULL;
}

const char *nh_tip_adi(uint16_t tip)
{
    const nh_tip_t *t = nh_tip_bul(tip);
    return t ? t->ad : NULL;
}

/*
 * Ham register'lari birlestirir.
 *
 * Sozlesme: 32 bitlik olcum iki ardisik register kaplar ve bayt
 * sirasi little-endian — dusuk register once.
 */
static uint32_t ham_al(const nh_tip_olcum_t *olcum, const uint16_t *ham,
                       uint16_t ilk_offset)
{
    uint16_t i = (uint16_t)(olcum->offset - ilk_offset);

    if (olcum->tip == NH_VT_INT32 || olcum->tip == NH_VT_UINT32) {
        return ((uint32_t)ham[i + 1] << 16) | ham[i];
    }
    return ham[i];
}

bool nh_tip_gecersiz(const nh_tip_olcum_t *olcum, const uint16_t *ham,
                     uint16_t ilk_offset)
{
    uint32_t h = ham_al(olcum, ham, ilk_offset);

    /* Sozlesme "gecersiz_deger" bolumu. */
    switch (olcum->tip) {
    case NH_VT_INT16:  return h == 0x8000u;
    case NH_VT_UINT16: return h == 0xFFFFu;
    case NH_VT_INT32:  return h == 0x80000000u;
    case NH_VT_UINT32: return h == 0xFFFFFFFFu;
    }
    return false;
}

float nh_tip_degeri(const nh_tip_olcum_t *olcum, const uint16_t *ham,
                    uint16_t ilk_offset)
{
    uint32_t h = ham_al(olcum, ham, ilk_offset);

    switch (olcum->tip) {
    case NH_VT_INT16:  return (float)(int16_t)h  * olcum->olcek;
    case NH_VT_INT32:  return (float)(int32_t)h  * olcum->olcek;
    case NH_VT_UINT16: return (float)(uint16_t)h * olcum->olcek;
    case NH_VT_UINT32: return (float)h           * olcum->olcek;
    }
    return 0.0f;
}
