/*
 * YENI BIR NODE YAPARKEN DOKUNACAGIN TEK DOSYA.
 *
 * Yapman gerekenler:
 *   1. NODE_TIPI'ne sozlesmeden bir kod ver
 *   2. Olcum degiskenlerini tanimla
 *   3. baslat() icinde sensoru kur
 *   4. olc() icinde olcumu yap
 *   5. olcumler[] dizisine register adreslerini yaz
 *
 * Modbus, kimlik atama, CRC ve zamanlama ortak katmanda. Onlara dokunma.
 */

#include "nh_node.h"

/* --------------------------------------------------------------- */
/* 1. Node kimligi                                                   */
/* --------------------------------------------------------------- */

#define NODE_TIPI    0x0001u        /* sozlesmedeki tip kodu       */
#define FW_SURUM     0x0100u        /* 1.0 — yuksek bayt ana surum */

/* --------------------------------------------------------------- */
/* 2. Olcum degiskenleri                                             */
/* --------------------------------------------------------------- */

static int16_t sicaklik;            /* x0.1 °C */

/* --------------------------------------------------------------- */
/* 3. Sensor kurulumu — bir kez, acilista                            */
/* --------------------------------------------------------------- */

static void baslat(void)
{
    /* I2C, SPI, ADC — sensorun ne istiyorsa.
       Su an gercek sensor yok, olcum uydurma. */
    sicaklik = NH_GECERSIZ_INT16;
}

/* --------------------------------------------------------------- */
/* 4. Olcum — periyodik                                              */
/* --------------------------------------------------------------- */

static void olc(void)
{
    /* Gercek sensor gelince burasi degisecek. Simdilik 20.0 ile 25.0
       arasinda gezinen sahte bir deger uretiyoruz ki hattan gelen
       verinin degistigi gorulsun. */
    static int16_t sayac = 200;

    sayac++;
    if (sayac > 250) {
        sayac = 200;
    }
    sicaklik = sayac;

    /* Sensor okunamazsa:
         sicaklik = NH_GECERSIZ_INT16;
         nh_durum_ayarla(NH_DURUM_SENSOR_HATASI);
       Duzelince:
         nh_durum_temizle(NH_DURUM_SENSOR_HATASI);            */
}

/* --------------------------------------------------------------- */
/* 5. Olcum alanlari — sozlesmedeki register haritasina gore         */
/* --------------------------------------------------------------- */

static const nh_olcum_t olcumler[] = {
    { .offset = 0x0010, .tip = NH_TIP_INT16, .kaynak = &sicaklik },
};

/* --------------------------------------------------------------- */
/* Ortak katmanin okudugu tanim                                      */
/* --------------------------------------------------------------- */

const nh_node_tanim_t nh_node_tanim = {
    .tip        = NODE_TIPI,
    .fw_surum   = FW_SURUM,
    .baslat     = baslat,
    .olc        = olc,
    .periyot_ms = 1000,
    .olcumler   = olcumler,
    .olcum_adet = sizeof(olcumler) / sizeof(olcumler[0]),
};
